#pragma once

#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        template< class I>
        class IDataSetBase : public I
        {
        public:

              IDataSetBase(CommonLib::CGuid workspaceId, eDatasetType datasetType,  std::string sDatasetName,  std::string sDatasetViewName) :
                    m_DatasetType(dtUndefined)
                    ,m_sDatasetName(sDatasetName)
                    ,m_sDatasetViewName(sDatasetViewName)
                    ,m_workspaceId(workspaceId)

            {

            }

            virtual ~IDataSetBase()
            {

            }

            virtual eDatasetType  GetDatasetType() const
            {
                return m_DatasetType;
            }

            const std::string&   GetDatasetName() const
            {
                return m_sDatasetName;
            }

            const  std::string&   GetDatasetViewName() const
            {
                return m_sDatasetViewName;
            }

            virtual CommonLib::CGuid GetWorkspaceId() const
            {
                return m_workspaceId;
            }


            virtual void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyGuid("WorkspaceId", m_workspaceId);
                pObj->AddPropertyInt32("DatasetType", (int32_t)m_DatasetType);
                pObj->AddPropertyString("DatasetName", m_sDatasetName);
                pObj->AddPropertyString("DatasetViewName", m_sDatasetViewName);
            }

            virtual void Load(CommonLib::ISerializeObjPtr pObj)
            {

            }

        protected:

            eDatasetType m_DatasetType;
            std::string m_sDatasetName;
            std::string m_sDatasetViewName;
            CommonLib::CGuid m_workspaceId;
        };


        }
    }