#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../OSMConvertor.h"
#include "../OSMNodeStore.h"
#include "../OSMGeometry.h"
#include "../OSMSplitFormat.h"
#include "../../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../../CartographyLib/Map.h"

#include "../../../../../GeometryCompression/ShapeFormat.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <atomic>
#include <thread>

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
        settings.nThreads = 4;     // several workers of the ways pass even on a small machine
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

    // the lookups of the threads: their own file handles and caches, the same results
    std::vector<std::thread> threads;
    std::atomic<int> nErrors(0);
    for(int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&store, &ids, &nErrors, t]()
        {
            std::unique_ptr<IOSMNodeLookup> ptrLookup = store.CreateLookup(64 * 1024);
            for(size_t i = (size_t)t; i < ids.size(); i += 7)
            {
                int32_t nLon = 0, nLat = 0;
                double dLon = 0, dLat = 0;
                if(!ptrLookup->GetE7(ids[i], nLon, nLat) || !ptrLookup->Get(ids[i], dLon, dLat) ||
                   nLon != (int32_t)std::lround(((ids[i] % 3600) / 10.0 - 180) * 1e7) || dLat != nLat / 1e7)
                    ++nErrors;
            }
            int32_t nLon, nLat;
            if(ptrLookup->GetE7(2, nLon, nLat))
                ++nErrors;
        });
    }
    for(std::thread& thread : threads)
        thread.join();
    REQUIRE(nErrors == 0);
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
    IOSMMapPtr ptrOSMMap = convertor.Analyze(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    REQUIRE(ptrOSMMap->FindTable("routes")->GetFeatureCount() == 1);
    REQUIRE(ptrOSMMap->FindTable("restrictions")->GetFeatureCount() == 1);
    REQUIRE(ptrOSMMap->FindTable("tags")->GetFeatureCount() == 17);
    REQUIRE(ptrOSMMap->FindTable("tags")->FindKey("key")->FindValue("name")->GetFeatureCount() == 3);

    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_all");
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    // the columns of the roads: the converter ones, the fields of the schema, the OSM keys (name:en)
    IOSMLayerPtr ptrRoads = ptrOSMMap->FindLayer("roads");
    REQUIRE(ptrRoads->FindColumn("osm_id") == 0);
    REQUIRE(ptrRoads->GetColumn(ptrRoads->FindColumn("type")).bRequired);
    REQUIRE(ptrRoads->GetColumn(ptrRoads->FindColumn("maxspeed")).bInteger);
    REQUIRE(ptrRoads->GetColumn(ptrRoads->FindColumn("maxspeed")).nCount == 1);
    REQUIRE(ptrRoads->FindColumn("highway") < 0);        // the key of the layer: class / type
    int nNameEn = ptrRoads->FindColumn("name_en");
    REQUIRE(nNameEn >= 0);
    REQUIRE(ptrRoads->GetColumn(nNameEn).sKey == "name:en");
    REQUIRE(ptrRoads->GetColumn(nNameEn).nCount == 1);

    convertor.Convert(ptrOSMMap, CLayerColumnExcludeList(), ptrMap, ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    // feature layers: only the ones with features
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 5);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetName() == "Places");
    // the small scale layer of the places: over them, where they aren't shown, with the index of its query column
    REQUIRE(ptrMap->GetLayers()->GetLayer(4)->GetName() == "Cities");
    REQUIRE(ptrMap->GetLayers()->GetLayer(4)->GetMaximumScale() == ptrMap->GetLayers()->GetLayer(3)->GetMinimumScale());
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetMinimumScale() > 0.);
    REQUIRE(Query(ptrDb, "osm_places", "SELECT count(*) FROM sqlite_master WHERE type = 'index' AND name = 'osm_places_type'")->ReadInt64(0) == 1);
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
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_roads", "SELECT name, oneway, maxspeed, type, name_en FROM osm_roads WHERE osm_id = 203");
        REQUIRE(ptrCursor->ReadText(0) == "Main");     // name:en
        REQUIRE(ptrCursor->ReadInt64(1) == 1);
        REQUIRE(ptrCursor->ReadInt64(2) == 50);
        REQUIRE(ptrCursor->ReadText(3) == "primary");
        REQUIRE(ptrCursor->ReadText(4) == "Main");     // the OSM key column
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
    convertor.Convert(ptrOSMMap, CLayerColumnExcludeList(), ptrMap, ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    // the same as after Analyze: the empty layers aren't added to the map
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 5);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetName() == "Places");
    // the small scale layer of the places: over them, where they aren't shown, with the index of its query column
    REQUIRE(ptrMap->GetLayers()->GetLayer(4)->GetName() == "Cities");
    REQUIRE(ptrMap->GetLayers()->GetLayer(4)->GetMaximumScale() == ptrMap->GetLayers()->GetLayer(3)->GetMinimumScale());
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetMinimumScale() > 0.);
    REQUIRE(Query(ptrDb, "osm_places", "SELECT count(*) FROM sqlite_master WHERE type = 'index' AND name = 'osm_places_type'")->ReadInt64(0) == 1);
    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    REQUIRE(RowCount(ptrDb, "osm_roads") == 2);
    REQUIRE(RowCount(ptrDb, "osm_pois") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes_members") == 3);
    REQUIRE(RowCount(ptrDb, "osm_restrictions") == 1);
    REQUIRE(RowCount(ptrDb, "osm_tags") == 17);
    // no analysis: only the columns of the converter / schema
    REQUIRE(ptrDb->GetTable("osm_roads")->GetFields()->FindField("maxspeed") >= 0);
    REQUIRE(ptrDb->GetTable("osm_roads")->GetFields()->FindField("name_en") < 0);

    // ConvertFromXML doesn't read the map first either
    GeoDatabase::IDatabaseWorkspacePtr ptrDb2 = CreateDatabase("osm_from_xml");
    Cartography::IMapPtr ptrMap2 = std::make_shared<Cartography::CMap>();
    convertor.ConvertFromXML(sPath, ptrMap2, ptrDb2, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrMap2->GetLayers()->GetLayerCount() == 5);
    REQUIRE(RowCount(ptrDb2, "osm_roads") == 2);
}

