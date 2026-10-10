#pragma once
#include "OSMConvertorLib.h"
#include <unordered_map>

namespace GraphEngine {
    namespace Convertors {

        class COSMTagValue : public IOSMTagValue
        {
        public:
            explicit COSMTagValue(const std::string& sValue, uint64_t nCount = 0, bool bEnabled = true);

            virtual const std::string&  GetValue() const;
            virtual uint64_t            GetFeatureCount() const;
            virtual bool                GetEnabled() const;
            virtual void                SetEnabled(bool bEnabled);

            void AddFeature() { ++m_nCount; }

        private:
            std::string m_sValue;
            uint64_t    m_nCount;
            bool        m_bEnabled;
        };

        typedef std::shared_ptr<COSMTagValue> COSMTagValuePtr;

        class COSMTagKey : public IOSMTagKey
        {
        public:
            // values over this number are counted as OtherValue (typos and free texts in the data)
            static const int MaxValues = 500;
            static const char* OtherValue;

            explicit COSMTagKey(const std::string& sKey, bool bEnabled = true);

            virtual const std::string&  GetKey() const;
            virtual uint64_t            GetFeatureCount() const;
            virtual bool                GetEnabled() const;
            virtual void                SetEnabled(bool bEnabled);
            virtual int                 GetValueCount() const;
            virtual IOSMTagValuePtr     GetValue(int nIndex) const;
            virtual IOSMTagValuePtr     FindValue(const std::string& sValue) const;

            void AddFeature(const std::string& sValue);
            COSMTagValuePtr AddValue(const std::string& sValue, uint64_t nCount, bool bEnabled);
            void SortValues();
            bool IsValueEnabled(const std::string& sValue) const;

        private:
            std::string m_sKey;
            bool        m_bEnabled;
            uint64_t    m_nCount;
            std::vector<COSMTagValuePtr> m_vecValues;
            std::unordered_map<std::string, size_t> m_mapValues;
        };

        typedef std::shared_ptr<COSMTagKey> COSMTagKeyPtr;

        // keys of a dataset (layer / table)
        class COSMKeySet
        {
        public:
            COSMKeySet() {}
            explicit COSMKeySet(const std::vector<std::string>& vecKeys);

            int             Count() const { return (int)m_vecKeys.size(); }
            COSMTagKeyPtr   Get(int nIndex) const;
            COSMTagKeyPtr   Find(const std::string& sKey) const;
            COSMTagKeyPtr   Add(const std::string& sKey, bool bEnabled = true);
            bool            IsValueEnabled(const std::string& sKey, const std::string& sValue) const;
            void            SortValues();
            void            Save(CommonLib::ISerializeObjPtr pObj) const;
            void            Load(CommonLib::ISerializeObjPtr pObj);

        private:
            std::vector<COSMTagKeyPtr> m_vecKeys;
        };

        // common part of the layers and the tables
        template<class I>
        class COSMDatasetBase : public I
        {
        public:
            COSMDatasetBase() : m_bEnabled(true), m_nCount(0) {}
            COSMDatasetBase(const std::string& sName, const std::string& sDisplayName, const std::vector<std::string>& vecKeys) :
                    m_sName(sName), m_sDisplayName(sDisplayName), m_sTableName("osm_" + sName), m_bEnabled(true), m_nCount(0), m_keys(vecKeys)
            {}

            virtual const std::string&  GetName() const { return m_sName; }
            virtual const std::string&  GetDisplayName() const { return m_sDisplayName; }
            virtual const std::string&  GetTableName() const { return m_sTableName; }
            virtual void                SetTableName(const std::string& sTableName) { m_sTableName = sTableName; }
            virtual uint64_t            GetFeatureCount() const { return m_nCount; }
            virtual bool                GetEnabled() const { return m_bEnabled; }
            virtual void                SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }
            virtual int                 GetKeyCount() const { return m_keys.Count(); }
            virtual IOSMTagKeyPtr       GetKey(int nIndex) const { return m_keys.Get(nIndex); }
            virtual IOSMTagKeyPtr       FindKey(const std::string& sKey) const { return m_keys.Find(sKey); }
            virtual bool                IsValueEnabled(const std::string& sKey, const std::string& sValue) const { return m_keys.IsValueEnabled(sKey, sValue); }
            virtual bool                IsEnabled(const std::string& sKey, const std::string& sValue) const { return m_bEnabled && m_keys.IsValueEnabled(sKey, sValue); }

            void AddFeature(const std::string& sKey, const std::string& sValue)
            {
                COSMTagKeyPtr ptrKey = m_keys.Find(sKey);
                if(!ptrKey.get())
                    ptrKey = m_keys.Add(sKey);
                ptrKey->AddFeature(sValue);
                ++m_nCount;
            }

