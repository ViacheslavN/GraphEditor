#include "OSMSchema.h"
#include <algorithm>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            SOSMKeyRule Rule(const std::string& sKey, const std::vector<std::string>& vecInclude = std::vector<std::string>(),
                             const std::vector<std::string>& vecExclude = std::vector<std::string>(), const std::string& sGroupKey = std::string())
            {
                SOSMKeyRule rule;
                rule.sKey = sKey;
                rule.vecInclude = vecInclude;
                rule.vecExclude = vecExclude;
                rule.sGroupKey = sGroupKey;
                return rule;
            }

            SOSMLayerDef Layer(const std::string& sName, const std::string& sDisplayName, eOSMGeometryType type, const std::vector<SOSMKeyRule>& vecRules)
            {
                SOSMLayerDef layer;
                layer.sName = sName;
                layer.sDisplayName = sDisplayName;
                layer.geometryType = type;
                layer.vecRules = vecRules;
                return layer;
            }

            SOSMFieldDef Field(const std::string& sName, const std::string& sKey, eOSMFieldKind kind = OSMFieldText)
            {
                SOSMFieldDef field;
                field.sName = sName;
                field.sKey = sKey;
                field.kind = kind;
                return field;
            }

            bool Contains(const std::vector<std::string>& vec, const char* pszValue)
            {
                for(size_t i = 0; i < vec.size(); ++i)
                {
                    if(vec[i] == pszValue)
                        return true;
                }
                return false;
            }
        }

        std::vector<SOSMLayerDef> COSMSchema::DefaultLayers()
        {
            std::vector<SOSMLayerDef> layers;

            // polygons: the order decides, f.e. landuse=reservoir is water, not landuse
            layers.push_back(Layer("boundaries", "Administrative boundaries", OSMGeometryPolygon, {
                    Rule("boundary", {"administrative"}, {}, "admin_level")}));
            layers.back().vecFields = {Field("admin_level", "admin_level", OSMFieldInteger)};
            layers.push_back(Layer("buildings", "Buildings", OSMGeometryPolygon, {
                    Rule("building", {}, {"no"})}));
            layers.back().vecFields = {Field("housenumber", "addr:housenumber"), Field("street", "addr:street"), Field("levels", "building:levels", OSMFieldInteger)};
            layers.push_back(Layer("water", "Water", OSMGeometryPolygon, {
                    Rule("natural", {"water", "bay", "strait"}),
                    Rule("water"),
                    Rule("waterway", {"riverbank", "dock", "boatyard"}),
                    Rule("landuse", {"reservoir", "basin", "salt_pond"})}));
            layers.push_back(Layer("landuse", "Landuse", OSMGeometryPolygon, {
                    Rule("landuse"),
                    Rule("natural", {"wood", "scrub", "heath", "grassland", "wetland", "beach", "sand", "bare_rock", "scree", "shingle", "glacier", "fell", "mud"}),
                    Rule("leisure", {"park", "garden", "pitch", "golf_course", "nature_reserve", "playground", "sports_centre", "stadium", "common",
                                     "recreation_ground", "dog_park", "track"}),
                    Rule("amenity", {"parking", "school", "university", "college", "hospital", "kindergarten", "grave_yard"})}));

            // lines
            layers.push_back(Layer("roads", "Roads", OSMGeometryLine, {
                    Rule("highway", {}, {"proposed", "construction", "abandoned", "razed", "platform", "bus_stop", "elevator", "emergency_access_point",
                                         "rest_area", "services", "traffic_signals", "street_lamp", "crossing"})}));
            layers.back().vecFields = {Field("ref", "ref"), Field("oneway", "oneway", OSMFieldOneway), Field("bridge", "bridge", OSMFieldBool),
                                       Field("tunnel", "tunnel", OSMFieldBool), Field("layer", "layer", OSMFieldInteger), Field("maxspeed", "maxspeed", OSMFieldInteger)};
            layers.push_back(Layer("railways", "Railways", OSMGeometryLine, {
                    Rule("railway", {"rail", "subway", "tram", "light_rail", "narrow_gauge", "monorail", "funicular", "preserved", "miniature", "disused"})}));
            layers.back().vecFields = {Field("bridge", "bridge", OSMFieldBool), Field("tunnel", "tunnel", OSMFieldBool), Field("layer", "layer", OSMFieldInteger)};
            layers.push_back(Layer("waterways", "Waterways", OSMGeometryLine, {
                    Rule("waterway", {"river", "stream", "canal", "drain", "ditch", "brook", "tidal_channel"})}));

            // points
            layers.push_back(Layer("places", "Places", OSMGeometryPoint, {
                    Rule("place")}));
            layers.back().vecFields = {Field("population", "population", OSMFieldInteger)};
            layers.push_back(Layer("pois", "Points of interest", OSMGeometryPoint, {
                    Rule("amenity"),
                    Rule("shop"),
                    Rule("tourism"),
                    Rule("leisure"),
                    Rule("historic"),
                    Rule("office"),
                    Rule("craft"),
                    Rule("healthcare"),
                    Rule("emergency", {}, {"fire_hydrant"}),
                    Rule("railway", {"station", "halt", "tram_stop", "subway_entrance"}),
                    Rule("public_transport", {"station"}),
                    Rule("highway", {"bus_stop"}),
                    Rule("aeroway", {"aerodrome", "helipad", "terminal"}),
                    Rule("man_made", {"lighthouse", "tower", "windmill", "water_tower"}),
                    Rule("natural", {"peak", "volcano", "spring", "cave_entrance"})}));
            return layers;
        }

        std::vector<SOSMTableDef> COSMSchema::DefaultTables()
        {
            std::vector<SOSMTableDef> tables(3);
            tables[0].sName = "routes";
            tables[0].sDisplayName = "Routes";
            tables[0].tableType = OSMTableRoutes;
            tables[1].sName = "restrictions";
            tables[1].sDisplayName = "Turn restrictions";
            tables[1].tableType = OSMTableRestrictions;
            tables[2].sName = "tags";
            tables[2].sDisplayName = "Tags";
            tables[2].tableType = OSMTableTags;
            return tables;
        }

        const char* COSMSchema::TableKey(eOSMTableType type)
        {
            switch(type)
            {
                case OSMTableRoutes:       return "route";
                case OSMTableRestrictions: return "restriction";
                default:                   return "key";
            }
        }

        const SOSMLayerDef* COSMSchema::FindLayer(const std::string& sName) const
        {
            int nIndex = FindLayerIndex(sName);
            return nIndex >= 0 ? &m_vecLayers[nIndex] : nullptr;
        }

        int COSMSchema::FindLayerIndex(const std::string& sName) const
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
            {
                if(m_vecLayers[i].sName == sName)
                    return (int)i;
            }
            return -1;
        }

        const char* COSMSchema::MatchRoute(const COSMTags& tags)
        {
            if(!tags.Is("type", "route"))
                return nullptr;
            const char* pszRoute = tags.Get("route");
            return pszRoute && *pszRoute ? pszRoute : "(none)";
        }

        const char* COSMSchema::MatchRestriction(const COSMTags& tags)
        {
            if(!tags.Is("type", "restriction"))
                return nullptr;
            const char* pszRestriction = tags.Get("restriction");
            if(!pszRestriction)
            {
                // restriction:hgv=..., restriction:bus=...
                for(const char* pszKey : {"restriction:motorcar", "restriction:hgv", "restriction:bus", "restriction:bicycle", "restriction:conditional"})
                {
                    pszRestriction = tags.Get(pszKey);
                    if(pszRestriction)
                        break;
                }
            }
            return pszRestriction && *pszRestriction ? pszRestriction : "(none)";
        }

        COSMSchema::COSMSchema() : m_vecLayers(DefaultLayers())
        {

        }

        COSMSchema::COSMSchema(const std::vector<SOSMLayerDef>& vecLayers) : m_vecLayers(vecLayers)
        {

        }

        COSMMapPtr COSMSchema::CreateMap() const
        {
            COSMMapPtr ptrMap = std::make_shared<COSMMap>();
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
            {
                const SOSMLayerDef& def = m_vecLayers[i];
                std::vector<std::string> vecKeys;
                for(size_t r = 0; r < def.vecRules.size(); ++r)
                    vecKeys.push_back(def.vecRules[r].sGroupKey.empty() ? def.vecRules[r].sKey : def.vecRules[r].sGroupKey);
                ptrMap->AddLayer(std::make_shared<COSMLayer>(def.sName, def.sDisplayName, def.geometryType, vecKeys));
            }

            std::vector<SOSMTableDef> tables = DefaultTables();
            for(size_t i = 0; i < tables.size(); ++i)
                ptrMap->AddTable(std::make_shared<COSMTable>(tables[i].sName, tables[i].sDisplayName, tables[i].tableType,
                                                             std::vector<std::string>(1, TableKey(tables[i].tableType))));
            return ptrMap;
        }

        bool COSMSchema::MatchRule(const SOSMKeyRule& rule, const COSMTags& tags, SOSMMatch& match)
        {
            const char* pszValue = tags.Get(rule.sKey.c_str());
            if(!pszValue || !*pszValue)
                return false;
            if(!rule.vecInclude.empty() && !Contains(rule.vecInclude, pszValue))
                return false;
            if(Contains(rule.vecExclude, pszValue))
                return false;

            if(rule.sGroupKey.empty())
            {
                match.sKey = rule.sKey;
                match.sValue = pszValue;
            }
            else
            {
                const char* pszGroupValue = tags.Get(rule.sGroupKey.c_str());
                match.sKey = rule.sGroupKey;
                match.sValue = pszGroupValue && *pszGroupValue ? pszGroupValue : "(none)";
            }
            return true;
        }

        SOSMMatch COSMSchema::Match(const COSMTags& tags, eOSMGeometryType geometryType) const
        {
            SOSMMatch match;
            if(tags.Count() == 0)
                return match;

            for(size_t i = 0; i < m_vecLayers.size(); ++i)
            {
                const SOSMLayerDef& def = m_vecLayers[i];
                if(def.geometryType != geometryType)
                    continue;

                for(size_t r = 0; r < def.vecRules.size(); ++r)
                {
                    if(MatchRule(def.vecRules[r], tags, match))
                    {
                        match.nLayer = (int)i;
                        return match;
                    }
                }
            }
            return SOSMMatch();
        }

        SOSMMatch COSMSchema::MatchNode(const COSMTags& tags) const
        {
            return Match(tags, OSMGeometryPoint);
        }

        void COSMSchema::MatchWay(const COSMTags& tags, bool bClosed, SOSMMatch& line, SOSMMatch& polygon) const
        {
            line = SOSMMatch();
            polygon = SOSMMatch();
            if(tags.Count() == 0)
                return;

            const char* pszArea = tags.Get("area");
            bool bAreaYes = pszArea && strcmp(pszArea, "yes") == 0;
            bool bAreaNo = pszArea && strcmp(pszArea, "no") == 0;

            // a closed highway / railway is a line (a roundabout) unless area=yes
            if(!(bClosed && bAreaYes))
                line = Match(tags, OSMGeometryLine);
            if(bClosed && !bAreaNo)
                polygon = Match(tags, OSMGeometryPolygon);
        }

        SOSMMatch COSMSchema::MatchRelation(const COSMTags& tags) const
        {
            const char* pszType = tags.Get("type");
            if(!pszType || (strcmp(pszType, "multipolygon") != 0 && strcmp(pszType, "boundary") != 0))
                return SOSMMatch();

            return Match(tags, OSMGeometryPolygon);
        }
    }
}
