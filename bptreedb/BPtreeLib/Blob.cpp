#include "stdafx.h"
#include "Blob.h"

namespace bptreedb
{
	//-------------------------------------------------------------------------------------------
	// CInlineBlob

	CInlineBlob::CInlineBlob(const BlobValue& value)
	{
		if (!value.IsInline())
			throw CommonLib::CExcBase("CInlineBlob: the blob is in the stream pages, addr: {0}", value.Addr());

		if (value.InlineSize() != 0)
		{
			m_data.assign(value.Data(), value.Data() + value.InlineSize());
			m_stream.AttachBuffer(m_data.data(), m_data.size(), false);
		}
	}

	CInlineBlob::~CInlineBlob()
	{
	}

	int64_t CInlineBlob::Size() const
	{
		return (int64_t)m_data.size();
	}

	CommonLib::ReadStreamPtr CInlineBlob::GetReadStream()
	{
		if (!m_data.empty())
			m_stream.Seek(0, CommonLib::soFromBegin);

		return CommonLib::ReadStreamPtr(shared_from_this(), &m_stream);   // keeps the blob alive
	}

	//-------------------------------------------------------------------------------------------
	// CPageBlob

	CPageBlob::CPageBlob(storage::IFilePagerPtr ptrPager, int64_t nAddr) :
		m_nAddr(nAddr), m_stream(ptrPager)
	{
		if (ptrPager.get() == nullptr)
			throw CommonLib::CExcBase("CPageBlob: pager is null");
		if (nAddr <= 0)
			throw CommonLib::CExcBase("CPageBlob: wrong addr {0}", nAddr);
	}

	CPageBlob::~CPageBlob()
	{
	}

	void CPageBlob::Rewind() const
	{
		try
		{
			m_stream.Open(m_nAddr);
			int64_t nSize = m_stream.ReadInt64();
			if (nSize < 0)
				throw CommonLib::CExcBase("wrong blob size {0}", nSize);

			m_nSize = nSize;
		}
		catch (std::exception& exc)
		{
			CommonLib::CExcBase::RegenExcT("CPageBlob: failed to open the stream, addr: {0}", m_nAddr, exc);
			throw;
		}
	}

	int64_t CPageBlob::Size() const
	{
		if (m_nSize < 0)
			Rewind();

		return m_nSize;
	}

	CommonLib::ReadStreamPtr CPageBlob::GetReadStream()
	{
		Rewind();
		return CommonLib::ReadStreamPtr(shared_from_this(), &m_stream);   // keeps the blob alive
	}
}
