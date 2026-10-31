#include "TransactionPager.h"
#include "../../StorageLib/FilePage.h"

namespace bptreedb
{
	namespace db
	{
		CTransactionPager::CTransactionPager(CommonLib::IAllocPtr ptrAlloc, CTranStorageIOPtr ptrIO, storage::CFreeSpaceMapPtr ptrFreeSpace) :
			storage::CPagerBase(ptrAlloc, ptrIO), m_ptrIO(ptrIO)
		{
			m_ptrFreeSpace = ptrFreeSpace;
		}

		CTransactionPager::~CTransactionPager()
		{
		}

		void CTransactionPager::Flush()
		{
		}

		int64_t CTransactionPager::AllocBlocks(uint32_t nSize)
		{
			int64_t nAddr = -1;
			uint32_t nBlocks = nSize / storage::BlockSize;
			if (m_ptrFreeSpace.get() != nullptr && m_ptrFreeSpace->Take(nBlocks, nAddr))
			{
				m_taken.emplace_back(nAddr, nBlocks);
				return nAddr;
			}
			return m_ptrIO->GetNewFilePageAddr(nSize);
		}

		void CTransactionPager::ReleaseBlocks(int64_t nAddr, uint32_t nSize)
		{
			m_ptrIO->DropData(nAddr, nSize);      // its private copy isn't written
			m_dropped.emplace_back(nAddr, nSize / storage::BlockSize);
		}

		void CTransactionPager::SaveFreeSpace(storage::IFilePagerPtr ptrPager, int64_t nRootPage)
		{
			try
			{
				if (m_ptrFreeSpace.get() == nullptr || nRootPage < 0)
					throw CommonLib::CExcBase("no free space map");
				if (ptrPager.get() != this)
					throw CommonLib::CExcBase("the map is saved through its own pager");
				m_ptrFreeSpace->Save(ptrPager, nRootPage, m_dropped);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("[CTransactionPager] failed to save the free space map", excSrc);
				throw;
			}
		}

		void CTransactionPager::Committed()
		{
			if (m_ptrFreeSpace.get() != nullptr)
				for (const auto& page : m_dropped)
					m_ptrFreeSpace->Add(page.first, page.second);
			m_dropped.clear();
			m_taken.clear();
		}

		void CTransactionPager::RolledBack()
		{
			if (m_ptrFreeSpace.get() != nullptr)
				for (const auto& page : m_taken)
					m_ptrFreeSpace->Add(page.first, page.second);
			m_dropped.clear();
			m_taken.clear();
		}
	}
}