TEST_CASE("OSM conversion: exclude list", "[osm][convert]")
{
    std::string sPath = WriteFile(ConvertOSM, "exclude.osm");
    COSMConvertor convertor(TestSettings());
    IOSMMapPtr ptrOSMMap = convertor.Analyze(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    ptrOSMMap->RemoveLayer("pois");
    ptrOSMMap->FindLayer("roads")->FindKey("highway")->FindValue("residential")->SetEnabled(false);
    ptrOSMMap->FindTable("tags")->FindKey("key")->FindValue("name")->SetEnabled(false);

    CLayerColumnExcludeList excludeList;
    REQUIRE(excludeList.IsEmpty());
    excludeList.ExcludeLayer("places");
    excludeList.ExcludeColumn("roads", "maxspeed");
    excludeList.ExcludeColumn("roads", "name_en");
    excludeList.ExcludeColumn("roads", "osm_id");              // required: written anyway
    excludeList.ExcludeColumn("restrictions", "except_for");
    REQUIRE(excludeList.IsLayerExcluded("places"));
    REQUIRE(excludeList.IsColumnExcluded("roads", "maxspeed"));
    REQUIRE_FALSE(excludeList.IsColumnExcluded("landuse", "maxspeed"));

    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_exclude");
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    convertor.Convert(ptrOSMMap, excludeList, ptrMap, ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    // one conversion: landuse, roads (only primary), the tables; not the excluded places, not the removed pois
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 2);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(1)->GetName() == "Roads");
    REQUIRE(RowCount(ptrDb, "osm_roads") == 1);     // residential is disabled
    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes") == 1);
    REQUIRE(RowCount(ptrDb, "osm_restrictions") == 1);

    GeoDatabase::IFieldsPtr ptrRoadFields = ptrDb->GetTable("osm_roads")->GetFields();
    REQUIRE(ptrRoadFields->FindField("maxspeed") < 0);
    REQUIRE(ptrRoadFields->FindField("name_en") < 0);
    REQUIRE(ptrRoadFields->FindField("osm_id") >= 0);
    REQUIRE(ptrRoadFields->FindField("oneway") >= 0);
    REQUIRE(Query(ptrDb, "osm_roads", "SELECT oneway FROM osm_roads WHERE osm_id = 203")->ReadInt64(0) == 1);
    REQUIRE(ptrDb->GetTable("osm_restrictions")->GetFields()->FindField("except_for") < 0);
    REQUIRE(Query(ptrDb, "osm_restrictions", "SELECT to_way FROM osm_restrictions")->ReadInt64(0) == 204);

    // tags: no names; the converted datasets are landuse, roads (only primary), the relations:
    // 203 (4 of 5 tags), 2000, 2001, 2002 (2 each)
    REQUIRE(RowCount(ptrDb, "osm_tags") == 10);
}

