#include "UpdateCursor.h"
#include "../Transactions/Transaction.h"

namespace bptreedb
{
	namespace db
	{
		IUpdateCursorPtr CUpdateCursor::Create(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, IRowSourcePtr ptrSource)
		{
			std::vector<int64_t> rows;
			{
				STableTreesPtr ptrTrees = ptrTran->Trees(*ptrTable);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				int64_t nRowId = 0;
				while (ptrSource->Next(nRowId))
					rows.push_back(nRowId);
			}
			return std::make_shared<CUpdateCursor>(ptrTran, ptrTable, ptrFieldSet, std::make_shared<CListRowSource>(std::move(rows)));
		}

		CUpdateCursor::CUpdateCursor(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, IRowSourcePtr ptrRows) :
			TSelectCursor<IUpdateCursor>(ptrTran, ptrTable, ptrFieldSet, ptrRows, true)
		{
		}

		void CUpdateCursor::UpdateRow(IRowPtr ptrRow)
		{
			try
			{
				CheckRow();
				m_ptrTran->CheckWrite();
				const CTablePtr& ptrTable = m_cols.Table();
				STableTreesPtr ptrTrees = m_ptrTran->Trees(*ptrTable);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				ptrTable->UpdateRow(*m_ptrTran, m_nRowId, CCursorColumns::RowValues(ptrTable, ptrRow));
				ReadRow();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("update cursor of table '{0}': failed to update row {1}", m_cols.Table()->GetName(), m_nRowId, excSrc);
				throw;
			}
		}

		void CUpdateCursor::UpdateValue(int32_t col, const CommonLib::CVariant& value)
		{
			try
			{
				CheckRow();
				m_cols.CheckCol(col);
				m_ptrTran->CheckWrite();
				const CTablePtr& ptrTable = m_cols.Table();
				STableTreesPtr ptrTrees = m_ptrTran->Trees(*ptrTable);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				std::vector<std::pair<CFieldPtr, CommonLib::CVariant> > values;
				values.emplace_back(m_cols.Field(col), value);
				ptrTable->UpdateRow(*m_ptrTran, m_nRowId, values);
				ReadRow();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("update cursor of table '{0}': failed to update column {1} of row {2}", m_cols.Table()->GetName(), col, m_nRowId, excSrc);
				throw;
			}
		}
	}
}
