#include "SQLiteUtils.h"
#include <limits>
#include "../../CommonLib/str/str.h"
#include "../Fields.h"
#include "../Field.h"
#include "../GeoDatabaseUtils.h"
extern "C" {
#include "../../CommonLib/sqlitelib/sqlite/db/sqlite3.h"
}

namespace GraphEngine {
    namespace GeoDatabase {

        std::string CSQLiteUtils::FieldType2SQLiteType(eDataTypes type)
        {
            switch(type)
            {
                case dtInteger8:
                case dtInteger16:
                case dtInteger32:
                case dtInteger64:
                case dtUInteger8:
                case dtUInteger16:
                case dtUInteger32:
                case dtUInteger64:
                    return "INTEGER";
                    break;
                case dtFloat:
                case dtDouble:
                    return "REAL";
                    break;
                case  dtBlob:
                case  dtGeometry:
                    return "BLOB";
                    break;
                case  dtString:
                    return "TEXT";
                    break;
                default:
                    throw CommonLib::CExcBase("Failed to convert to sqlite type, Unknown type: {0}", (int)type);
            }
        }

        eDataTypes  CSQLiteUtils::SQLiteType2FieldType(const std::string& sDeclaredType)
        {
            // declared type, e.g. "INTEGER", "integer", "VARCHAR(10)" (SQLite type affinity rules)
            std::string sSQLiteType = sDeclaredType;
            std::transform(sSQLiteType.begin(), sSQLiteType.end(), sSQLiteType.begin(), [](unsigned char c){ return (char)toupper(c); });
            if(sSQLiteType.find("INT") != std::string::npos)
                return dtInteger64;
            if(sSQLiteType.find("CHAR") != std::string::npos || sSQLiteType.find("CLOB") != std::string::npos)
                return dtString;
            if(sSQLiteType.find("REAL") != std::string::npos || sSQLiteType.find("FLOA") != std::string::npos || sSQLiteType.find("DOUB") != std::string::npos)
                return dtDouble;

            if(sSQLiteType == "INTEGER")
                return dtInteger64;
            if(sSQLiteType == "REAL")
                return dtDouble;
            if(sSQLiteType == "BLOB")
                return dtBlob;
            if(sSQLiteType == "TEXT")
                return dtString;

            return dtUnknown;
        }

        eDataTypes CSQLiteUtils::SQLiteType2FieldType(CommonLib::database::EDBFieldType nSQLiteFieldType)
        {
            switch(nSQLiteFieldType)
            {
                case CommonLib::database::EDBFieldType::ftInt64_t:
                    return dtInteger64;
                    break;
                case CommonLib::database::EDBFieldType::ftDouble:
                    return dtDouble;
                    break;
                case CommonLib::database::EDBFieldType::ftBlob:
                    return dtBlob;
                    break;
                case CommonLib::database::EDBFieldType::ftString:
                    return dtString;
                    break;
                default:
                    return dtUnknown;
                    break;
            }
        }

       IFieldsPtr CSQLiteUtils::ReadFields(const std::string& sTable, CommonLib::database::IDatabasePtr ptrDatabase)
       {
           try
           {
               IFieldsPtr pFields = std::make_shared<CFields>();

               CommonLib::database::IStatmentPtr ptrStatment = ptrDatabase->PrepareQuery(CommonLib::str_format::StrFormatSafe("pragma table_info ('{0}')", sTable).c_str());
               while (ptrStatment->Next())
               {
                   std::string sName = ptrStatment->ReadText(1);
                   std::string sType = ptrStatment->ReadText(2);
                   std::string sNotnull = ptrStatment->ReadText(3);
                   std::string sDefValue = ptrStatment->ReadText(4);
                   std::string sPK = ptrStatment->ReadText(5);

                   eDataTypes type = SQLiteType2FieldType(sType);
                   IFieldPtr pField = std::make_shared<CField>();
                   pField->SetType(type);
                   pField->SetName(sName);
                   pField->SetIsNullable(sType != "1");
                   if(!sDefValue.empty())
                   {
                       std::any var = CGeoDatabaseUtils::GetVariantFromString(type, sDefValue);
                       pField->SetIsDefault(var);
                   }
                   pField->SetIsPrimaryKey(sPK == "1");
                   pFields->AddField(pField);
               }


               return pFields;
           }
           catch (std::exception& exc)
           {
               CommonLib::CExcBase::RegenExc("Failed to read fileds info for table {0}", sTable,  exc);
               throw;
           }
       }