            // only the number of features / records (f.e. the tags table counts the tags, its keys count the objects)
            void AddCount(uint64_t nCount = 1) { m_nCount += nCount; }
            COSMKeySet& Keys() { return m_keys; }
            void SortValues() { m_keys.SortValues(); }

        protected:
            void SaveBase(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyString("Name", m_sName);
                pObj->AddPropertyString("DisplayName", m_sDisplayName);
                pObj->AddPropertyString("TableName", m_sTableName);
                pObj->AddPropertyBool("Enabled", m_bEnabled);
                pObj->AddPropertyIntU64("Count", m_nCount);
                m_keys.Save(pObj->CreateChildNode("Keys"));
            }

            void LoadBase(CommonLib::ISerializeObjPtr pObj)
            {
                m_sName = pObj->GetPropertyString("Name", m_sName);
                m_sDisplayName = pObj->GetPropertyString("DisplayName", m_sDisplayName);
                m_sTableName = pObj->GetPropertyString("TableName", m_sTableName);
                m_bEnabled = pObj->GetPropertyBool("Enabled", m_bEnabled);
                m_nCount = pObj->GetPropertyIntU64("Count", 0);
                m_keys = COSMKeySet();
                if(pObj->IsChildExists("Keys"))
                    m_keys.Load(pObj->GetChild("Keys"));
            }

        protected:
            std::string m_sName;
            std::string m_sDisplayName;
            std::string m_sTableName;
            bool        m_bEnabled;
            uint64_t    m_nCount;
            COSMKeySet  m_keys;
        };

        class COSMLayer : public COSMDatasetBase<IOSMLayer>
        {
        public:
            COSMLayer() : m_geometryType(OSMGeometryPoint) {}
            COSMLayer(const std::string& sName, const std::string& sDisplayName, eOSMGeometryType geometryType, const std::vector<std::string>& vecKeys) :
                    COSMDatasetBase<IOSMLayer>(sName, sDisplayName, vecKeys), m_geometryType(geometryType)
            {}

            virtual eOSMGeometryType GetGeometryType() const { return m_geometryType; }

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            eOSMGeometryType m_geometryType;
        };

        typedef std::shared_ptr<COSMLayer> COSMLayerPtr;

        class COSMTable : public COSMDatasetBase<IOSMTable>
        {
        public:
            COSMTable() : m_tableType(OSMTableTags) {}
            COSMTable(const std::string& sName, const std::string& sDisplayName, eOSMTableType tableType, const std::vector<std::string>& vecKeys) :
                    COSMDatasetBase<IOSMTable>(sName, sDisplayName, vecKeys), m_tableType(tableType)
            {}

            virtual eOSMTableType GetTableType() const { return m_tableType; }

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            eOSMTableType m_tableType;
        };

        typedef std::shared_ptr<COSMTable> COSMTablePtr;

        class COSMMap : public IOSMMap
        {
        public:
            COSMMap();
            virtual ~COSMMap();

            virtual const std::string&  GetPath() const;
            virtual eOSMFileFormat      GetFormat() const;
            virtual const CommonLib::bbox& GetBounds() const;
            virtual uint64_t            GetNodeCount() const;
            virtual uint64_t            GetWayCount() const;
            virtual uint64_t            GetRelationCount() const;
            virtual bool                IsSorted() const;
            virtual int                 GetLayerCount() const;
            virtual IOSMLayerPtr        GetLayer(int nIndex) const;
            virtual IOSMLayerPtr        FindLayer(const std::string& sName) const;
            virtual void                RemoveLayer(const std::string& sName);
            virtual int                 GetTableCount() const;
            virtual IOSMTablePtr        GetTable(int nIndex) const;
            virtual IOSMTablePtr        FindTable(const std::string& sName) const;
            virtual void                RemoveTable(const std::string& sName);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            static eOSMFileFormat FormatByPath(const std::string& sPath);

            void SetPath(const std::string& sPath);
            void SetCounts(uint64_t nNodes, uint64_t nWays, uint64_t nRelations);
            void SetBounds(const CommonLib::bbox& bounds);
            void SetSorted(bool bSorted);
            void AddLayer(COSMLayerPtr ptrLayer);
            void AddTable(COSMTablePtr ptrTable);
            COSMLayerPtr GetLayerImpl(int nIndex) const;
            COSMLayerPtr FindLayerImpl(const std::string& sName) const;
            COSMTablePtr FindTableImpl(const std::string& sName) const;
            COSMTablePtr FindTableByType(eOSMTableType type) const;
            void SortValues();

        private:
            std::string     m_sPath;
            eOSMFileFormat  m_format;
            CommonLib::bbox m_bounds;
            uint64_t        m_nNodes;
            uint64_t        m_nWays;
            uint64_t        m_nRelations;
            bool            m_bSorted;
            std::vector<COSMLayerPtr> m_vecLayers;
            std::vector<COSMTablePtr> m_vecTables;
        };

        typedef std::shared_ptr<COSMMap> COSMMapPtr;
    }
}
