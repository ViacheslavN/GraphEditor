#pragma once

// The file pager of a transaction (IFilePager): the trees and the schema of a write transaction get and save
// their pages through it, so the changes stay private to the transaction until the commit (CTranStorageIO).
// The pages by their references (storage::CPagerBase): the tree nodes and the stream pages of the blobs sized for them.
//
// The free pages (the map of the committed file, SDBContext::m_ptrFreeSpace):
//   - a new page takes a free page of its size (Taken: they go back to the map if the transaction is rolled back);
//   - a dropped page stays in use until the commit (the committed state still has it): Dropped, they join the map
//     when the commit is applied; the map with them is saved by the commit (SaveFreeSpace), so the redo log has it.

#include "../DBCommon.h"
#include "../../StorageLib/PagerBase.h"
#include "TranStorageIO.h"

namespace bptreedb
{
	namespace db
	{
		class CTransactionPager : public storage::CPagerBase
		{
		public:
			typedef std::vector<std::pair<int64_t, uint32_t> > TPages;   // block address, blocks

			CTransactionPager(CommonLib::IAllocPtr ptrAlloc, CTranStorageIOPtr ptrIO, storage::CFreeSpaceMapPtr ptrFreeSpace);
			virtual ~CTransactionPager();

			virtual void Flush();             // nothing: the pages are written by the commit

			CTranStorageIOPtr IO() const { return m_ptrIO; }

			bool HasFreeSpaceChanges() const { return !m_taken.empty() || !m_dropped.empty(); }
			// the commit: the map with the dropped pages into the private pages of ptrPager (this pager; before the redo log)
			void SaveFreeSpace(storage::IFilePagerPtr ptrPager, int64_t nRootPage);
			// the commit is applied: the dropped pages are free
			void Committed();
			// the rollback: the taken pages are free again
			void RolledBack();

		protected:
			virtual int64_t AllocBlocks(uint32_t nSize);
			virtual void ReleaseBlocks(int64_t nAddr, uint32_t nSize);

		private:
			CTranStorageIOPtr m_ptrIO;
			TPages m_taken;
			TPages m_dropped;
		};
		typedef std::shared_ptr<CTransactionPager> CTransactionPagerPtr;
	}
}
