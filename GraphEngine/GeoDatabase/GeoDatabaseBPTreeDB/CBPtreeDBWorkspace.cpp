#include "CBPtreeDBWorkspace.h"
#include "BPtreeDBUtils.h"
#include "BPtreeDBTable.h"
#include "BPtreeDBSpatialTable.h"
#include "BPtreeDBTransaction.h"
#include "../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"
#include "../../GisGeometry/Envelope.h"

namespace GraphEngine {
    namespace GeoDatabase {
        std::string CBPtreeDBWorkspace::m_DatabasePathProps = "DatabasePath";

        CBPtreeDBWorkspace::CBPtreeDBWorkspace() :
                TBase(wtUndefined, CommonLib::CGuid::CreateNull())
        {

        }

        CBPtreeDBWorkspace::CBPtreeDBWorkspace(const char *pszName, const char *pszDatabasePath, CommonLib::CGuid id, CBPtreeDBContextPtr ptrContext) :
                TBase(wtBPtreeDB, id)
        {
            m_sName = pszName;
            m_sDatabasePath = pszDatabasePath;
            m_ptrContext = ptrContext;
            m_ptrContext->SetDatabasePath(m_sDatabasePath);
        }

        CBPtreeDBWorkspace::~CBPtreeDBWorkspace()
        {
            // the datasets hold the context too: the database is closed by the last of them
            m_vecDatasets.clear();
            m_DataSetMap.clear();
        }

        CBPtreeDBContextPtr CBPtreeDBWorkspace::OpenDatabase(const std::string& sPath, bool bCreate, const bptreedb::SDatabaseParams& params)
        {
            bptreedb::IDatabasePtr ptrDatabase = bptreedb::CreateDatabase();
            if(bCreate)
                ptrDatabase->Create(sPath, params);
            else
                ptrDatabase->Open(sPath, params);

            return std::make_shared<CBPtreeDBContext>(ptrDatabase);
        }

        IDatabaseWorkspacePtr CBPtreeDBWorkspace::Open(const char *pszName, const char *pszPath, CommonLib::CGuid id, const bptreedb::SDatabaseParams& params)
        {
            try
            {
                std::shared_ptr<CBPtreeDBWorkspace> ptrWrks(new CBPtreeDBWorkspace(pszName, pszPath, id, OpenDatabase(pszPath, false, params)));
                ptrWrks->LoadSpatialTables();
                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open BPtreeDB workspace, name: {0}, path: {1}", pszName, pszPath, exc);
                throw;
            }
        }

        IDatabaseWorkspacePtr CBPtreeDBWorkspace::Open(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                IDatabaseWorkspacePtr ptrWrks(new CBPtreeDBWorkspace());
                ptrWrks->Load(pObj);
                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open BPtreeDB workspace", exc);
                throw;
            }
        }

        IDatabaseWorkspacePtr CBPtreeDBWorkspace::Create(const char *pszName, const char *pszPath, CommonLib::CGuid id, const bptreedb::SDatabaseParams& params)
        {
            try
            {
                return IDatabaseWorkspacePtr(new CBPtreeDBWorkspace(pszName, pszPath, id, OpenDatabase(pszPath, true, params)));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create BPtreeDB workspace, name: {0}, path: {1}", pszName, pszPath, exc);
                throw;
            }
        }

