#include "TransactionPager.h"
#include "../../StorageLib/FilePage.h"

namespace bptreedb
{
	namespace db
	{
		CTransactionPager::CTransactionPager(CommonLib::IAllocPtr ptrAlloc, CTranStorageIOPtr ptrIO) : m_ptrAlloc(ptrAlloc), m_ptrIO(ptrIO)
		{
		}

		CTransactionPager::~CTransactionPager()
		{
		}

		int64_t CTransactionPager::GetNewFilePageAddr()
		{
			return m_ptrIO->GetNewFilePageAddr();
		}

		storage::IFilePagePtr CTransactionPager::GetNewPage(int64_t nAddr)
		{
			try
			{
				return std::make_shared<storage::CFilePage>(m_ptrAlloc, m_ptrIO->GetPageSize(), nAddr);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTransactionPager] failed to create a new page, addr: {0}", nAddr, excSrc);
				throw;
			}
		}

		storage::IFilePagePtr CTransactionPager::ReadPage(int64_t nAddr)
		{
			try
			{
				return storage::CFilePage::Read(m_ptrIO, m_ptrAlloc, nAddr);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTransactionPager] failed to read page {0}", nAddr, excSrc);
				throw;
			}
		}

		void CTransactionPager::ReadPage(storage::IFilePagePtr ptrPage, int64_t nAddr)
		{
			try
			{
				if (ptrPage.get() == nullptr)
					throw CommonLib::CExcBase("the page is null");
				ptrPage->Read(m_ptrIO, nAddr);
				ptrPage->SetAddr(nAddr);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTransactionPager] failed to read page {0}", nAddr, excSrc);
				throw;
			}
		}

		void CTransactionPager::SavePage(storage::IFilePagePtr ptrPage)
		{
			try
			{
				if (ptrPage.get() == nullptr)
					throw CommonLib::CExcBase("the page is null");
				ptrPage->Save(m_ptrIO);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("[CTransactionPager] failed to save page {0}", ptrPage.get() ? ptrPage->GetAddr() : -1, excSrc);
				throw;
			}
		}

		void CTransactionPager::DropFilePage(int64_t nAddr)
		{
			m_ptrIO->DropData(nAddr, m_ptrIO->GetPageSize());
		}

		void CTransactionPager::Flush()
		{
		}
	}
}
