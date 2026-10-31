#include "pch.h"
#include "FilePager.h"
#include <algorithm>

namespace bptreedb
{
	namespace storage
	{
		namespace
		{
			IStoragePtr CheckStorage(IStoragePtr ptrStorage)
			{
				if (ptrStorage.get() == nullptr)
					throw CommonLib::CExcBase("CFilePager: storage is null");
				return ptrStorage;
			}
		}

		//
		// CFilePager
		//

		CFilePager::CFilePager(CommonLib::IAllocPtr ptrAlloc, IStoragePtr ptrStorage, uint32_t nCacheSize) :
			CPagerBase(ptrAlloc, std::make_shared<CCachedStorageIO>(ptrAlloc, CheckStorage(ptrStorage), nCacheSize))
		{
			m_ptrIO = std::static_pointer_cast<CCachedStorageIO>(m_ptrPageIO);
		}

		CFilePager::~CFilePager()
		{
			// Flush must be called explicitly; a destructor cannot safely propagate I/O errors.
		}

		void CFilePager::Flush()
		{
			m_ptrIO->Flush();
		}

		void CFilePager::ClearCache()
		{
			m_ptrIO->Clear();
		}

		IStoragePtr CFilePager::GetStorage() const
		{
			return m_ptrIO->GetStorage();
		}

		IStorageIOPtr CFilePager::GetStorageIO() const
		{
			return m_ptrIO;
		}

		uint32_t CFilePager::GetCacheSize() const
		{
			return m_ptrIO->GetCacheSize();
		}

		uint32_t CFilePager::GetCachedPageCount() const
		{
			return m_ptrIO->GetCachedPageCount();
		}

		uint64_t CFilePager::GetCachedBytes() const
		{
			return m_ptrIO->GetCachedBytes();
		}

		//
		// CFilePager::CCachedStorageIO
		//

		CFilePager::CCachedStorageIO::CCachedStorageIO(CommonLib::IAllocPtr ptrAlloc, IStoragePtr ptrStorage, uint32_t nCacheSize) :
			m_ptrStorage(ptrStorage), m_cacheSize(nCacheSize), m_pageCache(ptrAlloc)
		{
			m_nMaxBytes = (uint64_t)nCacheSize * ptrStorage->GetPageSize();
			m_nBigPage = (uint32_t)(std::min)((uint64_t)UINT32_MAX, (std::max)((uint64_t)64 * 1024, m_nMaxBytes / 8));
		}

		CFilePager::CCachedStorageIO::~CCachedStorageIO()
		{
		}

		uint32_t CFilePager::CCachedStorageIO::GetPageSize() const
		{
			return m_ptrStorage->GetPageSize();
		}

		int64_t CFilePager::CCachedStorageIO::GetNewFilePageAddr(uint32_t nSize)
		{
			return m_ptrStorage->GetNewFilePageAddr(nSize);
		}

		void CFilePager::CCachedStorageIO::_CheckSize(uint32_t nSize) const
		{
			if (nSize == 0 || (nSize % BlockSize) != 0)
				throw CommonLib::CExcBase("Wrong size: {0}, block size: {1}", nSize, BlockSize);
		}

		void CFilePager::CCachedStorageIO::ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize)
		{
			try
			{
				_CheckSize(nSize);
				std::lock_guard<std::recursive_mutex> locker(m_mutex);

				TCacheFilePagePtr ptrPage = _Find(nAddr, nSize);
				if (ptrPage.get() != nullptr)
				{
					ptrPage->CopyTo(pData);
					return;
				}

				m_ptrStorage->ReadData(nAddr, pData, nSize);
				if (nSize <= m_nBigPage)
					_AddToCache(nAddr, pData, nSize, CLEAN);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to read data, addr: {0}, size: {1}", nAddr, nSize, excSrc);
				throw;
			}
		}

