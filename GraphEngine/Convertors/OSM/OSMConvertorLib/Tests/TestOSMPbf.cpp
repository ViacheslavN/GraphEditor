// COSMPbfReader: a small .osm.pbf written by the test (a raw and a zlib block, dense and plain nodes,
// a way and a relation) is read with several threads, by object types, from the index; errors and the stop
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "../OSMPbfReader.h"
#include "../OSMConvertor.h"
#include "../../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../../CartographyLib/Map.h"
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
    // a minimal protobuf writer
    class CPbfWriter
    {
    public:
        void Varint(uint64_t v)
        {
            while(v >= 0x80)
            {
                s += (char)(v | 0x80);
                v >>= 7;
            }
            s += (char)v;
        }
        void Key(uint32_t nField, uint32_t nWire) { Varint((nField << 3) | nWire); }
        void FieldVarint(uint32_t nField, uint64_t v) { Key(nField, 0); Varint(v); }
        void FieldSVarint(uint32_t nField, int64_t v) { FieldVarint(nField, ((uint64_t)v << 1) ^ (uint64_t)(v >> 63)); }
        void FieldBytes(uint32_t nField, const std::string& bytes) { Key(nField, 2); Varint(bytes.size()); s += bytes; }
        void FieldPacked(uint32_t nField, const std::vector<uint64_t>& values)
        {
            CPbfWriter packed;
            for(uint64_t v : values)
                packed.Varint(v);
            FieldBytes(nField, packed.s);
        }
        void FieldPackedSigned(uint32_t nField, const std::vector<int64_t>& values)   // delta + zigzag
        {
            CPbfWriter packed;
            int64_t nPrev = 0;
            for(int64_t v : values)
            {
                int64_t d = v - nPrev;
                nPrev = v;
                packed.Varint(((uint64_t)d << 1) ^ (uint64_t)(d >> 63));
            }
            FieldBytes(nField, packed.s);
        }

        std::string s;
    };

    std::string StringTable(const std::vector<std::string>& strings)
    {
        CPbfWriter table;
        for(const std::string& str : strings)
            table.FieldBytes(1, str);
        return table.s;
    }

    void WriteBlob(std::ofstream& file, const std::string& sType, const std::string& sData, bool bZlib)
    {
        CPbfWriter blob;
        if(bZlib)
        {
            uLongf nSize = compressBound((uLong)sData.size());
            std::string compressed(nSize, '\0');
            REQUIRE(compress((Bytef*)&compressed[0], &nSize, (const Bytef*)sData.data(), (uLong)sData.size()) == Z_OK);
            compressed.resize(nSize);
            blob.FieldVarint(2, sData.size());
            blob.FieldBytes(3, compressed);
        }
        else
            blob.FieldBytes(1, sData);

        CPbfWriter header;
        header.FieldBytes(1, sType);
        header.FieldVarint(3, blob.s.size());
        uint32_t n = (uint32_t)header.s.size();
        char len[4] = {(char)(n >> 24), (char)(n >> 16), (char)(n >> 8), (char)n};
        file.write(len, 4);
        file.write(header.s.data(), header.s.size());
        file.write(blob.s.data(), blob.s.size());
    }

    // 3 dense nodes + 1 plain node (raw block), a highway way + a multipolygon relation (zlib block)
    std::string WriteTestPbf()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineOSMTests";
        std::filesystem::create_directories(dir);
        std::string sPath = (dir / "reader_test.osm.pbf").u8string();
        std::ofstream file(std::filesystem::u8path(sPath), std::ios::binary);

        CPbfWriter headerBlock;
        headerBlock.FieldBytes(4, "OsmSchema-V0.6");
        headerBlock.FieldBytes(4, "DenseNodes");
        WriteBlob(file, "OSMHeader", headerBlock.s, false);

        {
            CPbfWriter dense;
            dense.FieldPackedSigned(1, {1, 2, 3});
            dense.FieldPackedSigned(8, {500000000, 500000000, 501000000});   // latitude in 100 nanodegrees (granularity 100)
            dense.FieldPackedSigned(9, {140000000, 141000000, 141000000});
            dense.FieldPacked(10, {1, 2, 3, 4, 0, 0, 0});                     // node 1: amenity=cafe, name=Cafe
            CPbfWriter group1;
            group1.FieldBytes(2, dense.s);

            CPbfWriter node;
            node.FieldSVarint(1, 4);
            node.FieldPacked(2, {5});
            node.FieldPacked(3, {6});
            node.FieldSVarint(8, -100000000);   // -10 degrees
            node.FieldSVarint(9, 200000000);    // 20 degrees
            CPbfWriter group2;
            group2.FieldBytes(1, node.s);

            CPbfWriter block;
            block.FieldBytes(1, StringTable({"", "amenity", "cafe", "name", "Cafe", "place", "town"}));
            block.FieldBytes(2, group1.s);
            block.FieldBytes(2, group2.s);
            block.FieldVarint(17, 100);
            WriteBlob(file, "OSMData", block.s, false);
        }
        {
            CPbfWriter way;
            way.FieldVarint(1, 10);
            way.FieldPacked(2, {1});
            way.FieldPacked(3, {2});
            way.FieldPackedSigned(8, {1, 2, 3});
            CPbfWriter group1;
            group1.FieldBytes(3, way.s);

            CPbfWriter relation;
            relation.FieldVarint(1, 20);
            relation.FieldPacked(2, {3, 1});
            relation.FieldPacked(3, {4, 6});
            relation.FieldPacked(8, {5, 0});
            relation.FieldPackedSigned(9, {10, 4});
            relation.FieldPacked(10, {1, 0});
            CPbfWriter group2;
            group2.FieldBytes(4, relation.s);

            CPbfWriter block;
            block.FieldBytes(1, StringTable({"", "highway", "primary", "type", "multipolygon", "outer", "forest"}));
            block.FieldBytes(2, group1.s);
            block.FieldBytes(2, group2.s);
            WriteBlob(file, "OSMData", block.s, true);
        }
        return sPath;
    }

    struct SCollector : public IOSMReadHandler
    {
        std::vector<std::string> log;
        int nStopAfter = -1;

        bool Stop() { return nStopAfter < 0 || (int)log.size() < nStopAfter; }

        static std::string Tags(const readosm_tag* pTags, int nCount)
        {
            std::string s;
            for(int i = 0; i < nCount; ++i)
                s += std::string(" ") + pTags[i].key + "=" + pTags[i].value;
            return s;
        }

        virtual bool OnNode(const readosm_node& node)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "n%lld %.7f %.7f", node.id, node.latitude, node.longitude);
            log.push_back(buf + Tags(node.tags, node.tag_count));
            return Stop();
        }

        virtual bool OnWay(const readosm_way& way)
        {
            std::string s = "w" + std::to_string(way.id);
            for(int i = 0; i < way.node_ref_count; ++i)
                s += " " + std::to_string(way.node_refs[i]);
            log.push_back(s + Tags(way.tags, way.tag_count));
            return Stop();
        }

        virtual bool OnRelation(const readosm_relation& relation)
        {
            std::string s = "r" + std::to_string(relation.id);
            for(int i = 0; i < relation.member_count; ++i)
                s += " " + std::string(relation.members[i].member_type == READOSM_MEMBER_WAY ? "w" : "n") +
                     std::to_string(relation.members[i].id) + ":" + relation.members[i].role;
            log.push_back(s + Tags(relation.tags, relation.tag_count));
            return Stop();
        }
    };
}

