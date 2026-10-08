#pragma once

#include "../WorkspaceBase.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        // Folder of raster files. Dataset name = file name relative to the workspace path
        // (an absolute path is accepted too), e.g. OpenRasterDataset("ortho.tif").
        class CRasterWorkspace : public IWorkspaceBase<IRasterWorkspace>
        {
            CRasterWorkspace();
            CRasterWorkspace(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid nID);
            CRasterWorkspace(const char *pszName, const char *pszPath, CommonLib::CGuid nID);
        public:
            typedef IWorkspaceBase<IRasterWorkspace> TBase;

            static IWorkspacePtr Open(const char *pszName, const char *pszPath, CommonLib::CGuid nID);
            static IWorkspacePtr Open(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid nID);
            static IWorkspacePtr Open(CommonLib::ISerializeObjPtr pObj);

            // IRasterWorkspace
            virtual IRasterDatasetPtr OpenRasterDataset(const std::string& name);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            const std::string& GetPath() const;
            std::string GetFilePath(const std::string& name) const;

        protected:
            virtual IDatasetPtr LoadDataset(const std::string& sName);
            virtual IDatasetPtr LoadTable(const std::string& sName);
            virtual IDatasetPtr LoadSpatialTable(const std::string& sName);

        private:
            static std::string m_PathProps;
            static std::string m_NameProps;

            std::string m_sPath;
        };

    }
}
