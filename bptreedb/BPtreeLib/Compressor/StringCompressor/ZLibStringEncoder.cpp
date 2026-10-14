#include "stdafx.h"
#include "ZLibStringEncoder.h"


namespace bptreedb
{
	CZlibStringComp::CZlibStringComp(TAllocsSetPtr pAllocsSet, TCompressorParamsBasePtr pParamsBase, ECompressParams type)
	{

		m_pAlloc = pAllocsSet->GetAlloc(eStringAlloc);

		TCompressorParamsPtr pParams = pParamsBase->GetCompressParams(type);

		if (pParams.get() != nullptr)
		{
			m_compressLevel = pParams->GetIntParam("compressLevel", m_compressLevel);
			m_compressRate = pParams->GetIntParam("compressRate", m_compressRate);
			m_nMaxLen = pParams->GetIntParam("MaxLen", m_nMaxLen);
			m_nDecBufferSize = pParams->GetIntParam("DecBuffer", m_nDecBufferSize);
		}
		if (m_compressRate <= 0)
			m_compressRate = 1;
		m_dRatio = (double)m_compressRate;
	}

	CZlibStringComp::~CZlibStringComp()
	{

	}

	void CZlibStringComp::AddSymbol(uint32_t nSize, int nIndex, const StringValue& value, const TValueMemSet& vecValues)
	{
		m_nCount += 1;
		m_nRowSize += value.Length();
	}

	void CZlibStringComp::RemoveSymbol(uint32_t nSize, int nIndex, const StringValue& value, const TValueMemSet& vecValues)
	{
		m_nCount -= 1;
		m_nRowSize -= value.Length();
	}

	void CZlibStringComp::UpdateSymbol(int nIndex, StringValue& newValue, const StringValue& oldValue, const TValueMemSet& vecValues)
	{
		m_nRowSize -= oldValue.Length();
		m_nRowSize += newValue.Length();
	}

	// estimate: raw size / compression ratio. The ratio starts with the param compressRate and is
	// replaced by the real one of this node after every encode / decode (5% margin, zlib header 8 bytes)
	uint32_t CZlibStringComp::GetCompressSize() const
	{
		if (m_nCount == 0)
			return 0;
		return (uint32_t)((double)m_nRowSize / m_dRatio * 1.05) + 8;
	}

	void CZlibStringComp::BeginEncoding(const TValueMemSet& vecValues)
	{

	}

	uint32_t CZlibStringComp::Encode(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream *pStream, uint32_t maxCompSize, CBPTreeContext *pContext)
	{
		try
		{
			if (m_nCount != vecValues.size())
				throw CommonLib::CExcBase("CZlibStringComp encoder wrong size, count: {0}, values size: {1}", m_nCount, vecValues.size());


			uint32_t streamSize = uint32_t(pStream->Size() - pStream->Pos());
			streamSize = streamSize > maxCompSize ? maxCompSize : streamSize;

			CommonLib::CZCompressStream zStream(m_compressLevel);
			zStream.AttachOut((Bytef*)pStream->Buffer() + pStream->Pos(), streamSize);

			uint64_t nRawIn = 0;
			for (uint32_t i = 0, sz = (uint32_t)vecValues.size(); i < sz; ++i)
			{
				zStream.AttachIn((Bytef*)vecValues[i].CStr(), (uInt)vecValues[i].Length());

				while (zStream.GetAvailIn() != 0)
				{
					if (zStream.GetAvailOut() == 0)
					{
						UpdateRatioOverflow(nRawIn + vecValues[i].Length() - zStream.GetAvailIn(), streamSize);
						return (uint32_t)(vecValues.size()) / 2;
					}

					zStream.Compress();

				}
				nRawIn += vecValues[i].Length();
			}

			// Z_STREAM_END: everything is written; Z_OK / Z_BUF_ERROR: the output buffer is full
			int res = zStream.Deflate(Z_FINISH);
			if (res != Z_STREAM_END)
			{
				if (res != Z_OK && res != Z_BUF_ERROR)
					throw CommonLib::CExcBase("deflate finish error {0}", res);
				UpdateRatioOverflow(nRawIn, streamSize);
				return (uint32_t)(vecValues.size()) / 2;
			}

			uint32_t nCompSize = streamSize - zStream.GetAvailOut();
			pStream->Seek(nCompSize, CommonLib::soFromCurrent);
			UpdateRatio(nCompSize);

		}
		catch (std::exception& exc)
		{
			CommonLib::CExcBase::RegenExcT("[CZlibStringComp] failed to encode", exc);
			throw;
		}

		return 0;
	}