TEST_CASE("OSM conversion: compression settings", "[osm][convert]")
{
    std::string sPath = WriteFile(ConvertOSM, "compress.osm");
    SOSMConvertSettings settings = TestSettings();

    // the parameters: auto by the units, maximum by the extent (the bounds of a read map, the world otherwise), manual
    COSMConvertor convertor(settings);
    IOSMMapPtr ptrRead = convertor.Analyze(sPath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
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
        manual.Convert(manual.CreateMap(sPath), CLayerColumnExcludeList(), Cartography::IMapPtr(), ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
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
        raw.Convert(raw.CreateMap(sPath), CLayerColumnExcludeList(), Cartography::IMapPtr(), ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_roads", "SELECT Shape FROM osm_roads");
        REQUIRE(ptrCursor->ReadShape(0)->Data()[0] == 0);
    }
}

TEST_CASE("OSM split: records and the index", "[osm][split]")
{
    COSMStringTable strings;
    COSMRecordBuffer record;
    readosm_tag tags[2] = {{"highway", "residential"}, {"name", "Hlavni"}};
    int32_t lons[3] = {140000000, 139999990, -1800000000};
    int32_t lats[3] = {500000000, 500000123, 899999999};
    COSMSplitCodec::EncodeWay(record, strings, 4200000000LL, OSMSplit::WayClosed, tags, 2, lons, lats, 3);

    SOSMSplitObject way;
    COSMSplitCodec::DecodeWay(record.Data(), strings, way);
    REQUIRE(way.nId == 4200000000LL);
    REQUIRE(way.nFlags == OSMSplit::WayClosed);
    REQUIRE(way.tags.size() == 2);
    REQUIRE(std::string(way.tags[1].key) == "name");
    REQUIRE(std::string(way.tags[1].value) == "Hlavni");
    REQUIRE(way.lons == std::vector<int32_t>(lons, lons + 3));
    REQUIRE(way.lats == std::vector<int32_t>(lats, lats + 3));

    // the index: the ids out of order are sorted, a duplicate id keeps the last offset
    std::string sIndex = (TestDir() / "split_test.idx").u8string();
    {
        COSMIdIndexWriter writer;
        writer.Create(sIndex);
        for(int64_t i = 0; i < 10000; ++i)
            writer.Add(i * 3, (uint64_t)i * 100);
        writer.Add(5, 77);
        writer.Add(6, 88);
        writer.Finish();
    }
    COSMIdIndex index;
    index.Open(sIndex, 16);
    uint64_t nOffset = 0;
    REQUIRE(index.Find(9000, nOffset));
    REQUIRE(nOffset == 300000);
    REQUIRE(index.Find(5, nOffset));
    REQUIRE(nOffset == 77);
    REQUIRE(index.Find(6, nOffset));
    REQUIRE(nOffset == 88);    // 6 = 2 * 3 was added before
    REQUIRE_FALSE(index.Find(7, nOffset));
    REQUIRE(index.Count() == 10001);
}

TEST_CASE("OSM split: split and export", "[osm][split]")
{
    std::string sPath = WriteFile(ConvertOSM, "split.osm");
    std::string sFolder = (TestDir() / "split_data").u8string();
    COSMConvertor convertor(TestSettings());
    IOSMMapPtr ptrSplit = convertor.Split(sPath, sFolder, true, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    REQUIRE(ptrSplit->GetFormat() == OSMFormatSplit);
    REQUIRE(ptrSplit->IsScanned());
    REQUIRE(ptrSplit->IsSorted());
    REQUIRE(ptrSplit->GetNodeCount() == 10);
    REQUIRE(ptrSplit->GetWayCount() == 5);
    REQUIRE(ptrSplit->GetRelationCount() == 3);
    // a small file: the sorted node store (Auto), its coordinates are kept as nodes.sorted
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::u8path(OSMSplit::FilePath(sFolder, OSMSplit::NodesFlatFile))));
    REQUIRE(std::filesystem::file_size(std::filesystem::u8path(OSMSplit::FilePath(sFolder, OSMSplit::NodesSortedFile))) == 10 * 16);
    for(const char* pszFile : {OSMSplit::MetaFile, OSMSplit::StringsFile, OSMSplit::NodesSortedFile, OSMSplit::NodesData, OSMSplit::NodesIndex,
                               OSMSplit::WaysData, OSMSplit::WaysIndex, OSMSplit::RelationsData, OSMSplit::RelationsIndex})
        REQUIRE(std::filesystem::exists(std::filesystem::u8path(OSMSplit::FilePath(sFolder, pszFile))));

    // the split data are opened at once: the analysis, the columns
    std::string sMeta = OSMSplit::FilePath(sFolder, OSMSplit::MetaFile);
    IOSMMapPtr ptrOpened = convertor.Analyze(sMeta, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrOpened->GetFormat() == OSMFormatSplit);
    REQUIRE(ptrOpened->FindLayer("roads")->GetFeatureCount() == 2);
    REQUIRE(ptrOpened->FindLayer("roads")->FindColumn("name_en") >= 0);
    REQUIRE(ptrOpened->FindTable("tags")->GetFeatureCount() == 17);

    // the ways keep the coordinates of their nodes
    {
        COSMStringTable strings;
        strings.Load(OSMSplit::FilePath(sFolder, OSMSplit::StringsFile));
        COSMIdIndex index;
        index.Open(OSMSplit::FilePath(sFolder, OSMSplit::WaysIndex));
        COSMDataFile ways;
        ways.Open(OSMSplit::FilePath(sFolder, OSMSplit::WaysData));
        uint64_t nOffset = 0;
        REQUIRE(index.Find(202, nOffset));
        std::vector<uint8_t> record;
        ways.ReadRecordAt(nOffset, record);
        SOSMSplitObject way;
        COSMSplitCodec::DecodeWay(record, strings, way);
        REQUIRE(way.nId == 202);
        REQUIRE(way.lons.size() == 5);
        REQUIRE(way.nFlags == OSMSplit::WayClosed);
        REQUIRE(way.lons[1] == 140600000);
        REQUIRE(way.lats[2] == 500600000);
    }

    // the export: the same result as the conversion of the OSM file
    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_split_export");
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    convertor.Convert(ptrOpened, CLayerColumnExcludeList(), ptrMap, ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

    REQUIRE(ptrMap->GetLayers()->GetLayerCount() == 5);
    REQUIRE(ptrMap->GetLayers()->GetLayer(0)->GetName() == "Landuse");
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetName() == "Places");
    // the small scale layer of the places: over them, where they aren't shown, with the index of its query column
    REQUIRE(ptrMap->GetLayers()->GetLayer(4)->GetName() == "Cities");
    REQUIRE(ptrMap->GetLayers()->GetLayer(4)->GetMaximumScale() == ptrMap->GetLayers()->GetLayer(3)->GetMinimumScale());
    REQUIRE(ptrMap->GetLayers()->GetLayer(3)->GetMinimumScale() > 0.);
    REQUIRE(Query(ptrDb, "osm_places", "SELECT count(*) FROM sqlite_master WHERE type = 'index' AND name = 'osm_places_type'")->ReadInt64(0) == 1);
    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    {
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_landuse", "SELECT osm_id, osm_type, type, Shape FROM osm_landuse");
        REQUIRE(ptrCursor->ReadInt64(0) == 2000);
        REQUIRE(ptrCursor->ReadText(1) == "r");
        REQUIRE(ptrCursor->ReadText(2) == "forest");
        CommonLib::IGeoShapePtr ptrShape = ptrCursor->ReadShape(3);
        REQUIRE(ptrShape->GetPartCount() == 2);     // the multipolygon from ways.dat
        REQUIRE(ptrShape->GetPointCnt() == 10);
    }
    REQUIRE(RowCount(ptrDb, "osm_roads") == 2);
    {
        GeoDatabase::ISelectCursorPtr ptrCursor = Query(ptrDb, "osm_roads", "SELECT name, oneway, maxspeed, type, name_en FROM osm_roads WHERE osm_id = 203");
        REQUIRE(ptrCursor->ReadText(0) == "Main");
        REQUIRE(ptrCursor->ReadInt64(1) == 1);
        REQUIRE(ptrCursor->ReadInt64(2) == 50);
        REQUIRE(ptrCursor->ReadText(3) == "primary");
        REQUIRE(ptrCursor->ReadText(4) == "Main");
    }
    REQUIRE(Query(ptrDb, "osm_places", "SELECT population FROM osm_places")->ReadInt64(0) == 5000);
    REQUIRE(RowCount(ptrDb, "osm_pois") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes") == 1);
    REQUIRE(RowCount(ptrDb, "osm_routes_members") == 3);
    REQUIRE(Query(ptrDb, "osm_restrictions", "SELECT via_id, to_way FROM osm_restrictions")->ReadInt64(1) == 204);
    REQUIRE(RowCount(ptrDb, "osm_tags") == 17);

    // an exclude list works the same way
    CLayerColumnExcludeList excludeList;
    excludeList.ExcludeLayer("places");
    excludeList.ExcludeColumn("roads", "maxspeed");
    GeoDatabase::IDatabaseWorkspacePtr ptrDb2 = CreateDatabase("osm_split_export_excluded");
    Cartography::IMapPtr ptrMap2 = std::make_shared<Cartography::CMap>();
    convertor.Convert(convertor.Analyze(sMeta, IProgressUpdaterPtr(), Display::ITrackCancelPtr()), excludeList, ptrMap2, ptrDb2,
                      IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrMap2->GetLayers()->GetLayerCount() == 3);
    REQUIRE(ptrDb2->GetTable("osm_roads")->GetFields()->FindField("maxspeed") < 0);
}

TEST_CASE("OSM split: an unsorted file, two passes", "[osm][split]")
{
    // node 3 comes after the way: the ways can't be resolved in one pass
    const char* UnsortedOSM = R"(<?xml version="1.0" encoding="UTF-8"?>
<osm version="0.6" generator="test">
  <node id="1" lat="50.00" lon="14.00"/>
  <node id="2" lat="50.01" lon="14.01"/>
  <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><tag k="highway" v="residential"/></way>
  <node id="3" lat="50.02" lon="14.02"/>
</osm>
)";
    std::string sPath = WriteFile(UnsortedOSM, "split_unsorted.osm");
    std::string sFolder = (TestDir() / "split_unsorted").u8string();
    COSMConvertor convertor(TestSettings());
    IOSMMapPtr ptrSplit = convertor.Split(sPath, sFolder, false, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE_FALSE(ptrSplit->IsSorted());
    REQUIRE(ptrSplit->GetNodeCount() == 3);
    REQUIRE(ptrSplit->GetWayCount() == 1);
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::u8path(OSMSplit::FilePath(sFolder, OSMSplit::NodesFlatFile))));   // not kept
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::u8path(OSMSplit::FilePath(sFolder, OSMSplit::NodesSortedFile))));

    COSMStringTable strings;
    strings.Load(OSMSplit::FilePath(sFolder, OSMSplit::StringsFile));
    COSMDataFile ways;
    ways.Open(OSMSplit::FilePath(sFolder, OSMSplit::WaysData));
    std::vector<uint8_t> record;
    REQUIRE(ways.ReadRecord(record));
    SOSMSplitObject way;
    COSMSplitCodec::DecodeWay(record, strings, way);
    REQUIRE(way.lons.size() == 3);
    REQUIRE(way.nFlags == 0);
    REQUIRE(way.lats[2] == 500200000);
    REQUIRE_FALSE(ways.ReadRecord(record));
}

