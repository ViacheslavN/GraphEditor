#include "stdafx.h"
#include "BlobValueEncoder.h"

namespace bptreedb
{
	CBlobValueEncoder::CBlobValueEncoder(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParamsBase, ECompressParams type)
	{
		m_pAlloc = pAllocsSet->GetAlloc(eBlobAlloc);
	}

	CBlobValueEncoder::~CBlobValueEncoder()
	{
	}

	void CBlobValueEncoder::AddSymbol(uint32_t nSize, int nIndex, const BlobValue& value, const TValueMemSet& vecValues)
	{
		m_nCount += 1;
		m_nRowSize += value.EncodedSize();
	}

	void CBlobValueEncoder::RemoveSymbol(uint32_t nSize, int nIndex, const BlobValue& value, const TValueMemSet& vecValues)
	{
		m_nCount -= 1;
		m_nRowSize -= value.EncodedSize();
	}

	void CBlobValueEncoder::UpdateSymbol(int nIndex, BlobValue& newValue, const BlobValue& oldValue, const TValueMemSet& vecValues)
	{
		m_nRowSize -= oldValue.EncodedSize();
		m_nRowSize += newValue.EncodedSize();
	}

	uint32_t CBlobValueEncoder::GetCompressSize() const
	{
		return m_nRowSize;
	}

	void CBlobValueEncoder::BeginEncoding(const TValueMemSet& vecValues)
	{
	}

	// returns 0 if everything is written, otherwise the split position (as the other encoders: count / 2)
	uint32_t CBlobValueEncoder::Encode(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, CBPTreeContext* pContext)
	{
		try
		{
			uint32_t nCount = (uint32_t)vecValues.size();
			if (m_nCount != nCount)
				throw CommonLib::CExcBase("wrong size, count: {0}, values size: {1}", m_nCount, nCount);

			size_t nFree = pStream->Size() - pStream->Pos();
			if (m_nRowSize > maxCompSize || m_nRowSize > nFree)
			{
				if (nCount < 2)
					throw CommonLib::CExcBase("one blob value doesn't fit in the node, size: {0}, free: {1}", m_nRowSize, std::min<size_t>(nFree, maxCompSize));

				return nCount / 2;
			}

			for (uint32_t i = 0; i < nCount; ++i)
			{
				const BlobValue& value = vecValues[i];
				pStream->Write(value.Addr());
				if (value.IsInline())
				{
					pStream->Write((uint16_t)value.InlineSize());
					if (value.InlineSize() != 0)
						pStream->WriteBytes(value.Data(), value.InlineSize());
				}
			}

			return 0;
		}
		catch (std::exception& exc)
		{
			CommonLib::CExcBase::RegenExcT("[CBlobValueEncoder] failed to encode", exc);
			throw;
		}
	}

	void CBlobValueEncoder::Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, CBPTreeContext* pContext)
	{
		try
		{
			size_t nStartPos = pStream->Pos();
			m_nRowSize = 0;

			for (uint32_t i = 0; i < nCount; ++i)
			{
				int64_t nAddr = pStream->ReadInt64();
				if (nAddr == 0)
				{
					uint16_t nSize = pStream->Readintu16();
					BlobValue value(nSize, m_pAlloc);
					if (nSize != 0)
						pStream->ReadBytes(value.Data(), nSize);

					m_nRowSize += value.EncodedSize();
					vecValues.push_back(std::move(value));
				}
				else
				{
					if (nAddr < 0)
						throw CommonLib::CExcBase("wrong blob addr: {0}", nAddr);

					BlobValue value(nAddr);
					m_nRowSize += value.EncodedSize();
					vecValues.push_back(std::move(value));
				}
			}

			if (pStream->Pos() - nStartPos != nCompSize)
				throw CommonLib::CExcBase("wrong encoded size {0}, expected {1}", (uint64_t)(pStream->Pos() - nStartPos), nCompSize);

			m_nCount = nCount;
		}
		catch (std::exception& exc)
		{
			CommonLib::CExcBase::RegenExcT("[CBlobValueEncoder] failed to decode", exc);
			throw;
		}
	}

	void CBlobValueEncoder::Clear()
	{
		m_nCount = 0;
		m_nRowSize = 0;
	}

	uint32_t CBlobValueEncoder::Count() const
	{
		return m_nCount;
	}
}
