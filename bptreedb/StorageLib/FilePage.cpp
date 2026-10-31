#include "pch.h"
#include "FilePage.h"


namespace bptreedb
{
	namespace storage
	{
		

		IFilePagePtr CFilePage::Read(IStorageIOPtr ptrPageIO, CommonLib::IAllocPtr ptrAlloc, int64_t nAddr)
		{
			try
			{
				if (nAddr < 0)
					throw CommonLib::CExcBase("Wrong page reference");
				CPageMemoryBufferPtr ptrBuffer = std::make_shared<CPageMemoryBuffer>(ptrAlloc, PageRef::Size(nAddr, ptrPageIO->GetPageSize()));
				ptrPageIO->ReadData(PageRef::Addr(nAddr), ptrBuffer->GetFullData(), ptrBuffer->GetFullSize());

				CFilePagePtr ptrPage = std::make_shared<CFilePage>(ptrBuffer, nAddr);
				ptrPage->CheckCRCAndThrow();
				
				return std::static_pointer_cast<storage::IFilePage>(ptrPage);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("Failed to read file page addr: {0}", nAddr, excSrc);
				throw;
			}
		}
		
	}
}