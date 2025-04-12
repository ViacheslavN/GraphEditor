#include "ShapefileWorkspace.h"
#include "ShapefileSpatialTable.h"

namespace GraphEngine
{
    namespace GeoDatabase {


        std::string CShapfileWorkspace::m_PathProps = "Path";
        std::string CShapfileWorkspace::m_NameProps = "Name";

        CShapfileWorkspace::CShapfileWorkspace(CommonLib::IPropertySetPtr ptrProperties, int32_t nID) :
                TBase(wtShapeFile, nID)
        {
            m_sName = std::any_cast<std::string>(ptrProperties->GetProperty(m_NameProps));
            m_sPath = std::any_cast<std::string>(ptrProperties->GetProperty(m_PathProps));
        }

        CShapfileWorkspace:: CShapfileWorkspace()
        {

        }

        CShapfileWorkspace::CShapfileWorkspace(const char *pszName, const char *pszPath, int32_t nID) :
                TBase(wtShapeFile, nID)

        {
            m_sName = pszName;
            m_sPath = pszPath;
        }


        IWorkspacePtr CShapfileWorkspace::Open(const char *pszName, const char *pszPath, int32_t nID)
        {
            return IWorkspacePtr(new CShapfileWorkspace(pszName, pszPath, nID));
        }

        IWorkspacePtr CShapfileWorkspace::Open(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                IWorkspacePtr ptrWrks(new CShapfileWorkspace());
                ptrWrks->Load(pObj);

                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open Workspace", exc);
                throw;
            }
        }


        ITablePtr CShapfileWorkspace::CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields)
        {
            throw CommonLib::CExcBase("Create table not implement");
        }

        ITablePtr CShapfileWorkspace::CreateTableWithSpatialIndex(const std::string& name,
                                                  const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                  CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference)
        {
            return std::make_shared<CShapefileSpatialTable>(m_sPath, name, viewName, shapeFieldName,ptrFields);
        }

        IDatasetPtr CShapfileWorkspace::LoadDataset(const std::string& sName)
        {
            return std::make_shared<CShapefileSpatialTable>(m_sPath, sName, sName);
        }

        IDatasetPtr CShapfileWorkspace::LoadTable(const std::string& sName)
        {
            return  LoadDataset(sName);
        }

        IDatasetPtr CShapfileWorkspace::LoadSpatialTable(const std::string& sName)
        {
            return  LoadDataset(sName);
        }


        ITransactionPtr CShapfileWorkspace::StartTransaction(eTransactionType type)
        {
            throw CommonLib::CExcBase("StartTransaction not implement");
        }

        void CShapfileWorkspace::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyString(m_PathProps, m_sPath);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save Workspace", exc);
            }
        }

        void CShapfileWorkspace::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_sPath = pObj->GetPropertyString(m_PathProps);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load Workspace", exc);
            }
        }

    }
}
