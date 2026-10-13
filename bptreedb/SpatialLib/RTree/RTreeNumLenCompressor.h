#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/exception/exc_base.h"
#include "../../../CommonLib/stream/stream.h"
#include "../../../CommonLib/stream/MemoryStream.h"
#include "../../../CommonLib/stream/FixedMemoryStream.h"
#include "../../BPtreeLib/Compressor/CompressorParams.h"
#include "../../BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../BPtreeLib/Compressor/EntropyEncoder/SignedNumLenEncoder.h"
#include "RTreeEntry.h"
#include <vector>
#include <numeric>
#include <type_traits>
#include <cstring>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		// Node compressor of TRTree for integer coordinates (fixed point): the entries are sorted by minX
		// (then minY) and written as 5 streams of the NumLen entropy encoders (BPtreeLib):
		//   minX   - deltas between the sorted entries          (unsigned)
		//   minY   - offsets from the min Y of the node          (unsigned)
		//   width  - maxX - minX                                 (unsigned)
		//   height - maxY - minY                                 (unsigned)
		//   value  - deltas between the ids / child addresses   (signed)
		// The entries are decoded in the sorted order (the order in a node doesn't matter to the tree).
		// If the streams are bigger than the raw columns (few entries), the raw columns are written.
		//
		//   uint8  mode       0 - raw, 2 - NumLen
		//   uint32 size       bytes that follow
		//   raw:    minX[], minY[], maxX[], maxY[], value[]
		//   NumLen: TCoord minX of the first, TCoord min Y of the node, int64 value of the first,
		//           5 x (uint32 size, stream)
		template <class _TCoord>
		class TRTreeNumLenCompressor
		{
		public:
			typedef _TCoord TCoord;
			typedef TRTreeEntry<TCoord> TEntry;
			typedef typename std::make_unsigned<TCoord>::type TUCoord;

			static_assert(std::is_integral<TCoord>::value, "TRTreeNumLenCompressor needs integer coordinates (fixed point)");

			static constexpr uint32_t RawEntrySize = 4 * sizeof(TCoord) + sizeof(int64_t);
			static constexpr uint32_t HeadSize = sizeof(uint8_t) + sizeof(uint32_t);

			enum EMode : uint8_t
			{
				eRaw = 0,
				eNumLen = 2
			};

			explicit TRTreeNumLenCompressor(int32_t /*nCompressLevel, not used*/ = 0) :
				m_pParams(MakeParams()),
				m_minX(TAllocsSetPtr(), m_pParams, eLeafKey),
				m_minY(TAllocsSetPtr(), m_pParams, eLeafKey),
				m_width(TAllocsSetPtr(), m_pParams, eLeafKey),
				m_height(TAllocsSetPtr(), m_pParams, eLeafKey),
				m_value(TAllocsSetPtr(), m_pParams, eLeafValue)
			{
			}

			// upper estimate of how much the encoded size grows when one entry is added, removed or changed:
			// the entry itself, plus a lower node min Y (or a removed neighbour) can add a bit to every
			// symbol of the node, plus the bit length tables of the 5 streams
			static uint32_t MaxGrowth(size_t nCount)
			{
				return RawEntrySize + (uint32_t)(nCount / 8) * 2 + 5 * 8;
			}

			bool Fits(const TEntry* pEntries, size_t nCount, uint32_t nMaxSize)
			{
				if (HeadSize + nCount * RawEntrySize <= nMaxSize)
					return true;

				return Prepare(pEntries, nCount, nMaxSize);
			}

			bool Prepare(const TEntry* pEntries, size_t nCount, uint32_t nMaxSize)
			{
				m_out.clear();
				if (!nCount || !EncodeNumLen(pEntries, nCount) || m_out.size() >= nCount * RawEntrySize)
				{
					m_mode = eRaw;
					m_out.resize(nCount * RawEntrySize);
					WriteRaw(pEntries, nCount, m_out.data());
				}
				else
					m_mode = eNumLen;

				return HeadSize + m_out.size() <= nMaxSize;
			}

			uint32_t EncodedSize() const { return HeadSize + (uint32_t)m_out.size(); }

			void Write(CommonLib::IWriteStream* pStream)
			{
				pStream->Write((uint8_t)m_mode);
				pStream->Write((uint32_t)m_out.size());
				if (!m_out.empty())
					pStream->WriteBytes(m_out.data(), m_out.size());
			}

			void Read(size_t nCount, std::vector<TEntry>& entries, CommonLib::IReadStream* pStream)
			{
				uint8_t mode = pStream->ReadByte();
				uint32_t nSize = pStream->ReadIntu32();
				m_out.resize(nSize);
				if (nSize)
					pStream->ReadBytes(m_out.data(), nSize);
				entries.resize(nCount);

				if (mode == eRaw)
				{
					if (nSize != nCount * RawEntrySize)
						throw CommonLib::CExcBase("RTreeNumLenCompressor: wrong raw size {0}, count {1}", nSize, (uint64_t)nCount);
					ReadRaw(m_out.data(), entries);
				}
				else if (mode == eNumLen)
					DecodeNumLen(entries);
				else
					throw CommonLib::CExcBase("RTreeNumLenCompressor: wrong mode {0}", (uint32_t)mode);
			}

		private:
			static TCompressorParamsBasePtr MakeParams()
			{
				// the NumLen encoders read their params in the constructor
				TCompressorParamsBasePtr pParams(new CompressorParamsBase());
				for (ECompressParams id : { eInnerKey, eInnerValue, eLeafKey, eLeafValue })
					pParams->AddCompressParams(TCompressorParamsPtr(new CompressorParams()), id);
				return pParams;
			}

			// the ids are any int64: a delta near the int64 range doesn't go through the signed encoder
			static bool IsSafeDelta(int64_t d)
			{
				return d > -(int64_t(1) << 62) && d < (int64_t(1) << 62);
			}

			bool EncodeNumLen(const TEntry* pEntries, size_t n)
			{
				m_order.resize(n);
				std::iota(m_order.begin(), m_order.end(), 0);
				std::sort(m_order.begin(), m_order.end(), [pEntries](uint32_t a, uint32_t b)
				{
					const TRect2D<TCoord>& ra = pEntries[a].m_rect;
					const TRect2D<TCoord>& rb = pEntries[b].m_rect;
					// a full key: the same entries always give the same encoding (sizes stay reproducible)
					if (ra.m_minX != rb.m_minX) return ra.m_minX < rb.m_minX;
					if (ra.m_minY != rb.m_minY) return ra.m_minY < rb.m_minY;
					if (ra.m_maxX != rb.m_maxX) return ra.m_maxX < rb.m_maxX;
					if (ra.m_maxY != rb.m_maxY) return ra.m_maxY < rb.m_maxY;
					return pEntries[a].m_nValue < pEntries[b].m_nValue;
				});

				const TEntry& first = pEntries[m_order[0]];
				TCoord nodeMinY = first.m_rect.m_minY;
				for (size_t i = 0; i < n; ++i)
					nodeMinY = (std::min)(nodeMinY, pEntries[i].m_rect.m_minY);

				m_minX.Clear(); m_minY.Clear(); m_width.Clear(); m_height.Clear(); m_value.Clear();
				m_symX.resize(n); m_symY.resize(n); m_symW.resize(n); m_symH.resize(n); m_symV.resize(n);
				for (size_t i = 0; i < n; ++i)
				{
					const TEntry& e = pEntries[m_order[i]];
					const TEntry& prev = pEntries[m_order[i ? i - 1 : 0]];
					m_symX[i] = (uint64_t)(TUCoord)(e.m_rect.m_minX - prev.m_rect.m_minX);
					m_symY[i] = (uint64_t)(TUCoord)(e.m_rect.m_minY - nodeMinY);
					m_symW[i] = (uint64_t)(TUCoord)(e.m_rect.m_maxX - e.m_rect.m_minX);
					m_symH[i] = (uint64_t)(TUCoord)(e.m_rect.m_maxY - e.m_rect.m_minY);
					m_symV[i] = (int64_t)((uint64_t)e.m_nValue - (uint64_t)prev.m_nValue);
					if (!IsSafeDelta(m_symV[i]))
						return false;
				}

				// symbol 0 of minX and value is the first entry itself, written raw
				m_out.resize(sizeof(TCoord) * 2 + sizeof(int64_t));
				byte_t* p = m_out.data();
				memcpy(p, &first.m_rect.m_minX, sizeof(TCoord)); p += sizeof(TCoord);
				memcpy(p, &nodeMinY, sizeof(TCoord)); p += sizeof(TCoord);
				memcpy(p, &first.m_nValue, sizeof(int64_t));

				EncodeColumn(m_minX, m_symX, 1);
				EncodeColumn(m_minY, m_symY, 0);
				EncodeColumn(m_width, m_symW, 0);
				EncodeColumn(m_height, m_symH, 0);
				EncodeColumn(m_value, m_symV, 1);
				return true;
			}

			// appends uint32 size + the stream of symbols[nFrom..]
			template<class TEncoder, class TSymbol>
			void EncodeColumn(TEncoder& enc, const std::vector<TSymbol>& symbols, size_t nFrom)
			{
				for (size_t i = nFrom; i < symbols.size(); ++i)
					enc.AddSymbol(symbols[i]);

				size_t nSizePos = m_out.size();
				m_out.resize(nSizePos + sizeof(uint32_t));
				uint32_t nSize = 0;
				if (symbols.size() > nFrom)
				{
					m_buf.assign(enc.GetCompressSize() + 64, 0);     // the bit writer needs a zeroed buffer
					CommonLib::CFxMemoryWriteStream stream;
					stream.AttachBuffer(m_buf.data(), m_buf.size());
					bool bOk = enc.BeginEncoding(&stream);
					for (size_t i = nFrom; bOk && i < symbols.size(); ++i)
						bOk = enc.EncodeSymbol(symbols[i]);
					if (!bOk || !enc.FinishEncoding(&stream))
						throw CommonLib::CExcBase("RTreeNumLenCompressor: the stream is bigger than its estimate");
					nSize = (uint32_t)stream.Pos();
					m_out.insert(m_out.end(), m_buf.begin(), m_buf.begin() + nSize);
				}
				memcpy(m_out.data() + nSizePos, &nSize, sizeof(nSize));
			}

			template<class TEncoder, class TSymbol>
			const byte_t* DecodeColumn(TEncoder& dec, const byte_t* p, const byte_t* pEnd, std::vector<TSymbol>& symbols, size_t nCount, size_t nFrom)
			{
				uint32_t nSize = 0;
				if (p + sizeof(nSize) > pEnd)
					throw CommonLib::CExcBase("RTreeNumLenCompressor: truncated block");
				memcpy(&nSize, p, sizeof(nSize));
				p += sizeof(nSize);
				if (p + nSize > pEnd)
					throw CommonLib::CExcBase("RTreeNumLenCompressor: truncated stream");

				symbols.resize(nCount);
				if (nCount > nFrom)
				{
					CommonLib::CReadMemoryStream stream;
					stream.AttachBuffer((byte_t*)p, nSize);
					dec.Clear();
					dec.BeginDecoding(&stream);
					for (size_t i = nFrom; i < nCount; ++i)
						dec.DecodeSymbol(symbols[i]);
					dec.FinishDecoding(&stream);
				}
				return p + nSize;
			}

			void DecodeNumLen(std::vector<TEntry>& entries)
			{
				size_t n = entries.size();
				const byte_t* p = m_out.data();
				const byte_t* pEnd = p + m_out.size();
				if (m_out.size() < sizeof(TCoord) * 2 + sizeof(int64_t))
					throw CommonLib::CExcBase("RTreeNumLenCompressor: truncated block");

				TCoord firstX, nodeMinY;
				int64_t firstValue;
				memcpy(&firstX, p, sizeof(TCoord)); p += sizeof(TCoord);
				memcpy(&nodeMinY, p, sizeof(TCoord)); p += sizeof(TCoord);
				memcpy(&firstValue, p, sizeof(int64_t)); p += sizeof(int64_t);

				p = DecodeColumn(m_minX, p, pEnd, m_symX, n, 1);
				p = DecodeColumn(m_minY, p, pEnd, m_symY, n, 0);
				p = DecodeColumn(m_width, p, pEnd, m_symW, n, 0);
				p = DecodeColumn(m_height, p, pEnd, m_symH, n, 0);
				p = DecodeColumn(m_value, p, pEnd, m_symV, n, 1);

				TCoord x = firstX;
				int64_t v = firstValue;
				for (size_t i = 0; i < n; ++i)
				{
					if (i)
					{
						x = (TCoord)((TUCoord)x + (TUCoord)m_symX[i]);
						v = (int64_t)((uint64_t)v + (uint64_t)m_symV[i]);
					}
					TRect2D<TCoord>& r = entries[i].m_rect;
					r.m_minX = x;
					r.m_minY = (TCoord)((TUCoord)nodeMinY + (TUCoord)m_symY[i]);
					r.m_maxX = (TCoord)((TUCoord)x + (TUCoord)m_symW[i]);
					r.m_maxY = (TCoord)((TUCoord)r.m_minY + (TUCoord)m_symH[i]);
					entries[i].m_nValue = v;
				}
			}

			static void WriteRaw(const TEntry* pEntries, size_t n, byte_t* p)
			{
				for (size_t i = 0; i < n; ++i) { memcpy(p, &pEntries[i].m_rect.m_minX, sizeof(TCoord)); p += sizeof(TCoord); }
				for (size_t i = 0; i < n; ++i) { memcpy(p, &pEntries[i].m_rect.m_minY, sizeof(TCoord)); p += sizeof(TCoord); }
				for (size_t i = 0; i < n; ++i) { memcpy(p, &pEntries[i].m_rect.m_maxX, sizeof(TCoord)); p += sizeof(TCoord); }
				for (size_t i = 0; i < n; ++i) { memcpy(p, &pEntries[i].m_rect.m_maxY, sizeof(TCoord)); p += sizeof(TCoord); }
				for (size_t i = 0; i < n; ++i) { memcpy(p, &pEntries[i].m_nValue, sizeof(int64_t)); p += sizeof(int64_t); }
			}

			static void ReadRaw(const byte_t* p, std::vector<TEntry>& entries)
			{
				for (TEntry& e : entries) { memcpy(&e.m_rect.m_minX, p, sizeof(TCoord)); p += sizeof(TCoord); }
				for (TEntry& e : entries) { memcpy(&e.m_rect.m_minY, p, sizeof(TCoord)); p += sizeof(TCoord); }
				for (TEntry& e : entries) { memcpy(&e.m_rect.m_maxX, p, sizeof(TCoord)); p += sizeof(TCoord); }
				for (TEntry& e : entries) { memcpy(&e.m_rect.m_maxY, p, sizeof(TCoord)); p += sizeof(TCoord); }
				for (TEntry& e : entries) { memcpy(&e.m_nValue, p, sizeof(int64_t)); p += sizeof(int64_t); }
			}

		private:
			TCompressorParamsBasePtr m_pParams;
			CUnsignedNumLenEncoder m_minX;
			CUnsignedNumLenEncoder m_minY;
			CUnsignedNumLenEncoder m_width;
			CUnsignedNumLenEncoder m_height;
			CSignedNumLenEncoder m_value;

			std::vector<uint32_t> m_order;
			std::vector<uint64_t> m_symX, m_symY, m_symW, m_symH;
			std::vector<int64_t> m_symV;
			std::vector<byte_t> m_buf;
			std::vector<byte_t> m_out;
			EMode m_mode{ eRaw };
		};
	}
	}
}
