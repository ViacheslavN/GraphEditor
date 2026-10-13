#include "DeleteCursor.h"
#include "../Transactions/Transaction.h"

namespace bptreedb
{
	namespace db
	{
		CDeleteCursor::CDeleteCursor(CTransactionPtr ptrTran, CTablePtr ptrTable) : m_ptrTran(ptrTran), m_ptrTable(ptrTable)
		{
		}

		bool CDeleteCursor::DeleteRow(int64_t nRowID)
		{
			try
			{
				m_ptrTran->CheckWrite();
				return m_ptrTable->DeleteRow(*m_ptrTran, nRowID);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("delete cursor of table '{0}': failed to delete row {1}", m_ptrTable->GetName(), nRowID, excSrc);
				throw;
			}
		}
	}
}
