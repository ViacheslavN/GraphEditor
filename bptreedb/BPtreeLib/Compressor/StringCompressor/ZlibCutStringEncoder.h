#pragma once

#include "../../DeVector.h"
#include "../CompressorParams.h"
#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/alloc/alloc.h"
#include "../../../../CommonLib/alloc/stl_alloc.h"
#include "../../../../CommonLib/compress/zlib/zlib.h"
#include "../../StringVal.h"
#include "../../BPTreeContext.h"
#include "../../AllocsSet.h"
#include <vector>
#include <cstring>
#include <algorithm>

namespace bptreedb
{
	// String values (with the terminating zeros) in one raw deflate stream, for TCutNodeCompressor:
	// EncodeCut writes as many first values as fit in the space and says how many.
	//
	// deflate keeps its output inside (up to 16K symbols) until a flush, so the space used isn't known value by
	// value. The values go in chunks: a chunk is half of the input that fits in the space left by the ratio
	// measured so far, then Z_SYNC_FLUSH - the output so far is exact and complete (a checkpoint). The chunks
	// get smaller as the page fills. A chunk that doesn't fit is dropped: the stream ends at the last
	// checkpoint, the values of the chunk go to the next node. The last chunk of all the values ends the stream
	// (Z_FINISH). So the node is encoded once; the flushes cost some bytes (block headers) per page.
	// Only when the first chunk doesn't fit (the ratio of the node far too optimistic: f.e. a run of equal values,
	// then values that don't compress) it is encoded again with a smaller ratio - until the first chunk fits; with
	// the ratio 1 it always does (half of the space of input, a value is at most a part of the page).
	//
	// The stream: raw deflate (no zlib header / adler32), ends at a checkpoint (sync flush) or with the final
	// block. The decoder inflates until it has the count of the values.
	//
	// GetCompressSize: raw size / ratio (+5%): the ratio starts with 4 (param "cutCompressRate") and follows the
	// last encode / decode of the node. The node compressor decides when to encode by it, the size is decided
	// by the encoding.
	class CZlibCutStringComp
	{
	public:
		typedef CommonLib::STLAllocator<StringValue> TAlloc;
		typedef TDeVector<StringValue, TAlloc> TValueMemSet;

		static constexpr uint32_t MinFreeBytes = 32;      // a checkpoint with less space left ends the stream
		static constexpr bool CanCut = true;              // NodeCompressorTraits.h

		CZlibCutStringComp(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParamsBase, ECompressParams type)
		{
			m_pAlloc = pAllocsSet->GetAlloc(eStringAlloc);
			TCompressorParamsPtr pParams = pParamsBase.get() ? pParamsBase->GetCompressParams(type) : TCompressorParamsPtr();
			if (pParams.get() != nullptr)
			{
				m_nLevel = pParams->GetIntParam("compressLevel", m_nLevel);
				m_dStartRatio = (double)pParams->GetIntParam("cutCompressRate", (int32_t)m_dStartRatio);
			}
			if (m_dStartRatio < 1.0)
				m_dStartRatio = 1.0;
			m_dRatio = m_dStartRatio;
		}

		void AddSymbol(uint32_t, int, const StringValue& value, const TValueMemSet&)
		{
			m_nCount += 1;
			m_nRowSize += value.Length();
		}

		void RemoveSymbol(uint32_t, int, const StringValue& value, const TValueMemSet&)
		{
			m_nCount -= 1;
			m_nRowSize -= value.Length();
		}

		void UpdateSymbol(int, StringValue& newValue, const StringValue& oldValue, const TValueMemSet&)
		{
			m_nRowSize -= oldValue.Length();
			m_nRowSize += newValue.Length();
		}

		uint32_t GetCompressSize() const
		{
			if (m_nCount == 0)
				return 0;
			return (uint32_t)((double)m_nRowSize / m_dRatio * 1.05) + 8;
		}

		void BeginEncoding(const TValueMemSet&) {}

		// all the values or nothing (the interface of the other encoders): 0 - written, otherwise a split count
		uint32_t Encode(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* /*pContext*/)
		{
			uint32_t nEncoded = 0;
			EncodeCut(vecValues, pStream, maxCompSize, nEncoded);
			return nEncoded == (uint32_t)vecValues.size() ? 0 : (std::max)(1u, (uint32_t)vecValues.size() / 2);
		}

