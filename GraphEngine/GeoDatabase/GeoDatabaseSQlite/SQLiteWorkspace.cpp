#include "SQLiteWorkspace.h"
#include "SQLiteTable.h"


namespace GraphEngine {
    namespace GeoDatabase {
        std::string CSQLiteWorkspace::m_DatabasePathProps = "DatabasePath";
        std::string CSQLiteWorkspace::m_NameProps = "Name";

        CSQLiteWorkspace::CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, int32_t nID, CommonLib::database::IDatabasePtr ptrDatabase) :
                TBase(wtSqlLite, nID)
        {
            m_sName = pszName;
            m_sDatabasePath = pszDatabasePath;

            m_ptrDatabase = ptrDatabase;
        }

        CSQLiteWorkspace::CSQLiteWorkspace(CommonLib::IPropertySetPtr ptrProperties, int32_t nID) :
                TBase(wtSqlLite, nID)
        {
            m_sName = std::any_cast<std::string>(ptrProperties->GetProperty(m_NameProps));
            m_sDatabasePath = std::any_cast<std::string>(ptrProperties->GetProperty(m_DatabasePathProps));
        }

        CSQLiteWorkspace:: CSQLiteWorkspace()
        {

        }

        CSQLiteWorkspace::CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, int32_t nID) :
                TBase(wtSqlLite, nID)

        {
            m_sName = pszName;
            m_sDatabasePath = pszDatabasePath;
            m_ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(pszDatabasePath, uint32_t(CommonLib::database::WAL));
        }


        IDatabaseWorkspacePtr CSQLiteWorkspace::Open(const char *pszName, const char *pszPath, int32_t nID)
        {
            try
            {
                CommonLib::database::IDatabasePtr ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(pszPath, uint32_t(CommonLib::database::WAL));
                IDatabaseWorkspacePtr ptrWrks(new CSQLiteWorkspace(pszName, pszPath, nID, ptrDatabase));
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

        IDatabaseWorkspacePtr CSQLiteWorkspace::Create(const char *pszName, const char *pszPath, int32_t nID )
        {
            try
            {

                CommonLib::database::IDatabasePtr ptrDatabase = CommonLib::database::IDatabaseSQLiteCreator::Create(pszPath, uint32_t(CommonLib::database::CreateDatabase | CommonLib::database::WAL));
                IDatabaseWorkspacePtr ptrWrks(new CSQLiteWorkspace(pszName, pszPath, nID, ptrDatabase));
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
            throw CommonLib::CExcBase("Create table not implement");
        }

        ISpatialTablePtr CSQLiteWorkspace::CreateSpatialTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields, const std::string& shapeFiledName, const std::string& spatialIndexName)
        {
            return ISpatialTablePtr();
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
            throw CommonLib::CExcBase("StartTransaction not implement");
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