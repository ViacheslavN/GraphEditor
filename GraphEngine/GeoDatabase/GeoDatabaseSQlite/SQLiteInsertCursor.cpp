#include "SQLiteInsertCursor.h"
#include "SQLiteUtils.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/str/str.h"
namespace GraphEngine {
    namespace GeoDatabase {

        CSQLiteInsertCursor::CSQLiteInsertCursor(const std::string&  sqlInsertQuery, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                m_ptrStatment = ptrDatabase->PrepareQuery(sqlInsertQuery.c_str());
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CSQLiteInsertCursor: Failed to create insert cursor", exc);
            }
        }

        CSQLiteInsertCursor::CSQLiteInsertCursor(ITablePtr pTable, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                IFieldsPtr ptrFields = pTable->GetFields();

                std::string sqlInsertQuery = "INSERT INTO  " + pTable->GetDatasetName() + " ( ";
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

                m_ptrStatment = ptrDatabase->PrepareQuery(sqlInsertQuery.c_str());
                m_ptrFields = ptrFields;

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CSQLiteInsertCursor: Failed to create insert cursor", exc);
            }
        }

        CSQLiteInsertCursor::~CSQLiteInsertCursor()
        {

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
            m_ptrStatment->BindInt32(col + 1, val);
        }

        void CSQLiteInsertCursor::BindUInt32(int32_t col, uint32_t val)
        {
            m_ptrStatment->BindUInt32(col + 1, val);
        }

        void CSQLiteInsertCursor::BindInt64(int32_t col, int64_t val)
        {
            m_ptrStatment->BindInt64(col + 1, val);
        }

        void CSQLiteInsertCursor::BindUInt64(int32_t col, uint64_t val)
        {
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
            m_ptrStatment->BindBlob(col + 1, ptrShape->Data(), ptrShape->Size(), copy);
        }



    }
}