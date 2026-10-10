#include "OSMMap.h"
#include <algorithm>
#include <cctype>

namespace GraphEngine {
    namespace Convertors {

        // ---------------- value ----------------

        COSMTagValue::COSMTagValue(const std::string& sValue, uint64_t nCount, bool bEnabled) : m_sValue(sValue), m_nCount(nCount), m_bEnabled(bEnabled)
        {

        }

        const std::string& COSMTagValue::GetValue() const
        {
            return m_sValue;
        }

        uint64_t COSMTagValue::GetFeatureCount() const
        {
            return m_nCount;
        }

        bool COSMTagValue::GetEnabled() const
        {
            return m_bEnabled;
        }

        void COSMTagValue::SetEnabled(bool bEnabled)
        {
            m_bEnabled = bEnabled;
        }

        // ---------------- key ----------------

        const char* COSMTagKey::OtherValue = "(other)";

        COSMTagKey::COSMTagKey(const std::string& sKey, bool bEnabled) : m_sKey(sKey), m_bEnabled(bEnabled), m_nCount(0)
        {

        }

        const std::string& COSMTagKey::GetKey() const
        {
            return m_sKey;
        }

        uint64_t COSMTagKey::GetFeatureCount() const
        {
            return m_nCount;
        }

        bool COSMTagKey::GetEnabled() const
        {
            return m_bEnabled;
        }

        void COSMTagKey::SetEnabled(bool bEnabled)
        {
            m_bEnabled = bEnabled;
        }

        int COSMTagKey::GetValueCount() const
        {
            return (int)m_vecValues.size();
        }

        IOSMTagValuePtr COSMTagKey::GetValue(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecValues.size())
                throw CommonLib::CExcBase("OSM key {0}: value index {1} is out of range", m_sKey, nIndex);
            return m_vecValues[nIndex];
        }

        IOSMTagValuePtr COSMTagKey::FindValue(const std::string& sValue) const
        {
            auto it = m_mapValues.find(sValue);
            return it != m_mapValues.end() ? m_vecValues[it->second] : IOSMTagValuePtr();
        }

        COSMTagValuePtr COSMTagKey::AddValue(const std::string& sValue, uint64_t nCount, bool bEnabled)
        {
            auto it = m_mapValues.find(sValue);
            if(it != m_mapValues.end())
                return m_vecValues[it->second];

            COSMTagValuePtr ptrValue = std::make_shared<COSMTagValue>(sValue, nCount, bEnabled);
            m_mapValues[sValue] = m_vecValues.size();
            m_vecValues.push_back(ptrValue);
            m_nCount += nCount;
            return ptrValue;
        }

        void COSMTagKey::AddFeature(const std::string& sValue)
        {
            ++m_nCount;
            auto it = m_mapValues.find(sValue);
            if(it != m_mapValues.end())
            {
                m_vecValues[it->second]->AddFeature();
                return;
            }

            const std::string& sAdded = (int)m_vecValues.size() < MaxValues ? sValue : std::string(OtherValue);
            it = m_mapValues.find(sAdded);
            if(it != m_mapValues.end())
            {
                m_vecValues[it->second]->AddFeature();
                return;
            }

            m_mapValues[sAdded] = m_vecValues.size();
            m_vecValues.push_back(std::make_shared<COSMTagValue>(sAdded, 1, true));
        }

        void COSMTagKey::SortValues()
        {
            std::stable_sort(m_vecValues.begin(), m_vecValues.end(), [](const COSMTagValuePtr& a, const COSMTagValuePtr& b)
            {
                return a->GetFeatureCount() > b->GetFeatureCount();
            });
            m_mapValues.clear();
            for(size_t i = 0; i < m_vecValues.size(); ++i)
                m_mapValues[m_vecValues[i]->GetValue()] = i;
        }

        bool COSMTagKey::IsValueEnabled(const std::string& sValue) const
        {
            if(!m_bEnabled)
                return false;

            auto it = m_mapValues.find(sValue);
            if(it != m_mapValues.end())
                return m_vecValues[it->second]->GetEnabled();

            // not met by ReadMap: the values over MaxValues are counted as "other"
            if((int)m_vecValues.size() >= MaxValues)
            {
                it = m_mapValues.find(OtherValue);
                if(it != m_mapValues.end())
                    return m_vecValues[it->second]->GetEnabled();
            }
            return true;
        }

        // ---------------- key set ----------------

