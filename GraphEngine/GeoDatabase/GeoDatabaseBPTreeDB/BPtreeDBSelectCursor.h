#pragma once

#include "../GeoDatabase.h"
#include "../CursorBase.h"
#include "BPtreeDBContext.h"


namespace GraphEngine {
    namespace GeoDatabase {

        // Select cursor over a bptreedb cursor. It reads in the write transaction of the thread if there is one,
        // else in its own direct read transaction, which ends when the rows are read (Next gives false) or the
        // cursor is destroyed.
        class CBPtreeDBSelectCursor : public ICursorBase<ISelectCursor>
        {
        public:
            typedef ICursorBase<ISelectCursor> TBase;

            // SQL of bptreedb
            CBPtreeDBSelectCursor(const std::string& sQuery, CBPtreeDBContextPtr ptrContext);
            // a table: the spatial index (filter extent), the where clause and the joins (by SQL)
            CBPtreeDBSelectCursor(const std::string& sTableName, const std::string& sShapeFieldName, IFieldsPtr ptrSourceFields,
                                  IQueryFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrSpatRefSource, CBPtreeDBContextPtr ptrContext);
            virtual ~CBPtreeDBSelectCursor();

            virtual bool Next();
            virtual bool ColumnIsNull(int32_t col) const;
            virtual int32_t GetColumnBytes(int32_t col) const;

            virtual int8_t ReadInt8(int32_t col) const;
            virtual uint8_t ReadUInt8(int32_t col) const ;
            virtual int16_t ReadInt16(int32_t col) const ;
            virtual uint16_t ReadUInt16(int32_t col) const ;
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
            virtual void ReadBlob(int col, byte_t **pBuf, int32_t& size) const;
            virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const;
            virtual CommonLib::CGuid  ReadGuid(int32_t col) const;
            virtual IRowPtr CreateRow() const;
            virtual void FillRow(IRowPtr ptrRow) const;

            // the row id of bptreedb (-1 for a query of several tables / with aggregation)
            int64_t GetRowID() const;

        private:
            void InitFields(IFieldsPtr ptrSourceFields);
            bool GetQueryExtent(CommonLib::bbox& extent) const;
            std::string CreateSQLQuery(IFieldsPtr ptrSourceFields, const std::string& sTableName, bool bSpatial, const CommonLib::bbox& extent);
            const bptreedb::ISelectCursor* Cursor() const;
            void Close();

        private:
            CBPtreeDBContextPtr m_ptrContext;            // first: released last
            bptreedb::ITransactionPtr m_ptrTran;
            bool m_bOwnTran{ false };
            bptreedb::ISelectCursorPtr m_ptrCursor;
            int32_t m_nRowIdCol{ -1 };                    // the column ROWID after the columns of m_ptrCursor (the API path)

            mutable CommonLib::IGeoShapePtr  m_ptrCacheShape;
            std::string m_sShapeFieldName;
            Geometry::ISpatialReferencePtr m_ptrSpatRefSource;   // of the table: shapes are projected to the output one of the filter
        };

    }
}
