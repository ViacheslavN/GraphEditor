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

        void FieldType2SQLiteType(eDataTypes type, std::string& sSQLiteType)
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
                    sSQLiteType = "INTEGER";
                    break;
                case dtFloat:
                case dtDouble:
                    sSQLiteType = "REAL";
                    break;
                case  dtBlob:
                case  dtGeometry:
                    sSQLiteType = "BLOB";
                    break;
                case  dtString:
                case  dtAnnotation:
                    sSQLiteType = "TEXT";
                    break;
            }
        }

        eDataTypes  SQLiteType2FieldType(std::string& sSQLiteType)
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

        eDataTypes SQLiteType2FieldType(int nSQLiteFieldType)
        {
            switch(nSQLiteFieldType)
            {
                case SQLITE_INTEGER:
                    return dtInteger64;
                    break;
                case SQLITE_FLOAT:
                    return dtDouble;
                    break;
                case SQLITE_BLOB:
                    return dtBlob;
                    break;
                case SQLITE_TEXT:
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

               CommonLib::database::IStatmentPtr ptrStatment = ptrDatabase->PrepareQuery(CommonLib::str_format::StrFormatSafe("pragma table_info ('{0}')", sTable).c_str()));
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
           }
       }

    }
}