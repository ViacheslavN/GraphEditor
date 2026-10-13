#include "stdafx.h"
#include "BlobVal.h"

namespace bptreedb
{
	BlobValue::BlobValue()
	{
	}

	BlobValue::BlobValue(const byte_t* pData, uint32_t nSize, CommonLib::IAllocPtr pAlloc) : BlobValue(nSize, pAlloc)
	{
		if (nSize != 0)
			memcpy(m_pData, pData, nSize);
	}

	BlobValue::BlobValue(uint32_t nSize, CommonLib::IAllocPtr pAlloc) : m_nSize(nSize), m_pAlloc(pAlloc)
	{
		if (nSize > MaxInlineLimit)
			throw CommonLib::CExcBase("BlobValue: inline size {0} exceeds {1}", nSize, MaxInlineLimit);

		if (nSize != 0)
		{
			if (m_pAlloc.get() == nullptr)
				throw CommonLib::CExcBase("BlobValue: no allocator for inline data");
			m_pData = (byte_t*)m_pAlloc->Alloc(nSize);
		}
	}

	BlobValue::BlobValue(int64_t nAddr) : m_nAddr(nAddr)
	{
		if (nAddr <= 0)
			throw CommonLib::CExcBase("BlobValue: wrong stream blob address {0}", nAddr);
	}

	BlobValue::~BlobValue()
	{
		Clear();
	}

	void BlobValue::Clear()
	{
		if (m_pData != nullptr && m_pAlloc.get() != nullptr)
			m_pAlloc->Free(m_pData);

		m_pData = nullptr;
		m_nSize = 0;
		m_nAddr = 0;
	}

	void BlobValue::CopyFrom(const BlobValue& val)
	{
		m_pAlloc = val.m_pAlloc;
		m_nAddr = val.m_nAddr;
		m_nSize = val.m_nSize;
		m_pData = nullptr;

		if (val.m_pData != nullptr && m_nSize != 0)
		{
			m_pData = (byte_t*)m_pAlloc->Alloc(m_nSize);
			memcpy(m_pData, val.m_pData, m_nSize);
		}
	}

	void BlobValue::MoveFrom(BlobValue& val)
	{
		m_pAlloc = val.m_pAlloc;
		m_nAddr = val.m_nAddr;
		m_nSize = val.m_nSize;
		m_pData = val.m_pData;

		val.m_pData = nullptr;
		val.m_nSize = 0;
		val.m_nAddr = 0;
	}

	BlobValue::BlobValue(const BlobValue& val)
	{
		CopyFrom(val);
	}

	BlobValue::BlobValue(BlobValue&& val)
	{
		MoveFrom(val);
	}

	BlobValue& BlobValue::operator= (const BlobValue& val)
	{
		if (this != &val)
		{
			Clear();
			CopyFrom(val);
		}
		return *this;
	}

	BlobValue& BlobValue::operator= (BlobValue&& val)
	{
		if (this != &val)
		{
			Clear();
			MoveFrom(val);
		}
		return *this;
	}
}
