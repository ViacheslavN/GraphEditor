#include "BPtreeDBSpatialTable.h"


namespace GraphEngine {
    namespace GeoDatabase {

        CBPtreeDBSpatialTable::CBPtreeDBSpatialTable(CommonLib::CGuid workspaceId, const std::string& tableName,
                                                     const std::string& viewName,  const std::string& spatialIndexName,
                                                     const std::string& shapeFieldName, const std::string& oidFieldName,
                                                     CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                                     CBPtreeDBContextPtr ptrContext) :
                TBase(workspaceId, eDatasetType::dtSpatialTable, tableName, viewName, ptrContext)
        {
            m_spatialIndexName = spatialIndexName;
            m_ShapeType = shapeType;
            m_ptrSpatialReference = ptrSpatialReference;
            m_ptrExtent = ptrExtent;
            // no OID field (f.e. the OSM converter): the row id, the column ROWID
            m_sOIDFieldName = oidFieldName.empty() ? std::string(CBPtreeDBUtils::RowIdField()) : oidFieldName;
            SetShapeFieldName(shapeFieldName);
        }

        CBPtreeDBSpatialTable::~CBPtreeDBSpatialTable()
        {

        }

        void CBPtreeDBSpatialTable::SetShapeFieldName(const std::string& fieldName)
        {
            TBase::SetShapeFieldName(fieldName);

            int nIndex = m_pFields.get() ? m_pFields->FindField(fieldName) : -1;
            if(nIndex >= 0)
                m_pFields->GetField(nIndex)->SetType(dtGeometry);
        }

        void CBPtreeDBSpatialTable::SetExtent(Geometry::IEnvelopePtr ptrEnvelope)
        {
            TBase::SetExtent(ptrEnvelope);
            if(!ptrEnvelope.get())
                return;

            try
            {
                std::string sTable = GetDatasetName();
                CommonLib::bbox extent = ptrEnvelope->GetBoundingBox();
                CBPtreeDBContext* pContext = m_ptrContext.get();

                // the open write transaction of the thread (f.e. at the end of a load), else an own one
                m_ptrContext->RunInWriteTransaction(false, [&](bptreedb::ITransactionPtr ptrTran)
                {
                    CBPtreeDBUtils::SSpatialTableInfo info;
                    if(!CBPtreeDBUtils::ReadSpatialTableInfo(sTable, info, ptrTran, pContext))
                        return;

                    info.extent = extent;
                    CBPtreeDBUtils::WriteSpatialTableInfo(info, ptrTran, pContext);
                });
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to set the extent of the table {0}", GetDatasetName(), exc);
                throw;
            }
        }

        void CBPtreeDBSpatialTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            // workspace id + dataset name: CDatasetLoader::LoadTable reopens the table through its workspace
            TBase::Save(pObj);
        }

        void CBPtreeDBSpatialTable::Load(CommonLib::ISerializeObjPtr pObj)
        {

        }

    }
}
