#pragma once

#include "BPtreeDBTableBase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        // bptreedb table with a geometry field and a spatial index on it (R-tree or Z-order).
        // Its description is stored in GE_SPATIAL_TABLES (see CBPtreeDBUtils::WriteSpatialTableInfo)
        class CBPtreeDBSpatialTable : public CBPtreeDBTableBase<ITable>
        {
        public:

            typedef CBPtreeDBTableBase<ITable> TBase;

            CBPtreeDBSpatialTable(CommonLib::CGuid workspaceId, const std::string& name,
                                  const std::string& viewName,  const std::string& spatialIndexName,
                                  const std::string& shapeFieldName, const std::string& oidFieldName,
                                  CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                  CBPtreeDBContextPtr ptrContext);

            virtual  ~CBPtreeDBSpatialTable();

            virtual void SetShapeFieldName(const std::string& fieldName);
            // the extent is also written to GE_SPATIAL_TABLES (f.e. after the rows are inserted)
            virtual void SetExtent(Geometry::IEnvelopePtr ptrEnvelope);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);
        };

    }
}
