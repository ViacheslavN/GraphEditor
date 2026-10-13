#pragma once

// Select cursor: the rows of a source (a scan of the row tree, an index range, a list), the values of the columns
// read from the value trees of the fields. TSelectCursor is also the base of the update cursor
// (instantiated in SelectCursor.cpp for ISelectCursor and IUpdateCursor).

#include "../DBCommon.h"
#include "../DBTrees.h"
#include "CursorColumns.h"

namespace bptreedb
{
	namespace db
	{
		template <class TInterface>
		class TSelectCursor : public TInterface
		{
		public:
			// bCheckRows: skip the rows of the source that don't exist (a list made before changes)
			TSelectCursor(CTransactionPtr ptrTran, CTablePtr ptrTable, IFieldSetPtr ptrFieldSet, IRowSourcePtr ptrSource, bool bCheckRows);
			virtual ~TSelectCursor() {}

			// ICursor
			virtual int32_t ColumnCount() const;
			virtual std::string ColumnName(int32_t col) const;
			virtual bool ColumnIsNull(int32_t col) const;
			virtual eDataTypes GetColumnType(int32_t col) const;
			virtual int32_t GetColumnBytes(int32_t col) const;
			virtual int32_t FindFieldByName(const std::string& name) const;
			virtual IFieldSetPtr GetFieldSet() const;
			virtual int64_t GetRowID() const;
			virtual bool Next();

			// ISelectCursor
			virtual int8_t ReadInt8(int32_t col) const;
			virtual uint8_t ReadUInt8(int32_t col) const;
			virtual int16_t ReadInt16(int32_t col) const;
			virtual uint16_t ReadUInt16(int32_t col) const;
			virtual int32_t ReadInt32(int32_t col) const;
			virtual uint32_t ReadUInt32(int32_t col) const;
			virtual int64_t ReadInt64(int32_t col) const;
			virtual uint64_t ReadUInt64(int32_t col) const;
			virtual float ReadFloat(int32_t col) const;
			virtual double ReadDouble(int32_t col) const;
			virtual void ReadText(int32_t col, std::string& text) const;
			virtual std::string ReadText(int32_t col) const;
			virtual void ReadTextW(int32_t col, std::wstring& text) const;
			virtual std::wstring ReadTextW(int32_t col) const;
			virtual void ReadBlob(int32_t col, const byte_t** pBuf, int32_t& size) const;
			virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const;
			virtual const CommonLib::CVariant& ReadValue(int32_t col) const;
			virtual IRowPtr CreateRow() const;
			virtual void FillRow(IRowPtr ptrRow) const;

		protected:
			void ReadRow();                  // the values of m_nRowId
			void CheckRow() const;           // there is a current row
			const CommonLib::CVariant& Value(int32_t col) const;    // not NULL
			template <class T>
			T Number(int32_t col) const;
			const std::string& Text(int32_t col) const;

		protected:
			CTransactionPtr m_ptrTran;
			CCursorColumns m_cols;
			IRowSourcePtr m_ptrSource;
			std::vector<IValueReaderPtr> m_readers;
			std::vector<CommonLib::CVariant> m_values;
			int64_t m_nRowId{ -1 };
			bool m_bEnd{ false };
			bool m_bCheckRows{ false };
		};

		typedef TSelectCursor<ISelectCursor> CSelectCursor;
	}
}