		// writes the first nEncoded values (at most maxSize bytes); nEncoded = 0 - not even one value fits
		void EncodeCut(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, uint32_t maxSize, uint32_t& nEncoded)
		{
			nEncoded = 0;
			uint32_t n = (uint32_t)vecValues.size();
			if (m_nCount != n)
				throw CommonLib::CExcBase("[CZlibCutStringComp] wrong size, count: {0}, values: {1}", m_nCount, n);
			if (n == 0)
				return;

			size_t nStreamFree = pStream->Size() - pStream->Pos();
			uint32_t nOut = (uint32_t)(std::min)((size_t)maxSize, nStreamFree);
			byte_t* pOut = (byte_t*)pStream->Buffer() + pStream->Pos();

			double dRatio = m_dRatio;
			for (;;)
			{
				SCutResult res = DeflateCut(vecValues, pOut, nOut, dRatio);
				if (res.m_nCount == 0 && res.m_nOverflowRaw != 0 && dRatio > 1.0)
				{
					// the first chunk didn't fit. The input that went in until the output was full overstates the
					// ratio (deflate keeps input inside): at most half of the last ratio
					dRatio = (std::max)(1.0, (std::min)(dRatio / 2, (double)res.m_nOverflowRaw / (double)nOut * 0.5));
					continue;
				}

				// a chunk that didn't fit left bytes after the stream: zeroed, the encoders after it may OR bits into
				// the buffer (NumLen)
				memset(pOut + res.m_nOut, 0, nOut - res.m_nOut);
				nEncoded = res.m_nCount;
				if (nEncoded == 0)
					return;
				pStream->Seek(res.m_nOut, CommonLib::soFromCurrent);
				UpdateRatio(res.m_nRaw, res.m_nOut);
				return;
			}
		}

		void Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, CBPTreeContext* pContext)
		{
			z_stream z;
			memset(&z, 0, sizeof(z));
			if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
				throw CommonLib::CExcBase("[CZlibCutStringComp] inflateInit2 failed");

			try
			{
				z.next_in = (Bytef*)(pStream->Buffer() + pStream->Pos());
				z.avail_in = nCompSize;

				const uint32_t nBufSize = 4096;
				pContext->ResizeCommonBuf(nBufSize);
				byte_t* pBuf = pContext->GetCommonBuf();

				std::vector<byte_t> part;
				uint32_t nRead = 0;
				uint64_t nRaw = 0;
				while (nRead < nCount)
				{
					z.next_out = pBuf;
					z.avail_out = nBufSize;
					int res = inflate(&z, Z_SYNC_FLUSH);
					if (res != Z_OK && res != Z_STREAM_END && res != Z_BUF_ERROR)
						throw CommonLib::CExcBase("inflate error {0}", res);

					uint32_t nProduced = nBufSize - z.avail_out;
					nRaw += nProduced;
					uint32_t nBegin = 0;
					for (uint32_t i = 0; i < nProduced; ++i)
					{
						if (pBuf[i] != 0)
							continue;
						if (nRead == nCount)
							throw CommonLib::CExcBase("more values than {0}", nCount);

						uint32_t nLen = (uint32_t)part.size() + (i - nBegin) + 1;
						StringValue value(nLen, m_pAlloc);
						byte_t* pStr = value.Str();
						if (!part.empty())
						{
							memcpy(pStr, part.data(), part.size());
							pStr += part.size();
							part.clear();
						}
						memcpy(pStr, pBuf + nBegin, i - nBegin + 1);
						vecValues.push_back(std::move(value));
						++nRead;
						nBegin = i + 1;
					}
					if (nBegin < nProduced)
						part.insert(part.end(), pBuf + nBegin, pBuf + nProduced);

					if (res == Z_STREAM_END || (nProduced == 0 && (z.avail_in == 0 || res == Z_BUF_ERROR)))
						break;
				}

				if (nRead != nCount || !part.empty())
					throw CommonLib::CExcBase("wrong count of strings {0}, expected {1}", nRead, nCount);

				inflateEnd(&z);
				m_nCount = nCount;
				m_nRowSize = (uint32_t)nRaw;
				UpdateRatio(nRaw, nCompSize);
				pStream->Seek(nCompSize, CommonLib::soFromCurrent);
			}
			catch (std::exception& exc)
			{
				inflateEnd(&z);
				CommonLib::CExcBase::RegenExcT("[CZlibCutStringComp] failed to decode", exc);
				throw;
			}
		}

