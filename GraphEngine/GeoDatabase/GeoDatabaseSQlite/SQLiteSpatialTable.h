#pragma once

#include "SQLiteTableBase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        // SQLite table with a geometry (BLOB) column and an R-tree spatial index.
        // Its description is stored in GE_SPATIAL_TABLES (see CSQLiteUtils::WriteSpatialTableInfo)
        class CSQLiteSpatialTable : public CSQLiteTableBase<ITable >
        {
        public:

            typedef CSQLiteTableBase<ITable> TBase;

            CSQLiteSpatialTable(CommonLib::CGuid workspaceId, const std::string& name,
                                const std::string& viewName,  const std::string& spatialIndexName,
                                const std::string& shapeFieldName, const std::string& oidFieldName,
                                CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                CommonLib::database::IDatabasePtr ptrDatabase);

            virtual  ~CSQLiteSpatialTable();

            virtual void SetShapeFieldName(const std::string& fieldName);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);
        };

    }
}
