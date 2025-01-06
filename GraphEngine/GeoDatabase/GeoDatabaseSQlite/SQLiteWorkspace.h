#pragma once

#include "../GeoDatabase.h"
#include "../WorkspaceBase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteWorkspace : public IWorkspaceBase<IDatabaseWorkspace>
        {

            CSQLiteWorkspace();
            CSQLiteWorkspace(CommonLib::IPropertySetPtr ptrProperties, int32_t nID);
            CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, int32_t nID);
        public:
            typedef  IWorkspaceBase<IDatabaseWorkspace> TBase;

            static IWorkspacePtr Open(const char *pszName, const char *pszPath, int32_t nID = -1);
            static IWorkspacePtr Open(CommonLib::ISerializeObjPtr pObj);

            virtual ITablePtr CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields);
            virtual ISpatialTablePtr CreateSpatialTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields, const std::string& shapeFiledName, const std::string& spatialIndexName = "");
            virtual ITransactionPtr StartTransaction(eTransactionType type);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        protected:
            virtual IDatasetPtr LoadDataset(const std::string& sName);

        private:

            static std::string m_DatabasePathProps;
            static std::string m_NameProps;

            std::string m_sDatabasePath;
        };

    }
    }