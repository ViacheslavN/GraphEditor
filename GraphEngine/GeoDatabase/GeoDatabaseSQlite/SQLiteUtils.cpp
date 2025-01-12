#include "SQLiteUtils.h"
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

        eDataTypes  CSQLiteUtils::SQLiteType2FieldType(const std::string& sSQLiteType)
        {
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
                    eDataTypes dtType = SQLiteType2FieldType(ptrStatment->GetColumnType(i + 1));
                    std::string sName = ptrStatment->ColumnName(i);
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
               std::string sql =  CommonLib::str_format::AStrFormatSafeT("CREATE VIRTUAL TABLE {0}} USING rtree({1}, minX, maxX, minY, maxY)", sIndexName, sIndexField );
               ptrDatabase->Execute(sql.c_str());
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create spatial index, name: {0}, field: {1}", sIndexName, sIndexField, exc);

            }
        }


        void CSQLiteUtils::CreateCreateTable(IFieldsPtr pFields, const std::string& sTableName, CommonLib::database::IDatabasePtr ptrDatabase)
        {
            try
            {
                if(pFields->GetFieldCount() == 0)
                    CommonLib::CExcBase("Fields aren't set");


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
                       sql += "NOT NULL ";



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