        COSMKeySet::COSMKeySet(const std::vector<std::string>& vecKeys)
        {
            for(size_t i = 0; i < vecKeys.size(); ++i)
            {
                if(!Find(vecKeys[i]).get())
                    Add(vecKeys[i]);
            }
        }

        COSMTagKeyPtr COSMKeySet::Get(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecKeys.size())
                throw CommonLib::CExcBase("OSM dataset: key index {0} is out of range", nIndex);
            return m_vecKeys[nIndex];
        }

        COSMTagKeyPtr COSMKeySet::Find(const std::string& sKey) const
        {
            for(size_t i = 0; i < m_vecKeys.size(); ++i)
            {
                if(m_vecKeys[i]->GetKey() == sKey)
                    return m_vecKeys[i];
            }
            return COSMTagKeyPtr();
        }

        COSMTagKeyPtr COSMKeySet::Add(const std::string& sKey, bool bEnabled)
        {
            COSMTagKeyPtr ptrKey = std::make_shared<COSMTagKey>(sKey, bEnabled);
            m_vecKeys.push_back(ptrKey);
            return ptrKey;
        }

        bool COSMKeySet::IsValueEnabled(const std::string& sKey, const std::string& sValue) const
        {
            COSMTagKeyPtr ptrKey = Find(sKey);
            return ptrKey.get() ? ptrKey->IsValueEnabled(sValue) : false;
        }

        void COSMKeySet::SortValues()
        {
            for(size_t i = 0; i < m_vecKeys.size(); ++i)
                m_vecKeys[i]->SortValues();
        }

        void COSMKeySet::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            for(size_t i = 0; i < m_vecKeys.size(); ++i)
            {
                const COSMTagKey& key = *m_vecKeys[i];
                CommonLib::ISerializeObjPtr ptrKey = pObj->CreateChildNode("Key");
                ptrKey->AddPropertyString("Key", key.GetKey());
                ptrKey->AddPropertyBool("Enabled", key.GetEnabled());
                for(int v = 0; v < key.GetValueCount(); ++v)
                {
                    IOSMTagValuePtr ptrValue = key.GetValue(v);
                    CommonLib::ISerializeObjPtr ptrValueNode = ptrKey->CreateChildNode("Value");
                    ptrValueNode->AddPropertyString("Value", ptrValue->GetValue());
                    ptrValueNode->AddPropertyIntU64("Count", ptrValue->GetFeatureCount());
                    ptrValueNode->AddPropertyBool("Enabled", ptrValue->GetEnabled());
                }
            }
        }

        void COSMKeySet::Load(CommonLib::ISerializeObjPtr pObj)
        {
            m_vecKeys.clear();
            for(uint32_t i = 0, sz = pObj->GetChildCnt(); i < sz; ++i)
            {
                CommonLib::ISerializeObjPtr ptrKeyNode = pObj->GetChild(i);
                COSMTagKeyPtr ptrKey = Add(ptrKeyNode->GetPropertyString("Key", std::string()), ptrKeyNode->GetPropertyBool("Enabled", true));
                for(uint32_t v = 0, nValues = ptrKeyNode->GetChildCnt(); v < nValues; ++v)
                {
                    CommonLib::ISerializeObjPtr ptrValueNode = ptrKeyNode->GetChild(v);
                    ptrKey->AddValue(ptrValueNode->GetPropertyString("Value", std::string()), ptrValueNode->GetPropertyIntU64("Count", 0),
                                     ptrValueNode->GetPropertyBool("Enabled", true));
                }
            }
        }

        // ---------------- layer, table ----------------

        void COSMLayer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            SaveBase(pObj);
            pObj->AddPropertyInt32("GeometryType", m_geometryType);
        }

        void COSMLayer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                LoadBase(pObj);
                m_geometryType = (eOSMGeometryType)pObj->GetPropertyInt32("GeometryType", m_geometryType);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load OSM layer {0}", m_sName, exc);
            }
        }

        void COSMTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            SaveBase(pObj);
            pObj->AddPropertyInt32("TableType", m_tableType);
        }

        void COSMTable::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                LoadBase(pObj);
                m_tableType = (eOSMTableType)pObj->GetPropertyInt32("TableType", m_tableType);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load OSM table {0}", m_sName, exc);
            }
        }

        // ---------------- map ----------------

        COSMMap::COSMMap() : m_format(OSMFormatUnknown), m_nNodes(0), m_nWays(0), m_nRelations(0), m_bSorted(true)
        {
            m_bounds.xMin = m_bounds.yMin = m_bounds.xMax = m_bounds.yMax = 0.;
            m_bounds.type = CommonLib::bbox_type_null;
        }

        COSMMap::~COSMMap()
        {

        }

        eOSMFileFormat COSMMap::FormatByPath(const std::string& sPath)
        {
            std::string sLower = sPath;
            std::transform(sLower.begin(), sLower.end(), sLower.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
            auto endsWith = [&sLower](const char* pszSuffix)
            {
                size_t len = strlen(pszSuffix);
                return sLower.size() >= len && sLower.compare(sLower.size() - len, len, pszSuffix) == 0;
            };

            if(endsWith(".pbf"))
                return OSMFormatPBF;
            if(endsWith(".osm"))
                return OSMFormatXML;
            return OSMFormatUnknown;
        }

        const std::string& COSMMap::GetPath() const
        {
            return m_sPath;
        }

        eOSMFileFormat COSMMap::GetFormat() const
        {
            return m_format;
        }

        const CommonLib::bbox& COSMMap::GetBounds() const
        {
            return m_bounds;
        }

        uint64_t COSMMap::GetNodeCount() const
        {
            return m_nNodes;
        }

        uint64_t COSMMap::GetWayCount() const
        {
            return m_nWays;
        }

        uint64_t COSMMap::GetRelationCount() const
        {
            return m_nRelations;
        }

        bool COSMMap::IsSorted() const
        {
            return m_bSorted;
        }

        int COSMMap::GetLayerCount() const
        {
            return (int)m_vecLayers.size();
        }

        IOSMLayerPtr COSMMap::GetLayer(int nIndex) const
        {
            return GetLayerImpl(nIndex);
        }

        COSMLayerPtr COSMMap::GetLayerImpl(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecLayers.size())
                throw CommonLib::CExcBase("OSM map: layer index {0} is out of range", nIndex);
            return m_vecLayers[nIndex];
        }

        COSMLayerPtr COSMMap::FindLayerImpl(const std::string& sName) const
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
            {
                if(m_vecLayers[i]->GetName() == sName)
                    return m_vecLayers[i];
            }
            return COSMLayerPtr();
        }

        IOSMLayerPtr COSMMap::FindLayer(const std::string& sName) const
        {
            return FindLayerImpl(sName);
        }

        void COSMMap::RemoveLayer(const std::string& sName)
        {
            m_vecLayers.erase(std::remove_if(m_vecLayers.begin(), m_vecLayers.end(), [&sName](const COSMLayerPtr& ptr) { return ptr->GetName() == sName; }),
                              m_vecLayers.end());
        }

        int COSMMap::GetTableCount() const
        {
            return (int)m_vecTables.size();
        }

        IOSMTablePtr COSMMap::GetTable(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecTables.size())
                throw CommonLib::CExcBase("OSM map: table index {0} is out of range", nIndex);
            return m_vecTables[nIndex];
        }

        COSMTablePtr COSMMap::FindTableImpl(const std::string& sName) const
        {
            for(size_t i = 0; i < m_vecTables.size(); ++i)
            {
                if(m_vecTables[i]->GetName() == sName)
                    return m_vecTables[i];
            }
            return COSMTablePtr();
        }

        COSMTablePtr COSMMap::FindTableByType(eOSMTableType type) const
        {
            for(size_t i = 0; i < m_vecTables.size(); ++i)
            {
                if(m_vecTables[i]->GetTableType() == type)
                    return m_vecTables[i];
            }
            return COSMTablePtr();
        }

        IOSMTablePtr COSMMap::FindTable(const std::string& sName) const
        {
            return FindTableImpl(sName);
        }

        void COSMMap::RemoveTable(const std::string& sName)
        {
            m_vecTables.erase(std::remove_if(m_vecTables.begin(), m_vecTables.end(), [&sName](const COSMTablePtr& ptr) { return ptr->GetName() == sName; }),
                              m_vecTables.end());
        }

        void COSMMap::SetPath(const std::string& sPath)
        {
            m_sPath = sPath;
            m_format = FormatByPath(sPath);
        }

        void COSMMap::SetCounts(uint64_t nNodes, uint64_t nWays, uint64_t nRelations)
        {
            m_nNodes = nNodes;
            m_nWays = nWays;
            m_nRelations = nRelations;
        }

        void COSMMap::SetBounds(const CommonLib::bbox& bounds)
        {
            m_bounds = bounds;
        }

        void COSMMap::SetSorted(bool bSorted)
        {
            m_bSorted = bSorted;
        }

        void COSMMap::AddLayer(COSMLayerPtr ptrLayer)
        {
            m_vecLayers.push_back(ptrLayer);
        }

        void COSMMap::AddTable(COSMTablePtr ptrTable)
        {
            m_vecTables.push_back(ptrTable);
        }

        void COSMMap::SortValues()
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                m_vecLayers[i]->SortValues();
            for(size_t i = 0; i < m_vecTables.size(); ++i)
                m_vecTables[i]->SortValues();
        }

        void COSMMap::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                CommonLib::ISerializeObjPtr ptrMapNode = pObj->CreateChildNode("OSMMap");
                ptrMapNode->AddPropertyString("Path", m_sPath);
                ptrMapNode->AddPropertyInt32("Format", m_format);
                ptrMapNode->AddPropertyIntU64("Nodes", m_nNodes);
                ptrMapNode->AddPropertyIntU64("Ways", m_nWays);
                ptrMapNode->AddPropertyIntU64("Relations", m_nRelations);
                ptrMapNode->AddPropertyBool("Sorted", m_bSorted);
                ptrMapNode->AddPropertyBool("HasBounds", (m_bounds.type & CommonLib::bbox_type_normal) != 0);
                ptrMapNode->AddPropertyDouble("XMin", m_bounds.xMin);
                ptrMapNode->AddPropertyDouble("YMin", m_bounds.yMin);
                ptrMapNode->AddPropertyDouble("XMax", m_bounds.xMax);
                ptrMapNode->AddPropertyDouble("YMax", m_bounds.yMax);

                CommonLib::ISerializeObjPtr ptrLayers = ptrMapNode->CreateChildNode("Layers");
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->Save(ptrLayers->CreateChildNode("Layer"));

                CommonLib::ISerializeObjPtr ptrTables = ptrMapNode->CreateChildNode("Tables");
                for(size_t i = 0; i < m_vecTables.size(); ++i)
                    m_vecTables[i]->Save(ptrTables->CreateChildNode("Table"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save OSM map", exc);
            }
        }

        void COSMMap::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                CommonLib::ISerializeObjPtr ptrMapNode = pObj->IsChildExists("OSMMap") ? pObj->GetChild("OSMMap") : pObj;
                m_sPath = ptrMapNode->GetPropertyString("Path", m_sPath);
                m_format = (eOSMFileFormat)ptrMapNode->GetPropertyInt32("Format", FormatByPath(m_sPath));
                m_nNodes = ptrMapNode->GetPropertyIntU64("Nodes", 0);
                m_nWays = ptrMapNode->GetPropertyIntU64("Ways", 0);
                m_nRelations = ptrMapNode->GetPropertyIntU64("Relations", 0);
                m_bSorted = ptrMapNode->GetPropertyBool("Sorted", true);
                m_bounds.xMin = ptrMapNode->GetPropertyDouble("XMin", 0.);
                m_bounds.yMin = ptrMapNode->GetPropertyDouble("YMin", 0.);
                m_bounds.xMax = ptrMapNode->GetPropertyDouble("XMax", 0.);
                m_bounds.yMax = ptrMapNode->GetPropertyDouble("YMax", 0.);
                m_bounds.type = ptrMapNode->GetPropertyBool("HasBounds", false) ? CommonLib::bbox_type_normal : CommonLib::bbox_type_null;

                m_vecLayers.clear();
                if(ptrMapNode->IsChildExists("Layers"))
                {
                    CommonLib::ISerializeObjPtr ptrLayers = ptrMapNode->GetChild("Layers");
                    for(uint32_t i = 0, sz = ptrLayers->GetChildCnt(); i < sz; ++i)
                    {
                        COSMLayerPtr ptrLayer = std::make_shared<COSMLayer>();
                        ptrLayer->Load(ptrLayers->GetChild(i));
                        m_vecLayers.push_back(ptrLayer);
                    }
                }

                m_vecTables.clear();
                if(ptrMapNode->IsChildExists("Tables"))
                {
                    CommonLib::ISerializeObjPtr ptrTables = ptrMapNode->GetChild("Tables");
                    for(uint32_t i = 0, sz = ptrTables->GetChildCnt(); i < sz; ++i)
                    {
                        COSMTablePtr ptrTable = std::make_shared<COSMTable>();
                        ptrTable->Load(ptrTables->GetChild(i));
                        m_vecTables.push_back(ptrTable);
                    }
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load OSM map", exc);
            }
        }
    }
}
