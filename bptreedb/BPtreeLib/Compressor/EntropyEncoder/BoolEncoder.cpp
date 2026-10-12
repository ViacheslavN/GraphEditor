#include "stdafx.h"
#include "BoolEncoder.h"
#include "../CompressUtils.h"

namespace bptreedb
{
	CBoolEncoder::CBoolEncoder()
	{
		Clear();
	}

	CBoolEncoder::~CBoolEncoder()
	{

	}

	void CBoolEncoder::Clear()
	{
		m_encodeType = BitEncode;
		m_dataType = ectByte;
		m_bit = false;
		m_bools[0] = 0;
		m_bools[1] = 0;
		m_pos.clear();
		m_bitsRW.Clear();
	}

	void CBoolEncoder::AddSymbol(bool bSign)
	{
		m_bools[bSign ? 1 : 0] += 1;

	}

	void CBoolEncoder::RemoveSymbol(bool bSign)
	{
		m_bools[bSign ? 1 : 0] -= 1;
	}

	uint32_t CBoolEncoder::GetCompressSize() const
	{
		// Must match BeginEncoding exactly: flag byte + two uint32 counts + payload
		const uint32_t nHeaderSize = 1 + 2 * sizeof(uint32_t);

		if (m_bools[0] == 0 || m_bools[1] == 0)
			return nHeaderSize; // OneBit

		uint32_t nByteSize = ((m_bools[0] + m_bools[1] + 7) / 8) + 1;
		uint32_t nMinCount = min(m_bools[0], m_bools[1]);

		eCompressDataType type = GetCompressType(m_bools[0] + m_bools[1]);
		uint32_t nBytePosCodeSize = GetLenForDiffLen(type, nMinCount);

		if (nByteSize < nBytePosCodeSize)
			return nHeaderSize + nByteSize; // BitEncode

		// PosBits: position count + positions
		return nHeaderSize + GetSizeTypeValue(type) + nBytePosCodeSize;

	}


	bool CBoolEncoder::BeginEncoding(CommonLib::IMemoryWriteStream *ptrStream)
	{
		byte_t nFlag = 0;
		if (m_bools[0] == 0 || m_bools[1] == 0)
		{
			m_encodeType = OneBit;
			nFlag = m_encodeType;
			if (m_bools[1] != 0)
				nFlag |= (1 << SignBit);

			if (!ptrStream->WriteSafe(nFlag))
				return false;
			if (!ptrStream->WriteSafe(m_bools[0]))
				return false;
			if (!ptrStream->WriteSafe(m_bools[1]))
				return false;

			return true;
		}
		else
		{
 

			uint32_t nByteSize = ((m_bools[0] + m_bools[1] + 7) / 8) + 1;
			uint32_t nMinCount = min(m_bools[0], m_bools[1]);
			m_dataType = GetCompressType(m_bools[0] + m_bools[1]);
			uint32_t nBytePosCodeSize = GetLenForDiffLen(m_dataType, nMinCount);

			if (nByteSize < nBytePosCodeSize)
			{
				m_encodeType = BitEncode;
				if (!ptrStream->WriteSafe((byte_t)m_encodeType))
					return false;
				if (!ptrStream->WriteSafe(m_bools[0]))
					return false;
				if (!ptrStream->WriteSafe(m_bools[1]))
					return false;
				
				m_bitsRW.Attach(ptrStream->BufferFromCurPos());
				return ptrStream->SeekSafe(nByteSize, CommonLib::soFromCurrent);

			}
			else
			{
				m_encodeType = PosBits;
				nFlag = m_encodeType;
				// store positions of the minority bit, exactly nMinCount of them
				m_bit = m_bools[1] < m_bools[0];
				if (m_bit)
					nFlag |= (1 << SignPosBit);
				nFlag |= ((byte_t)m_dataType << DataTypePosBit);

				if (!ptrStream->WriteSafe(nFlag))
					return false;
				if (!ptrStream->WriteSafe(m_bools[0]))
					return false;
				if (!ptrStream->WriteSafe(m_bools[1]))
					return false;

				if (ptrStream->Pos() + GetSizeTypeValue(m_dataType) > ptrStream->Size())
					return false;
				WriteCompressValue(m_dataType, nMinCount, ptrStream);

				m_WriteStream.AttachBuffer(ptrStream->BufferFromCurPos(), nBytePosCodeSize);
				return ptrStream->SeekSafe(nBytePosCodeSize, CommonLib::soFromCurrent);
			}
		}
		
 
	}

	void CBoolEncoder::EncodeBit(bool bit, uint32_t pos)
	{
		switch (m_encodeType)
		{
			case PosBits:
			{
				if (bit == m_bit)
				{
					// consistency check: more minority bits than counted in AddSymbol/RemoveSymbol
					if (m_WriteStream.Pos() + GetSizeTypeValue(m_dataType) > m_WriteStream.Size())
						throw CommonLib::CExcBase("CBoolEncoder: sign statistics mismatch, pos {0}, counts {1}/{2}, bit {3}", pos, m_bools[0], m_bools[1], (int)m_bit);
					WriteCompressValue(m_dataType, pos, &m_WriteStream);
				}
				break;
			}
			case BitEncode:
				m_bitsRW.WriteBit(bit);
				break;
		}
	}

	void CBoolEncoder::BeginDecoding(CommonLib::IMemoryReadStream *ptrStream)
	{
		byte_t nFlag = ptrStream->ReadByte();
		m_bools[0] = ptrStream->ReadIntu32();
		m_bools[1] = ptrStream->ReadIntu32();

		uint32_t count = m_bools[0] + m_bools[1];

		m_encodeType = (EncodeType)(nFlag & 0x03);

		if (m_encodeType == OneBit)
		{
			m_bit = nFlag & (1 << SignBit) ? true : false;
			m_bools[m_bit ? 1 : 0] = count;
		}
		else if (m_encodeType == PosBits)
		{
		
			m_bit = nFlag & (1 << SignPosBit) ? true : false;
			m_dataType =  (eCompressDataType)(nFlag >> DataTypePosBit);
			uint32_t posCount = 0;
			ReadCompressValue<uint32_t>(m_dataType, posCount, ptrStream);
			m_pos.clear();
			m_pos.reserve(posCount);

			for (size_t i = 0; i < posCount; ++i)
			{
				uint32_t nPos = 0;
				ReadCompressValue<uint32_t>(m_dataType, nPos, ptrStream);
				m_pos.push_back(nPos);
			}
		}
		else if (m_encodeType == BitEncode)
		{

			uint32_t nByteSize = ((count + 7) / 8) + 1;
			m_bitsRW.Attach(ptrStream->BufferFromCurPos());
			ptrStream->Seek(nByteSize, CommonLib::soFromCurrent);
		}
	}

	bool CBoolEncoder::DecodeBit(uint32_t pos)
	{
		switch (m_encodeType)
		{
		case OneBit:
			return m_bit;
			break;
		case PosBits:
			if (std::binary_search(m_pos.begin(), m_pos.end(), pos))
				return m_bit;
			return !m_bit;

			break;

		case BitEncode:
			return m_bitsRW.ReadBit();
			break;

		default:
			throw CommonLib::CExcBase(L"CBoolEncoder: DecodeBit unknown encodeType: {0}", (int)m_encodeType);
		}
	
	}
}