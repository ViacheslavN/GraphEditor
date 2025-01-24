#include "SQLiteSpatialTable.h"


namespace GraphEngine {
    namespace GeoDatabase {




        CSQLiteSpatialTable::CSQLiteSpatialTable(const std::string& tableName,
                                                 const std::string& viewName,  const std::string& spatialIndexName,
                                                 CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                                 CommonLib::database::IDatabasePtr ptrDatabase) :
                TBase(eDatasetType::dtSpatialTable, tableName, viewName, ptrDatabase)
        {
            m_spatialIndexName = spatialIndexName;
            m_ShapeType = shapeType;
            m_ptrSpatialReference =ptrSpatialReference;

            m_ptrExtent = ptrExtent;
        }



        CSQLiteSpatialTable:: ~CSQLiteSpatialTable()
        {

        }

        void CSQLiteSpatialTable::SetGeometryType(CommonLib::eShapeType shapeType)
        {
            m_ShapeType = shapeType;
        }

        CommonLib::eShapeType CSQLiteSpatialTable::GetGeometryType() const
        {
            return m_ShapeType;
        }


        Geometry::IEnvelopePtr	 CSQLiteSpatialTable::GetExtent() const
        {
            return m_ptrExtent;
        }

        Geometry::ISpatialReferencePtr CSQLiteSpatialTable::GetSpatialReference() const
        {
            return m_ptrSpatialReference;
        }

        void CSQLiteSpatialTable::SetExtent(Geometry::IEnvelopePtr ptrEnvelope)
        {
            m_ptrExtent = ptrEnvelope;
        }

        void CSQLiteSpatialTable::SetSpatialReference(Geometry::ISpatialReferencePtr ptrSpatRef)
        {
            m_ptrSpatialReference = ptrSpatRef;
        }


        void CSQLiteSpatialTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {

        }

        void CSQLiteSpatialTable::Load(CommonLib::ISerializeObjPtr pObj)
        {

        }

    }
    }