TEST_CASE("OSM split: the flat node store, the coordinates by parts in memory", "[osm][split]")
{
    // the nodes far from each other: with 1 MB of memory (131072 ids per part) the ids are 4 parts,
    // the ways take the coordinates from the streams of the parts 1..3 and from the memory (the part 4)
    const char* FlatOSM = R"(<?xml version="1.0" encoding="UTF-8"?>
<osm version="0.6" generator="test">
  <node id="5" lat="50.00" lon="14.00"/>
  <node id="150000" lat="50.01" lon="14.01"/>
  <node id="300000" lat="50.02" lon="14.02"/>
  <node id="450000" lat="50.03" lon="14.03"><tag k="amenity" v="cafe"/></node>
  <way id="1"><nd ref="5"/><nd ref="300000"/><nd ref="450000"/><nd ref="5"/><tag k="highway" v="residential"/></way>
  <way id="2"><nd ref="150000"/><nd ref="5"/><nd ref="999999999"/><tag k="highway" v="service"/></way>
  <way id="3"><nd ref="450000"/><nd ref="150000"/></way>
  <relation id="7"><member type="way" ref="1" role="outer"/><tag k="type" v="multipolygon"/><tag k="landuse" v="forest"/></relation>
</osm>
)";
    std::string sPath = WriteFile(FlatOSM, "split_flat.osm");
    std::string sFolder = (TestDir() / "split_flat").u8string();
    SOSMConvertSettings settings = TestSettings();
    settings.nodeStore = OSMNodeStoreFlat;
    settings.nWaysNodeMemoryMB = 1;
    COSMConvertor convertor(settings);
    IOSMMapPtr ptrSplit = convertor.Split(sPath, sFolder, true, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrSplit->GetNodeCount() == 4);
    REQUIRE(ptrSplit->GetWayCount() == 3);
    REQUIRE(ptrSplit->GetRelationCount() == 1);
    REQUIRE(ptrSplit->IsSorted());
    REQUIRE(std::filesystem::exists(std::filesystem::u8path(OSMSplit::FilePath(sFolder, OSMSplit::NodesFlatFile))));
    for(int nPart = 1; nPart <= 3; ++nPart)    // the streams are deleted
        REQUIRE_FALSE(std::filesystem::exists(std::filesystem::u8path(OSMSplit::FilePath(settings.sTempDir, ("_coords_part" + std::to_string(nPart) + ".tmp").c_str()))));

    COSMStringTable strings;
    strings.Load(OSMSplit::FilePath(sFolder, OSMSplit::StringsFile));
    COSMDataFile ways;
    ways.Open(OSMSplit::FilePath(sFolder, OSMSplit::WaysData));
    std::vector<uint8_t> record;
    SOSMSplitObject way;

    REQUIRE(ways.ReadRecord(record));
    COSMSplitCodec::DecodeWay(record, strings, way);
    REQUIRE(way.nId == 1);
    REQUIRE(way.nFlags == OSMSplit::WayClosed);
    REQUIRE(way.lons == std::vector<int32_t>{140000000, 140200000, 140300000, 140000000});
    REQUIRE(way.lats == std::vector<int32_t>{500000000, 500200000, 500300000, 500000000});

    REQUIRE(ways.ReadRecord(record));
    COSMSplitCodec::DecodeWay(record, strings, way);
    REQUIRE(way.nId == 2);
    REQUIRE(way.nFlags == OSMSplit::WayMissingNodes);    // 999999999 is after the end of the node file
    REQUIRE(way.lons == std::vector<int32_t>{140100000, 140000000});

    REQUIRE(ways.ReadRecord(record));
    COSMSplitCodec::DecodeWay(record, strings, way);
    REQUIRE(way.nId == 3);
    REQUIRE(way.lats == std::vector<int32_t>{500300000, 500100000});
    REQUIRE_FALSE(ways.ReadRecord(record));

    // the export: the multipolygon from the way with the coordinates, the point
    GeoDatabase::IDatabaseWorkspacePtr ptrDb = CreateDatabase("osm_split_flat_export");
    convertor.Convert(convertor.Analyze(OSMSplit::FilePath(sFolder, OSMSplit::MetaFile), IProgressUpdaterPtr(), Display::ITrackCancelPtr()),
                      CLayerColumnExcludeList(), Cartography::IMapPtr(), ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
    REQUIRE(RowCount(ptrDb, "osm_roads") == 2);
    REQUIRE(RowCount(ptrDb, "osm_landuse") == 1);
    REQUIRE(RowCount(ptrDb, "osm_pois") == 1);
}