        IFieldsPtr CSQLiteUtils::ReadFields( CommonLib::database::IStatmentPtr ptrStatment)
        {
            try
            {
                IFieldsPtr pFields = std::make_shared<CFields>();
                int32_t colums = ptrStatment->ColumnCount();
                for(int32_t i = 0; i <  colums; ++i)
                {
                    IFieldPtr pField = std::make_shared<CField>();
                    std::string sName = ptrStatment->ColumnName(i);
                    eDataTypes dtType = SQLiteType2FieldType(ptrStatment->GetColumnType(i));

                    uint32_t size = ptrStatment->GetColumnBytes(i);

                    pField->SetName(sName);
                    pField->SetLength(size);
                    pField->SetType(dtType);
                    pFields->AddField(pField);
                }

                return pFields;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read fileds from statment",  exc);
                throw;
            }
        }

        void CSQLiteUtils::CreateSpatialIndex( const std::string& sIndexName, const std::string& sIndexField, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
               std::string sql =  CommonLib::str_format::AStrFormatSafeT("CREATE VIRTUAL TABLE {0} USING rtree({1}, minX, maxX, minY, maxY)", sIndexName, sIndexField );
               ptrDatabase->Execute(sql.c_str());
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create spatial index, name: {0}, field: {1}", sIndexName, sIndexField, exc);

            }
        }


        const char* CSQLiteUtils::SpatialTablesMetaName()
        {
            return "GE_SPATIAL_TABLES";
        }

