#include "InsertCursor.h"
#include "Row.h"
#include "../Values.h"
#include "../Transactions/Transaction.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include "../../../CommonLib/data/blob.h"

namespace bptreedb
{
	namespace db
	{
		CInsertCursor::CInsertCursor(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, bool bMinSplit) :
			m_ptrTran(ptrTran), m_bMinSplit(bMinSplit)
		{
			m_cols.Init(ptrTable, ptrFieldSet);
			m_values.resize(m_cols.Count());
			m_bound.resize(m_cols.Count(), false);
		}

		int32_t CInsertCursor::ColumnCount() const
		{
			return m_cols.Count();
		}

		std::string CInsertCursor::ColumnName(int32_t col) const
		{
			return m_cols.Name(col);
		}

		bool CInsertCursor::ColumnIsNull(int32_t col) const
		{
			m_cols.CheckCol(col);
			return m_values[col].IsNull();
		}

		eDataTypes CInsertCursor::GetColumnType(int32_t col) const
		{
			return m_cols.Type(col);
		}

		int32_t CInsertCursor::GetColumnBytes(int32_t col) const
		{
			m_cols.CheckCol(col);
			return ValueBytes(m_values[col]);
		}

		IFieldSetPtr CInsertCursor::GetFieldSet() const
		{
			return m_cols.FieldSet();
		}

		int64_t CInsertCursor::Next()
		{
			try
			{
				CheckUsable();
				std::vector<std::pair<CFieldPtr, CommonLib::CVariant> > values;
				for (int32_t i = 0; i < m_cols.Count(); ++i)
					if (m_bound[i])
						values.emplace_back(m_cols.Field(i), m_values[i]);

				int64_t nRowId = m_cols.Table()->InsertRow(*m_ptrTran, values, m_bMinSplit);
				for (int32_t i = 0; i < m_cols.Count(); ++i)
				{
					m_values[i] = CommonLib::CVariant();
					m_bound[i] = false;
				}
				return nRowId;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("insert cursor of table '{0}': failed to insert a row", m_cols.Table()->GetName(), excSrc);
				throw;
			}
		}

		int64_t CInsertCursor::Insert(IRowPtr ptrRow)
		{
			try
			{
				CheckUsable();
				return m_cols.Table()->InsertRow(*m_ptrTran, CCursorColumns::RowValues(m_cols.Table(), ptrRow), m_bMinSplit);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("insert cursor of table '{0}': failed to insert a row", m_cols.Table()->GetName(), excSrc);
				throw;
			}
		}

		IRowPtr CInsertCursor::CreateRow() const
		{
			return std::make_shared<CRow>(m_cols.FieldSet());
		}

		void CInsertCursor::BindNull(int32_t col) { Bind(col, CommonLib::CVariant()); }
		void CInsertCursor::BindInt8(int32_t col, int8_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindUInt8(int32_t col, uint8_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindInt16(int32_t col, int16_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindUInt16(int32_t col, uint16_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindInt32(int32_t col, int32_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindUInt32(int32_t col, uint32_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindInt64(int32_t col, int64_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindUInt64(int32_t col, uint64_t val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindFloat(int32_t col, float val) { Bind(col, CommonLib::CVariant(val)); }
		void CInsertCursor::BindDouble(int32_t col, double val) { Bind(col, CommonLib::CVariant(val)); }

		void CInsertCursor::BindText(int32_t col, const std::string& text, bool /*copy*/)
		{
			Bind(col, CommonLib::CVariant(CommonLib::astr_t(text)));
		}

		void CInsertCursor::BindTextW(int32_t col, const std::wstring& text, bool /*copy*/)
		{
			Bind(col, CommonLib::CVariant(CommonLib::StringEncoding::str_w2utf8_safe(text)));
		}

		void CInsertCursor::BindBlob(int32_t col, const byte_t* pBuf, int32_t size, bool /*copy*/)
		{
			if (size < 0 || (size > 0 && pBuf == nullptr))
				throw CommonLib::CExcBase("BindBlob: wrong buffer, size {0}", size);
			CommonLib::CBlobPtr_t ptrBlob = std::make_shared<CommonLib::Data::CBlob>();
			if (size > 0)
				ptrBlob->Copy(pBuf, (uint32_t)size);
			Bind(col, CommonLib::CVariant(ptrBlob));
		}

		void CInsertCursor::BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape)
		{
			Bind(col, ptrShape.get() ? CommonLib::CVariant(ptrShape) : CommonLib::CVariant());
		}

		void CInsertCursor::BindValue(int32_t col, const CommonLib::CVariant& value)
		{
			Bind(col, value);
		}

		void CInsertCursor::Finish()
		{
			m_bFinished = true;
		}

		void CInsertCursor::CheckUsable() const
		{
			if (m_bFinished)
				throw CommonLib::CExcBase("the cursor is finished");
			m_ptrTran->CheckWrite();
		}

		void CInsertCursor::Bind(int32_t col, const CommonLib::CVariant& value)
		{
			CheckUsable();
			m_cols.CheckCol(col);
			m_values[col] = value;
			m_bound[col] = true;
		}
	}
}
