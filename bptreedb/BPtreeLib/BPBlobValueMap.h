#pragma once

#include "BPMap.h"
#include "BlobVal.h"
#include "Blob.h"
#include "../StorageLib/WriteStreamPage.h"

namespace bptreedb
{
	// B+ tree map with blob values (any key type of TBPMap, e.g. int64_t).
	//
	// A blob up to nMaxInlineSize bytes (default 1024) is kept in the leaf node; a bigger one is written
	// to a chain of stream pages (TWriteStreamPage: int64 size, then the bytes) and the leaf keeps only
	// its address, see BlobValue.
	// Values are read through IBlob (GetBlob): an inline blob gives a memory stream, a big one TReadStreamPage.
	//
	// The leaves encode the values with CBlobValueEncoder:
	//   BPTreeLeafNodeMap<int64_t, BlobValue, TBaseNodeCompressor<int64_t, BlobValue, TKeyEncoder, CBlobValueEncoder>>
	//
	// The stream pages of a blob are owned by the tree entry: remove() drops them. A BlobValue made by
	// WriteBlob() that doesn't end up in the tree has to be released with FreeBlob().
	template <class _TKey, class _TComp, class _TStorage,
		class _TInnerNode,
		class _TLeafNode,
		class _TNodeHolder = TBPMapNodeHolder<_TInnerNode, _TLeafNode>
	>
	class TBPBlobValueMap : public TBPMap<_TKey, BlobValue, _TComp, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder>
	{
	public:
		typedef TBPMap<_TKey, BlobValue, _TComp, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder> TBase;
		typedef typename TBase::TKey TKey;
		typedef typename TBase::TStorage TStorage;
		typedef typename TBase::iterator iterator;

		static constexpr uint32_t DefaultMaxInlineSize = 1024;

		TBPBlobValueMap(int64_t nPageBTreeInfo, std::shared_ptr<TStorage> pStorage, CommonLib::IAllocPtr pAlloc, uint32_t nCacheSize, uint32_t nNodesPageSize,
			uint32_t nMaxInlineSize = DefaultMaxInlineSize, bool bMinSplit = false) :
			TBase(nPageBTreeInfo, pStorage, pAlloc, nCacheSize, nNodesPageSize, bMinSplit),
			m_nMaxInlineSize(nMaxInlineSize)
		{
			if (m_nMaxInlineSize > BlobValue::MaxInlineLimit)
				throw CommonLib::CExcBase("TBPBlobValueMap: max inline size {0} exceeds {1}", m_nMaxInlineSize, BlobValue::MaxInlineLimit);

			// one entry has to fit in a leaf page with room for other entries
			if ((m_nMaxInlineSize + 2 * sizeof(int64_t) + sizeof(TKey)) * 2 > nNodesPageSize)
				throw CommonLib::CExcBase("TBPBlobValueMap: max inline size {0} is too big for the page size {1}", m_nMaxInlineSize, nNodesPageSize);

			m_pBlobAlloc = this->m_pAllocsSet->GetAlloc(eBlobAlloc);
		}

		using TBase::insert;

		void insert(const TKey& key, const byte_t* pData, int64_t nSize)
		{
			InsertValue(key, WriteBlob(pData, nSize));
		}

		void insert(const TKey& key, const std::vector<byte_t>& data)
		{
			insert(key, data.data(), (int64_t)data.size());
		}

		// nSize bytes from the stream; a big blob is copied to the stream pages by chunks
		void insert(const TKey& key, CommonLib::IReadStream* pSrc, int64_t nSize)
		{
			InsertValue(key, WriteBlob(pSrc, nSize));
		}

		// removes the entry and drops the stream pages of its blob
		bool remove(const TKey& key)
		{
			try
			{
				iterator it = this->find(key);
				if (it.IsNull())
					return false;

				int64_t nAddr = it.Value().Addr();
				if (!TBase::remove(key))
					return false;

				if (nAddr != 0)
					DropBlobPages(nAddr);

				return true;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPBlobValueMap] failed to remove", exc);
				throw;
			}
		}

		//------------------------------------------------------------------------------------------
		// read

		// the stream pages of a big blob (the chain is followed: the next page is in the header of a page)
		uint64_t StreamPageCount(const BlobValue& value) const
		{
			uint64_t nCount = 0;
			int64_t nAddr = value.IsInline() ? -1 : value.Addr();
			while (nAddr > 0)
			{
				storage::IFilePagePtr ptrPage = this->m_pStorage->ReadPage(nAddr);
				nAddr = ptrPage->GetReadStream()->ReadInt64();
				if (++nCount > ((uint64_t)1 << 40))
					throw CommonLib::CExcBase("TBPBlobValueMap: the page chain of blob {0} has a loop", value.Addr());
			}
			return nCount;
		}

		IBlobPtr GetBlob(const BlobValue& value) const
		{
			if (value.IsInline())
				return std::make_shared<CInlineBlob>(value);

			return std::make_shared<CPageBlob>(this->m_pStorage, value.Addr());
		}

		IBlobPtr GetBlob(const iterator& it) const
		{
			if (it.IsNull())
				throw CommonLib::CExcBase("TBPBlobValueMap: GetBlob for a null iterator");

			return GetBlob(it.Value());
		}

