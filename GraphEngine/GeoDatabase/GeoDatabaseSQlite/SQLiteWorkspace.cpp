#include "SQLiteWorkspace.h"
#include "SQLiteTable.h"
#include "SQLiteUtils.h"
#include "SQLiteTransaction.h"
#include "SQLiteSpatialTable.h"

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
                IDatabaseWorkspacePtr ptrWrks(new CSQLiteWorkspace(pszName, pszPath, id, ptrDatabase));
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

            return std::make_shared<CSQLiteTable>(name, viewName, m_ptrDatabase);

        }

        ITablePtr CSQLiteWorkspace::CreateTableWithSpatialIndex(const std::string& name,
                                                       const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                       CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference)
        {
            CSQLiteUtils::CreateCreateTable(ptrFields, name, m_ptrDatabase);
            if(!spatialIndexName.empty())
                CSQLiteUtils::CreateSpatialIndex(spatialIndexName, "feature_id", m_ptrDatabase);

            return std::make_shared<CSQLiteSpatialTable>(name, viewName, spatialIndexName, shapeType, ptrExtent, ptrSpatialReference, m_ptrDatabase);

        }

        IDatasetPtr CSQLiteWorkspace::LoadDataset(const std::string& sName)
        {
            return LoadTable(sName);
        }

        IDatasetPtr CSQLiteWorkspace::LoadTable(const std::string& sName)
        {
            return std::make_shared<CSQLiteTable>(sName, sName, m_ptrDatabase);
        }

        IDatasetPtr CSQLiteWorkspace::LoadSpatialTable(const std::string& sName)
        {
            return  IDatasetPtr();
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
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load Workspace", exc);
            }
        }
    }
}