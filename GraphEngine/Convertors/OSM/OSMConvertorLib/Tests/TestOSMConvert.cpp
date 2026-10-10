#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../OSMConvertor.h"
#include "../OSMConvertSession.h"
#include "../OSMNodeStore.h"
#include "../OSMGeometry.h"
#include "../../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../../CartographyLib/Map.h"

#include "../../../../GeometryCompression/ShapeFormat.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>

using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
    // a forest multipolygon with a hole (the outer ring of two ways), two roads with a turn restriction, a bus route, a cafe, a town
    const char* ConvertOSM = R"(<?xml version="1.0" encoding="UTF-8"?>
<osm version="0.6" generator="test">
  <node id="21" lat="50.00" lon="14.00"/>
  <node id="22" lat="50.00" lon="14.10"/>
  <node id="23" lat="50.10" lon="14.10"/>
  <node id="24" lat="50.10" lon="14.00"/>
  <node id="31" lat="50.04" lon="14.04"/>
  <node id="32" lat="50.04" lon="14.06"/>
  <node id="33" lat="50.06" lon="14.06"/>
  <node id="34" lat="50.06" lon="14.04"/>
  <node id="40" lat="50.05" lon="14.05"><tag k="amenity" v="cafe"/><tag k="name" v="Cafe"/></node>
  <node id="41" lat="50.07" lon="14.07"><tag k="place" v="town"/><tag k="name" v="Town"/><tag k="population" v="5000"/></node>
  <way id="200"><nd ref="21"/><nd ref="22"/><nd ref="23"/></way>
  <way id="201"><nd ref="23"/><nd ref="24"/><nd ref="21"/></way>
  <way id="202"><nd ref="31"/><nd ref="32"/><nd ref="33"/><nd ref="34"/><nd ref="31"/></way>
  <way id="203"><nd ref="21"/><nd ref="22"/><tag k="highway" v="primary"/><tag k="oneway" v="yes"/><tag k="maxspeed" v="50 mph"/><tag k="name" v="Hlavni"/><tag k="name:en" v="Main"/></way>
  <way id="204"><nd ref="22"/><nd ref="23"/><tag k="highway" v="residential"/></way>
  <relation id="2000"><member type="way" ref="200" role="outer"/><member type="way" ref="202" role="inner"/><member type="way" ref="201" role="outer"/><tag k="type" v="multipolygon"/><tag k="landuse" v="forest"/></relation>
  <relation id="2001"><member type="way" ref="203" role="from"/><member type="node" ref="22" role="via"/><member type="way" ref="204" role="to"/><tag k="type" v="restriction"/><tag k="restriction" v="no_left_turn"/></relation>
  <relation id="2002"><member type="way" ref="203" role=""/><member type="way" ref="204" role=""/><member type="node" ref="22" role="stop"/><tag k="type" v="route"/><tag k="route" v="bus"/></relation>
</osm>
)";

    std::filesystem::path TestDir()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineOSMTests";
        std::filesystem::create_directories(dir);
        return dir;
    }

    std::string WriteFile(const char* pszContent, const std::string& sName)
    {
        std::filesystem::path path = TestDir() / sName;
        std::ofstream file(path, std::ios::binary);
        file << pszContent;
        return path.string();
    }

    GeoDatabase::IDatabaseWorkspacePtr CreateDatabase(const std::string& sName)
    {
        std::string sPath = (TestDir() / (sName + ".sqlite")).string();
        for(const char* ext : {"", "-wal", "-shm"})
            std::filesystem::remove(sPath + ext);
        return GeoDatabase::CSQLiteWorkspace::Create(sName.c_str(), sPath.c_str(), CommonLib::CGuid::CreateNew());
    }

    GeoDatabase::ISelectCursorPtr Query(GeoDatabase::IDatabaseWorkspacePtr ptrDb, const std::string& sTable, const std::string& sSql)
    {
        GeoDatabase::ISelectCursorPtr ptrCursor = ptrDb->GetTable(sTable)->Select(sSql);
        REQUIRE(ptrCursor->Next());
        return ptrCursor;
    }

    int64_t RowCount(GeoDatabase::IDatabaseWorkspacePtr ptrDb, const std::string& sTable)
    {
        return Query(ptrDb, sTable, "SELECT count(*) FROM " + sTable)->ReadInt64(0);
    }

    SOSMConvertSettings TestSettings()
    {
        SOSMConvertSettings settings;
        settings.sTempDir = TestDir().string();
        settings.nNodeCacheMB = 1;
        settings.sNameLanguage = "en";
        return settings;
    }
}

