#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/exception/exc_base.h"
#include "../../../CommonLib/stream/stream.h"
#include "../../../CommonLib/compress/zlib/zlib.h"
#include "RTreeEntry.h"
#include <vector>
#include <cstring>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		// Node compressor of TRTree: the entries are written by columns (all minX, all minY, all maxX,
		// all maxY, all values) and the block is compressed with zlib. If zlib doesn't make it smaller
		// the block is stored as it is.
		//
		//   uint8  mode       0 - raw, 1 - zlib
		//   uint32 size       bytes that follow
		//   data
		//
		// Coordinates and values are copied as they are in memory (as the other encoders do).
		template <class _TCoord>
		class TRTreeZlibCompressor
		{
		public:
			typedef _TCoord TCoord;
			typedef TRTreeEntry<TCoord> TEntry;

			static constexpr uint32_t RawEntrySize = 4 * sizeof(TCoord) + sizeof(int64_t);
			static constexpr uint32_t HeadSize = sizeof(uint8_t) + sizeof(uint32_t);

			enum EMode : uint8_t
			{
				eRaw = 0,
				eZlib = 1
			};

			explicit TRTreeZlibCompressor(int32_t nCompressLevel = 6) : m_nCompressLevel(nCompressLevel)
			{
			}

			// upper estimate of how much the encoded size of a node of nCount entries grows when one entry
			// is added, removed or changed (TRTree keeps an upper estimate of the node size with it)
			static uint32_t MaxGrowth(size_t /*nCount*/)
			{
				return RawEntrySize + 8;
			}

			// true if the entries fit in nMaxSize bytes (the zlib check only when the raw block doesn't fit)
			bool Fits(const TEntry* pEntries, size_t nCount, uint32_t nMaxSize)
			{
				if (HeadSize + nCount * RawEntrySize <= nMaxSize)
					return true;

				return Prepare(pEntries, nCount, nMaxSize);
			}

			// encodes the entries into the internal buffer; false if they don't fit in nMaxSize bytes
			bool Prepare(const TEntry* pEntries, size_t nCount, uint32_t nMaxSize)
			{
				m_raw.resize(nCount * RawEntrySize);
				byte_t* p = m_raw.data();
				p = WriteColumn(p, pEntries, nCount, [](const TEntry& e) { return e.m_rect.m_minX; });
				p = WriteColumn(p, pEntries, nCount, [](const TEntry& e) { return e.m_rect.m_minY; });
				p = WriteColumn(p, pEntries, nCount, [](const TEntry& e) { return e.m_rect.m_maxX; });
				p = WriteColumn(p, pEntries, nCount, [](const TEntry& e) { return e.m_rect.m_maxY; });
				p = WriteColumn(p, pEntries, nCount, [](const TEntry& e) { return e.m_nValue; });

				uLongf nCompSize = compressBound((uLong)m_raw.size());
				m_comp.resize(nCompSize);
				int res = compress2(m_comp.data(), &nCompSize, m_raw.data(), (uLong)m_raw.size(), m_nCompressLevel);
				if (res != Z_OK)
					throw CommonLib::CExcBase("RTreeZlibCompressor: compress error {0}", res);

				if (nCompSize < m_raw.size())
				{
					m_mode = eZlib;
					m_nOutSize = (uint32_t)nCompSize;
				}
				else
				{
					m_mode = eRaw;
					m_nOutSize = (uint32_t)m_raw.size();
				}

				return HeadSize + m_nOutSize <= nMaxSize;
			}

			// bytes of the block of the last Prepare / Read
			uint32_t EncodedSize() const { return HeadSize + m_nOutSize; }

			// writes the block made by the last Prepare
			void Write(CommonLib::IWriteStream* pStream)
			{
				pStream->Write((uint8_t)m_mode);
				pStream->Write(m_nOutSize);
				pStream->WriteBytes(m_mode == eZlib ? m_comp.data() : m_raw.data(), m_nOutSize);
			}

			void Read(size_t nCount, std::vector<TEntry>& entries, CommonLib::IReadStream* pStream)
			{
				uint8_t mode = pStream->ReadByte();
				uint32_t nSize = pStream->ReadIntu32();
				m_nOutSize = nSize;

				m_raw.resize(nCount * RawEntrySize);
				if (mode == eRaw)
				{
					if (nSize != m_raw.size())
						throw CommonLib::CExcBase("RTreeZlibCompressor: wrong raw size {0}, expected {1}", nSize, (uint64_t)m_raw.size());
					pStream->ReadBytes(m_raw.data(), nSize);
				}
				else if (mode == eZlib)
				{
					m_comp.resize(nSize);
					pStream->ReadBytes(m_comp.data(), nSize);
					uLongf nRawSize = (uLongf)m_raw.size();
					int res = uncompress(m_raw.data(), &nRawSize, m_comp.data(), nSize);
					if (res != Z_OK || nRawSize != m_raw.size())
						throw CommonLib::CExcBase("RTreeZlibCompressor: uncompress error {0}, size {1}, expected {2}", res, (uint64_t)nRawSize, (uint64_t)m_raw.size());
				}
				else
					throw CommonLib::CExcBase("RTreeZlibCompressor: wrong mode {0}", (uint32_t)mode);

				entries.resize(nCount);
				const byte_t* p = m_raw.data();
				p = ReadColumn(p, entries, [](TEntry& e) -> TCoord& { return e.m_rect.m_minX; });
				p = ReadColumn(p, entries, [](TEntry& e) -> TCoord& { return e.m_rect.m_minY; });
				p = ReadColumn(p, entries, [](TEntry& e) -> TCoord& { return e.m_rect.m_maxX; });
				p = ReadColumn(p, entries, [](TEntry& e) -> TCoord& { return e.m_rect.m_maxY; });
				p = ReadColumn(p, entries, [](TEntry& e) -> int64_t& { return e.m_nValue; });
			}

		private:
			template<class TGet>
			static byte_t* WriteColumn(byte_t* p, const TEntry* pEntries, size_t nCount, TGet get)
			{
				for (size_t i = 0; i < nCount; ++i)
				{
					auto v = get(pEntries[i]);
					memcpy(p, &v, sizeof(v));
					p += sizeof(v);
				}
				return p;
			}

			template<class TGet>
			static const byte_t* ReadColumn(const byte_t* p, std::vector<TEntry>& entries, TGet get)
			{
				for (TEntry& e : entries)
				{
					auto& v = get(e);
					memcpy(&v, p, sizeof(v));
					p += sizeof(v);
				}
				return p;
			}

		private:
			int32_t m_nCompressLevel;
			std::vector<byte_t> m_raw;
			std::vector<byte_t> m_comp;
			EMode m_mode{ eRaw };
			uint32_t m_nOutSize{ 0 };
		};
	}
	}
}
