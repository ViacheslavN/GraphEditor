#include "pch.h"
#include "PagerBase.h"

namespace bptreedb
{
	namespace storage
	{
		CPagerBase::CPagerBase(CommonLib::IAllocPtr ptrAlloc, IStorageIOPtr ptrIO) : m_ptrAlloc(ptrAlloc), m_ptrPageIO(ptrIO)
		{
		}

		CPagerBase::~CPagerBase()
		{
		}

		uint32_t CPagerBase::GetPageSize() const
		{
			return m_ptrPageIO->GetPageSize();
		}

		int64_t CPagerBase::GetNewFilePageAddr()
		{
			return GetNewFilePageRef(0);
		}

		int64_t CPagerBase::AllocBlocks(uint32_t nSize)
		{
			int64_t nAddr = -1;
			if (m_ptrFreeSpace.get() != nullptr && m_ptrFreeSpace->Take(nSize / BlockSize, nAddr))
				return nAddr;
			return m_ptrPageIO->GetNewFilePageAddr(nSize);
		}

		void CPagerBase::ReleaseBlocks(int64_t nAddr, uint32_t nSize)
		{
			m_ptrPageIO->DropData(nAddr, nSize);
			if (m_ptrFreeSpace.get() != nullptr)
				m_ptrFreeSpace->Add(nAddr, nSize / BlockSize);
		}

		int64_t CPagerBase::GetNewFilePageRef(uint32_t nSize)
		{
			try
			{
				uint32_t nPageSize = nSize == 0 ? GetPageSize() : PageRef::RoundUp(nSize);
				if (nPageSize > PageRef::MaxPageSize())
					throw CommonLib::CExcBase("the page size {0} is over the maximum {1}", nSize, PageRef::MaxPageSize());

				int64_t nAddr = AllocBlocks(nPageSize);
				return PageRef::Make(nAddr, nPageSize == GetPageSize() ? 0 : nPageSize / BlockSize);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to get a new page of {0} bytes", nSize, excSrc);
				throw;
			}
		}

		IFilePagePtr CPagerBase::GetNewPage(int64_t nRef)
		{
			try
			{
				CFilePagePtr ptrPage = std::make_shared<CFilePage>(m_ptrAlloc, GetPageSize(nRef), nRef);
				return std::static_pointer_cast<IFilePage>(ptrPage);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to create new page addr: {0}", nRef, excSrc);
				throw;
			}
		}

		IFilePagePtr CPagerBase::ReadPage(int64_t nRef)
		{
			try
			{
				return CFilePage::Read(m_ptrPageIO, m_ptrAlloc, nRef);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to read page addr: {0}", nRef, excSrc);
				throw;
			}
		}

		void CPagerBase::ReadPage(IFilePagePtr ptrPage, int64_t nRef)
		{
			try
			{
				if (ptrPage.get() == nullptr)
					throw CommonLib::CExcBase("Page is null");

				ptrPage->Resize(GetPageSize(nRef));
				ptrPage->Read(m_ptrPageIO, nRef);
				ptrPage->SetAddr(nRef);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to read page addr: {0}", nRef, excSrc);
				throw;
			}
		}

		void CPagerBase::SavePage(IFilePagePtr ptrPage)
		{
			try
			{
				if (ptrPage.get() == nullptr)
					throw CommonLib::CExcBase("Page is null");

				ptrPage->Save(m_ptrPageIO);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to save page addr: {0}", ptrPage.get() ? ptrPage->GetAddr() : -1, excSrc);
				throw;
			}
		}

		void CPagerBase::DropFilePage(int64_t nRef)
		{
			try
			{
				if (nRef < 0)
					throw CommonLib::CExcBase("Wrong page reference");
				ReleaseBlocks(PageRef::Addr(nRef), GetPageSize(nRef));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to drop page addr: {0}", nRef, excSrc);
				throw;
			}
		}
	}
}
