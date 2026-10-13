#pragma once

// The columns of a cursor: the fields of its field set (a null field set - all fields of the table).

#include "../DBCommon.h"
#include "../Table.h"

namespace bptreedb
{
	namespace db
	{
		class CTransaction;
		typedef std::shared_ptr<CTransaction> CTransactionPtr;

		class CCursorColumns
		{
		public:
			void Init(CTablePtr ptrTable, IFieldSetPtr ptrFieldSet);

			void CheckCol(int32_t nCol) const;
			int32_t Count() const { return (int32_t)m_fields.size(); }
			std::string Name(int32_t nCol) const;
			eDataTypes Type(int32_t nCol) const;
			int32_t Find(const std::string& sName) const;      // -1 if not found

			const CTablePtr& Table() const { return m_ptrTable; }
			const CFieldPtr& Field(int32_t nCol) const { return m_fields[nCol]; }
			const IFieldSetPtr& FieldSet() const { return m_ptrFieldSet; }

			// the fields and values of a row given by the user (its own field set)
			static std::vector<std::pair<CFieldPtr, CommonLib::CVariant> > RowValues(const CTablePtr& ptrTable, IRowPtr ptrRow);

		private:
			CTablePtr m_ptrTable;
			std::vector<CFieldPtr> m_fields;
			IFieldSetPtr m_ptrFieldSet;      // a copy: the cursor doesn't follow later changes of the caller's set
		};
	}
}
