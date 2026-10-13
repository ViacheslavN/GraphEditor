#pragma once

// Update cursor: a select cursor that changes its current row. The rows are selected when the cursor is
// created (Create reads the source), so updates don't change which rows the cursor visits.

#include "SelectCursor.h"

namespace bptreedb
{
	namespace db
	{
		class CUpdateCursor : public TSelectCursor<IUpdateCursor>
		{
		public:
			static IUpdateCursorPtr Create(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, IRowSourcePtr ptrSource);

			CUpdateCursor(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, IRowSourcePtr ptrRows);
			virtual ~CUpdateCursor() {}

			virtual void UpdateRow(IRowPtr ptrRow);
			virtual void UpdateValue(int32_t col, const CommonLib::CVariant& value);
		};
	}
}
