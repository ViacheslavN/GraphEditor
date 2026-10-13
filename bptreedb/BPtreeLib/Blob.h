#pragma once
#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/stream/stream.h"
#include "../../CommonLib/stream/MemoryStream.h"
#include "../StorageLib/Storage.h"
#include "../StorageLib/ReadStreamPage.h"
#include "BlobVal.h"

namespace bptreedb
{
	// Read access to a blob value of TBPBlobValueMap.
	// The blob doesn't depend on the tree node it came from (the node may leave the cache).
	//
	// The blob owns one read stream: GetReadStream() rewinds it to the first byte and returns it.
	// The returned pointer shares ownership of the blob, so the stream stays valid after the IBlobPtr is gone.
	// A blob has to be owned by a std::shared_ptr (TBPBlobValueMap::GetBlob makes it so).
	class IBlob
	{
	public:
		IBlob() {}
		virtual ~IBlob() {}

		virtual int64_t Size() const = 0;
		virtual bool IsInline() const = 0;

		// inline blob - CReadMemoryStream, blob in the stream pages - TReadStreamPage
		virtual CommonLib::ReadStreamPtr GetReadStream() = 0;
	};

	typedef std::shared_ptr<IBlob> IBlobPtr;


	// blob stored in the leaf node: a copy of the bytes (up to 64K)
	class CInlineBlob : public IBlob, public std::enable_shared_from_this<CInlineBlob>
	{
	public:
		CInlineBlob(const BlobValue& value);
		virtual ~CInlineBlob();

		virtual int64_t Size() const;
		virtual bool IsInline() const { return true; }
		virtual CommonLib::ReadStreamPtr GetReadStream();

	private:
		std::vector<byte_t> m_data;
		CommonLib::CReadMemoryStream m_stream;     // attached to m_data, no copy
	};


	// blob stored in a chain of stream pages: int64 size, then the bytes.
	// The size is read from the stream on the first Size() / GetReadStream().
	// The stream doesn't know the blob size (TReadStreamPage::Size() / Pos() aren't implemented),
	// read at most Size() bytes: past the end it gives the rest of the last page.
	class CPageBlob : public IBlob, public std::enable_shared_from_this<CPageBlob>
	{
	public:
		CPageBlob(storage::IFilePagerPtr ptrPager, int64_t nAddr);
		virtual ~CPageBlob();

		virtual int64_t Size() const;
		virtual bool IsInline() const { return false; }
		virtual CommonLib::ReadStreamPtr GetReadStream();

		int64_t Addr() const { return m_nAddr; }

	private:
		void Rewind() const;   // opens the first page and reads the size header

		int64_t m_nAddr{ 0 };
		mutable storage::TReadStreamPage m_stream;
		mutable int64_t m_nSize{ -1 };   // -1: not read yet
	};
}
