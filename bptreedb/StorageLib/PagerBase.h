#pragma once
#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/alloc/alloc.h"
#include "Storage.h"
#include "FilePage.h"
#include "FreeSpaceMap.h"

namespace bptreedb
{
	namespace storage
	{
		// The pages (CFilePage) of an IStorageIO by their references (PageRef): the size of a page comes from its reference,
		// a new page of a size gets a reference with that size (the default page size - a plain block address).
		// The pagers (CFilePager - the cached file, the pager of a transaction) give it their IO.
		// With a free space map (SetFreeSpaceMap) a new page takes a dropped page of its size and a dropped page goes to the
		// map (the owner keeps the map: CFreeSpaceMap::Save / Load); a pager may defer it (AllocBlocks / ReleaseBlocks).
		class CPagerBase : public IFilePager
		{
		private:
			CPagerBase(const CPagerBase&);
			CPagerBase& operator = (const CPagerBase&);

		public:
			CPagerBase(CommonLib::IAllocPtr ptrAlloc, IStorageIOPtr ptrIO);
			virtual ~CPagerBase();

			// IFilePager
			virtual uint32_t GetPageSize() const;
			virtual int64_t GetNewFilePageAddr();
			virtual int64_t GetNewFilePageRef(uint32_t nSize);
			virtual IFilePagePtr GetNewPage(int64_t nRef);
			virtual IFilePagePtr ReadPage(int64_t nRef);
			virtual void ReadPage(IFilePagePtr ptrPage, int64_t nRef);
			virtual void SavePage(IFilePagePtr ptrPage);
			virtual void DropFilePage(int64_t nRef);

			using IFilePager::GetPageSize;

			// (not GetFreeSpace / SetFreeSpace: GetFreeSpace is a macro of winbase.h)
			void SetFreeSpaceMap(CFreeSpaceMapPtr ptrFreeSpace) { m_ptrFreeSpace = ptrFreeSpace; }
			CFreeSpaceMapPtr GetFreeSpaceMap() const { return m_ptrFreeSpace; }

		protected:
			// the block address of a new page of nSize bytes (a multiple of the block): a free page of the size, otherwise
			// the end of the storage
			virtual int64_t AllocBlocks(uint32_t nSize);
			// a dropped page: the IO drops it, the map gets it
			virtual void ReleaseBlocks(int64_t nAddr, uint32_t nSize);

		protected:
			CFreeSpaceMapPtr m_ptrFreeSpace;
			CommonLib::IAllocPtr m_ptrAlloc;
			IStorageIOPtr m_ptrPageIO;
		};
	}
}
