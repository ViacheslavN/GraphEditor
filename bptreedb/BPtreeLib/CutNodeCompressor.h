#pragma once

#include "BaseNodeCompressor.h"
#include "NodeCompressorTraits.h"
#include "../../CommonLib/stream/FixedMemoryStream.h"
#include <vector>
#include <cstring>
#include <algorithm>

namespace bptreedb
{
	// Node compressor of the leaves where one encoder computes its size and the other one (zlib) knows it only
	// by encoding: the cut encoder (NodeCompressorTraits.h, EncodeCut - e.g. CZlibCutStringComp, the values of
	// the string value trees; TStringMultiKeyCutEncoder, the keys of the string indexes) writes as many first
	// elements as fit, Write saves those - the rest is for the next node: Write returns their count, the page in
	// the stream is complete for the first Count() - returned elements (the tree splits the rest off and writes
	// the page as it is, TBPlusTreeSetBase::CheckLeafNode).
	//
	// The page is the one of TBaseNodeCompressor: count, key size, value size, keys, values.
	//   values cut: the key size of all the elements is reserved (an upper bound of any prefix), the values are
	//               encoded after it, then the keys of the values that fit and the values are moved next to them;
	//   keys cut:   the keys are encoded first in the space left by the size of all the values, then the values
	//               of the keys that fit.
	//
	// IsNeedSplit: the estimate above the page by the param "cutOverfill" (percent, default 125): the estimate of
	// the cut encoder is a guess (an assumed ratio, then the one of the last encode of the node), the leaf grows
	// over it and is cut where the page is really full when it is encoded.
	template<class _TKey, class _TValue, class _TKeyEncoder, class _TValueEncoder>
	class TCutNodeCompressor : public TBaseNodeCompressor<_TKey, _TValue, _TKeyEncoder, _TValueEncoder>
	{
	public:
		typedef TBaseNodeCompressor<_TKey, _TValue, _TKeyEncoder, _TValueEncoder> TBase;
		typedef typename TBase::TKeyMemSet TKeyMemSet;
		typedef typename TBase::TValueMemSet TValueMemSet;

		static constexpr bool CutWrite = true;      // NodeCompressorTraits.h
		static constexpr bool ValuesCut = TIsCutEncoder<_TValueEncoder>::value;
		static constexpr bool KeysCut = TIsCutEncoder<_TKeyEncoder>::value;
		static_assert(ValuesCut != KeysCut, "TCutNodeCompressor: one cut encoder, the other one computes its size");

		TCutNodeCompressor(uint32_t nPageSize, TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParams, ECompressNodeType type) :
			TBase(nPageSize, pAllocsSet, pParams, type)
		{
			TCompressorParamsPtr pCompParams = pParams.get() ? pParams->GetCompressParams(type == eInnerNode ? eInnerKey : eLeafKey) : TCompressorParamsPtr();
			if (pCompParams.get())
				m_nOverfill = (uint32_t)pCompParams->GetIntParam("cutOverfill", (int32_t)m_nOverfill);
			if (m_nOverfill < 100)
				m_nOverfill = 100;
		}

		virtual bool IsNeedSplit() const
		{
			if (this->m_nCount > this->m_nMaxCount)
				return true;
			return (uint64_t)this->Size() * 100 > (uint64_t)this->m_nPageSize * m_nOverfill;
		}