        void CSQLiteUtils::WriteSpatialTableInfo(const SSpatialTableInfo& info, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                if(!ptrDatabase->IsTableExists(SpatialTablesMetaName()))
                {
                    std::string sql = std::string("CREATE TABLE ") + SpatialTablesMetaName() +
                            " (TableName TEXT NOT NULL PRIMARY KEY, ViewName TEXT, ShapeField TEXT, OIDField TEXT, SpatialIndex TEXT,"
                            " ShapeType INTEGER, XMin REAL, YMin REAL, XMax REAL, YMax REAL, SpatialReference TEXT)";
                    ptrDatabase->Execute(sql.c_str());
                }

                std::string sql = std::string("INSERT OR REPLACE INTO ") + SpatialTablesMetaName() +
                        " (TableName, ViewName, ShapeField, OIDField, SpatialIndex, ShapeType, XMin, YMin, XMax, YMax, SpatialReference)"
                        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
                CommonLib::database::IStatmentPtr ptrStatment = ptrDatabase->PrepareQuery(sql.c_str());
                ptrStatment->BindText(1, info.sTableName, true);
                ptrStatment->BindText(2, info.sViewName, true);
                ptrStatment->BindText(3, info.sShapeField, true);
                ptrStatment->BindText(4, info.sOIDField, true);
                ptrStatment->BindText(5, info.sSpatialIndex, true);
                ptrStatment->BindInt64(6, (int64_t)info.shapeType);
                // no extent (an empty table): NULL (SQLite stores NaN as NULL), ReadSpatialTableInfo gives a null bbox
                bool bExtent = (info.extent.type & CommonLib::bbox_type_normal) != 0;
                const double dNull = std::numeric_limits<double>::quiet_NaN();
                ptrStatment->BindDouble(7, bExtent ? info.extent.xMin : dNull);
                ptrStatment->BindDouble(8, bExtent ? info.extent.yMin : dNull);
                ptrStatment->BindDouble(9, bExtent ? info.extent.xMax : dNull);
                ptrStatment->BindDouble(10, bExtent ? info.extent.yMax : dNull);
                ptrStatment->BindText(11, info.sSpatialReference, true);
                ptrStatment->Next();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to write spatial table info, table: {0}", info.sTableName, exc);
            }
        }

        bool CSQLiteUtils::ReadSpatialTableInfo(const std::string& sTableName, SSpatialTableInfo& info, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                if(!ptrDatabase->IsTableExists(SpatialTablesMetaName()))
                    return false;

                std::string sql = std::string("SELECT TableName, ViewName, ShapeField, OIDField, SpatialIndex, ShapeType, XMin, YMin, XMax, YMax, SpatialReference FROM ") +
                        SpatialTablesMetaName() + " WHERE TableName = ?";
                CommonLib::database::IStatmentPtr ptrStatment = ptrDatabase->PrepareQuery(sql.c_str());
                ptrStatment->BindText(1, sTableName, true);
                if(!ptrStatment->Next())
                    return false;

                info.sTableName = ptrStatment->ReadText(0);
                info.sViewName = ptrStatment->ReadText(1);
                info.sShapeField = ptrStatment->ReadText(2);
                info.sOIDField = ptrStatment->ReadText(3);
                info.sSpatialIndex = ptrStatment->ReadText(4);
                info.shapeType = (CommonLib::eShapeType)ptrStatment->ReadInt64(5);
                info.extent = CommonLib::bbox();
                bool bNullExtent = ptrStatment->ColumnIsNull(6) || ptrStatment->ColumnIsNull(7) ||
                                   ptrStatment->ColumnIsNull(8) || ptrStatment->ColumnIsNull(9);
                if(!bNullExtent)
                {
                    double xMin = ptrStatment->ReadDouble(6);
                    double yMin = ptrStatment->ReadDouble(7);
                    double xMax = ptrStatment->ReadDouble(8);
                    double yMax = ptrStatment->ReadDouble(9);
                    // an empty table written before the NULL extent: 0, 0, 0, 0 - not a point at the origin
                    // (it would stretch the full extent of the map to (0, 0))
                    bool bEmpty = xMin == 0. && yMin == 0. && xMax == 0. && yMax == 0.;
                    if(!bEmpty && xMin <= xMax && yMin <= yMax)
                    {
                        info.extent.type = CommonLib::bbox_type_normal;
                        info.extent.xMin = xMin;
                        info.extent.yMin = yMin;
                        info.extent.xMax = xMax;
                        info.extent.yMax = yMax;
                    }
                }
                info.sSpatialReference = ptrStatment->ReadText(10);
                return true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read spatial table info, table: {0}", sTableName, exc);
                throw;
            }
        }

        std::vector<std::string> CSQLiteUtils::ReadSpatialTableNames(CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                std::vector<std::string> vecNames;
                if(!ptrDatabase->IsTableExists(SpatialTablesMetaName()))
                    return vecNames;

                std::string sql = std::string("SELECT TableName FROM ") + SpatialTablesMetaName() + " ORDER BY TableName";
                CommonLib::database::IStatmentPtr ptrStatment = ptrDatabase->PrepareQuery(sql.c_str());
                while(ptrStatment->Next())
                    vecNames.push_back(ptrStatment->ReadText(0));

                return vecNames;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read spatial tables", exc);
                throw;
            }
        }

        void CSQLiteUtils::CreateCreateTable(IFieldsPtr pFields, const std::string& sTableName, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                if(pFields->GetFieldCount() == 0)
                    throw CommonLib::CExcBase("Fields aren't set");


                std::vector<std::string> vecPrimaryKey;
                std::string sql = "CREATE TABLE ";
                sql += sTableName + " (";

                for(int i = 0, sz = pFields->GetFieldCount(); i < sz; ++i)
                {
                   IFieldPtr ptrField = pFields->GetField(i);
                   std::any defValue = ptrField->GetDefaultValue();
                   std::string type =   FieldType2SQLiteType(ptrField->GetType());
                    if(ptrField->GetIsPrimaryKey())
                    {
                        vecPrimaryKey.push_back(ptrField->GetName());
                    }

                   if( i != 0)
                       sql += ", ";

                   sql += ptrField->GetName() + " ";

                    sql += type;

                   if(defValue.has_value())
                   {
                       if(ptrField->GetType() == dtString)
                           sql += " DEFAULT  '" + CommonLib::str_format::StrFormatAnySafe(defValue) + "' ";
                       else
                           sql += " DEFAULT  " + CommonLib::str_format::StrFormatAnySafe(defValue) + " ";
                   }
                   
                   if(!ptrField->GetIsNullable())
                       sql += " NOT NULL ";



                }

                if(!vecPrimaryKey.empty())
                {
                    sql += ", PRIMARY KEY( ";

                    for (size_t i = 0, sz = vecPrimaryKey.size(); i < sz; ++i)
                    {
                        if(i != 0)
                            sql += ", ";

                        sql += vecPrimaryKey[i];
                    }

                    sql += ")";
                }
                sql += ");";

                ptrDatabase->Execute(sql.c_str());


            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create table, name: {0}", sTableName, exc);

            }
        }

    }
}