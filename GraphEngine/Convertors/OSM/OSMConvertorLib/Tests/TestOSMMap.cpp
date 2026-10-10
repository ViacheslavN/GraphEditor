#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../OSMConvertor.h"
#include "../OSMMap.h"
#include "../../../../CommonLib/Serialize/SerializeXML.h"
#include "../../../../CommonLib/xml/XMLNode.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
    const char* TestOSM = R"(<?xml version="1.0" encoding="UTF-8"?>
<osm version="0.6" generator="test">
  <node id="1" lat="50.00" lon="14.00"><tag k="place" v="city"/><tag k="name" v="Big City"/></node>
  <node id="2" lat="50.01" lon="14.01"><tag k="amenity" v="restaurant"/><tag k="name" v="Food"/></node>
  <node id="3" lat="50.02" lon="14.02"><tag k="amenity" v="cafe"/></node>
  <node id="4" lat="50.03" lon="14.03"><tag k="highway" v="bus_stop"/></node>
  <node id="5" lat="50.04" lon="14.04"/>
  <node id="6" lat="50.05" lon="14.05"/>
  <node id="7" lat="50.06" lon="14.06"/>
  <node id="8" lat="50.07" lon="14.07"/>
  <node id="9" lat="49.90" lon="13.90"/>
  <node id="10" lat="50.10" lon="14.10"/>
  <way id="100"><nd ref="5"/><nd ref="6"/><tag k="highway" v="residential"/><tag k="name" v="Main street"/></way>
  <way id="101"><nd ref="6"/><nd ref="7"/><tag k="highway" v="residential"/></way>
  <way id="102"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="building" v="yes"/></way>
  <way id="103"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="landuse" v="forest"/></way>
  <way id="104"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="highway" v="primary"/><tag k="junction" v="roundabout"/></way>
  <way id="105"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="highway" v="pedestrian"/><tag k="area" v="yes"/></way>
  <way id="106"><nd ref="5"/><nd ref="6"/><nd ref="7"/><nd ref="5"/><tag k="landuse" v="reservoir"/></way>
  <way id="107"><nd ref="7"/><nd ref="8"/><tag k="waterway" v="river"/></way>
  <way id="108"><nd ref="8"/><nd ref="9"/><nd ref="10"/><nd ref="8"/></way>
  <way id="109"><nd ref="7"/><nd ref="8"/><tag k="highway" v="construction"/></way>
  <relation id="1000"><member type="way" ref="108" role="outer"/><tag k="type" v="multipolygon"/><tag k="natural" v="water"/></relation>
  <relation id="1001"><member type="way" ref="108" role="outer"/><tag k="type" v="boundary"/><tag k="boundary" v="administrative"/><tag k="admin_level" v="4"/></relation>
  <relation id="1002"><member type="way" ref="100" role=""/><tag k="type" v="route"/><tag k="route" v="bus"/></relation>
</osm>
)";

    std::string WriteTestFile(const char* pszContent, const std::string& sName)
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / sName;
        std::ofstream file(path, std::ios::binary);
        file << pszContent;
        return path.string();
    }

    uint64_t ValueCount(IOSMMapPtr ptrMap, const char* pszLayer, const char* pszKey, const char* pszValue)
    {
        IOSMLayerPtr ptrLayer = ptrMap->FindLayer(pszLayer);
        if(!ptrLayer.get())
            return 0;
        IOSMTagKeyPtr ptrKey = ptrLayer->FindKey(pszKey);
        if(!ptrKey.get())
            return 0;
        IOSMTagValuePtr ptrValue = ptrKey->FindValue(pszValue);
        return ptrValue.get() ? ptrValue->GetFeatureCount() : 0;
    }
}

