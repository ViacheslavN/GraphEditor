#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../OSMFlatNodeStore.h"
#include "../OSMWayStore.h"
#include "../OSMTempFile.h"
#include "../OSMConvertor.h"
#include "../../../../CommonLib/filesystem/FileMapping.h"
#include "../../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../../CartographyLib/Map.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
    std::filesystem::path StoresDir()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineOSMTests";
        std::filesystem::create_directories(dir);
        return dir;
    }

    double TestLon(int64_t nId) { return (nId % 3600) / 10.0 - 180; }
    double TestLat(int64_t nId) { return (nId % 1800) / 10.0 - 90; }
}

TEST_CASE("File mapping", "[osm][stores]")
{
    std::string sPath = (StoresDir() / "mapping.bin").u8string();
    CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFileA(sPath.c_str(), CommonLib::file::ofmCreateAlways,
            CommonLib::file::aeReadWrite, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
    CommonLib::file::CFileMapping::SetSparse(*ptrFile);
    size_t nGranularity = CommonLib::file::CFileMapping::Granularity();
    REQUIRE(nGranularity >= 4096);

    uint64_t nSize = nGranularity * 3;
    CommonLib::file::CFileMapping::SetFileSize(*ptrFile, nSize);
    REQUIRE(ptrFile->GetFileSize() == nSize);
    {
        CommonLib::file::CFileMapping mapping;
        mapping.Map(*ptrFile, nGranularity, (size_t)nGranularity * 2, true);
        REQUIRE(mapping.IsMapped());
        REQUIRE(mapping.Data()[0] == 0);           // the extended part reads as zeros
        mapping.Data()[5] = 42;
        mapping.Flush();
    }
    byte_t nValue = 0;
    ptrFile->SetFilePos64(nGranularity + 5, CommonLib::soFromBegin);
    REQUIRE(ptrFile->Read(&nValue, 1) == 1);
    REQUIRE(nValue == 42);

    CommonLib::file::CFileMapping mapping;
    REQUIRE_THROWS(mapping.Map(*ptrFile, 1, 10, false));     // not aligned
    ptrFile->CloseFile();
    std::filesystem::remove(std::filesystem::u8path(sPath));
}

TEST_CASE("OSM flat node store", "[osm][stores]")
{
    std::string sFile;
    {
        COSMFlatNodeStore store(StoresDir().u8string());
        sFile = store.FilePath();

        // a sorted file with gaps (small - in the buffer, big - holes), across the segment border, an unsorted tail
        std::vector<int64_t> ids;
        for(int64_t i = 1; i < 300000; i += 3)
            ids.push_back(i);
        for(int64_t i = 0; i < 1000; ++i)
            ids.push_back(5000000 + i * 10000);
        int64_t nBorder = (int64_t)1 << COSMFlatNodeStore::SegmentBits;
        for(int64_t i = nBorder - 50; i < nBorder + 50; ++i)
            ids.push_back(i);
        ids.push_back(17);          // back: a new run
        ids.push_back(-5);          // a negative id

        for(int64_t nId : ids)
            store.Add(nId, TestLon(std::abs(nId)), TestLat(std::abs(nId)));
        store.Add(4, 1.5, 2.5);     // a duplicate replaces
        store.Finish();
        REQUIRE(store.IsFinished());
        REQUIRE(store.FileSize() == (uint64_t)(nBorder + 50) * 8);

        double dLon = 0, dLat = 0;
        for(int64_t nId : ids)
        {
            if(nId == 4)
                continue;
            REQUIRE(store.Get(nId, dLon, dLat));
            REQUIRE(dLon == Catch::Approx(TestLon(std::abs(nId))).margin(1e-7));
            REQUIRE(dLat == Catch::Approx(TestLat(std::abs(nId))).margin(1e-7));
        }
        REQUIRE(store.Get(4, dLon, dLat));
        REQUIRE(dLon == Catch::Approx(1.5));
        REQUIRE(dLat == Catch::Approx(2.5));

        // the extreme coordinates
        REQUIRE_FALSE(store.Get(2, dLon, dLat));                // in a gap
        REQUIRE_FALSE(store.Get(4000000, dLon, dLat));          // in a hole
        REQUIRE_FALSE(store.Get(nBorder + 50, dLon, dLat));     // after the end
        REQUIRE_FALSE(store.Get(-6, dLon, dLat));
        REQUIRE_THROWS(store.Add(1000, 0, 0));
        REQUIRE(std::filesystem::exists(std::filesystem::u8path(sFile)));
    }
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::u8path(sFile)));

    COSMFlatNodeStore poles(StoresDir().u8string());
    poles.Add(1, -180, -90);
    poles.Add(2, 180, 90);
    poles.Finish();
    double dLon = 0, dLat = 0;
    REQUIRE(poles.Get(1, dLon, dLat));
    REQUIRE(dLon == -180.);
    REQUIRE(dLat == -90.);
    REQUIRE(poles.Get(2, dLon, dLat));
    REQUIRE(dLon == 180.);
    REQUIRE(dLat == 90.);
}

