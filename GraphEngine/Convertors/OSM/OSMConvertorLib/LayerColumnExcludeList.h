#pragma once
#include "../../../CommonLib/Serialize/SerializeObj.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // What the OSM conversion leaves out: whole layers / tables (by the dataset name: "roads", "tags" ...) and
        // columns of a dataset (by the column name: "addr_street" ...). Empty - everything is converted.
        // The required columns of a dataset (SOSMColumn::bRequired) are written even when they are in the list.
        class CLayerColumnExcludeList : public CommonLib::ISerialize
        {
        public:
            CLayerColumnExcludeList() {}
            virtual ~CLayerColumnExcludeList() {}

            bool IsEmpty() const { return m_setLayers.empty() && m_mapColumns.empty(); }
            void Clear();

            void ExcludeLayer(const std::string& sLayer);
            void IncludeLayer(const std::string& sLayer);
            bool IsLayerExcluded(const std::string& sLayer) const;
            void SetLayerExcluded(const std::string& sLayer, bool bExcluded);

            void ExcludeColumn(const std::string& sLayer, const std::string& sColumn);
            void IncludeColumn(const std::string& sLayer, const std::string& sColumn);
            bool IsColumnExcluded(const std::string& sLayer, const std::string& sColumn) const;
            void SetColumnExcluded(const std::string& sLayer, const std::string& sColumn, bool bExcluded);

            std::vector<std::string> GetExcludedLayers() const;
            std::vector<std::string> GetExcludedColumns(const std::string& sLayer) const;

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            std::set<std::string>                          m_setLayers;
            std::map<std::string, std::set<std::string> >  m_mapColumns;   // layer -> its excluded columns
        };
    }
}