TEST_CASE("OSM map: hierarchy of an XML file", "[osm]")
{
    std::string sPath = WriteTestFile(TestOSM, "graphengine_test_map.osm");
    COSMConvertor convertor;
    IOSMMapPtr ptrMap = convertor.ReadMap(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    std::remove(sPath.c_str());

    REQUIRE(ptrMap != nullptr);
    REQUIRE(ptrMap->GetFormat() == OSMFormatXML);
    REQUIRE(ptrMap->GetNodeCount() == 10);
    REQUIRE(ptrMap->GetWayCount() == 10);
    REQUIRE(ptrMap->GetRelationCount() == 3);
    REQUIRE(ptrMap->IsSorted());
    REQUIRE(ptrMap->GetBounds().xMin == Catch::Approx(13.9));
    REQUIRE(ptrMap->GetBounds().yMax == Catch::Approx(50.1));

    REQUIRE(ValueCount(ptrMap, "places", "place", "city") == 1);
    REQUIRE(ValueCount(ptrMap, "pois", "amenity", "restaurant") == 1);
    REQUIRE(ValueCount(ptrMap, "pois", "amenity", "cafe") == 1);
    REQUIRE(ValueCount(ptrMap, "pois", "highway", "bus_stop") == 1);
    REQUIRE(ValueCount(ptrMap, "roads", "highway", "residential") == 2);
    REQUIRE(ValueCount(ptrMap, "roads", "highway", "primary") == 1);       // closed roundabout is a line
    REQUIRE(ValueCount(ptrMap, "roads", "highway", "pedestrian") == 0);    // area=yes is not a line
    REQUIRE(ValueCount(ptrMap, "roads", "highway", "construction") == 0);  // excluded
    REQUIRE(ValueCount(ptrMap, "buildings", "building", "yes") == 1);
    REQUIRE(ValueCount(ptrMap, "landuse", "landuse", "forest") == 1);
    REQUIRE(ValueCount(ptrMap, "landuse", "landuse", "reservoir") == 0);   // goes to water
    REQUIRE(ValueCount(ptrMap, "water", "landuse", "reservoir") == 1);
    REQUIRE(ValueCount(ptrMap, "water", "natural", "water") == 1);         // multipolygon relation
    REQUIRE(ValueCount(ptrMap, "waterways", "waterway", "river") == 1);
    REQUIRE(ValueCount(ptrMap, "boundaries", "admin_level", "4") == 1);

    IOSMLayerPtr ptrRoads = ptrMap->FindLayer("roads");
    REQUIRE(ptrRoads->GetGeometryType() == OSMGeometryLine);
    REQUIRE(ptrRoads->GetFeatureCount() == 3);
    REQUIRE(ptrRoads->GetTableName() == "osm_roads");
    // values are sorted by the number of features
    REQUIRE(ptrRoads->FindKey("highway")->GetValue(0)->GetValue() == "residential");
}

TEST_CASE("OSM map: selection", "[osm]")
{
    COSMSchema schema;
    COSMMapPtr ptrMap = schema.CreateMap();
    COSMLayerPtr ptrRoads = std::dynamic_pointer_cast<COSMLayer>(ptrMap->FindLayer("roads"));
    REQUIRE(ptrRoads != nullptr);
    ptrRoads->AddFeature("highway", "motorway");
    ptrRoads->AddFeature("highway", "footway");

    REQUIRE(ptrRoads->IsEnabled("highway", "motorway"));
    REQUIRE(ptrRoads->IsEnabled("highway", "unknown_value"));   // not met while reading - enabled
    REQUIRE_FALSE(ptrRoads->IsEnabled("building", "yes"));      // not a key of the layer

    ptrRoads->FindKey("highway")->FindValue("footway")->SetEnabled(false);
    REQUIRE_FALSE(ptrRoads->IsEnabled("highway", "footway"));
    REQUIRE(ptrRoads->IsEnabled("highway", "motorway"));

    ptrRoads->FindKey("highway")->SetEnabled(false);
    REQUIRE_FALSE(ptrRoads->IsEnabled("highway", "motorway"));

    ptrRoads->FindKey("highway")->SetEnabled(true);
    ptrRoads->SetEnabled(false);
    REQUIRE_FALSE(ptrRoads->IsEnabled("highway", "motorway"));
}

TEST_CASE("OSM map: save / load", "[osm]")
{
    COSMSchema schema;
    COSMMapPtr ptrMap = schema.CreateMap();
    ptrMap->SetPath("C:/data/czech-republic-latest.osm.pbf");
    ptrMap->SetCounts(100, 20, 3);
    COSMLayerPtr ptrRoads = std::dynamic_pointer_cast<COSMLayer>(ptrMap->FindLayer("roads"));
    ptrRoads->AddFeature("highway", "motorway");
    ptrRoads->AddFeature("highway", "footway");
    ptrRoads->AddFeature("highway", "footway");
    ptrRoads->SetTableName("roads_cz");
    ptrRoads->FindKey("highway")->FindValue("footway")->SetEnabled(false);
    ptrMap->FindLayer("buildings")->SetEnabled(false);

    CommonLib::xml::IXMLNodePtr ptrNode = std::make_shared<CommonLib::xml::CXMLNode>(CommonLib::xml::IXMLNodePtr(), "root");
    CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(ptrNode);
    ptrMap->Save(ptrRoot);

    COSMMap loaded;
    loaded.Load(ptrRoot);
    REQUIRE(loaded.GetFormat() == OSMFormatPBF);
    REQUIRE(loaded.GetNodeCount() == 100);
    REQUIRE(loaded.GetLayerCount() == ptrMap->GetLayerCount());
    REQUIRE_FALSE(loaded.FindLayer("buildings")->GetEnabled());

    IOSMLayerPtr ptrLoadedRoads = loaded.FindLayer("roads");
    REQUIRE(ptrLoadedRoads->GetTableName() == "roads_cz");
    REQUIRE(ptrLoadedRoads->GetGeometryType() == OSMGeometryLine);
    REQUIRE(ptrLoadedRoads->GetFeatureCount() == 3);
    REQUIRE(ptrLoadedRoads->FindKey("highway")->FindValue("footway")->GetFeatureCount() == 2);
    REQUIRE_FALSE(ptrLoadedRoads->IsEnabled("highway", "footway"));
    REQUIRE(ptrLoadedRoads->IsEnabled("highway", "motorway"));
}