TEST_CASE("OSM PBF reader", "[osm][pbf]")
{
    COSMPbfReader::ClearIndex();
    std::string sPath = WriteTestPbf();
    const std::vector<std::string> all = {
            "n1 50.0000000 14.0000000 amenity=cafe name=Cafe",
            "n2 50.0000000 14.1000000",
            "n3 50.1000000 14.1000000",
            "n4 -10.0000000 20.0000000 place=town",
            "w10 1 2 3 highway=primary",
            "r20 w10:outer n4: type=multipolygon highway=forest"};

    for(int nThreads : {1, 3})
    {
        SCollector collector;
        REQUIRE(COSMReader::Read(sPath, collector, COSMReader::ReadAll, nThreads));
        REQUIRE(collector.log == all);
    }
    REQUIRE(COSMPbfReader::GetIndexedBlockCount(sPath) == 3);

    // by the types, from the index
    SCollector ways;
    REQUIRE(COSMReader::Read(sPath, ways, COSMReader::ReadWays, 2));
    REQUIRE(ways.log == std::vector<std::string>{all[4]});
    SCollector nodesRelations;
    REQUIRE(COSMReader::Read(sPath, nodesRelations, COSMReader::ReadNodes | COSMReader::ReadRelations, 2));
    REQUIRE(nodesRelations.log == std::vector<std::string>{all[0], all[1], all[2], all[3], all[5]});

    // the progress: the whole file, then only the block with the ways (from the index)
    struct SProgress : public SCollector
    {
        std::vector<std::pair<uint64_t, uint64_t> > progress;
        virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
        {
            progress.push_back(std::make_pair(nBytesRead, nBytesTotal));
            return true;
        }
    };
    {
        uint64_t nFileSize = std::filesystem::file_size(std::filesystem::u8path(sPath));
        SProgress all;
        COSMPbfReader::ClearIndex();
        REQUIRE(COSMReader::Read(sPath, all, COSMReader::ReadAll, 2));
        REQUIRE(all.progress.size() == 3);
        REQUIRE(all.progress.back() == std::make_pair(nFileSize, nFileSize));
        REQUIRE(all.progress[0].first < all.progress[1].first);

        SProgress waysOnly;
        REQUIRE(COSMReader::Read(sPath, waysOnly, COSMReader::ReadWays, 2));
        REQUIRE(waysOnly.progress.size() == 1);
        REQUIRE(waysOnly.progress[0].first == waysOnly.progress[0].second);
        REQUIRE(waysOnly.progress[0].second < nFileSize);

        // the progress stops the reading
        struct SStop : public SProgress
        {
            virtual bool OnProgress(uint64_t, uint64_t) { return false; }
        } stop;
        REQUIRE_FALSE(COSMReader::Read(sPath, stop, COSMReader::ReadAll, 2));
    }

    // the handler stops the reading
    SCollector stopped;
    stopped.nStopAfter = 2;
    REQUIRE_FALSE(COSMReader::Read(sPath, stopped, COSMReader::ReadAll, 2));
    REQUIRE(stopped.log.size() == 2);

    // an exception of the handler goes through
    struct SThrowing : public SCollector
    {
        virtual bool OnWay(const readosm_way&) { throw CommonLib::CExcBase("handler error"); }
    } throwing;
    REQUIRE_THROWS_WITH(COSMReader::Read(sPath, throwing, COSMReader::ReadAll, 2), Catch::Matchers::ContainsSubstring("handler error"));

    // a cut file
    std::string sCut = sPath + ".cut.osm.pbf";
    {
        std::ifstream in(std::filesystem::u8path(sPath), std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::ofstream out(std::filesystem::u8path(sCut), std::ios::binary);
        out.write(data.data(), data.size() - 10);
    }
    SCollector cut;
    REQUIRE_THROWS_WITH(COSMReader::Read(sCut, cut, COSMReader::ReadAll, 2), Catch::Matchers::ContainsSubstring("unexpected end"));

    // the conversion of the file
    std::filesystem::path dbPath = std::filesystem::temp_directory_path() / "GraphEngineOSMTests" / "reader_test.sqlite";
    for(const char* ext : {"", "-wal", "-shm", "-journal"})
        std::filesystem::remove(dbPath.string() + ext);
    GeoDatabase::IDatabaseWorkspacePtr ptrDb = GeoDatabase::CSQLiteWorkspace::Create("pbf", dbPath.string().c_str(), CommonLib::CGuid::CreateNew());
    COSMConvertor convertor;
    Cartography::IMapPtr ptrMap = std::make_shared<Cartography::CMap>();
    struct SUpdater : public IProgressUpdater
    {
        std::vector<SOSMProgress> log;
        virtual void UpdateStatusInfo(const SOSMProgress& progress) { log.push_back(progress); }
    };
    auto ptrUpdater = std::make_shared<SUpdater>();
    convertor.Convert(convertor.Analyze(sPath, ptrUpdater, Display::ITrackCancelPtr()), CLayerColumnExcludeList(), ptrMap, ptrDb, ptrUpdater, Display::ITrackCancelPtr());

    // Analyze: one pass over the file; the conversion: 2 passes, the second one reads only the ways
    // (the file is indexed already: the passes read only the blocks with the objects, the header block is skipped)
    uint64_t nFileSize = std::filesystem::file_size(std::filesystem::u8path(sPath));
    bool bMapRead = false, bPass1 = false, bPass2 = false;
    for(const SOSMProgress& progress : ptrUpdater->log)
    {
        REQUIRE(progress.nBytesRead <= progress.nBytesTotal);
        if(progress.stage == OSMStageAnalyze && progress.nBytesTotal > 0 && progress.nBytesRead == progress.nBytesTotal)
            bMapRead = progress.nPass == 1 && progress.nPassCount == 1;
        if(progress.stage == OSMStageNodes && progress.nBytesTotal > 0 && progress.nBytesRead == progress.nBytesTotal)
            bPass1 = progress.nPass == 1 && progress.nPassCount == 2 && progress.nFileSize == nFileSize;
        if(progress.stage == OSMStageWays && progress.nBytesTotal > 0 && progress.nBytesRead == progress.nBytesTotal)
            bPass2 = progress.nPass == 2 && progress.nBytesTotal < nFileSize;
    }
    REQUIRE(bMapRead);
    REQUIRE(bPass1);
    REQUIRE(bPass2);
    REQUIRE(ptrUpdater->log.back().sMessage == "Done");

    // the details: the finished stages of the conversion with the reading of the file in the passes
    const SOSMProgress& last = ptrUpdater->log.back();
    REQUIRE(last.vecItems.empty());
    bool bNodesStage = false, bWaysStage = false;
    for(const SOSMStageSummary& stage : last.vecStages)
    {
        if(stage.stage == OSMStageNodes || stage.stage == OSMStageWays)
        {
            REQUIRE_FALSE(stage.vecItems.empty());
            REQUIRE(stage.vecItems[0].sName == "Reading the file");
            REQUIRE(stage.vecItems[0].nBytes > 0);
            REQUIRE(stage.vecItems[0].nCount > 0);
        }
        bNodesStage = bNodesStage || stage.stage == OSMStageNodes;
        bWaysStage = bWaysStage || stage.stage == OSMStageWays;
    }
    REQUIRE(bNodesStage);
    REQUIRE(bWaysStage);
    REQUIRE(last.vecStages.back().stage == OSMStageFinish);
    REQUIRE(COSMConvertor::FormatReport(last).find("Reading the file") != std::string::npos);
    REQUIRE(ptrDb->GetTable("osm_roads").get() != nullptr);
    REQUIRE(ptrMap->GetLayers()->GetLayerCount() >= 2);   // roads, pois / places
}
