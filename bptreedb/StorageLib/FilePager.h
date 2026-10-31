#pragma once
#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/alloc/alloc.h"
#include "../UtilsLib/CacheLRU_2Q.h"
#include "Storage.h"
#include "FilePage.h"
#include "PagerBase.h"

namespace bptreedb
{
	namespace storage
	{
		// Provides file pages on top of an IStorage, with a write-back page cache.
		// Dirty pages reach the underlying storage on eviction or on Flush().
		// The pages have different sizes (PageRef): the cache keeps whole pages by their block addresses, its limit is
		// in bytes - nCacheSize pages of the default size. A big page (over an eighth of the cache, at least 64 KB: the
		// big blobs) isn't cached: it would push out many tree nodes, it is read and written directly.
		class CFilePager : public CPagerBase
		{
		private:
			CFilePager(const CFilePager&);
			CFilePager(const CFilePager&&);
			CFilePager& operator = (const CFilePager&);
			CFilePager& operator = (const CFilePager&&);

		public:
			CFilePager(CommonLib::IAllocPtr ptrAlloc, IStoragePtr ptrStorage, uint32_t nCacheSize);
			virtual ~CFilePager();

			// Writes all dirty pages to the storage and flushes it.
			virtual void Flush();
			// Drops every cached page without writing (dirty data is lost).
			void ClearCache();

			IStoragePtr GetStorage() const;
			// the cached page IO (pages of other pagers can read and write through the same cache)
			IStorageIOPtr GetStorageIO() const;
			uint32_t GetCacheSize() const;        // in the pages of the default size
			uint32_t GetCachedPageCount() const;
			uint64_t GetCachedBytes() const;

		private:

			enum EFilePageState
			{
				CLEAN,
				DIRTY
			};

			struct SCacheFilePage
			{
				SCacheFilePage(int64_t _nAddr, const byte_t* pByte, size_t size) : nAddr(_nAddr)
				{
					pageData.assign(pByte, pByte + size);
				}

				int64_t	nAddr;
				EFilePageState pageState{ CLEAN };
				std::vector<byte_t> pageData;

				void CopyTo(byte_t* pData) const
				{
					memcpy(pData, pageData.data(), pageData.size());
				}

				void CopyFrom(const byte_t* pData)
				{
					memcpy(pageData.data(), pData, pageData.size());
					pageState = DIRTY;
				}
			};

			typedef std::shared_ptr<SCacheFilePage> TCacheFilePagePtr;

			// Internal cached view of the storage. IFilePage::Read/Save work through an
			// IStorageIO, so this is what the pager hands to the pages; it is never
			// exposed outside CFilePager.
			class CCachedStorageIO : public IStorageIO
			{
			public:
				CCachedStorageIO(CommonLib::IAllocPtr ptrAlloc, IStoragePtr ptrStorage, uint32_t nCacheSize);
				virtual ~CCachedStorageIO();

				virtual uint32_t GetPageSize() const;
				virtual int64_t GetNewFilePageAddr(uint32_t nSize = 0);
				virtual void ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize);
				virtual void WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize);
				virtual void DropData(int64_t nAddr, uint32_t nSize);

				void Flush();
				void Clear();
				IStoragePtr GetStorage() const;
				uint32_t GetCacheSize() const;
				uint32_t GetCachedPageCount() const;
				uint64_t GetCachedBytes() const;

			private:
				void _CheckSize(uint32_t nSize) const;
				// the cached page of nAddr; a page cached with another size is written back (dirty) and dropped
				TCacheFilePagePtr _Find(int64_t nAddr, uint32_t nSize);
				void _Remove(int64_t nAddr);
				// Puts a page into the cache, evicting (and writing back if dirty) the least recently used
				// ones while the cache is over its bytes.
				void _AddToCache(int64_t nAddr, const byte_t* pData, uint32_t nSize, EFilePageState state);

			private:
				IStoragePtr m_ptrStorage;
				uint32_t m_cacheSize{ 0 };
				uint64_t m_nMaxBytes{ 0 };
				uint64_t m_nBytes{ 0 };
				uint32_t m_nBigPage{ 0 };      // the pages over it aren't cached
				mutable std::recursive_mutex m_mutex;
				utils::TCacheLRU_2Q<int64_t, TCacheFilePagePtr> m_pageCache;
			};

			typedef std::shared_ptr<CCachedStorageIO> TCachedStorageIOPtr;

		private:
			TCachedStorageIOPtr m_ptrIO;
		};

		typedef std::shared_ptr<CFilePager> CFilePagerPtr;
	}
}
