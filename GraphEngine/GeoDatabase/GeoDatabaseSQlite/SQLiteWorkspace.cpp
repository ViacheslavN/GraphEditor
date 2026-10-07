#include "SQLiteWorkspace.h"
#include "SQLiteTable.h"
#include "SQLiteUtils.h"
#include "SQLiteTransaction.h"
#include "SQLiteSpatialTable.h"
#include "../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"
#include "../../GisGeometry/Envelope.h"

namespace GraphEngine {
    namespace GeoDatabase {
        std::string CSQLiteWorkspace::m_DatabasePathProps = "DatabasePath";
        std::string CSQLiteWorkspace::m_NameProps = "Name";

        CSQLiteWorkspace::CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, CommonLib::CGuid id, CommonLib::database::IDatabasePtr ptrDatabase) :
                TBase(wtSqlLite, id)
        {
            m_sName = pszName;
            m_sDatabasePath = pszDatabasePath;

            m_ptrDatabase = ptrDatabase;
        }

        CSQLiteWorkspace::CSQLiteWorkspace(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid id) :
                TBase(wtSqlLite, id)
        {
            m_sName = std::any_cast<std::string>(ptrProperties->GetProperty(m_NameProps));
            m_sDatabasePath = std::any_cast<std::string>(ptrProperties->GetProperty(m_DatabasePathProps));
        }

        CSQLiteWorkspace:: CSQLiteWorkspace() :
                TBase(wtUndefined, CommonLib::CGuid::CreateNull())
        {

        }

        CSQLiteWorkspace::CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, CommonLib::CGuid id) :
                TBase(wtSqlLite, id)

        {
            m_sName = pszName;
            m_sDatabasePath = pszDatabasePath;
            m_ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(pszDatabasePath, uint32_t(CommonLib::database::WAL));
        }


        IDatabaseWorkspacePtr CSQLiteWorkspace::Open(const char *pszName, const char *pszPath, CommonLib::CGuid id)
        {
            try
            {
                CommonLib::database::IDatabasePtr ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(pszPath, uint32_t(CommonLib::database::WAL));
                std::shared_ptr<CSQLiteWorkspace> ptrWrks(new CSQLiteWorkspace(pszName, pszPath, id, ptrDatabase));
                ptrWrks->LoadSpatialTables();
                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open Workspace", exc);
                throw;
            }

        }

        IDatabaseWorkspacePtr CSQLiteWorkspace::Open(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                IDatabaseWorkspacePtr ptrWrks(new CSQLiteWorkspace());
                ptrWrks->Load(pObj);

                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open Workspace", exc);
                throw;
            }
        }

        IDatabaseWorkspacePtr CSQLiteWorkspace::Create(const char *pszName, const char *pszPath,CommonLib::CGuid id)
        {
            try
            {

                CommonLib::database::IDatabasePtr ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(pszPath, uint32_t(CommonLib::database::CreateDatabase | CommonLib::database::WAL));
                IDatabaseWorkspacePtr ptrWrks(new CSQLiteWorkspace(pszName, pszPath, id, ptrDatabase));
                return ptrWrks;

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create workspace, name: {0}, path: {1}", pszName, pszPath, exc);
                throw;
            }
        }


        ITablePtr CSQLiteWorkspace::CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields)
        {
            CSQLiteUtils::CreateCreateTable(ptrFields, name, m_ptrDatabase);

            return std::make_shared<CSQLiteTable>(GetWorkspaceId(), name, viewName, m_ptrDatabase);

        }

        ITablePtr CSQLiteWorkspace::CreateTableWithSpatialIndex(const std::string& name,
                                                       const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                       CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference)
        {
            try
            {
                std::string sSpatialIndex = spatialIndexName.empty() ? name + "_sidx" : spatialIndexName;

                CSQLiteUtils::CreateCreateTable(ptrFields, name, m_ptrDatabase);
                CSQLiteUtils::CreateSpatialIndex(sSpatialIndex, "feature_id", m_ptrDatabase);

                CSQLiteUtils::SSpatialTableInfo info;
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
                CSQLiteUtils::WriteSpatialTableInfo(info, m_ptrDatabase);

                ITablePtr ptrTable = std::make_shared<CSQLiteSpatialTable>(GetWorkspaceId(), name, viewName, sSpatialIndex, shapeFieldName, sOIDFieldName,
                                                                           shapeType, ptrExtent.get() ? ptrExtent->Clone() : Geometry::IEnvelopePtr(),
                                                                           ptrSpatialReference, m_ptrDatabase);
                AddDataset(ptrTable);
                return ptrTable;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create spatial table {0}", name, exc);
                throw;
            }

        }

        IDatasetPtr CSQLiteWorkspace::LoadDataset(const std::string& sName)
        {
            CSQLiteUtils::SSpatialTableInfo info;
            if(CSQLiteUtils::ReadSpatialTableInfo(sName, info, m_ptrDatabase))
                return LoadSpatialTable(sName);

            return LoadTable(sName);
        }

        std::vector<std::string> CSQLiteWorkspace::GetSpatialTableNames() const
        {
            return CSQLiteUtils::ReadSpatialTableNames(m_ptrDatabase);
        }

        void CSQLiteWorkspace::LoadSpatialTables()
        {
            std::vector<std::string> vecNames = CSQLiteUtils::ReadSpatialTableNames(m_ptrDatabase);
            for(size_t i = 0; i < vecNames.size(); ++i)
                GetDataset(vecNames[i]); // loads and caches
        }

        IDatasetPtr CSQLiteWorkspace::LoadTable(const std::string& sName)
        {
            return std::make_shared<CSQLiteTable>(GetWorkspaceId(), sName, sName, m_ptrDatabase);
        }

        IDatasetPtr CSQLiteWorkspace::LoadSpatialTable(const std::string& sName)
        {
            try
            {
                CSQLiteUtils::SSpatialTableInfo info;
                if(!CSQLiteUtils::ReadSpatialTableInfo(sName, info, m_ptrDatabase))
                    throw CommonLib::CExcBase("Table {0} isn't spatial", sName);

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
                return std::make_shared<CSQLiteSpatialTable>(GetWorkspaceId(), info.sTableName, info.sViewName.empty() ? info.sTableName : info.sViewName,
                                                             info.sSpatialIndex, info.sShapeField, info.sOIDField, info.shapeType, ptrExtent, ptrSpatRef, m_ptrDatabase);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load spatial table {0}", sName, exc);
                throw;
            }
        }

        ITransactionPtr CSQLiteWorkspace::StartTransaction(eTransactionType type)
        {
            return std::make_shared<CSQLiteTransaction>(m_ptrDatabase);
        }

        void CSQLiteWorkspace::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyString(m_DatabasePathProps, m_sDatabasePath);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save Workspace", exc);
            }
        }

        void CSQLiteWorkspace::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_sDatabasePath = pObj->GetPropertyString(m_DatabasePathProps);
                m_ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(m_sDatabasePath.c_str(), uint32_t(CommonLib::database::WAL));
                LoadSpatialTables();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load Workspace", exc);
            }
        }
    }
}