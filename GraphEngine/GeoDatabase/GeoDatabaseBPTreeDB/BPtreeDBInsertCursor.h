#pragma once

#include "../GeoDatabase.h"
#include "BPtreeDBContext.h"


namespace GraphEngine {
    namespace GeoDatabase {

        // Insert cursor over a bptreedb insert cursor (a table) or a prepared SQL statement with "?" parameters
        // (an INSERT text). The values are converted to the types of the fields by bptreedb (range checked).
        class CBPtreeDBInsertCursor : public IInsertCursor
        {
        public:

            CBPtreeDBInsertCursor(const std::string&  sqlInsertQuery, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContextPtr ptrContext);
            CBPtreeDBInsertCursor(ITablePtr pTable, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContextPtr ptrContext);

            virtual ~CBPtreeDBInsertCursor();

            virtual int32_t  ColumnCount() const;
            virtual std::string ColumnName(int32_t col) const;
            virtual bool ColumnIsNull(int32_t col) const;
            virtual eDataTypes GetColumnType(int32_t col) const;
            virtual int32_t GetColumnBytes(int32_t col) const;

            virtual void Next();

            virtual void BindInt8(int32_t col, int8_t val);
            virtual void BindUInt8(int32_t col, uint8_t val);
            virtual void BindInt16(int32_t col, int16_t val);
            virtual void BindUInt16(int32_t col, uint16_t val);
            virtual void BindInt32(int32_t col, int32_t val);
            virtual void BindUInt32(int32_t col, uint32_t val);
            virtual void BindInt64(int32_t col, int64_t val) ;
            virtual void BindUInt64(int32_t col, uint64_t val);
            virtual void BindFloat(int32_t col, float val);
            virtual void BindDouble(int32_t col, double val);
            virtual void BindText(int32_t col, const std::string& text, bool copy);
            virtual void BindTextW(int32_t col, const std::wstring& text, bool copy);
            virtual void BindBlob(int32_t col, const byte_t *pBuf, int32_t size, bool copy);
            virtual void BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape, bool copy);
            virtual void BindGuid(int32_t col, const CommonLib::CGuid& guid);

            // A DDL transaction (the bulk load mode, or ttDDL): the spatial index of the table is dropped and built
            // once by Finish (STR packing). Otherwise the index is updated row by row (nothing to do). The arguments
            // (temporary files of the SQLite R-tree builder) aren't needed: bptreedb builds it in its transaction.
            virtual void SetBulkSpatialIndex(const std::string& sTempDirUtf8, uint64_t nMemoryBytes);
            // builds the indexes that wait for the end of the load (also done by the commit)
            virtual void Finish();

            // the row id of bptreedb of the last inserted row (-1 for a statement)
            int64_t GetLastRowID() const { return m_nLastRowID; }

        private:
            void Bind(int32_t col, const CommonLib::CVariant& value);
            void CheckColumn(int32_t col) const;

        private:
            CBPtreeDBContextPtr m_ptrContext;              // first: released last
            bptreedb::ITransactionPtr m_ptrTran;
            bptreedb::IInsertCursorPtr m_ptrCursor;        // a table
            bptreedb::IStatementPtr m_ptrStatement;        // an INSERT text
            std::vector<bool> m_vecNull;                   // the statement: NULL bound

            std::string m_sTableName;
            IFieldsPtr m_ptrFields;
            int m_nShapeFieldIndex{ -1 };
            SBPtreeDBPendingIndexesPtr m_ptrPending;       // the indexes built at the end (bulk load)
            CommonLib::bbox m_bbox;                        // the shape of the row (the extent of a pending Z-order index)
            int64_t m_nLastRowID{ -1 };
            bool m_bFinished{ false };
        };

    }
}
