#pragma once

#include "GeoDatabase.h"
#include "../CommonLib/utils/PropertySet.h"
#include "../CommonLib/guid/guid.h"

namespace GraphEngine {
    namespace GeoDatabase {

        template<class I>
        class IWorkspaceBase : public I
        {
        public:

            IWorkspaceBase(eWorkspaceType type, CommonLib::CGuid id) : m_WorkspaceType(type), m_id(id)
            {

            }

            virtual ~IWorkspaceBase(){}

            virtual const std::string& GetWorkspaceName() const
            {
                return m_sName;
            }

            virtual eWorkspaceType GetWorkspaceType() const
            {
                return m_WorkspaceType;
            }

            virtual uint32_t GetDatasetCount() const
            {
                std::scoped_lock lock (m_mutex);
                return (uint32_t)m_vecDatasets.size();
            }

            virtual IDatasetPtr GetDataset(uint32_t nIdx) const
            {
                std::scoped_lock lock (m_mutex);
                return m_vecDatasets[nIdx];
            }
            virtual void RemoveDataset(uint32_t nIdx)
            {
                std::scoped_lock lock (m_mutex);
                if(nIdx > (int)m_vecDatasets.size())
                {
                    throw CommonLib::CExcBase("Workspase failed to removing dataset, out of range");
                }
                m_DataSetMap.erase(m_vecDatasets[nIdx]->GetDatasetName());
                m_vecDatasets.erase(m_vecDatasets.begin() + nIdx);
                RebuildMap();
            }
            virtual void RemoveDataset(IDatasetPtr pDataset)
            {
                std::scoped_lock lock (m_mutex);

                typename TDatasetMap::iterator it = m_DataSetMap.find(pDataset->GetDatasetName());
                if(it == m_DataSetMap.end())
                    return;
                //assert(it->second < (int)m_vecDatasets.size());
                m_vecDatasets.erase(m_vecDatasets.begin() + it->second);
                m_DataSetMap.erase(it);
                RebuildMap();
            }


            virtual IDatasetPtr GetDataset(const std::string& sName)
            {
                TDatasetMap::const_iterator it = m_DataSetMap.find(sName);
                if(it == m_DataSetMap.end())
                {
                    IDatasetPtr ptrDataset = LoadDataset(sName);
                    AddDataset(ptrDataset);

                    return ptrDataset;
                }

                return m_vecDatasets[it->second];
            }

            virtual void AddDataset(IDatasetPtr pDataset)
            {
                m_vecDatasets.push_back(IDatasetPtr(pDataset));
                m_DataSetMap[pDataset->GetDatasetName()] = int(m_vecDatasets.size() - 1);

            }

            virtual ITablePtr GetTable(const std::string& sName)
            {
                return std::static_pointer_cast<ITable>( GetDataset(sName));
            }

            virtual CommonLib::CGuid GetWorkspaceId() const
            {
                return m_id;
            }


            virtual void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                try
                {
                    pObj->AddPropertyInt32U("WksType", (uint32_t)GetWorkspaceType());
                    pObj->AddPropertyString("Name", m_sName);
                    pObj->AddPropertyGuid("ID", m_id);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("Failed to save Workspace", exc);
                }
            }

            virtual void Load(CommonLib::ISerializeObjPtr pObj)
            {
                try
                {
                    m_WorkspaceType = (eWorkspaceType)pObj->GetPropertyInt32U("WksType");
                    m_sName = pObj->GetPropertyString("Name", m_sName);
                    m_id = pObj->GetPropertyGuid("ID", m_id);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("Failed to load Workspace", exc);
                }
            }

        protected:

            bool IsDatasetExists(const std::string& sName) const
            {
                return  m_DataSetMap.find(sName) != m_DataSetMap.end();
            }


            virtual IDatasetPtr LoadDataset(const std::string& sName) = 0;
            virtual IDatasetPtr LoadTable(const std::string& sName) = 0;
            virtual IDatasetPtr LoadSpatialTable(const std::string& sName) = 0;


            void RebuildMap()
            {
                m_DataSetMap.clear();
                for (uint32_t i = 0, sz = (uint32_t)m_vecDatasets.size(); i < sz; ++i)
                {
                    m_DataSetMap[m_vecDatasets[i]->GetDatasetName()] = i;
                }
            }


            typedef std::vector<IDatasetPtr> TVecDataset;
            TVecDataset m_vecDatasets;

            typedef std::map<std::string, int> TDatasetMap;
            TDatasetMap	m_DataSetMap;

            eWorkspaceType m_WorkspaceType;
            std::string m_sName;
            CommonLib::IPropertySetPtr  m_ConnectProp;
            mutable std::mutex m_mutex;
            CommonLib::CGuid m_id;
        };

    }
    }