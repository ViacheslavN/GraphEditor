#pragma once

// Values of one row; the columns follow the field set of the cursor that created it (IRow).

#include "../DBCommon.h"

namespace bptreedb
{
	namespace db
	{
		class CRow : public IRow
		{
		public:
			CRow(IFieldSetPtr ptrFieldSet);
			virtual ~CRow() {}

			virtual int32_t GetCount() const { return (int32_t)m_values.size(); }
			virtual const CommonLib::CVariant& GetValue(int32_t nCol) const;
			virtual void SetValue(int32_t nCol, const CommonLib::CVariant& value);
			virtual bool IsNull(int32_t nCol) const;
			virtual IFieldSetPtr GetFieldSet() const { return m_ptrFieldSet; }
			virtual int64_t GetRowID() const { return m_nRowId; }
			virtual void SetRowID(int64_t nRowID) { m_nRowId = nRowID; }

		private:
			void CheckCol(int32_t nCol) const;

			IFieldSetPtr m_ptrFieldSet;
			std::vector<CommonLib::CVariant> m_values;
			int64_t m_nRowId{ -1 };
		};
	}
}
