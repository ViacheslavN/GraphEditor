#include "SQLiteSpatialTable.h"


namespace GraphEngine {
    namespace GeoDatabase {

        CSQLiteSpatialTable::CSQLiteSpatialTable(CommonLib::CGuid workspaceId, const std::string& tableName,
                                                 const std::string& viewName,  const std::string& spatialIndexName,
                                                 const std::string& shapeFieldName, const std::string& oidFieldName,
                                                 CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                                 CommonLib::database::IDatabasePtr ptrDatabase) :
                TBase(workspaceId, eDatasetType::dtSpatialTable, tableName, viewName, ptrDatabase)
        {
            m_spatialIndexName = spatialIndexName;
            m_ShapeType = shapeType;
            m_ptrSpatialReference = ptrSpatialReference;
            m_ptrExtent = ptrExtent;
            m_sOIDFieldName = oidFieldName;
            SetShapeFieldName(shapeFieldName);
        }

        CSQLiteSpatialTable:: ~CSQLiteSpatialTable()
        {

        }

        void CSQLiteSpatialTable::SetShapeFieldName(const std::string& fieldName)
        {
            TBase::SetShapeFieldName(fieldName);

            // the shape is stored as BLOB, the table field describes it as geometry
            int nIndex = m_pFields.get() ? m_pFields->FindField(fieldName) : -1;
            if(nIndex >= 0)
                m_pFields->GetField(nIndex)->SetType(dtGeometry);
        }

        void CSQLiteSpatialTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            // workspace id + dataset name: CDatasetLoader::LoadTable reopens the table through its workspace
            TBase::Save(pObj);
        }

        void CSQLiteSpatialTable::Load(CommonLib::ISerializeObjPtr pObj)
        {

        }

    }
}