		void CFilePager::CCachedStorageIO::WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize)
		{
			try
			{
				_CheckSize(nSize);
				std::lock_guard<std::recursive_mutex> locker(m_mutex);

				if (nSize > m_nBigPage)
				{
					// a big page: directly
					_Remove(nAddr);
					m_ptrStorage->WriteData(nAddr, pData, nSize);
					return;
				}

				TCacheFilePagePtr ptrPage = _Find(nAddr, nSize);
				if (ptrPage.get() != nullptr)
				{
					ptrPage->CopyFrom(pData);
					return;
				}

				// Write-back: keep the page in the cache, it reaches the storage on eviction or Flush().
				_AddToCache(nAddr, pData, nSize, DIRTY);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to write data, addr: {0}, size: {1}", nAddr, nSize, excSrc);
				throw;
			}
		}

		void CFilePager::CCachedStorageIO::DropData(int64_t nAddr, uint32_t nSize)
		{
			try
			{
				_CheckSize(nSize);
				std::lock_guard<std::recursive_mutex> locker(m_mutex);

				// Dropped pages are discarded, dirty or not.
				_Remove(nAddr);
				m_ptrStorage->DropData(nAddr, nSize);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to drop data, addr: {0}, size: {1}", nAddr, nSize, excSrc);
				throw;
			}
		}

		// Cache internals (caller holds m_mutex)

		CFilePager::TCacheFilePagePtr CFilePager::CCachedStorageIO::_Find(int64_t nAddr, uint32_t nSize)
		{
			TCacheFilePagePtr ptrPage = m_pageCache.GetElem(nAddr);
			if (ptrPage.get() == nullptr || ptrPage->pageData.size() == nSize)
				return ptrPage;

			// the space was dropped and taken by a page of another size
			if (ptrPage->pageState == DIRTY)
				m_ptrStorage->WriteData(ptrPage->nAddr, ptrPage->pageData.data(), (uint32_t)ptrPage->pageData.size());
			_Remove(nAddr);
			return TCacheFilePagePtr();
		}

		void CFilePager::CCachedStorageIO::_Remove(int64_t nAddr)
		{
			TCacheFilePagePtr ptrPage = m_pageCache.GetElem(nAddr, true);
			if (ptrPage.get() == nullptr)
				return;
			m_nBytes -= ptrPage->pageData.size();
			m_pageCache.Remove(nAddr);
		}

		void CFilePager::CCachedStorageIO::_AddToCache(int64_t nAddr, const byte_t* pData, uint32_t nSize, EFilePageState state)
		{
			if (m_cacheSize == 0)
			{
				// Cache disabled: write-through.
				if (state == DIRTY)
					m_ptrStorage->WriteData(nAddr, pData, nSize);
				return;
			}

			TCacheFilePagePtr ptrReuse;
			while (m_pageCache.Size() != 0 && m_nBytes + nSize > m_nMaxBytes)
			{
				TCacheFilePagePtr ptrEvicted = m_pageCache.RemoveBack();
				if (ptrEvicted.get() == nullptr)
					break;
				m_nBytes -= ptrEvicted->pageData.size();
				if (ptrEvicted->pageState == DIRTY)
					m_ptrStorage->WriteData(ptrEvicted->nAddr, ptrEvicted->pageData.data(), (uint32_t)ptrEvicted->pageData.size());
				ptrReuse = ptrEvicted;
			}

			TCacheFilePagePtr ptrPage;
			if (ptrReuse.get() != nullptr)
			{
				// Reuse the evicted buffer instead of reallocating.
				ptrPage = ptrReuse;
				ptrPage->nAddr = nAddr;
				ptrPage->pageData.assign(pData, pData + nSize);
			}
			else
				ptrPage = std::make_shared<SCacheFilePage>(nAddr, pData, nSize);

			ptrPage->pageState = state;
			m_pageCache.AddElem(nAddr, ptrPage);
			m_nBytes += nSize;
		}

		void CFilePager::CCachedStorageIO::Flush()
		{
			try
			{
				std::lock_guard<std::recursive_mutex> locker(m_mutex);

				// the dirty pages in the order of the addresses (the file is written forward)
				std::vector<TCacheFilePagePtr> dirty;
				auto it = m_pageCache.Begin();
				while (!it.IsNull())
				{
					TCacheFilePagePtr& ptrPage = it.Object();
					if (ptrPage->pageState == DIRTY)
						dirty.push_back(ptrPage);
					it.Next();
				}
				std::sort(dirty.begin(), dirty.end(), [](const TCacheFilePagePtr& a, const TCacheFilePagePtr& b) { return a->nAddr < b->nAddr; });
				for (TCacheFilePagePtr& ptrPage : dirty)
				{
					m_ptrStorage->WriteData(ptrPage->nAddr, ptrPage->pageData.data(), (uint32_t)ptrPage->pageData.size());
					ptrPage->pageState = CLEAN;
				}

				m_ptrStorage->Flush();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to flush pager", excSrc);
				throw;
			}
		}

		void CFilePager::CCachedStorageIO::Clear()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_pageCache.Clear();
			m_nBytes = 0;
		}

		IStoragePtr CFilePager::CCachedStorageIO::GetStorage() const
		{
			return m_ptrStorage;
		}

		uint32_t CFilePager::CCachedStorageIO::GetCacheSize() const
		{
			return m_cacheSize;
		}

		uint32_t CFilePager::CCachedStorageIO::GetCachedPageCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_pageCache.Size();
		}

		uint64_t CFilePager::CCachedStorageIO::GetCachedBytes() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_nBytes;
		}
	}
}
