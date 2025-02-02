#include "SQLiteInsertCursor.h"
#include "SQLiteUtils.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/str/str.h"
#include "SQLiteSpatialTable.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CSQLiteInsertCursor::CSQLiteInsertCursor(const std::string&  sqlInsertQuery, CommonLib::database::IDatabasePtr ptrDatabase) :
                m_nShapeFieldIndex(-1),
                m_nOID(-1),
                m_nOIDFieldIndex(-1),
                m_ptrDatabase(ptrDatabase)
        {
            try
            {
                m_ptrStatment = m_ptrDatabase->PrepareQuery(sqlInsertQuery.c_str());
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CSQLiteInsertCursor: Failed to create insert cursor", exc);
            }
        }

        CSQLiteInsertCursor::CSQLiteInsertCursor(ITablePtr pTable, CommonLib::database::IDatabasePtr ptrDatabase) :
                m_nShapeFieldIndex(-1),
                m_nOIDFieldIndex(-1),
                m_nOID(-1),
                m_ptrDatabase(ptrDatabase)
        {
            try
            {
                IFieldsPtr ptrFields = pTable->GetFields();
                std::string sqlInsertQuery =  GetInsertSql(pTable->GetFields(), pTable->GetDatasetName() );
                m_ptrStatment = m_ptrDatabase->PrepareQuery(sqlInsertQuery.c_str());
                m_ptrFields = ptrFields;

                if(!pTable->GetShapeFieldName().empty())
                    m_nShapeFieldIndex = m_ptrFields->FindField(pTable->GetShapeFieldName());

                if(!pTable->GetOIDFieldName().empty())
                    m_nOIDFieldIndex = m_ptrFields->FindField(pTable->GetOIDFieldName());


                if(m_nShapeFieldIndex != -1)
                {
                    if(!pTable->GetSpatialIndexName().empty()) {
                        std::string  spatialInsertSQL = "INSERT INTO  " + pTable->GetSpatialIndexName() + "(feature_id, minX, maxX, minY, maxY)" +
                                                        " VALUES (?, ?, ?, ?, ?)";
                        m_ptrStmtSpatial = m_ptrDatabase->PrepareQuery(spatialInsertSQL.c_str());
                        m_bbox.type = CommonLib::bbox_type::bbox_type_invalid;
                    }
                }


            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CSQLiteInsertCursor: Failed to create insert cursor", exc);
            }
        }



        void  CSQLiteInsertCursor::InserToSpatiallIndex()
        {
            try
            {
                if(m_bbox.type == CommonLib::bbox_type::bbox_type_invalid)
                    throw CommonLib::CExcBase("Bound box not set");

                int64_t nRowId = -1;

                if(m_nOIDFieldIndex != -1)
                    nRowId = m_nOID;
                else
                     nRowId = m_ptrDatabase->GetLastInsertRowID();

                m_ptrStmtSpatial->BindInt64(1, nRowId);
                m_ptrStmtSpatial->BindDouble(2, m_bbox.xMin);
                m_ptrStmtSpatial->BindDouble(3, m_bbox.xMax);
                m_ptrStmtSpatial->BindDouble(4, m_bbox.yMin);
                m_ptrStmtSpatial->BindDouble(5, m_bbox.yMax);

                m_ptrStmtSpatial->Next();
                m_ptrStmtSpatial->Reset();
                m_bbox.type = CommonLib::bbox_type::bbox_type_invalid;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CSQLiteInsertCursor: Failed to insert into spatial index", exc);
            }
        }



        CSQLiteInsertCursor::~CSQLiteInsertCursor()
        {

        }

        std::string CSQLiteInsertCursor::GetInsertSql(IFieldsPtr ptrFields, const std::string& tableName)
        {


            std::string sqlInsertQuery = "INSERT INTO  " + tableName + " ( ";
            std::string sValues =  " VALUES ( ";
            for (int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
            {
                IFieldPtr ptrFiled = ptrFields->GetField(i);
                if(i != 0)
                {
                    sqlInsertQuery += ", ";
                    sValues += ", ";
                }

                sqlInsertQuery += ptrFiled->GetName();
                sValues += "?";
            }

            sValues  += " )";
            sqlInsertQuery += " )";

            sqlInsertQuery += sValues;


            return  sqlInsertQuery;
        }

        int32_t  CSQLiteInsertCursor::ColumnCount() const
        {
            if(m_ptrFields.get() == nullptr)
                throw CommonLib::CExcBase("Not information");

            return m_ptrFields->GetFieldCount();
        }

        std::string CSQLiteInsertCursor::ColumnName(int32_t col) const
        {
            if(m_ptrFields.get() == nullptr)
                throw CommonLib::CExcBase("Not information");

            return m_ptrFields->GetField(col)->GetName();
        }

        bool CSQLiteInsertCursor::ColumnIsNull(int32_t col) const
        {
            return m_ptrStatment->ColumnIsNull(col);
        }

        eDataTypes CSQLiteInsertCursor::GetColumnType(int32_t col) const
        {
            if(m_ptrFields.get() == nullptr)
                throw CommonLib::CExcBase("Not information");

            return m_ptrFields->GetField(col)->GetType();
        }

        int32_t CSQLiteInsertCursor::GetColumnBytes(int32_t col) const
        {
            if(m_ptrFields.get() == nullptr)
                throw CommonLib::CExcBase("Not information");

            return m_ptrFields->GetField(col)->GetLength();
        }

        void CSQLiteInsertCursor::Next()
        {
            m_ptrStatment->Next();
            m_ptrStatment->Reset();

            if(m_ptrStmtSpatial.get() != nullptr)
            {
                InserToSpatiallIndex();
            }
        }


        void CSQLiteInsertCursor::BindInt8(int32_t col, int8_t val)
        {
            m_ptrStatment->BindInt16(col + 1, val);
        }

        void CSQLiteInsertCursor::BindUInt8(int32_t col, uint8_t val)
        {
            m_ptrStatment->BindInt16(col + 1, val);
        }

        void CSQLiteInsertCursor::BindInt16(int32_t col, int16_t val)
        {
            m_ptrStatment->BindInt16(col + 1, val);
        }

        void CSQLiteInsertCursor::BindUInt16(int32_t col, uint16_t val)
        {
            m_ptrStatment->BindUInt16(col + 1, val);
        }

        void CSQLiteInsertCursor::BindInt32(int32_t col, int32_t val)
        {
            SetOID(col, val);
            m_ptrStatment->BindInt32(col + 1, val);
        }

        void CSQLiteInsertCursor::BindUInt32(int32_t col, uint32_t val)
        {
            SetOID(col, val);
            m_ptrStatment->BindUInt32(col + 1, val);
        }

        void CSQLiteInsertCursor::BindInt64(int32_t col, int64_t val)
        {
            SetOID(col, val);
            m_ptrStatment->BindInt64(col + 1, val);
        }

        void CSQLiteInsertCursor::BindUInt64(int32_t col, uint64_t val)
        {
            SetOID(col, val);
            m_ptrStatment->BindUInt64(col + 1, val);
        }

        void CSQLiteInsertCursor::BindFloat(int32_t col, float val)
        {
            m_ptrStatment->BindFloat(col + 1, val);
        }

        void CSQLiteInsertCursor::BindDouble(int32_t col, double val)
        {
            m_ptrStatment->BindDouble(col + 1, val);
        }

        void CSQLiteInsertCursor::BindText(int32_t col, const std::string& text, bool copy)
        {
            m_ptrStatment->BindText(col + 1, text, copy);
        }

        void CSQLiteInsertCursor::BindTextW(int32_t col, const std::wstring& text, bool copy)
        {
            std::string aText = CommonLib::StringEncoding::str_w2utf8_safe(text);
            m_ptrStatment->BindText(col + 1, aText, true);
        }

        void CSQLiteInsertCursor::BindBlob(int32_t col, const byte_t *pBuf, int32_t size, bool copy)
        {
            m_ptrStatment->BindBlob(col + 1, pBuf, size, copy);
        }

        void CSQLiteInsertCursor::BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape, bool copy)
        {
            if(m_ptrStmtSpatial.get() != nullptr)
                m_bbox = ptrShape->GetBB();

            m_ptrStatment->BindBlob(col + 1, ptrShape->Data(), ptrShape->Size(), copy);
        }

        void CSQLiteInsertCursor::BindGuid(int32_t col, const CommonLib::CGuid& guid)
        {
            BindText(col, guid.ToAstr(false), true);
        }


        void CSQLiteInsertCursor::SetOID(int32_t col, int64_t  setOid)
        {
            if(m_nOIDFieldIndex == col)
                m_nOID = setOid;
        }

    }
}