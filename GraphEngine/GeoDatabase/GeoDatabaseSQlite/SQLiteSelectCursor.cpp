#include "SQLiteSelectCursor.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/str/str.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include "SQLiteUtils.h"

namespace GraphEngine {
    namespace GeoDatabase {


        CSQLiteSelectCursor::CSQLiteSelectCursor(const std::string& sqlSelectQuery, CommonLib::database::IDatabasePtr ptrDatabase):
                TBase(IFieldsPtr())
        {
            try
            {
                m_ptrStatment = ptrDatabase->PrepareQuery(sqlSelectQuery.c_str());
                m_ptrFields = CSQLiteUtils::ReadFields(m_ptrStatment);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CSQLiteTable: Failed to Select", exc);
            }
        }


        CSQLiteSelectCursor::CSQLiteSelectCursor(const std::string& sTableName,IFieldsPtr  ptrSourceFields, IQueryFilterPtr ptrFilter, CommonLib::database::IDatabasePtr ptrDatabase):
                TBase(IFieldsPtr(), ptrFilter)

        {

        }

        CSQLiteSelectCursor::CSQLiteSelectCursor(const std::string& sTableName, IFieldsPtr  ptrSourceFields, IQueryFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrSpatRefSource, CommonLib::database::IStatmentPtr ptrDatabase):
                TBase(IFieldsPtr(), ptrFilter, ptrSpatRefSource)
        {

        }


        std::string CSQLiteSelectCursor::CreateSQLQuery(IFieldsPtr  ptrSourceFields, IQueryFilterPtr ptrFilter, const std::string& sTableName, const std::string& sSpatialIndex)
        {

            std::string  strSqlQuery = CreateSelectPartQuery( ptrFilter, ptrSourceFields);
            const std::vector<IJoinPtr>& vecJoints =  ptrFilter->GetJoins();
            const std::string& strMainTablePrefix = ptrFilter->GetTablePrefix();
            const std::string& whereClause =  ptrFilter->GetWhereClause();

            strSqlQuery += " FROM ";
            if(strMainTablePrefix.empty())
                strSqlQuery += sTableName;
            else
                strSqlQuery += sTableName  +" AS " + strMainTablePrefix;



            if(!sSpatialIndex.empty())
            {
                strSqlQuery += ", " +  sSpatialIndex  + " AS " + "spatialIndexValue \n";
            }

            if(!vecJoints.empty())
            {
                strSqlQuery += CreateJoinPartQuery(vecJoints, strMainTablePrefix);
            }


            if(!sSpatialIndex.empty())
            {

                strSqlQuery += " WHERE ";

                CommonLib::bbox& bbox = m_ptrExtentSource->GetBoundingBox();

                if(m_spatialRel == srlUndefined || m_spatialRel == srlIntersects)
                {
                    strSqlQuery += CommonLib::str_format::AStrFormatSafeT("( spatialIndexValue.maxX>={0} AND spatialIndexValue.minX<={1} AND spatialIndexValue.maxY>={2} AND spatialIndexValue.minY<={3} ",
                                      bbox.xMin, bbox.xMax, bbox.yMin, bbox.yMax);
                }
                else
                {
                    strSqlQuery +=  CommonLib::str_format::AStrFormatSafeT("( spatialIndexValue.minX>={0} AND spatialIndexValue.maxX<={1} AND spatialIndexValue.minY>={2} AND spatialIndexValue.maxY<={3} ",
                                      bbox.xMin, bbox.xMax, bbox.yMin, bbox.yMax);
                }
                if(strMainTablePrefix.empty())
                    strSqlQuery += (" AND rowid =  spatialIndexValue.feature_id)");
                else
                    strSqlQuery += CommonLib::str_format::AStrFormatSafeT("{0}.rowid = spatialIndexValue.feature_id)", strMainTablePrefix);

                strSqlQuery +=  "\n";
            }

            if(!whereClause.empty())
            {
                if(!sSpatialIndex.empty())
                    strSqlQuery += " WHERE ";

                strSqlQuery += whereClause;

            }



            return strSqlQuery;

        }


        bool CSQLiteSelectCursor::Next()
        {
            return  m_ptrStatment->Next();
        }

        bool CSQLiteSelectCursor::ColumnIsNull(int32_t col) const
        {
            return m_ptrStatment->ColumnIsNull(col);
        }

        int8_t CSQLiteSelectCursor::ReadInt8(int32_t col) const
        {
            return (int8_t) m_ptrStatment->ReadInt16(col);
        }

        uint8_t CSQLiteSelectCursor::ReadUInt8(int32_t col) const
        {
            return (int8_t) m_ptrStatment->ReadUInt16(col);
        }

        int16_t CSQLiteSelectCursor::ReadInt16(int32_t col) const
        {
            return (int16_t) m_ptrStatment->ReadInt16(col);
        }

        uint16_t CSQLiteSelectCursor::ReadUInt16(int32_t col) const
        {
            return (uint16_t) m_ptrStatment->ReadUInt16(col);
        }

        int32_t CSQLiteSelectCursor::ReadInt32(int32_t col) const
        {
            return m_ptrStatment->ReadInt32(col);
        }

        uint32_t CSQLiteSelectCursor::ReadUInt32(int32_t col) const
        {
            return m_ptrStatment->ReadUInt32(col);
        }


        int64_t CSQLiteSelectCursor::ReadInt64(int32_t col) const
        {
            return m_ptrStatment->ReadInt64(col);
        }

        uint64_t CSQLiteSelectCursor::ReadUInt64(int32_t col) const
        {
            return m_ptrStatment->ReadUInt64(col);
        }

        float CSQLiteSelectCursor::ReadFloat(int32_t col) const
        {
            return m_ptrStatment->ReadFloat(col);
        }

        double CSQLiteSelectCursor::ReadDouble(int32_t col) const
        {
            return m_ptrStatment->ReadDouble(col);
        }

        void CSQLiteSelectCursor::ReadText(int32_t col, std::string& text) const
        {
            m_ptrStatment->ReadText(col, text);
        }

        std::string CSQLiteSelectCursor::ReadText(int32_t col) const
        {
            return  m_ptrStatment->ReadText(col);
        }

        void CSQLiteSelectCursor::ReadTextW(int32_t col, std::wstring& text) const
        {
            text = ReadTextW(col);
        }

        std::wstring CSQLiteSelectCursor::ReadTextW(int32_t col) const
        {
            std::string res = ReadText(col);

            return  CommonLib::StringEncoding::str_utf82w_safe(res);
        }

        void CSQLiteSelectCursor::ReadBlob(int col, byte_t **pBuf, int32_t& size) const
        {
            m_ptrStatment->ReadBlob(col, pBuf, size);
        }

        CommonLib::IGeoShapePtr CSQLiteSelectCursor::ReadShape(int32_t col) const
        {

            if(m_ptrCacheShape.get() == nullptr)
                m_ptrCacheShape = std::make_shared<CommonLib::CGeoShape>();


            byte_t *pBuf = nullptr;
            int32_t  size = 0;

            ReadBlob(col, &pBuf, size);

            m_ptrCacheShape->Import(pBuf, size);


            return m_ptrCacheShape;

        }

    }
    }