		// 0 - all the elements are written; n - the last n elements didn't fit, the page has the others
		virtual uint32_t Write(TKeyMemSet& vecKeys, TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, CBPTreeContext* pContext)
		{
			try
			{
				if (pStream == nullptr)
					throw CommonLib::CExcBase("write stream is zero");

				uint32_t n = (uint32_t)vecKeys.size();
				if (this->m_nCount != n || vecValues.size() != n)
					throw CommonLib::CExcBase("wrong size, count: {0}, keys: {1}, values: {2}", this->m_nCount, n, (uint32_t)vecValues.size());

				size_t nCountPos = pStream->Pos();
				pStream->Write(n);
				if (n == 0)
					return 0;

				size_t nSizePos = pStream->Pos();
				pStream->Write((uint32_t)0);
				pStream->Write((uint32_t)0);
				size_t nStart = pStream->Pos();
				size_t nStreamSize = pStream->Size();

				// the room of the side that computes its size: the size of all the elements (+ the coder error)
				uint32_t nReserve = ValuesCut ? this->m_KeyEncoder.GetCompressSize() : this->m_ValueEncoder.GetCompressSize();
				nReserve += nReserve / 16 + 32;

				for (int nAttempt = 0; nAttempt < 2; ++nAttempt)
				{
					if (nStart + nReserve >= nStreamSize)
						throw CommonLib::CExcBase("the elements don't leave room for the cut side, {0} bytes", nReserve);

					uint32_t nCut = 0, nCutSize = 0, nExactSize = 0;
					if constexpr (ValuesCut)
					{
						// values after the reserve, then the keys of those values
						size_t nValueStart = nStart + nReserve;
						pStream->Seek(nValueStart, CommonLib::soFromBegin);
						this->m_ValueEncoder.BeginEncoding(vecValues);
						this->m_ValueEncoder.EncodeCut(vecValues, pStream, (uint32_t)(nStreamSize - nValueStart), nCut);
						if (nCut == 0)
							throw CommonLib::CExcBase("not one value fits in the node, free {0} bytes", (uint32_t)(nStreamSize - nValueStart));
						nCutSize = (uint32_t)(pStream->Pos() - nValueStart);

						nExactSize = EncodePrefix(this->m_KeyEncoder, vecKeys, nCut, pContext);
						if (nStart + nExactSize + nCutSize > nStreamSize)
						{
							nReserve = nExactSize;      // rare: the keys took more than the reserve
							continue;
						}

						byte_t* pBuf = (byte_t*)pStream->Buffer();
						memmove(pBuf + nStart + nExactSize, pBuf + nValueStart, nCutSize);
						memcpy(pBuf + nStart, PrefixBuffer().data(), nExactSize);
						FinishPage(pStream, nCountPos, nSizePos, nCut, nExactSize, nCutSize, nStart + nExactSize + nCutSize);
					}
					else
					{
						// keys in the space the values leave, then the values of those keys
						pStream->Seek(nStart, CommonLib::soFromBegin);
						this->m_KeyEncoder.BeginEncoding(vecKeys);
						this->m_KeyEncoder.EncodeCut(vecKeys, pStream, (uint32_t)(nStreamSize - nStart - nReserve), nCut);
						if (nCut == 0)
							throw CommonLib::CExcBase("not one key fits in the node, free {0} bytes", (uint32_t)(nStreamSize - nStart - nReserve));
						nCutSize = (uint32_t)(pStream->Pos() - nStart);

						nExactSize = EncodePrefix(this->m_ValueEncoder, vecValues, nCut, pContext);
						if (nStart + nCutSize + nExactSize > nStreamSize)
						{
							nReserve = nExactSize;
							continue;
						}

						byte_t* pBuf = (byte_t*)pStream->Buffer();
						memcpy(pBuf + nStart + nCutSize, PrefixBuffer().data(), nExactSize);
						FinishPage(pStream, nCountPos, nSizePos, nCut, nCutSize, nExactSize, nStart + nCutSize + nExactSize);
					}
					return n - nCut;
				}
				throw CommonLib::CExcBase("failed to place the keys and the values");
			}
			catch (std::exception& exc_src)
			{
				CommonLib::CExcBase::RegenExcT("[CutNodeCompressor] failed to write the node", exc_src);
				throw;
			}
		}

	private:
		static void FinishPage(CommonLib::IMemoryWriteStream* pStream, size_t nCountPos, size_t nSizePos, uint32_t nCount, uint32_t nKeySize, uint32_t nValueSize, size_t nEnd)
		{
			pStream->Seek(nCountPos, CommonLib::soFromBegin);
			pStream->Write(nCount);
			pStream->Seek(nSizePos, CommonLib::soFromBegin);
			pStream->Write(nKeySize);
			pStream->Write(nValueSize);
			pStream->Seek(nEnd, CommonLib::soFromBegin);
		}

		static std::vector<byte_t>& PrefixBuffer()
		{
			static thread_local std::vector<byte_t> buf;
			return buf;
		}

		// the first nCount elements by the encoder that computes its size, into PrefixBuffer(); the statistics of
		// the encoder stay of all the elements
		template <class TEncoder, class TVec>
		static uint32_t EncodePrefix(TEncoder& encoder, const TVec& vec, uint32_t nCount, CBPTreeContext* pContext)
		{
			uint32_t n = (uint32_t)vec.size();
			uint32_t nBufSize = encoder.GetCompressSize() * 2 + 1024;
			std::vector<byte_t>& buf = PrefixBuffer();
			if (buf.size() < nBufSize)
				buf.resize(nBufSize);
			// zeroed: the bit writer of NumLen ORs the bits into the buffer (as SaveNode erases the page)
			std::fill(buf.begin(), buf.begin() + nBufSize, (byte_t)0);

			CommonLib::CFxMemoryWriteStream stream;
			stream.AttachBuffer(buf.data(), nBufSize);
			uint32_t nRet = 0;
			if (nCount == n)
			{
				encoder.BeginEncoding(vec);
				nRet = encoder.Encode(vec, &stream, nBufSize, pContext);
			}
			else
			{
				TVec prefix(vec.get_allocator());
				prefix.reserve(nCount);
				for (uint32_t i = 0; i < nCount; ++i)
					prefix.push_back(vec[i]);

				SetStats(encoder, prefix);
				encoder.BeginEncoding(prefix);
				nRet = encoder.Encode(prefix, &stream, nBufSize, pContext);
				SetStats(encoder, vec);
			}
			if (nRet != 0)
				throw CommonLib::CExcBase("the elements don't fit in {0} bytes", nBufSize);
			return (uint32_t)stream.Pos();
		}

		template <class TEncoder, class TVec>
		static void SetStats(TEncoder& encoder, const TVec& vec)
		{
			encoder.Clear();
			for (uint32_t i = 0, sz = (uint32_t)vec.size(); i < sz; ++i)
				encoder.AddSymbol(i + 1, i, vec[i], vec);
		}

	private:
		uint32_t m_nOverfill{ 125 };
	};
}