		// nullptr if there is no such key
		IBlobPtr GetBlob(const TKey& key)
		{
			iterator it = this->find(key);
			if (it.IsNull())
				return IBlobPtr();

			return GetBlob(it.Value());
		}

		//------------------------------------------------------------------------------------------
		// values for BulkLoad (TSource::Next gives BlobValue) or insert(key, BlobValue)

		// inline value if nSize <= MaxInlineSize(), otherwise writes the data to new stream pages
		BlobValue WriteBlob(const byte_t* pData, int64_t nSize)
		{
			if (nSize < 0 || (nSize != 0 && pData == nullptr))
				throw CommonLib::CExcBase("TBPBlobValueMap: wrong blob data, size: {0}", nSize);

			if (nSize <= (int64_t)m_nMaxInlineSize)
				return BlobValue(pData, (uint32_t)nSize, m_pBlobAlloc);

			try
			{
				int64_t nAddr = NewBlobPageAddr();
				storage::TWriteStreamPage stream(this->m_pStorage);
				stream.Open(nAddr, true);
				stream.Write(nSize);
				stream.WriteBytes(pData, (size_t)nSize);
				stream.Close();

				return BlobValue(nAddr);
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPBlobValueMap] failed to write blob, size: {0}", nSize, exc);
				throw;
			}
		}

		BlobValue WriteBlob(CommonLib::IReadStream* pSrc, int64_t nSize)
		{
			if (nSize < 0 || pSrc == nullptr)
				throw CommonLib::CExcBase("TBPBlobValueMap: wrong blob source, size: {0}", nSize);

			if (nSize <= (int64_t)m_nMaxInlineSize)
			{
				BlobValue value((uint32_t)nSize, m_pBlobAlloc);
				if (nSize != 0)
					pSrc->ReadBytes(value.Data(), (size_t)nSize);
				return value;
			}

			int64_t nAddr = -1;
			try
			{
				nAddr = NewBlobPageAddr();
				storage::TWriteStreamPage stream(this->m_pStorage);
				stream.Open(nAddr, true);
				stream.Write(nSize);

				std::vector<byte_t> buf((size_t)std::min<int64_t>(nSize, ChunkSize));
				int64_t nLeft = nSize;
				while (nLeft != 0)
				{
					size_t nChunk = (size_t)std::min<int64_t>(nLeft, (int64_t)buf.size());
					pSrc->ReadBytes(buf.data(), nChunk);
					stream.WriteBytes(buf.data(), nChunk);
					nLeft -= nChunk;
				}
				stream.Close();

				return BlobValue(nAddr);
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPBlobValueMap] failed to write blob from stream, size: {0}, addr: {1}", nSize, nAddr, exc);
				throw;
			}
		}

		// drops the stream pages of a value that isn't (or no longer) in the tree
		void FreeBlob(const BlobValue& value)
		{
			if (!value.IsInline())
				DropBlobPages(value.Addr());
		}

		uint32_t MaxInlineSize() const { return m_nMaxInlineSize; }

	private:

		static constexpr int64_t ChunkSize = 64 * 1024;

		void InsertValue(const TKey& key, BlobValue&& value)
		{
			try
			{
				TBase::insert(key, value);
			}
			catch (...)
			{
				// e.g. the key exists: the new pages aren't referenced by anything
				if (!value.IsInline())
				{
					try { DropBlobPages(value.Addr()); }
					catch (...) {}
				}
				throw;
			}
		}

		// 0 means "inline" in the leaf format: skip it if the storage ever gives page 0
		int64_t NewBlobPageAddr()
		{
			int64_t nAddr = this->m_pStorage->GetNewFilePageAddr();
			if (nAddr == 0)
				nAddr = this->m_pStorage->GetNewFilePageAddr();
			return nAddr;
		}

		// walks the page chain written by TWriteStreamPage (page header: int64 next, int64 prev; next of the last page is -1)
		void DropBlobPages(int64_t nAddr)
		{
			try
			{
				int64_t nPrev = -1;
				storage::IFilePagePtr pPage;
				while (nAddr != -1)
				{
					if (pPage.get() == nullptr)
						pPage = this->m_pStorage->ReadPage(nAddr);
					else
						this->m_pStorage->ReadPage(pPage, nAddr);

					CommonLib::IMemoryReadStreamPtr pStream = pPage->GetReadStream();
					int64_t nNext = pStream->ReadInt64();
					int64_t nPagePrev = pStream->ReadInt64();
					if (nPagePrev != nPrev)
						throw CommonLib::CExcBase("broken page chain, page: {0}, prev: {1}, expected prev: {2}", nAddr, nPagePrev, nPrev);

					this->m_pStorage->DropFilePage(nAddr);
					nPrev = nAddr;
					nAddr = nNext;
				}
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPBlobValueMap] failed to drop blob pages, addr: {0}", nAddr, exc);
				throw;
			}
		}

	private:
		uint32_t m_nMaxInlineSize;
		CommonLib::IAllocPtr m_pBlobAlloc;
	};
}
