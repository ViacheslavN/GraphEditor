#pragma once

// Delete cursor: deletes rows by row id (the values of all fields and their index entries).

#include "../DBCommon.h"
#include "../Table.h"

namespace bptreedb
{
	namespace db
	{
		class CTransaction;
		typedef std::shared_ptr<CTransaction> CTransactionPtr;

		class CDeleteCursor : public IDeleteCursor
		{
		public:
			CDeleteCursor(CTransactionPtr ptrTran, CTablePtr ptrTable);
			virtual ~CDeleteCursor() {}

			virtual bool DeleteRow(int64_t nRowID);

		private:
			CTransactionPtr m_ptrTran;
			CTablePtr m_ptrTable;
		};
	}
}
