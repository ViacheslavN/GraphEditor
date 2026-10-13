#include "Row.h"

namespace bptreedb
{
	namespace db
	{
		CRow::CRow(IFieldSetPtr ptrFieldSet) : m_ptrFieldSet(ptrFieldSet)
		{
			m_values.resize(ptrFieldSet->GetCount());
		}

		void CRow::CheckCol(int32_t nCol) const
		{
			if (nCol < 0 || nCol >= (int32_t)m_values.size())
				throw CommonLib::CExcBase("column {0} is out of range, columns {1}", nCol, (uint32_t)m_values.size());
		}

		const CommonLib::CVariant& CRow::GetValue(int32_t nCol) const
		{
			CheckCol(nCol);
			return m_values[nCol];
		}

		void CRow::SetValue(int32_t nCol, const CommonLib::CVariant& value)
		{
			CheckCol(nCol);
			m_values[nCol] = value;
		}

		bool CRow::IsNull(int32_t nCol) const
		{
			CheckCol(nCol);
			return m_values[nCol].IsNull();
		}
	}
}