TEST_CASE("OSM node store: unsorted nodes in a file", "[osm][nodes]")
{
    std::vector<int64_t> ids(50000);
    for(size_t i = 0; i < ids.size(); ++i)
        ids[i] = (int64_t)i * 7 + 1;
    std::shuffle(ids.begin(), ids.end(), std::mt19937(42));

    COSMNodeStore store(TestDir().string(), 1000 * 16);    // 1000 nodes in memory - many runs
    for(size_t i = 0; i < ids.size(); ++i)
        store.Add(ids[i], (ids[i] % 3600) / 10.0 - 180, (ids[i] % 1700) / 10.0 - 85);
    store.Finish();
    REQUIRE(store.IsInFile());
    REQUIRE(store.Count() == ids.size());

    for(size_t i = 0; i < ids.size(); i += 97)
    {
        double dLon = 0, dLat = 0;
        REQUIRE(store.Get(ids[i], dLon, dLat));
        REQUIRE(dLon == Catch::Approx((ids[i] % 3600) / 10.0 - 180).margin(1e-6));
        REQUIRE(dLat == Catch::Approx((ids[i] % 1700) / 10.0 - 85).margin(1e-6));
    }
    double dLon, dLat;
    REQUIRE_FALSE(store.Get(2, dLon, dLat));
    REQUIRE_FALSE(store.Get(1000000000, dLon, dLat));
}

TEST_CASE("OSM geometry: multipolygon with a hole", "[osm][geometry]")
{
    std::vector<TOSMPoints> ways = {
            {{0, 0}, {10, 0}, {10, 10}},
            {{4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4}},
            {{0, 0}, {0, 10}, {10, 10}}};    // reversed direction
    std::vector<TOSMPoints> rings;
    std::vector<bool> outer;
    REQUIRE(COSMGeometry::BuildMultipolygon(ways, rings, outer));
    REQUIRE(rings.size() == 2);
    REQUIRE(std::count(outer.begin(), outer.end(), true) == 1);
    for(size_t i = 0; i < rings.size(); ++i)
        REQUIRE(std::fabs(COSMGeometry::SignedArea(rings[i])) == Catch::Approx(outer[i] ? 100. : 4.));

    CommonLib::IGeoShapePtr ptrShape = COSMGeometry::CreatePolygon(rings, outer);
    REQUIRE(ptrShape->GetPartCount() == 2);
}

