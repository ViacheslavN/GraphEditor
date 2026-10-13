#include "CursorColumns.h"
#include "../Fields/FieldSet.h"

namespace bptreedb
{
	namespace db
	{
		void CCursorColumns::Init(CTablePtr ptrTable, IFieldSetPtr ptrFieldSet)
		{
			m_ptrTable = ptrTable;
			std::shared_ptr<CFieldSet> ptrCopy = std::make_shared<CFieldSet>();
			if (ptrFieldSet.get() == nullptr)
			{
				m_fields = ptrTable->GetFieldList();
				for (const CFieldPtr& ptrField : m_fields)
					ptrCopy->Add(ptrField->GetName());
			}
			else
			{
				for (int32_t i = 0; i < ptrFieldSet->GetCount(); ++i)
				{
					m_fields.push_back(ptrTable->GetFieldThrow(ptrFieldSet->Get(i)));
					ptrCopy->Add(ptrFieldSet->Get(i));
				}
			}
			m_ptrFieldSet = ptrCopy;
		}

		void CCursorColumns::CheckCol(int32_t nCol) const
		{
			if (nCol < 0 || nCol >= (int32_t)m_fields.size())
				throw CommonLib::CExcBase("column {0} is out of range, columns {1}", nCol, (uint32_t)m_fields.size());
		}

		std::string CCursorColumns::Name(int32_t nCol) const
		{
			CheckCol(nCol);
			return m_fields[nCol]->GetName();
		}

		eDataTypes CCursorColumns::Type(int32_t nCol) const
		{
			CheckCol(nCol);
			return m_fields[nCol]->GetType();
		}

		int32_t CCursorColumns::Find(const std::string& sName) const
		{
			for (size_t i = 0; i < m_fields.size(); ++i)
				if (m_fields[i]->GetName() == sName)
					return (int32_t)i;
			return -1;
		}

		std::vector<std::pair<CFieldPtr, CommonLib::CVariant> > CCursorColumns::RowValues(const CTablePtr& ptrTable, IRowPtr ptrRow)
		{
			if (ptrRow.get() == nullptr)
				throw CommonLib::CExcBase("the row is null");
			IFieldSetPtr ptrFieldSet = ptrRow->GetFieldSet();
			std::vector<std::pair<CFieldPtr, CommonLib::CVariant> > values;
			for (int32_t i = 0; i < ptrRow->GetCount(); ++i)
				values.emplace_back(ptrTable->GetFieldThrow(ptrFieldSet->Get(i)), ptrRow->GetValue(i));
			return values;
		}
	}
}
