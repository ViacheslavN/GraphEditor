#pragma once
#include "../GeoDatabase.h"
#include "../../CommonLib/sqlitelib/Database.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteUtils
        {
        public:
            static std::string FieldType2SQLiteType(eDataTypes type);
            static eDataTypes SQLiteType2FieldType(const std::string& sSQLiteType);
            static eDataTypes SQLiteType2FieldType(CommonLib::database::EDBFieldType nSQLiteFieldType);
            static IFieldsPtr ReadFields(const std::string& sTable, CommonLib::database::IDatabasePtr ptrDatabase);
            static IFieldsPtr ReadFields( CommonLib::database::IStatmentPtr ptrStatment);
            static void CreateCreateTable(IFieldsPtr pFields, const std::string& sTableName, CommonLib::database::IDatabasePtr ptrDatabase);
            static void CreateSpatialIndex( const std::string& sIndexName, const std::string& sIndexField, CommonLib::database::IDatabasePtr ptrDatabase);
        };

    }
}