TEST_CASE("OSM node store type", "[osm][stores]")
{
    const uint64_t GB = 1024ull * 1024 * 1024;
    REQUIRE(IOSMNodeStore::ResolveType(OSMNodeStoreAuto, "a.osm.pbf", 1 * GB) == OSMNodeStoreSorted);
    REQUIRE(IOSMNodeStore::ResolveType(OSMNodeStoreAuto, "a.osm.pbf", 90 * GB) == (sizeof(void*) == 8 ? OSMNodeStoreFlat : OSMNodeStoreSorted));
    REQUIRE(IOSMNodeStore::ResolveType(OSMNodeStoreAuto, "a.osm", 30 * GB) == OSMNodeStoreSorted);
    REQUIRE(IOSMNodeStore::ResolveType(OSMNodeStoreFlat, "a.osm", 1) == OSMNodeStoreFlat);
    REQUIRE(IOSMNodeStore::ResolveType(OSMNodeStoreSorted, "a.pbf", 90 * GB) == OSMNodeStoreSorted);
    REQUIRE(dynamic_cast<COSMFlatNodeStore*>(IOSMNodeStore::Create(OSMNodeStoreFlat, "a.pbf", 1, StoresDir().u8string(), 1024 * 1024).get()) != nullptr);
}

TEST_CASE("OSM temporary file", "[osm][stores]")
{
    COSMTempFile file(StoresDir().u8string(), "_test.bin", 4096);
    file.Rewind();
    int32_t nValue = 0;
    REQUIRE_FALSE(file.ReadValue(nValue));      // empty, not created

    std::vector<int64_t> big(5000);
    for(size_t i = 0; i < big.size(); ++i)
        big[i] = (int64_t)i * 3;
    for(int i = 0; i < 2000; ++i)
    {
        file.WriteValue((int32_t)i);
        file.WriteString(std::string(i % 7, 'a' + i % 26));
    }
    file.Write(big.data(), big.size() * sizeof(int64_t));    // bigger than the buffer: directly
    file.WriteString("end");
    std::string sPath = file.Path();
    REQUIRE(std::filesystem::exists(std::filesystem::u8path(sPath)));

    file.Rewind();
    for(int i = 0; i < 2000; ++i)
    {
        REQUIRE(file.ReadValue(nValue));
        REQUIRE(nValue == i);
        std::string s;
        REQUIRE(file.ReadString(s));
        REQUIRE(s == std::string(i % 7, 'a' + i % 26));
    }
    std::vector<int64_t> read(big.size());
    REQUIRE(file.Read(read.data(), read.size() * sizeof(int64_t)));
    REQUIRE(read == big);
    std::string sEnd;
    REQUIRE(file.ReadString(sEnd));
    REQUIRE(sEnd == "end");
    REQUIRE_FALSE(file.ReadValue(nValue));

    int64_t nAt = 0;
    file.ReadAt(file.Size() - 7 - 8, &nAt, 8);
    REQUIRE(nAt == big.back());
    REQUIRE_THROWS(file.ReadAt(file.Size() - 2, &nAt, 8));
}

TEST_CASE("OSM id set", "[osm][stores]")
{
    COSMIdSet ids;
    REQUIRE(ids.IsEmpty());
    for(int64_t nId : {5ll, 65535ll, 65536ll, 12000000000ll, -3ll, 5ll})
        ids.Add(nId);
    REQUIRE(ids.Count() == 5);
    REQUIRE(ids.Contains(5));
    REQUIRE(ids.Contains(65535));
    REQUIRE(ids.Contains(65536));
    REQUIRE(ids.Contains(12000000000ll));
    REQUIRE(ids.Contains(-3));
    REQUIRE_FALSE(ids.Contains(6));
    REQUIRE_FALSE(ids.Contains(-4));
    REQUIRE_FALSE(ids.Contains(13000000000ll));
}