        ITablePtr CBPtreeDBWorkspace::CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields)
        {
            try
            {
                CBPtreeDBContextPtr ptrContext = m_ptrContext;
                m_ptrContext->RunInWriteTransaction(true, [&](bptreedb::ITransactionPtr ptrTran)
                {
                    SBPtreeDBPendingIndexesPtr ptrPending;
                    if(ptrContext->IsBulkLoad())
                        ptrPending = ptrContext->AddPendingIndexes(name);

                    bptreedb::ITablePtr ptrTable = ptrContext->GetDatabase()->GetSchema()->CreateTable(name, ptrTran);
                    CBPtreeDBUtils::CreateFields(ptrTable, ptrFields, ptrContext->GetDefaultStringLength(), ptrPending, ptrTran);
                });

                ITablePtr ptrTable = std::make_shared<CBPtreeDBTable>(GetWorkspaceId(), name, viewName, m_ptrContext);
                AddDataset(ptrTable);
                return ptrTable;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create table {0}", name, exc);
                throw;
            }
        }

        ITablePtr CBPtreeDBWorkspace::CreateTableWithSpatialIndex(const std::string& name,
                                                                  const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                                  CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference)
        {
            return CreateTableWithSpatialIndex(name, viewName, spatialIndexName, shapeFieldName, sOIDFieldName, ptrFields, shapeType, ptrExtent, ptrSpatialReference,
                                               m_ptrContext->GetDefaultSpatialIndex());
        }

        ITablePtr CBPtreeDBWorkspace::CreateTableWithSpatialIndex(const std::string& name,
                                                                  const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                                  CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                                                  const SBPtreeDBSpatialIndexParams& spatialIndex)
        {
            try
            {
                if(shapeFieldName.empty() || !ptrFields.get() || ptrFields->FindField(shapeFieldName) < 0)
                    throw CommonLib::CExcBase("the shape field {0} isn't among the fields", shapeFieldName);

                // bptreedb has no named indexes (one index per field): the name is kept in the metadata
                std::string sSpatialIndex = spatialIndexName.empty() ? name + "_sidx" : spatialIndexName;

                CBPtreeDBUtils::SSpatialTableInfo info;
                info.sTableName = name;
                info.sViewName = viewName;
                info.sShapeField = shapeFieldName;
                info.sOIDField = sOIDFieldName;
                info.sSpatialIndex = sSpatialIndex;
                info.shapeType = shapeType;
                if(ptrExtent.get())
                    info.extent = ptrExtent->GetBoundingBox();
                if(ptrSpatialReference.get())
                    info.sSpatialReference = ptrSpatialReference->GetProjectionString();

                // the shape field is a geometry, whatever type the caller gave it
                IFieldsPtr ptrTableFields = ptrFields->Clone();
                ptrTableFields->GetField(ptrTableFields->FindField(shapeFieldName))->SetType(dtGeometry);

                CBPtreeDBContextPtr ptrContext = m_ptrContext;
                m_ptrContext->RunInWriteTransaction(true, [&](bptreedb::ITransactionPtr ptrTran)
                {
                    SBPtreeDBPendingIndexesPtr ptrPending;
                    if(ptrContext->IsBulkLoad())
                    {
                        // the indexes are built at the end of the load: the Z-order grid covers the inserted shapes
                        ptrPending = ptrContext->AddPendingIndexes(name);
                        ptrPending->sSpatialField = shapeFieldName;
                        ptrPending->spatialParams = spatialIndex;
                        ptrPending->AddExtent(info.extent);
                    }

                    bptreedb::ITablePtr ptrTable = ptrContext->GetDatabase()->GetSchema()->CreateTable(name, ptrTran);
                    CBPtreeDBUtils::CreateFields(ptrTable, ptrTableFields, ptrContext->GetDefaultStringLength(), ptrPending, ptrTran);
                    if(!ptrPending.get())
                        ptrTable->CreateIndex(shapeFieldName, CBPtreeDBUtils::MakeSpatialIndexProp(spatialIndex, info.extent), ptrTran);

                    CBPtreeDBUtils::WriteSpatialTableInfo(info, ptrTran, ptrContext.get());
                });

                ITablePtr ptrTable = std::make_shared<CBPtreeDBSpatialTable>(GetWorkspaceId(), name, viewName, sSpatialIndex, shapeFieldName, sOIDFieldName,
                                                                             shapeType, ptrExtent.get() ? ptrExtent->Clone() : Geometry::IEnvelopePtr(),
                                                                             ptrSpatialReference, m_ptrContext);
                AddDataset(ptrTable);
                return ptrTable;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create spatial table {0}", name, exc);
                throw;
            }
        }

        IDatasetPtr CBPtreeDBWorkspace::LoadDataset(const std::string& sName)
        {
            CBPtreeDBUtils::SSpatialTableInfo info;
            if(CBPtreeDBUtils::ReadSpatialTableInfo(sName, info, m_ptrContext.get()))
                return LoadSpatialTable(sName);

            return LoadTable(sName);
        }

        std::vector<std::string> CBPtreeDBWorkspace::GetSpatialTableNames() const
        {
            return CBPtreeDBUtils::ReadSpatialTableNames(m_ptrContext.get());
        }

        void CBPtreeDBWorkspace::LoadSpatialTables()
        {
            std::vector<std::string> vecNames = CBPtreeDBUtils::ReadSpatialTableNames(m_ptrContext.get());
            for(size_t i = 0; i < vecNames.size(); ++i)
                GetDataset(vecNames[i]); // loads and caches
        }

        IDatasetPtr CBPtreeDBWorkspace::LoadTable(const std::string& sName)
        {
            try
            {
                m_ptrContext->GetDBTableThrow(sName);
                return std::make_shared<CBPtreeDBTable>(GetWorkspaceId(), sName, sName, m_ptrContext);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load table {0}", sName, exc);
                throw;
            }
        }

        IDatasetPtr CBPtreeDBWorkspace::LoadSpatialTable(const std::string& sName)
        {
            try
            {
                CBPtreeDBUtils::SSpatialTableInfo info;
                if(!CBPtreeDBUtils::ReadSpatialTableInfo(sName, info, m_ptrContext.get()))
                    throw CommonLib::CExcBase("Table {0} isn't spatial", sName);

                m_ptrContext->GetDBTableThrow(sName);

                Geometry::ISpatialReferencePtr ptrSpatRef;
                if(!info.sSpatialReference.empty())
                {
                    try
                    {
                        ptrSpatRef = std::make_shared<Geometry::CSpatialReferenceProj4>(info.sSpatialReference);
                        if(!ptrSpatRef->IsValid())
                            ptrSpatRef.reset();
                    }
                    catch (std::exception&)
                    {
                        ptrSpatRef.reset();
                    }
                }
                if(!ptrSpatRef.get())
                    ptrSpatRef = std::make_shared<Geometry::CSpatialReferenceProj4>(info.extent);

                Geometry::IEnvelopePtr ptrExtent = std::make_shared<Geometry::CEnvelope>(info.extent, ptrSpatRef);
                return std::make_shared<CBPtreeDBSpatialTable>(GetWorkspaceId(), info.sTableName, info.sViewName.empty() ? info.sTableName : info.sViewName,
                                                               info.sSpatialIndex, info.sShapeField, info.sOIDField, info.shapeType, ptrExtent, ptrSpatRef, m_ptrContext);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load spatial table {0}", sName, exc);
                throw;
            }
        }

        ITransactionPtr CBPtreeDBWorkspace::StartTransaction(eTransactionType type)
        {
            try
            {
                return std::make_shared<CBPtreeDBTransaction>(m_ptrContext, type);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("BPtreeDB workspace {0}: failed to start a transaction", m_sName, exc);
                throw;
            }
        }

        void CBPtreeDBWorkspace::SetBulkLoadMode(bool bBulk)
        {
            try
            {
                if(m_ptrContext->HasWriteTransaction())
                    throw CommonLib::CExcBase("the mode is switched outside of a write transaction");

                if(!bBulk && m_ptrContext->HasPendingIndexes())
                {
                    // a load without Finish / commit of the indexes
                    CBPtreeDBContextPtr ptrContext = m_ptrContext;
                    m_ptrContext->RunInWriteTransaction(true, [&](bptreedb::ITransactionPtr ptrTran)
                    {
                        ptrContext->BuildAllPendingIndexes(ptrTran);
                    });
                }

                m_ptrContext->SetBulkLoad(bBulk);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExcT("BPtreeDB workspace: failed to switch the bulk load mode to {0}", bBulk ? "on" : "off", exc);
                throw;
            }
        }

        void CBPtreeDBWorkspace::SetDefaultSpatialIndex(const SBPtreeDBSpatialIndexParams& params)
        {
            m_ptrContext->SetDefaultSpatialIndex(params);
        }

        SBPtreeDBSpatialIndexParams CBPtreeDBWorkspace::GetDefaultSpatialIndex() const
        {
            return m_ptrContext->GetDefaultSpatialIndex();
        }

        void CBPtreeDBWorkspace::SetInsertOnRight(bool bOnRight)
        {
            m_ptrContext->SetInsertOnRight(bOnRight);
        }

        void CBPtreeDBWorkspace::SetDefaultStringLength(uint32_t nLength)
        {
            m_ptrContext->SetDefaultStringLength(nLength);
        }

        uint32_t CBPtreeDBWorkspace::GetDefaultStringLength() const
        {
            return m_ptrContext->GetDefaultStringLength();
        }

        bool CBPtreeDBWorkspace::GetSpatialIndexType(const std::string& sTable, eBPtreeDBSpatialIndexType& type) const
        {
            CBPtreeDBUtils::SSpatialTableInfo info;
            if(!CBPtreeDBUtils::ReadSpatialTableInfo(sTable, info, m_ptrContext.get()))
                return false;

            bptreedb::ITablePtr ptrTable = m_ptrContext->GetDBTable(sTable);
            bptreedb::IIndexPtr ptrIndex = ptrTable.get() ? ptrTable->GetIndex(info.sShapeField) : bptreedb::IIndexPtr();
            if(!ptrIndex.get())
                return false;

            if(ptrIndex->GetType() == bptreedb::itSpatialRTree)
                type = bsitRTree;
            else if(ptrIndex->GetType() == bptreedb::itSpatialZOrder)
                type = bsitZOrder;
            else
                return false;

            return true;
        }

        bptreedb::IDatabasePtr CBPtreeDBWorkspace::GetDatabase() const
        {
            return m_ptrContext.get() ? m_ptrContext->GetDatabase() : bptreedb::IDatabasePtr();
        }

        void CBPtreeDBWorkspace::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyString(m_DatabasePathProps, m_sDatabasePath);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save BPtreeDB workspace", exc);
                throw;
            }
        }

        void CBPtreeDBWorkspace::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_sDatabasePath = pObj->GetPropertyString(m_DatabasePathProps);
                m_ptrContext = OpenDatabase(m_sDatabasePath, false, bptreedb::SDatabaseParams());
                m_ptrContext->SetDatabasePath(m_sDatabasePath);
                LoadSpatialTables();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load BPtreeDB workspace", exc);
                throw;
            }
        }
    }
}
