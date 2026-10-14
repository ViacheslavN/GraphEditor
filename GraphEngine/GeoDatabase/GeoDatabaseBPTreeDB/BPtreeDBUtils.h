#pragma once

#include "../GeoDatabase.h"
#include "../../../bptreedb/DB/bptreedb.h"
#include "BPtreeDBContext.h"
#include <any>

namespace GraphEngine {
    namespace GeoDatabase {

        class CBPtreeDBUtils
        {
        public:
            // UTF-8 bytes of a string field created without a length: the longest string bptreedb keeps in the leaf
            // nodes (page size / 8 for the default 8K page); the OSM values (255 characters) fit.
            static const uint32_t DefaultStringLength = 1024;
            static const uint32_t MetaNameLength = 256;

            // Types. bptreedb keeps the exact type of a field: int8..uint64, float and double
            // (SQLite has only INTEGER and REAL). dtGuid and dtDate are stored as strings (read back as dtString).
            static bptreedb::eDataTypes FieldType2DBType(eDataTypes type);
            static eDataTypes DBType2FieldType(bptreedb::eDataTypes type);
            static bool IsIntegerType(eDataTypes type);

            // values: the default values of the fields (std::any) and the bptreedb variants
            static CommonLib::CVariant AnyToVariant(const std::any& value);
            static std::any VariantToAny(const CommonLib::CVariant& value);

            // the bptreedb length of a string field: nLength characters -> UTF-8 bytes (4 per character),
            // 0 - the default length of the workspace
            static uint32_t StringLength(int nLength, uint32_t nDefaultLength);

            // fields of a bptreedb table as GeoDatabase fields
            static IFieldsPtr ReadFields(bptreedb::ITablePtr ptrTable);

            // creates the fields of a new table; the primary key (one field) gets a unique index, at once or
            // in ptrPending (built at the end of a bulk load)
            static void CreateFields(bptreedb::ITablePtr ptrTable, IFieldsPtr ptrFields, uint32_t nDefaultStringLength,
                                     SBPtreeDBPendingIndexesPtr ptrPending, bptreedb::ITransactionPtr ptrTran);

            static bptreedb::IFieldSetPtr ToDBFieldSet(IFieldSetPtr ptrFieldSet, IFieldsPtr ptrSourceFields);

            // the spatial index of a field
            static bptreedb::SIndexProp MakeSpatialIndexProp(const SBPtreeDBSpatialIndexParams& params, const CommonLib::bbox& extent);
            static bool HasSpatialIndex(bptreedb::ITablePtr ptrTable, const std::string& sField);
            static bptreedb::eSpatialQueryMode SpatialRel2QueryMode(eSpatialRel rel);

            // Spatial tables metadata (table GE_SPATIAL_TABLES): what is needed to reopen a spatial table.
            // The kind of the spatial index isn't stored: it is the index of the shape field in bptreedb.
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

            // the row id as a column ("ROWID", as in SQL): the OID of a table that has no OID field of its own
            static const char* RowIdField();
            static bool IsRowIdName(const std::string& sName);
            static void WriteSpatialTableInfo(const SSpatialTableInfo& info, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext);
            static bool ReadSpatialTableInfo(const std::string& sTableName, SSpatialTableInfo& info, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext);
            static std::vector<std::string> ReadSpatialTableNames(bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext);

            // in a read transaction of the context
            static bool ReadSpatialTableInfo(const std::string& sTableName, SSpatialTableInfo& info, CBPtreeDBContext* pContext);
            static std::vector<std::string> ReadSpatialTableNames(CBPtreeDBContext* pContext);

            // the SQL text of a double (full precision)
            static std::string DoubleToSQL(double dValue);

        private:
            static void CreateSpatialTablesMeta(bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext);
        };

    }
}
