#pragma once

#include "SQLiteTableBase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteSpatialTable : public CSQLiteTableBase<ITable >
        {
        public:

            typedef CSQLiteTableBase<ITable> TBase;



            CSQLiteSpatialTable(const std::string& name,
                                const std::string& viewName,  const std::string& spatialIndexName,
                                CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                CommonLib::database::IDatabasePtr ptrDatabase);

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
            CommonLib::eShapeType m_ShapeType;
            Geometry::ISpatialReferencePtr m_ptrSpatialReference;

        };

    }
}