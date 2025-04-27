#pragma once

#include "../GeoDatabase.h"
#include "../WorkspaceBase.h"
#include "../../CommonLib/sqlitelib/Database.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteWorkspace : public IWorkspaceBase<IDatabaseWorkspace>
        {

            CSQLiteWorkspace();
            CSQLiteWorkspace(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid id);
            CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, CommonLib::CGuid id);
            CSQLiteWorkspace(const char *pszName, const char *pszDatabasePath, CommonLib::CGuid idD, CommonLib::database::IDatabasePtr ptrDatabase);
        public:
            typedef  IWorkspaceBase<IDatabaseWorkspace> TBase;

            static IDatabaseWorkspacePtr Open(const char *pszName, const char *pszPath, CommonLib::CGuid id);
            static IDatabaseWorkspacePtr Open(CommonLib::ISerializeObjPtr pObj);
            static IDatabaseWorkspacePtr Create(const char *pszName, const char *pszPath, CommonLib::CGuid id);

            virtual ITablePtr CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields);
            virtual ITablePtr CreateTableWithSpatialIndex(const std::string& name,
                                                        const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                        CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference);
            virtual ITransactionPtr StartTransaction(eTransactionType type);



            virtual void Save(CommonLib::ISerializeObjPtr ptrObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr ptrObj);

        protected:
            virtual IDatasetPtr LoadDataset(const std::string& sName);
            virtual IDatasetPtr LoadTable(const std::string& sName);
            virtual IDatasetPtr LoadSpatialTable(const std::string& sName);

        private:

            static std::string m_DatabasePathProps;
            static std::string m_NameProps;

            std::string m_sDatabasePath;
            CommonLib::database::IDatabasePtr m_ptrDatabase;

        };

    }
    }