TEST_CASE("OSM way store", "[osm][stores]")
{
    for(bool bSorted : {true, false})
    {
        std::vector<int64_t> ids;
        for(int64_t i = 0; i < 5000; ++i)
            ids.push_back(i * 5 + 3);
        if(!bSorted)
            std::shuffle(ids.begin(), ids.end(), std::mt19937(7));

        COSMWayStore store(StoresDir().u8string(), 1);     // the smallest cache: the blocks are read again
        for(int64_t nId : ids)
        {
            TOSMPoints points((size_t)(nId % 13) + 2);
            for(size_t i = 0; i < points.size(); ++i)
                points[i] = CommonLib::GisXYPoint{(double)nId, (double)i};
            store.Add(nId, points);
        }
        store.Add(ids[10], TOSMPoints(1));     // a duplicate (sorted: the same id again) is ignored
        REQUIRE_THROWS(store.Get(ids[0], *std::make_unique<TOSMPoints>()));
        store.Finish();
        REQUIRE(store.Count() == ids.size());

        std::vector<int64_t> lookups = ids;
        std::shuffle(lookups.begin(), lookups.end(), std::mt19937(11));
        TOSMPoints points;
        for(int64_t nId : lookups)
        {
            REQUIRE(store.Get(nId, points));
            REQUIRE(points.size() == (size_t)(nId % 13) + 2);
            REQUIRE(points.front().x == (double)nId);
            REQUIRE(points.back().y == (double)(points.size() - 1));
        }
        REQUIRE_FALSE(store.Get(4, points));
        REQUIRE_FALSE(store.Get(-1, points));
        REQUIRE_FALSE(store.Get(1000000, points));
    }
}

TEST_CASE("OSM conversion: the flat node store", "[osm][stores][convert]")
{
    const char* pszOSM = R"(<?xml version="1.0" encoding="UTF-8"?>
<osm version="0.6" generator="test">
  <node id="21" lat="50.00" lon="14.00"/>
  <node id="22" lat="50.00" lon="14.10"/>
  <node id="23" lat="50.10" lon="14.10"/>
  <node id="24" lat="50.10" lon="14.00"/>
  <node id="31" lat="50.04" lon="14.04"/>
  <node id="32" lat="50.04" lon="14.06"/>
  <node id="33" lat="50.06" lon="14.06"/>
  <node id="34" lat="50.06" lon="14.04"/>
  <way id="200"><nd ref="21"/><nd ref="22"/><nd ref="23"/></way>
  <way id="201"><nd ref="23"/><nd ref="24"/><nd ref="21"/></way>
  <way id="202"><nd ref="31"/><nd ref="32"/><nd ref="33"/><nd ref="34"/><nd ref="31"/></way>
  <way id="203"><nd ref="21"/><nd ref="22"/><nd ref="99"/><tag k="highway" v="primary"/></way>
  <relation id="2000"><member type="way" ref="200" role="outer"/><member type="way" ref="202" role="inner"/><member type="way" ref="201" role="outer"/><tag k="type" v="multipolygon"/><tag k="landuse" v="forest"/></relation>
  <relation id="2003"><member type="way" ref="999" role="outer"/><tag k="type" v="multipolygon"/><tag k="natural" v="water"/></relation>
</osm>
)";
    std::filesystem::path path = StoresDir() / "flat.osm";
    {
        std::ofstream file(path, std::ios::binary);
        file << pszOSM;
    }

    for(eOSMNodeStore type : {OSMNodeStoreSorted, OSMNodeStoreFlat})
    {
        // a new folder with a non-ASCII name: created by the conversion, empty after it (the temporary files are deleted)
        std::filesystem::path tempDir = StoresDir() / std::filesystem::u8path(u8"temp \u0444\u0430\u0439\u043B\u044B") / std::to_string((int)type);
        std::filesystem::remove_all(tempDir);
        SOSMConvertSettings settings;
        settings.sTempDir = tempDir.u8string();
        settings.nodeStore = type;
        settings.nThreads = 4;
        std::string sDb = (StoresDir() / ("flat_" + std::to_string((int)type) + ".sqlite")).u8string();
        for(const char* ext : {"", "-wal", "-shm", "-journal"})
            std::filesystem::remove(std::filesystem::u8path(sDb + ext));
        GeoDatabase::IDatabaseWorkspacePtr ptrDb = GeoDatabase::CSQLiteWorkspace::Create("flat", sDb.c_str(), CommonLib::CGuid::CreateNew());
        COSMConvertor convertor(settings);
        convertor.ConvertFromXML(path.u8string(), Cartography::IMapPtr(), ptrDb, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

        GeoDatabase::ISelectCursorPtr ptrCursor = ptrDb->GetTable("osm_landuse")->Select("SELECT osm_id, Shape FROM osm_landuse");
        REQUIRE(ptrCursor->Next());
        REQUIRE(ptrCursor->ReadInt64(0) == 2000);
        CommonLib::IGeoShapePtr ptrShape = ptrCursor->ReadShape(1);
        REQUIRE(ptrShape->GetPartCount() == 2);
        REQUIRE(ptrShape->GetPointCnt() == 10);
        REQUIRE_FALSE(ptrCursor->Next());

        REQUIRE(std::filesystem::is_directory(tempDir));
        REQUIRE(std::filesystem::is_empty(tempDir));

        // the missing node 99 is skipped
        ptrCursor = ptrDb->GetTable("osm_roads")->Select("SELECT Shape FROM osm_roads");
        REQUIRE(ptrCursor->Next());
        REQUIRE(ptrCursor->ReadShape(0)->GetPointCnt() == 2);
    }
}
