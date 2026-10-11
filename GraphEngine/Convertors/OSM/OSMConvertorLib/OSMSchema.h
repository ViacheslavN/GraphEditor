#pragma once
#include "OSMMap.h"
#include "OSMTags.h"
#include <unordered_map>

namespace GraphEngine {
    namespace Convertors {

        // OSM key of a layer: the object goes to the layer when it has the key with an allowed value
        struct SOSMKeyRule
        {
            std::string              sKey;
            std::vector<std::string> vecInclude;   // allowed values, empty - any value
            std::vector<std::string> vecExclude;   // values not allowed
            std::string              sGroupKey;    // key shown in the hierarchy (empty - sKey), its value groups the features
                                                   // (f.e. boundary=administrative grouped by admin_level)
        };

        enum eOSMFieldKind
        {
            OSMFieldText    = 0,
            OSMFieldInteger = 1,   // leading number of the value ("50 mph" -> 50), NULL if there is none
            OSMFieldBool    = 2,   // yes / true / 1 -> 1, otherwise 0
            OSMFieldOneway  = 3    // yes / true / 1 -> 1, -1 / reverse -> -1, otherwise 0
        };

        // attribute of a layer table taken from an OSM tag (besides the common fields osm_id, osm_type, name, class, type)
        struct SOSMFieldDef
        {
            std::string   sName;
            std::string   sKey;
            eOSMFieldKind kind;
        };

        struct SOSMLayerDef
        {
            std::string               sName;
            std::string               sDisplayName;
            eOSMGeometryType          geometryType;
            std::vector<SOSMKeyRule>  vecRules;    // the first matching rule gives the key / value
            std::vector<SOSMFieldDef> vecFields;
        };

        struct SOSMTableDef
        {
            std::string   sName;
            std::string   sDisplayName;
            eOSMTableType tableType;
        };

        // the layer, the key and the value an OSM object belongs to
        struct SOSMMatch
        {
            int         nLayer = -1;       // index in the schema (and in the OSM map made by CreateMap)
            std::string sKey;
            std::string sValue;

            bool IsValid() const { return nLayer >= 0; }
        };

        // Thematic layers of the conversion and the classification of the OSM objects:
        //  nodes -> point layers; ways -> a line layer and (closed ways) a polygon layer;
        //  multipolygon / boundary relations -> a polygon layer.
        // An object goes to at most one layer of every geometry type (the first layer in the schema order).
        class COSMSchema
        {
        public:
            // the columns of a layer table made by the converter
            static const char* ColumnOsmId;     // osm_id
            static const char* ColumnOsmType;   // osm_type: n / w / r
            static const char* ColumnName;      // name (name:<language> when it exists)
            static const char* ColumnClass;     // the key of the matched rule (highway, amenity ...)
            static const char* ColumnType;      // the value (residential, cafe ...), the map styles draw by it

            // Analyze: the OSM keys of a layer become its columns, sorted by the number of features: at most MaxTagColumns
            // of the keys met at least on MinTagColumnPerMille / 1000 of the features (the rest are typos and rare keys,
            // they are in the tags table)
            static const int MaxTagColumns = 200;
            static const int MinTagColumnPerMille = 1;

            COSMSchema();   // the default thematic layers
            explicit COSMSchema(const std::vector<SOSMLayerDef>& vecLayers);

            static std::vector<SOSMLayerDef> DefaultLayers();
            static std::vector<SOSMTableDef> DefaultTables();

            const std::vector<SOSMLayerDef>& Layers() const { return m_vecLayers; }
            const SOSMLayerDef* FindLayer(const std::string& sName) const;
            int FindLayerIndex(const std::string& sName) const;
            // the key of the table hierarchy: "route", "restriction", "key"
            static const char* TableKey(eOSMTableType type);

            // the OSM map with the layers of the schema, their keys (no values yet) and the converter / schema columns
            COSMMapPtr CreateMap() const;

            // osm_id, osm_type, name, class, type and the fields of the layer
            static std::vector<SOSMColumn> LayerColumns(const SOSMLayerDef& def);
            static std::vector<SOSMColumn> TableColumns(eOSMTableType type);
            // the column for an OSM key: addr:street -> addr_street (letters, digits, '_')
            static std::string KeyColumnName(const std::string& sKey);
            // the key is a column of the layer anyway (name, the keys of the rules, the fields): it isn't a tag column
            static bool IsLayerKey(const SOSMLayerDef& def, const std::string& sKey);
            // Analyze: the columns of the layer by the number of the features having every OSM key
            void SetAnalyzedColumns(int nLayer, COSMLayer& layer, const std::unordered_map<std::string, uint64_t>& keyCounts) const;

            SOSMMatch MatchNode(const COSMTags& tags) const;
            // closed - the first and the last node are the same (at least 4 nodes)
            void MatchWay(const COSMTags& tags, bool bClosed, SOSMMatch& line, SOSMMatch& polygon) const;
            SOSMMatch MatchRelation(const COSMTags& tags) const;
            // route relation: the value of "route", empty - not a route
            static const char* MatchRoute(const COSMTags& tags);
            // turn restriction: the value of "restriction" (or restriction:<transport>), empty - not a restriction
            static const char* MatchRestriction(const COSMTags& tags);

        private:
            SOSMMatch Match(const COSMTags& tags, eOSMGeometryType geometryType) const;
            static bool MatchRule(const SOSMKeyRule& rule, const COSMTags& tags, SOSMMatch& match);

        private:
            std::vector<SOSMLayerDef> m_vecLayers;
        };
    }
}