	void CZlibStringComp::Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream *pStream, uint32_t nCompSize, CBPTreeContext *pContext)
	{
		try
		{

			CommonLib::CZDecompressStream zStream;
			zStream.AttachIn(pStream->Buffer() + pStream->Pos(), nCompSize);

			pContext->ResizeCommonBuf(m_nDecBufferSize);
			byte_t *pCompBuf = pContext->GetCommonBuf();

			uint32_t readCount = 0;
			std::vector<byte_t> stringPart;
			uint64_t nRawSize = 0;

			// inflate until the end of the zlib stream: with all input consumed inflate can still have
			// output to give (the last output buffer was full), so the input size isn't the condition
			int res = Z_OK;
			while (res != Z_STREAM_END)
			{
				zStream.AttachOut(pCompBuf, m_nDecBufferSize);
				res = zStream.Inflate(Z_NO_FLUSH);
				if (res != Z_OK && res != Z_STREAM_END)
					throw CommonLib::CExcBase("inflate error {0}", res);

				uint32_t nOut = m_nDecBufferSize - zStream.GetAvailOut();
				if (nOut != 0)
					ProcessDecodeBuffer(pCompBuf, nOut, stringPart, readCount, nCount, vecValues);
				nRawSize += nOut;

				if (res != Z_STREAM_END && nOut == 0 && zStream.GetAvailIn() == 0)
					throw CommonLib::CExcBase("truncated data");
			}

			if (readCount != nCount || !stringPart.empty())
				throw CommonLib::CExcBase("wrong count of strings {0}, expected {1}", readCount, nCount);

			m_nCount = nCount;
			m_nRowSize = (uint32_t)nRawSize;
			UpdateRatio(nCompSize);
			pStream->Seek(nCompSize, CommonLib::soFromCurrent);

		}
		catch (std::exception& exc)
		{
			CommonLib::CExcBase::RegenExcT("[CZlibStringComp] failed to decode", exc);
			throw;
		}
	}

	void CZlibStringComp::ProcessDecodeBuffer(const byte_t* buffer, uint32_t bufSize, std::vector<byte_t>& strPart, uint32_t& readCount, uint32_t nCount, TValueMemSet& vecValues)
	{
		uint32_t beginpos = 0;
		for (uint32_t i = 0; i < bufSize; ++i)
		{
			if (buffer[i] == '\0')
			{

				readCount += 1;
				if (readCount > nCount)
					throw CommonLib::CExcBase("wrong read size");


				size_t len =  i + (uint32_t)strPart.size() + 1  - beginpos;;

				StringValue value((uint32_t)len, m_pAlloc);
	
				byte_t *pStrBuf = value.Str();
				uint32_t copySize = value.Length();

				if (!strPart.empty())
				{
					memcpy(pStrBuf, strPart.data(), strPart.size());
					pStrBuf += (uint32_t)strPart.size();
					copySize -= (uint32_t)strPart.size();
					strPart.clear();
				}

				memcpy(pStrBuf, buffer + beginpos, copySize);

				vecValues.push_back(std::move(value));

				beginpos = i + 1;

			}
		}

		if (beginpos < bufSize)
		{
			strPart.resize(bufSize - beginpos);
			memcpy(strPart.data(), buffer + beginpos, strPart.size());
		}
	}

	void CZlibStringComp::UpdateRatio(uint32_t nCompSize)
	{
		if (nCompSize != 0 && m_nRowSize > 256)   // small nodes say little about the data
			m_dRatio = (double)m_nRowSize / (double)nCompSize;
	}

	void CZlibStringComp::UpdateRatioOverflow(uint64_t nRawIn, uint32_t nOut)
	{
		// zlib keeps some input in its buffers: the ratio is a lower bound, the estimate stays on the safe side
		if (nOut != 0 && nRawIn > 256)
			m_dRatio = (std::max)(1.0, (double)nRawIn / (double)nOut);
	}

	void CZlibStringComp::Clear()
	{
		m_nCount = 0;
		m_nRowSize = 0;
	}

	uint32_t CZlibStringComp::Count() const
	{
		return m_nCount;
	}
}
