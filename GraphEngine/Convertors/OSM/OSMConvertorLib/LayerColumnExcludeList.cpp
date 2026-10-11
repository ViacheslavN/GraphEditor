#include "OSMConvertorLib.h"

namespace GraphEngine {
    namespace Convertors {

        void CLayerColumnExcludeList::Clear()
        {
            m_setLayers.clear();
            m_mapColumns.clear();
        }

        void CLayerColumnExcludeList::ExcludeLayer(const std::string& sLayer)
        {
            m_setLayers.insert(sLayer);
        }

        void CLayerColumnExcludeList::IncludeLayer(const std::string& sLayer)
        {
            m_setLayers.erase(sLayer);
        }

        bool CLayerColumnExcludeList::IsLayerExcluded(const std::string& sLayer) const
        {
            return m_setLayers.count(sLayer) > 0;
        }

        void CLayerColumnExcludeList::SetLayerExcluded(const std::string& sLayer, bool bExcluded)
        {
            if(bExcluded)
                ExcludeLayer(sLayer);
            else
                IncludeLayer(sLayer);
        }

        void CLayerColumnExcludeList::ExcludeColumn(const std::string& sLayer, const std::string& sColumn)
        {
            m_mapColumns[sLayer].insert(sColumn);
        }

        void CLayerColumnExcludeList::IncludeColumn(const std::string& sLayer, const std::string& sColumn)
        {
            auto it = m_mapColumns.find(sLayer);
            if(it == m_mapColumns.end())
                return;
            it->second.erase(sColumn);
            if(it->second.empty())
                m_mapColumns.erase(it);
        }

        bool CLayerColumnExcludeList::IsColumnExcluded(const std::string& sLayer, const std::string& sColumn) const
        {
            auto it = m_mapColumns.find(sLayer);
            return it != m_mapColumns.end() && it->second.count(sColumn) > 0;
        }

        void CLayerColumnExcludeList::SetColumnExcluded(const std::string& sLayer, const std::string& sColumn, bool bExcluded)
        {
            if(bExcluded)
                ExcludeColumn(sLayer, sColumn);
            else
                IncludeColumn(sLayer, sColumn);
        }

        std::vector<std::string> CLayerColumnExcludeList::GetExcludedLayers() const
        {
            return std::vector<std::string>(m_setLayers.begin(), m_setLayers.end());
        }

        std::vector<std::string> CLayerColumnExcludeList::GetExcludedColumns(const std::string& sLayer) const
        {
            auto it = m_mapColumns.find(sLayer);
            if(it == m_mapColumns.end())
                return std::vector<std::string>();
            return std::vector<std::string>(it->second.begin(), it->second.end());
        }

        void CLayerColumnExcludeList::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                CommonLib::ISerializeObjPtr ptrNode = pObj->CreateChildNode("ExcludeList");
                for(const std::string& sLayer : m_setLayers)
                    ptrNode->CreateChildNode("Layer")->AddPropertyString("Name", sLayer);
                for(const auto& layer : m_mapColumns)
                {
                    for(const std::string& sColumn : layer.second)
                    {
                        CommonLib::ISerializeObjPtr ptrColumn = ptrNode->CreateChildNode("Column");
                        ptrColumn->AddPropertyString("Layer", layer.first);
                        ptrColumn->AddPropertyString("Name", sColumn);
                    }
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save the OSM exclude list", exc);
            }
        }

        void CLayerColumnExcludeList::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                Clear();
                CommonLib::ISerializeObjPtr ptrNode = pObj->IsChildExists("ExcludeList") ? pObj->GetChild("ExcludeList") : pObj;
                for(uint32_t i = 0, sz = ptrNode->GetChildCnt(); i < sz; ++i)
                {
                    CommonLib::ISerializeObjPtr ptrChild = ptrNode->GetChild(i);
                    if(ptrChild->IsPropertyExists("Layer"))
                        ExcludeColumn(ptrChild->GetPropertyString("Layer", std::string()), ptrChild->GetPropertyString("Name", std::string()));
                    else
                        ExcludeLayer(ptrChild->GetPropertyString("Name", std::string()));
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load the OSM exclude list", exc);
            }
        }
    }
}
