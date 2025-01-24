#pragma once

#include "../GeoDatabase.h"
#include "../../CommonLib/sqlitelib/Database.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteInsertCursor : public IInsertCursor
        {
        public:

            CSQLiteInsertCursor(const std::string&  sqlInsertQuery, CommonLib::database::IDatabasePtr ptrDatabase);
            CSQLiteInsertCursor(ITablePtr pTable, CommonLib::database::IDatabasePtr ptrDatabase);

            virtual ~CSQLiteInsertCursor();

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

        private:
            std::string GetInsertSql(IFieldsPtr ptrFields, const std::string& tableName);
            void InserToSpatiallIndex();

            void SetOID(int32_t col, int64_t  setOid);

        private:
            CommonLib::database::IStatmentPtr m_ptrStatment;
            CommonLib::database::IStatmentPtr m_ptrStmtSpatial;
            CommonLib::database::IDatabasePtr m_ptrDatabase;
            IFieldsPtr      m_ptrFields;
            int m_nShapeFieldIndex;
            int m_nOIDFieldIndex;
            CommonLib::bbox m_bbox;
            int64_t  m_nOID;
        };

    }
    }