#include "stdafx.h"
#include "StringVal.h"

namespace bptreedb
{

	StringValue::StringValue() : m_utf8(nullptr), m_nLen(0)
	{

	}


	StringValue::StringValue(const byte_t *pStr, uint32_t len, CommonLib::IAllocPtr pAlloc)
	{
		m_pAlloc = pAlloc;
		CopyStr(pStr, len);
	}

	StringValue::StringValue(uint32_t len, CommonLib::IAllocPtr pAlloc)
	{
		m_pAlloc = pAlloc;
		m_nLen = len;

		m_utf8 = (byte_t*)m_pAlloc->Alloc(len);
	}


	StringValue::~StringValue()
	{
		Clear();
	}

	void StringValue::Clear()
	{
		if (m_pAlloc.get() && m_utf8 != nullptr)
			m_pAlloc->Free(m_utf8);

		m_utf8 = nullptr;
		m_nLen = 0;
	}

	// Stores the string with a terminating zero, Length() includes it (the string encoders split the
	// decoded data by the zeros). An empty string is stored as "\0" (length 1), so it can be encoded too.
	// A value without an allocator (default constructed) stays empty.
	void  StringValue::CopyStr(const byte_t *pStr, uint32_t len)
	{
		m_utf8 = nullptr;
		m_nLen = 0;
		if (m_pAlloc.get() == nullptr)
			return;

		if (pStr == nullptr)
			len = 0;

		uint32_t copyLen = len;
		if (len != 0 && pStr[len - 1] == '\0')
			copyLen = len - 1;

		m_nLen = copyLen + 1;
		m_utf8 = (byte_t*)m_pAlloc->Alloc(m_nLen);
		if (copyLen != 0)
			memcpy(m_utf8, pStr, copyLen);
		m_utf8[m_nLen - 1] = '\0';
	}

	uint32_t StringValue::Length() const
	{
		return m_nLen;
	}

	const byte_t * StringValue::CStr() const
	{
		static const byte_t empty[1] = { 0 };
		return m_utf8 != nullptr ? m_utf8 : empty;   // never null: comparisons and printing need a string
	}

	byte_t * StringValue::Str()
	{
		return m_utf8;
	}

	StringValue::StringValue(const StringValue& val)
	{
		m_pAlloc = val.m_pAlloc;
		CopyStr(val.m_utf8, val.m_nLen);
		
	}

	StringValue::StringValue( StringValue&& val)
	{
		m_pAlloc = val.m_pAlloc;
		m_nLen = val.m_nLen;
		m_utf8 = val.m_utf8;

		val.m_utf8 = nullptr;
		val.m_nLen = 0;
	}

	StringValue& StringValue::operator= (const StringValue& val)
	{
		if (this == &val)
			return *this;

		Clear();
		m_pAlloc = val.m_pAlloc;
		CopyStr(val.m_utf8, val.m_nLen);

		return *this;
	}

	StringValue& StringValue::operator= (StringValue&& val)
	{
		if (this == &val)
			return *this;

		Clear();
		m_pAlloc = val.m_pAlloc;
		m_nLen = val.m_nLen;
		m_utf8 = val.m_utf8;

		val.m_utf8 = nullptr;
		val.m_nLen = 0;

		return *this;
	}
}