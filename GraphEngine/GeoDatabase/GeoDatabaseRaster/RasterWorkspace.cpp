#include "RasterWorkspace.h"
#include "RasterDataset.h"

#include <filesystem>

namespace GraphEngine
{
    namespace GeoDatabase {

        std::string CRasterWorkspace::m_PathProps = "Path";
        std::string CRasterWorkspace::m_NameProps = "Name";

        CRasterWorkspace::CRasterWorkspace() : TBase(wtUndefined, CommonLib::CGuid::CreateNull())
        {

        }

        CRasterWorkspace::CRasterWorkspace(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid id) :
                TBase(wtRaster, id)
        {
            m_sName = std::any_cast<std::string>(ptrProperties->GetProperty(m_NameProps));
            m_sPath = std::any_cast<std::string>(ptrProperties->GetProperty(m_PathProps));
        }

        CRasterWorkspace::CRasterWorkspace(const char *pszName, const char *pszPath, CommonLib::CGuid nID) :
                TBase(wtRaster, nID)
        {
            m_sName = pszName;
            m_sPath = pszPath;
        }

        IWorkspacePtr CRasterWorkspace::Open(const char *pszName, const char *pszPath, CommonLib::CGuid nID)
        {
            return IWorkspacePtr(new CRasterWorkspace(pszName, pszPath, nID));
        }

        IWorkspacePtr CRasterWorkspace::Open(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid nID)
        {
            return IWorkspacePtr(new CRasterWorkspace(ptrProperties, nID));
        }

        IWorkspacePtr CRasterWorkspace::Open(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                IWorkspacePtr ptrWrks(new CRasterWorkspace());
                ptrWrks->Load(pObj);
                return ptrWrks;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open raster workspace", exc);
                throw;
            }
        }

        const std::string& CRasterWorkspace::GetPath() const
        {
            return m_sPath;
        }

        std::string CRasterWorkspace::GetFilePath(const std::string& name) const
        {
            std::filesystem::path file = std::filesystem::u8path(name);
            if(file.is_absolute() || m_sPath.empty())
                return file.u8string();

            return (std::filesystem::u8path(m_sPath) / file).u8string();
        }

        IRasterDatasetPtr CRasterWorkspace::OpenRasterDataset(const std::string& name)
        {
            try
            {
                return std::static_pointer_cast<IRasterDataset>(GetDataset(name));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Raster workspace failed to open dataset {0}", name, exc);
                throw;
            }
        }

        IDatasetPtr CRasterWorkspace::LoadDataset(const std::string& sName)
        {
            return std::make_shared<CRasterDataset>(GetWorkspaceId(), GetFilePath(sName), sName, sName);
        }

        IDatasetPtr CRasterWorkspace::LoadTable(const std::string& sName)
        {
            throw CommonLib::CExcBase("Raster workspace has no tables, name: {0}", sName);
        }

        IDatasetPtr CRasterWorkspace::LoadSpatialTable(const std::string& sName)
        {
            throw CommonLib::CExcBase("Raster workspace has no tables, name: {0}", sName);
        }

        void CRasterWorkspace::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyString(m_PathProps, m_sPath);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save raster workspace", exc);
            }
        }

        void CRasterWorkspace::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_sPath = pObj->GetPropertyString(m_PathProps);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load raster workspace", exc);
            }
        }
    }
}
