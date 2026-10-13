#pragma once

// Insert cursor: binds the values of the columns, Next() inserts the row. A column that isn't bound gets the
// default value of the field (NULL if it has none). The values are converted to the field types on insert.

#include "../DBCommon.h"
#include "CursorColumns.h"

namespace bptreedb
{
	namespace db
	{
		class CInsertCursor : public IInsertCursor
		{
		public:
			// bMinSplit: the rows come in the order of the indexed values (the index trees split with min split)
			CInsertCursor(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, bool bMinSplit);
			virtual ~CInsertCursor() {}

			virtual int32_t ColumnCount() const;
			virtual std::string ColumnName(int32_t col) const;
			virtual bool ColumnIsNull(int32_t col) const;
			virtual eDataTypes GetColumnType(int32_t col) const;
			virtual int32_t GetColumnBytes(int32_t col) const;
			virtual IFieldSetPtr GetFieldSet() const;

			virtual int64_t Next();
			virtual int64_t Insert(IRowPtr ptrRow);
			virtual IRowPtr CreateRow() const;

			virtual void BindNull(int32_t col);
			virtual void BindInt8(int32_t col, int8_t val);
			virtual void BindUInt8(int32_t col, uint8_t val);
			virtual void BindInt16(int32_t col, int16_t val);
			virtual void BindUInt16(int32_t col, uint16_t val);
			virtual void BindInt32(int32_t col, int32_t val);
			virtual void BindUInt32(int32_t col, uint32_t val);
			virtual void BindInt64(int32_t col, int64_t val);
			virtual void BindUInt64(int32_t col, uint64_t val);
			virtual void BindFloat(int32_t col, float val);
			virtual void BindDouble(int32_t col, double val);
			// the values are always copied (the copy flags are for the API compatibility)
			virtual void BindText(int32_t col, const std::string& text, bool copy);
			virtual void BindTextW(int32_t col, const std::wstring& text, bool copy);
			virtual void BindBlob(int32_t col, const byte_t* pBuf, int32_t size, bool copy);
			virtual void BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape);
			virtual void BindValue(int32_t col, const CommonLib::CVariant& value);
			virtual void Finish();

		private:
			void CheckUsable() const;
			void Bind(int32_t col, const CommonLib::CVariant& value);

		private:
			CTransactionPtr m_ptrTran;
			CCursorColumns m_cols;
			std::vector<CommonLib::CVariant> m_values;
			std::vector<bool> m_bound;
			bool m_bFinished{ false };
			bool m_bMinSplit{ false };
		};
	}
}