		void Clear()
		{
			m_nCount = 0;
			m_nRowSize = 0;
		}

		uint32_t Count() const { return m_nCount; }

	private:
		struct SCutResult
		{
			uint32_t m_nCount{ 0 };        // the values of the stream
			uint32_t m_nOut{ 0 };          // its bytes
			uint64_t m_nRaw{ 0 };          // their raw bytes
			uint64_t m_nOverflowRaw{ 0 };  // the raw bytes that went in when the output got full (0 - no overflow)
		};

		SCutResult DeflateCut(const TValueMemSet& vecValues, byte_t* pOut, uint32_t nOut, double dStartRatio)
		{
			SCutResult good;
			if (nOut < MinFreeBytes)
				return good;

			z_stream z;
			memset(&z, 0, sizeof(z));
			if (deflateInit2(&z, m_nLevel, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
				throw CommonLib::CExcBase("[CZlibCutStringComp] deflateInit2 failed");

			z.next_out = pOut;
			z.avail_out = nOut;

			uint32_t n = (uint32_t)vecValues.size();
			uint32_t i = 0;
			uint64_t nRaw = 0;
			double dRatio = dStartRatio;
			bool bOverflow = false;
			bool bDone = false;
			while (!bOverflow && !bDone)
			{
				// the next chunk: half of the input that fits in the space left by the ratio, at least one value
				double dBudget = (double)(nOut - good.m_nOut) * dRatio * 0.5;
				double dChunk = 0;
				do
				{
					const StringValue& value = vecValues[i];
					z.next_in = (Bytef*)value.CStr();
					z.avail_in = value.Length();
					while (z.avail_in != 0)
					{
						if (z.avail_out == 0)
						{
							bOverflow = true;
							break;
						}
						if (deflate(&z, Z_NO_FLUSH) == Z_STREAM_ERROR)
							throw CommonLib::CExcBase("[CZlibCutStringComp] deflate error");
					}
					if (bOverflow)
					{
						good.m_nOverflowRaw = nRaw + value.Length() - z.avail_in;
						break;
					}
					dChunk += value.Length();
					nRaw += value.Length();
					++i;
				} while (i < n && dChunk < dBudget);

				if (bOverflow)
					break;

				if (i == n)
				{
					// all the values: the final block
					int res = deflate(&z, Z_FINISH);
					if (res == Z_STREAM_END)
					{
						good.m_nCount = n;
						good.m_nOut = nOut - z.avail_out;
						good.m_nRaw = nRaw;
						bDone = true;
					}
					else
					{
						bOverflow = true;
						good.m_nOverflowRaw = nRaw;
					}
					continue;
				}

				// a checkpoint: with output space left the flush is complete
				deflate(&z, Z_SYNC_FLUSH);
				if (z.avail_out == 0)
				{
					bOverflow = true;
					good.m_nOverflowRaw = nRaw;
					continue;
				}

				good.m_nCount = i;
				good.m_nOut = nOut - z.avail_out;
				good.m_nRaw = nRaw;
				if (nRaw >= 256)
					dRatio = (std::max)(1.0, (double)nRaw / (double)good.m_nOut);
				if (nOut - good.m_nOut < MinFreeBytes)
					bDone = true;
			}

			deflateEnd(&z);
			if (good.m_nCount != 0)
				good.m_nOverflowRaw = bOverflow ? good.m_nOverflowRaw : 0;
			return good;
		}

		void UpdateRatio(uint64_t nRaw, uint64_t nComp)
		{
			if (nComp != 0 && nRaw >= 256)
				m_dRatio = (std::max)(1.0, (double)nRaw / (double)nComp);
		}

	private:
		uint32_t m_nCount{ 0 };
		uint32_t m_nRowSize{ 0 };
		int32_t m_nLevel{ 6 };
		double m_dStartRatio{ 4.0 };
		double m_dRatio{ 4.0 };
		CommonLib::IAllocPtr m_pAlloc;
	};
}
