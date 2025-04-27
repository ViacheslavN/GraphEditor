#pragma once
#include "../WorkspaceBase.h"


namespace GraphEngine
{
    namespace GeoDatabase {

        class CShapfileWorkspace : public IWorkspaceBase<IDatabaseWorkspace>
        {

            CShapfileWorkspace();
            CShapfileWorkspace(CommonLib::IPropertySetPtr ptrProperties, CommonLib::CGuid nID);
            CShapfileWorkspace(const char *pszName, const char *pszPath, CommonLib::CGuid nID);
        public:
            typedef  IWorkspaceBase<IDatabaseWorkspace> TBase;

            static IWorkspacePtr Open(const char *pszName, const char *pszPath, CommonLib::CGuid nID );
            static IWorkspacePtr Open(CommonLib::ISerializeObjPtr pObj);

            virtual ITablePtr CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields);
            virtual ITablePtr CreateTableWithSpatialIndex(const std::string& name,
                                          const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                          CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference);
            virtual ITransactionPtr StartTransaction(eTransactionType type);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

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