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

            // Spatial tables metadata (table GE_SPATIAL_TABLES): what is needed to reopen a spatial table
            struct SSpatialTableInfo
            {
                std::string sTableName;
                std::string sViewName;
                std::string sShapeField;
                std::string sOIDField;
                std::string sSpatialIndex;
                CommonLib::eShapeType shapeType = CommonLib::shape_type_null;
                CommonLib::bbox extent;
                std::string sSpatialReference; // proj4 string
            };

            static const char* SpatialTablesMetaName();
            static void WriteSpatialTableInfo(const SSpatialTableInfo& info, CommonLib::database::IDatabasePtr ptrDatabase);
            static bool ReadSpatialTableInfo(const std::string& sTableName, SSpatialTableInfo& info, CommonLib::database::IDatabasePtr ptrDatabase);
            static std::vector<std::string> ReadSpatialTableNames(CommonLib::database::IDatabasePtr ptrDatabase);
        };

    }
}