TEST_CASE("OSM conversion: the whole map", "[osm][convert]")
{
    std::string sPath = WriteFile(ConvertOSM, "convert.osm");
    COSMConvertor convertor(TestSettings());
    IOSMMapPtr ptrOSMMap = convertor.ReadMap(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    REQUIRE(ptrOSMMap->FindTable("routes")->GetFeatureCount() == 1);
    REQUIRE(ptrOSMMap->FindTable("restrictions")->GetFeatureCount() == 1);
    REQUIRE(ptrOSMMap->FindTable("tags")->GetFeatureCount() == 17);
    REQUIRE(ptrOSMMap->FindTable("tags")->FindKey("key")->FindValue("name")->GetFeatureCount() == 3);

    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_all");
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    convertor.Convert(ptrOSMMap, ptrMap, ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    // feature layers: only the ones with features
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 4);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetName() == "Places");
    REQUIRE(ptrMap->GetSpatialReference().get() != nullptr);

    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    {
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_landuse", "SELECT osm_id, osm_type, class, type, Shape FROM osm_landuse");
        REQUIRE(ptrCursor->ReadInt64(0) == 2000);
        REQUIRE(ptrCursor->ReadText(1) == "r");
        REQUIRE(ptrCursor->ReadText(2) == "landuse");
        REQUIRE(ptrCursor->ReadText(3) == "forest");
        CommonLib::IGeoShapePtr ptrShape = ptrCursor->ReadShape(4);
        REQUIRE(ptrShape->GetPartCount() == 2);
        // the geometry is written compressed (SOSMConvertSettings::compression)
        REQUIRE(ptrShape->Data()[0] == GeometryCompression::ShapeCompressedFlag);
        REQUIRE(ptrShape->GetPointCnt() == 10);
    }

    REQUIRE(RowCount(ptrDb, "osm_roads") == 2);
    {
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_roads", "SELECT name, oneway, maxspeed, type FROM osm_roads WHERE osm_id = 203");
        REQUIRE(ptrCursor->ReadText(0) == "Main");     // name:en
        REQUIRE(ptrCursor->ReadInt64(1) == 1);
        REQUIRE(ptrCursor->ReadInt64(2) == 50);
        REQUIRE(ptrCursor->ReadText(3) == "primary");
    }
    REQUIRE(Query(ptrDb, "osm_places", "SELECT population FROM osm_places")->ReadInt64(0) == 5000);
    REQUIRE(RowCount(ptrDb, "osm_pois") == 1);
    REQUIRE(ptrDb->GetTable("osm_roads")->GetExtent()->GetBoundingBox().xMax == Catch::Approx(14.1 * 20037508.342789244 / 180).epsilon(1e-6));

    REQUIRE(RowCount(ptrDb, "osm_routes") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes_members") == 3);
    REQUIRE(Query(ptrDb, "osm_routes_members", "SELECT role FROM osm_routes_members WHERE seq = 2")->ReadText(0) == "stop");
    {
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_restrictions", "SELECT restriction, from_way, via_type, via_id, to_way FROM osm_restrictions");
        REQUIRE(ptrCursor->ReadText(0) == "no_left_turn");
        REQUIRE(ptrCursor->ReadInt64(1) == 203);
        REQUIRE(ptrCursor->ReadText(2) == "n");
        REQUIRE(ptrCursor->ReadInt64(3) == 22);
        REQUIRE(ptrCursor->ReadInt64(4) == 204);
    }
    REQUIRE(RowCount(ptrDb, "osm_tags") == 17);
    REQUIRE(Query(ptrDb, "osm_tags", "SELECT value FROM osm_tags WHERE osm_type = 'w' AND osm_id = 203 AND key = 'maxspeed'")->ReadText(0) == "50 mph");
}

TEST_CASE("OSM conversion: without reading the map first", "[osm][convert]")
{
    std::string sPath = WriteFile(ConvertOSM, "convert_all.osm");
    COSMConvertor convertor(TestSettings());
    IOSMMapPtr ptrOSMMap = convertor.CreateMap(sPath);
    REQUIRE_FALSE(ptrOSMMap->IsScanned());
    REQUIRE(ptrOSMMap->GetNodeCount() == 0);
    REQUIRE(ptrOSMMap->FindLayer("roads")->GetFeatureCount() == 0);

    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_all_unscanned");
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    convertor.Convert(ptrOSMMap, ptrMap, ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    // the same as after ReadMap: the empty layers aren't added to the map
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 4);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetName() == "Places");
    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    REQUIRE(RowCount(ptrDb, "osm_roads") == 2);
    REQUIRE(RowCount(ptrDb, "osm_pois") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes_members") == 3);
    REQUIRE(RowCount(ptrDb, "osm_restrictions") == 1);
    REQUIRE(RowCount(ptrDb, "osm_tags") == 17);

    // ConvertFromXML doesn't read the map first either
    GeoDatabase::IDatabaseWorkspacePtr ptrDb2 = CreateDatabase("osm_from_xml");
    Cartography::IMapPtr ptrMap2 = std::make_shared<Cartography::CMap>();
    convertor.ConvertFromXML(sPath, ptrMap2, ptrDb2, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrMap2->GetLayers()->GetLayerCount() == 4);
    REQUIRE(RowCount(ptrDb2, "osm_roads") == 2);
}

TEST_CASE("OSM conversion: session, datasets one by one", "[osm][convert]")
{
    std::string sPath = WriteFile(ConvertOSM, "session.osm");
    COSMConvertor convertor(TestSettings());
    IOSMMapPtr ptrOSMMap = convertor.ReadMap(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    ptrOSMMap->RemoveLayer("pois");
    ptrOSMMap->FindLayer("roads")->FindKey("highway")->FindValue("residential")->SetEnabled(false);
    ptrOSMMap->FindTable("tags")->FindKey("key")->FindValue("name")->SetEnabled(false);

    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_session");
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    IOSMConvertSessionPtr ptrSession = convertor.CreateSession(ptrOSMMap, ptrMap, ptrDb);
    COSMConvertSession* pSession = dynamic_cast<COSMConvertSession*>(ptrSession.get());

    ptrSession->ConvertDataset(ptrOSMMap->FindLayer("roads"), IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrSession->IsConverted(ptrOSMMap->FindLayer("roads")));
    REQUIRE_FALSE(ptrSession->IsConverted(ptrOSMMap->FindLayer("landuse")));
    REQUIRE(pSession->HasNodeStore());
    REQUIRE(RowCount(ptrDb, "osm_roads") == 1);     // residential is disabled
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 1);

    // a disabled dataset can be converted alone
    ptrOSMMap->FindLayer("places")->SetEnabled(false);
    ptrSession->ConvertDataset(ptrOSMMap->FindLayer("places"), IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(RowCount(ptrDb, "osm_places") == 1);

    // the rest: landuse and the tables (the node store is reused)
    ptrSession->Convert(IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes") == 1);

    // the map layers keep the cartographic order: landuse, roads, places
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 3);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(1)->GetName() == "Roads");
    REQUIRE(ptrMap->GetLayers()->GetLayer(2)->GetName() == "Places");

    // tags: no names; the selected datasets are landuse, roads (only primary), places (converted), the relations,
    // not the removed pois: 203 (4 of 5 tags), 41 (2 of 3), 2000, 2001, 2002 (2 each)
    REQUIRE(RowCount(ptrDb, "osm_tags") == 12);
    ptrSession->Close();
}

TEST_CASE("OSM conversion: compression settings", "[osm][convert]")
{
    std::string sPath = WriteFile(ConvertOSM, "compress.osm");
    SOSMConvertSettings settings = TestSettings();

    // the parameters: auto by the units, maximum by the extent (the bounds of a read map, the world otherwise), manual
    COSMConvertor convertor(settings);
    IOSMMapPtr ptrRead = convertor.ReadMap(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    IOSMMapPtr ptrNotRead = convertor.CreateMap(sPath);
    REQUIRE(convertor.GetCompressParams(ptrRead).nScaleExponent == 2);
    settings.compression.scale = OSMCompressScaleMaximum;
    REQUIRE(COSMConvertor::CompressParams(settings, ptrRead).nScaleExponent == 9);      // y ~ 6.4e6 m
    REQUIRE(COSMConvertor::CompressParams(settings, ptrNotRead).nScaleExponent == 8);   // the world, 2e7 m
    settings.bWebMercator = false;
    REQUIRE(COSMConvertor::CompressParams(settings, ptrRead).nScaleExponent == 14);     // 50 degrees
    settings.compression.scale = OSMCompressScaleAuto;
    REQUIRE(COSMConvertor::CompressParams(settings, ptrRead).nScaleExponent == 7);
    settings.compression.scale = OSMCompressScaleManual;
    settings.compression.nManualScaleExponent = 30;
    REQUIRE_THROWS(COSMConvertor::CompressParams(settings, ptrRead));

    // a manual scale: the coordinates are rounded to meters
    settings = TestSettings();
    settings.compression.scale = OSMCompressScaleManual;
    settings.compression.nManualScaleExponent = 0;
    {
        COSMConvertor manual(settings);
        GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_compress_manual");
        manual.Convert(manual.CreateMap(sPath), Cartography::IMapPtr(), ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_pois", "SELECT Shape FROM osm_pois");
        CommonLib::IGeoShapePtr ptrShape = ptrCursor->ReadShape(0);
        REQUIRE(ptrShape->Data()[0] == GeometryCompression::ShapeCompressedFlag);
        CommonLib::GisXYPoint pt;
        REQUIRE(ptrShape->NextPoint(0, pt));
        REQUIRE(pt.x == std::round(pt.x));
        REQUIRE(pt.x == Catch::Approx(14.05 * 20037508.342789244 / 180).margin(0.5));
    }

    // disabled: the shapes are raw
    settings = TestSettings();
    settings.compression.bEnabled = false;
    {
        COSMConvertor raw(settings);
        GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_compress_off");
        raw.Convert(raw.CreateMap(sPath), Cartography::IMapPtr(), ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_roads", "SELECT Shape FROM osm_roads");
        REQUIRE(ptrCursor->ReadShape(0)->Data()[0] == 0);
    }
}
