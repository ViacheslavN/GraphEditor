#include "SQLiteWorkspace.h"

namespace GraphEngine {
    namespace GeoDatabase {
        std::string CSQLiteWorkspace::m_DatabasePathProps = "DatabasePath";
        std::string CSQLiteWorkspace::m_NameProps = "Name";

        CSQLiteWorkspace::CSQLiteWorkspace(CommonLib::IPropertySetPtr ptrProperties, int32_t nID) :
                TBase(wtShapeFile, nID)
        {
            m_sName = std::any_cast<std::string>(ptrProperties->GetProperty(m_NameProps));
            m_sDatabasePath = std::any_cast<std::string>(ptrProperties->GetProperty(m_DatabasePathProps));
        }

        CSQLiteWorkspace:: CSQLiteWorkspace()
        {

        }

        CSQLiteWorkspace::CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, int32_t nID) :
                TBase(wtShapeFile, nID)

        {
            m_sName = pszName;
            m_sDatabasePath = pszDatabasePath;
        }


        IWorkspacePtr CSQLiteWorkspace::Open(const char *pszName, const char *pszPath, int32_t nID)
        {
            return IWorkspacePtr();
        }

        IWorkspacePtr CSQLiteWorkspace::Open(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                IWorkspacePtr ptrWrks(new CSQLiteWorkspace());
                ptrWrks->Load(pObj);

                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open Workspace", exc);
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
            return IDatasetPtr();
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