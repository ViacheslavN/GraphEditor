#pragma once

#include "SQLiteTableBase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteSpatialTable : public CSQLiteTableBase<ISpatialTable >
        {
        public:

            typedef CSQLiteTableBase<ISpatialTable> TBase;

            CSQLiteSpatialTable(const std::string& tableName,  const std::string& viewName, CommonLib::eShapeType shapeType,
            CommonLib::database::IDatabasePtr ptrDatabase, const std::string& spatialIndex);
            virtual  ~CSQLiteSpatialTable();

            virtual	void SetGeometryType(CommonLib::eShapeType shapeType);
            virtual CommonLib::eShapeType GetGeometryType() const;
            virtual Geometry::IEnvelopePtr	 GetExtent() const;
            virtual Geometry::ISpatialReferencePtr GetSpatialReference() const;
            virtual void SetExtent(Geometry::IEnvelopePtr ptrEnvelope);
            virtual void SetSpatialReference(Geometry::ISpatialReferencePtr ptrSpatRef);


            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:

            Geometry::IEnvelopePtr  m_ptrExtent;
            std::string m_spatialIndexName;
            CommonLib::eShapeType m_ShapeType;
            Geometry::ISpatialReferencePtr m_ptrSpatialReference;

        };

    }
}