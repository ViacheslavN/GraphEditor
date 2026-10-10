#pragma once
#include "OSMConvertorLib.h"

namespace GraphEngine {
    namespace Convertors {

        // Creates a table in the database workspace and inserts rows into it (in the given transaction).
        // The OID field (int64, primary key) is added and filled automatically; a spatial table has the field Shape,
        // its extent is calculated from the inserted shapes and saved by Finish.
        class COSMTableWriter
        {
        public:
            static const char* OIDField;
            static const char* ShapeField;

            struct SField
            {
                std::string             sName;
                GeoDatabase::eDataTypes type;
            };

            // shapeType == shape_type_null - a table without geometry
            COSMTableWriter(GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace, GeoDatabase::ITransactionPtr ptrTransaction,
                            const std::string& sTableName, const std::vector<SField>& vecFields,
                            CommonLib::eShapeType shapeType = CommonLib::shape_type_null,
                            Geometry::ISpatialReferencePtr ptrSpatRef = Geometry::ISpatialReferencePtr());

            // index of the field in vecFields, -1 - no such field
            int  FindField(const std::string& sName) const;

            // values of the current row, the fields which aren't set are NULL
            void SetInt64(int nField, int64_t nValue);
            void SetText(int nField, const std::string& sValue);
            void SetShape(CommonLib::IGeoShapePtr ptrShape);
            void Insert();

            // saves the extent of a spatial table
            void Finish();

            uint64_t                GetRowCount() const { return m_nRows; }
            GeoDatabase::ITablePtr  GetTable() const { return m_ptrTable; }
            const std::string&      GetTableName() const { return m_sTableName; }

        private:
            struct SValue
            {
                bool        bSet = false;
                bool        bText = false;
                int64_t     nValue = 0;
                std::string sValue;
            };

        private:
            std::string                       m_sTableName;
            std::vector<SField>               m_vecFields;
            std::vector<int32_t>              m_vecColumns;      // insert cursor column of every field
            int32_t                           m_nOIDColumn;
            int32_t                           m_nShapeColumn;
            std::vector<SValue>               m_vecValues;
            CommonLib::IGeoShapePtr           m_ptrShape;
            GeoDatabase::ITablePtr            m_ptrTable;
            GeoDatabase::IInsertCursorPtr     m_ptrInsert;
            Geometry::ISpatialReferencePtr    m_ptrSpatRef;
            CommonLib::bbox                   m_extent;
            uint64_t                          m_nRows;
        };

        typedef std::shared_ptr<COSMTableWriter> COSMTableWriterPtr;
    }
}
