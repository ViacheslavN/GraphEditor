#include "SelectCursor.h"
#include "Row.h"
#include "../Values.h"
#include "../Transactions/Transaction.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include "../../../CommonLib/data/blob.h"

namespace bptreedb
{
	namespace db
	{
		template <class TInterface>
		TSelectCursor<TInterface>::TSelectCursor(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, IRowSourcePtr ptrSource, bool bCheckRows) :
			m_ptrTran(ptrTran), m_ptrSource(ptrSource), m_bCheckRows(bCheckRows)
		{
			m_cols.Init(ptrTable, ptrFieldSet);
			m_values.resize(m_cols.Count());
			m_readers.resize(m_cols.Count());
		}

		template <class TInterface>
		int32_t TSelectCursor<TInterface>::ColumnCount() const
		{
			return m_cols.Count();
		}

		template <class TInterface>
		std::string TSelectCursor<TInterface>::ColumnName(int32_t col) const
		{
			return m_cols.Name(col);
		}

		template <class TInterface>
		bool TSelectCursor<TInterface>::ColumnIsNull(int32_t col) const
		{
			CheckRow();
			m_cols.CheckCol(col);
			return m_values[col].IsNull();
		}

		template <class TInterface>
		eDataTypes TSelectCursor<TInterface>::GetColumnType(int32_t col) const
		{
			return m_cols.Type(col);
		}

		template <class TInterface>
		int32_t TSelectCursor<TInterface>::GetColumnBytes(int32_t col) const
		{
			CheckRow();
			m_cols.CheckCol(col);
			return ValueBytes(m_values[col]);
		}

		template <class TInterface>
		int32_t TSelectCursor<TInterface>::FindFieldByName(const std::string& name) const
		{
			return m_cols.Find(name);
		}

		template <class TInterface>
		IFieldSetPtr TSelectCursor<TInterface>::GetFieldSet() const
		{
			return m_cols.FieldSet();
		}

		template <class TInterface>
		int64_t TSelectCursor<TInterface>::GetRowID() const
		{
			CheckRow();
			return m_nRowId;
		}

		template <class TInterface>
		bool TSelectCursor<TInterface>::Next()
		{
			try
			{
				m_ptrTran->CheckActive();
				const CTablePtr& ptrTable = m_cols.Table();
				ptrTable->CheckNotDropped();
				STableTreesPtr ptrTrees = m_ptrTran->Trees(*ptrTable);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);

				int64_t nRowId = -1;
				for (;;)
				{
					if (m_bEnd || !m_ptrSource->Next(nRowId))
					{
						m_bEnd = true;
						m_nRowId = -1;
						for (CommonLib::CVariant& v : m_values)
							v = CommonLib::CVariant();
						return false;
					}
					if (!m_bCheckRows || ptrTable->RowExists(*m_ptrTran, nRowId))
						break;
				}

				m_nRowId = nRowId;
				ReadRow();
				return true;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("cursor of table '{0}': failed to read the next row", m_cols.Table()->GetName(), excSrc);
				throw;
			}
		}

		template <class TInterface> int8_t TSelectCursor<TInterface>::ReadInt8(int32_t col) const { return Number<int8_t>(col); }
		template <class TInterface> uint8_t TSelectCursor<TInterface>::ReadUInt8(int32_t col) const { return Number<uint8_t>(col); }
		template <class TInterface> int16_t TSelectCursor<TInterface>::ReadInt16(int32_t col) const { return Number<int16_t>(col); }
		template <class TInterface> uint16_t TSelectCursor<TInterface>::ReadUInt16(int32_t col) const { return Number<uint16_t>(col); }
		template <class TInterface> int32_t TSelectCursor<TInterface>::ReadInt32(int32_t col) const { return Number<int32_t>(col); }
		template <class TInterface> uint32_t TSelectCursor<TInterface>::ReadUInt32(int32_t col) const { return Number<uint32_t>(col); }
		template <class TInterface> int64_t TSelectCursor<TInterface>::ReadInt64(int32_t col) const { return Number<int64_t>(col); }
		template <class TInterface> uint64_t TSelectCursor<TInterface>::ReadUInt64(int32_t col) const { return Number<uint64_t>(col); }
		template <class TInterface> float TSelectCursor<TInterface>::ReadFloat(int32_t col) const { return Number<float>(col); }
		template <class TInterface> double TSelectCursor<TInterface>::ReadDouble(int32_t col) const { return Number<double>(col); }

		template <class TInterface>
		void TSelectCursor<TInterface>::ReadText(int32_t col, std::string& text) const
		{
			text = Text(col);
		}

		template <class TInterface>
		std::string TSelectCursor<TInterface>::ReadText(int32_t col) const
		{
			return Text(col);
		}

		template <class TInterface>
		void TSelectCursor<TInterface>::ReadTextW(int32_t col, std::wstring& text) const
		{
			text = CommonLib::StringEncoding::str_utf82w_safe(Text(col));
		}

