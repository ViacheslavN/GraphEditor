#pragma once

// The file pager of a transaction (IFilePager): the trees and the schema of a write transaction get and save
// their pages through it, so the changes stay private to the transaction until the commit (CTranStorageIO).

#include "../DBCommon.h"
#include "TranStorageIO.h"

namespace bptreedb
{
	namespace db
	{
		class CTransactionPager : public storage::IFilePager
		{
		public:
			CTransactionPager(CommonLib::IAllocPtr ptrAlloc, CTranStorageIOPtr ptrIO);
			virtual ~CTransactionPager();

			// IFilePager
			virtual int64_t GetNewFilePageAddr();
			virtual storage::IFilePagePtr GetNewPage(int64_t nAddr);
			virtual storage::IFilePagePtr ReadPage(int64_t nAddr);
			virtual void ReadPage(storage::IFilePagePtr ptrPage, int64_t nAddr);
			virtual void SavePage(storage::IFilePagePtr ptrPage);
			virtual void DropFilePage(int64_t nAddr);
			virtual void Flush();             // nothing: the pages are written by the commit

			CTranStorageIOPtr IO() const { return m_ptrIO; }

		private:
			CommonLib::IAllocPtr m_ptrAlloc;
			CTranStorageIOPtr m_ptrIO;
		};
		typedef std::shared_ptr<CTransactionPager> CTransactionPagerPtr;
	}
}