		template <class TInterface>
		std::wstring TSelectCursor<TInterface>::ReadTextW(int32_t col) const
		{
			return CommonLib::StringEncoding::str_utf82w_safe(Text(col));
		}

		template <class TInterface>
		void TSelectCursor<TInterface>::ReadBlob(int32_t col, const byte_t** pBuf, int32_t& size) const
		{
			try
			{
				const CommonLib::CVariant& value = Value(col);
				if (value.IsType<CommonLib::CBlobPtr_t>())
				{
					const CommonLib::CBlobPtr_t& ptrBlob = value.Get<CommonLib::CBlobPtr_t>();
					*pBuf = ptrBlob->Buffer();
					size = (int32_t)ptrBlob->Size();
				}
				else if (value.IsType<CommonLib::astr_t>())
				{
					const CommonLib::astr_t& s = value.Get<CommonLib::astr_t>();
					*pBuf = (const byte_t*)s.data();
					size = (int32_t)s.size();
				}
				else if (value.IsType<CommonLib::IGeoShapePtr>())
				{
					const CommonLib::IGeoShapePtr& ptrShape = value.Get<CommonLib::IGeoShapePtr>();
					*pBuf = ptrShape->Data();
					size = (int32_t)ptrShape->Size();
				}
				else
					throw CommonLib::CExcBase("a {0} column isn't a blob", DataTypeName(m_cols.Type(col)));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to read column {0}", col, excSrc);
				throw;
			}
		}

		template <class TInterface>
		CommonLib::IGeoShapePtr TSelectCursor<TInterface>::ReadShape(int32_t col) const
		{
			try
			{
				const CommonLib::CVariant& value = Value(col);
				if (!value.IsType<CommonLib::IGeoShapePtr>())
					throw CommonLib::CExcBase("a {0} column isn't a geometry", DataTypeName(m_cols.Type(col)));
				return value.Get<CommonLib::IGeoShapePtr>();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to read column {0}", col, excSrc);
				throw;
			}
		}

		template <class TInterface>
		const CommonLib::CVariant& TSelectCursor<TInterface>::ReadValue(int32_t col) const
		{
			CheckRow();
			m_cols.CheckCol(col);
			return m_values[col];
		}

		template <class TInterface>
		IRowPtr TSelectCursor<TInterface>::CreateRow() const
		{
			return std::make_shared<CRow>(m_cols.FieldSet());
		}

		template <class TInterface>
		void TSelectCursor<TInterface>::FillRow(IRowPtr ptrRow) const
		{
			CheckRow();
			if (ptrRow.get() == nullptr || ptrRow->GetCount() != m_cols.Count())
				throw CommonLib::CExcBase("the row doesn't have the columns of the cursor");
			for (int32_t i = 0; i < m_cols.Count(); ++i)
				ptrRow->SetValue(i, m_values[i]);
			ptrRow->SetRowID(m_nRowId);
		}

		template <class TInterface>
		void TSelectCursor<TInterface>::ReadRow()
		{
			for (size_t i = 0; i < m_readers.size(); ++i)
			{
				if (m_readers[i].get() == nullptr)
					m_readers[i] = m_cols.Table()->CreateReader(*m_ptrTran, m_cols.Field((int32_t)i));
				m_readers[i]->Seek(m_nRowId, m_values[i]);
			}
		}

		template <class TInterface>
		void TSelectCursor<TInterface>::CheckRow() const
		{
			m_ptrTran->CheckActive();
			if (m_nRowId == -1)
				throw CommonLib::CExcBase(m_bEnd ? "the cursor is at the end" : "no current row: call Next()");
		}

		template <class TInterface>
		const CommonLib::CVariant& TSelectCursor<TInterface>::Value(int32_t col) const
		{
			CheckRow();
			m_cols.CheckCol(col);
			if (m_values[col].IsNull())
				throw CommonLib::CExcBase("column '{0}' is NULL", m_cols.Name(col));
			return m_values[col];
		}

		template <class TInterface>
		template <class T>
		T TSelectCursor<TInterface>::Number(int32_t col) const
		{
			try
			{
				return VariantToNumber<T>(Value(col));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to read column {0}", col, excSrc);
				throw;
			}
		}

		template <class TInterface>
		const std::string& TSelectCursor<TInterface>::Text(int32_t col) const
		{
			try
			{
				const CommonLib::CVariant& value = Value(col);
				if (!value.IsType<CommonLib::astr_t>())
					throw CommonLib::CExcBase("a {0} column isn't a string", DataTypeName(m_cols.Type(col)));
				return value.Get<CommonLib::astr_t>();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to read column {0}", col, excSrc);
				throw;
			}
		}

		template class TSelectCursor<ISelectCursor>;
		template class TSelectCursor<IUpdateCursor>;
	}
}
