#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>

#include "../BPTree/OSMBPTreeConvertor.h"
#include "../../../../../GeometryCompression/CompressedShapeReader.h"
#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <iostream>
#include <cmath>

using namespace GraphEngine;
using namespace GraphEngine::Convertors;
using namespace GraphEngine::Convertors::bptree;

namespace
{
    std::filesystem::path BPTreeDir()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineOSMTests";
        std::filesystem::create_directories(dir);
        return dir;
    }

    // coordinates with jumps over the whole range (negative / positive, the antimeridian) and small steps
    SOSMPointE7 TestPoint(std::mt19937_64& rng, const SOSMPointE7& prev)
    {
        if (rng() % 50 == 0)
            return SOSMPointE7((int32_t)((int64_t)(rng() % 1800000001ull) - 900000000), (int32_t)((int64_t)(rng() % 3600000001ull) - 1800000000));
        int32_t lat = (std::min)(900000000, (std::max)(-900000000, prev.lat + (int32_t)(rng() % 20001) - 10000));
        int32_t lon = (std::min)(1800000000, (std::max)(-1800000000, prev.lon + (int32_t)(rng() % 20001) - 10000));
        return SOSMPointE7(lat, lon);
    }
}

TEST_CASE("BPTree node tree: insert / remove in random order (TPointCompressor statistics)", "[osm][bptree]")
{
    std::string sPath = (BPTreeDir() / "bptree_nodes_random.btdb").u8string();
    const uint32_t nPageSize = 8192;

    std::mt19937_64 rng(11);
    std::map<int64_t, SOSMPointE7> model;
    SOSMPointE7 prev(500000000, 140000000);
    while (model.size() < 60000)
    {
        int64_t nId = (int64_t)(rng() % 10000000);
        prev = TestPoint(rng, prev);
        model.emplace(nId, prev);
    }
    std::vector<int64_t> ids;
    for (auto& kv : model)
        ids.push_back(kv.first);
    std::shuffle(ids.begin(), ids.end(), rng);

    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));
    auto ptrStorage = std::make_shared<bptreedb::storage::CFileStorage>(ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr());
    ptrStorage->Open(sPath.c_str(), true, 0, nPageSize);
    auto ptrPager = std::make_shared<bptreedb::storage::CFilePager>(ptrAlloc, ptrStorage, 100);
    int64_t nInfo = -1;
    {
        TNodeTree tree(-1, ptrPager, ptrAlloc, 50, nPageSize);
        tree.InnitTree(CreateCompressParams(), false, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
        nInfo = tree.GetPageBTreeInfo();
        for (int64_t nId : ids)
            tree.insert(nId, model[nId]);
        for (size_t i = 0; i < ids.size() / 2; ++i)
        {
            REQUIRE(tree.remove(ids[i]));
            model.erase(ids[i]);
        }
        tree.Flush();
    }
    ptrPager->Flush();

    {
        TNodeTree tree(nInfo, ptrPager, ptrAlloc, 50, nPageSize);
        tree.CheckTreeStructure();
        auto mit = model.begin();
        size_t n = 0;
        for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++mit, ++n)
        {
            REQUIRE(mit != model.end());
            REQUIRE(it.Key() == mit->first);
            REQUIRE(it.Value() == mit->second);
        }
        REQUIRE(n == model.size());
    }
    ptrStorage->Close();
    std::filesystem::remove(std::filesystem::u8path(sPath));
}

TEST_CASE("BPTree node conversion of an .osm file", "[osm][bptree]")
{
    std::filesystem::path dir = BPTreeDir();
    std::string sOSMPath = (dir / "bptree_nodes.osm").u8string();
    std::string sNodePath = (dir / "bptree_nodes.osmnodes").u8string();

    // sorted nodes, then a few out of the order and a duplicate id
    std::mt19937_64 rng(5);
    std::map<int64_t, SOSMPointE7> model;
    std::vector<std::pair<int64_t, SOSMPointE7> > fileNodes;
    SOSMPointE7 prev(500000000, 140000000);
    int64_t nId = 1;
    for (int i = 0; i < 100000; ++i)
    {
        nId += 1 + (int64_t)(rng() % 4 == 0 ? rng() % 1000 : 0);
        prev = TestPoint(rng, prev);
        fileNodes.emplace_back(nId, prev);
        model[nId] = prev;
    }
    for (int i = 0; i < 100; ++i)
    {
        int64_t nUnsorted = (int64_t)(rng() % (uint64_t)nId);
        if (model.count(nUnsorted))
            continue;
        SOSMPointE7 point(-(int32_t)(rng() % 900000000), -(int32_t)(rng() % 1800000000));
        fileNodes.emplace_back(nUnsorted, point);
        model[nUnsorted] = point;
    }
    fileNodes.emplace_back(fileNodes.front().first, SOSMPointE7(1, 1));   // a duplicate: skipped

    {
        std::ofstream file(std::filesystem::u8path(sOSMPath), std::ios::binary);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"test\">\n";
        char szLine[256];
        for (auto& node : fileNodes)
        {
            snprintf(szLine, sizeof(szLine), "  <node id=\"%lld\" lat=\"%.7f\" lon=\"%.7f\"/>\n", (long long)node.first, node.second.Lat(), node.second.Lon());
            file << szLine;
        }
        file << "</osm>\n";
    }

    // one thread (BulkLoad), several threads (the leaves encoded in parallel)
    for (int nThreads : { 1, 4 })
    {
        SOSMBPTreeSettings settings;
        settings.nBatchNodes = 4096;
        settings.nQueueBatches = 2;
        settings.nBuildThreads = nThreads;
        settings.nChunkNodes = 5000;        // many chunks: the leaves of the threads are written in the order
        COSMBPTreeConvertor convertor(settings);
        SOSMBPTreeNodeResult result = convertor.ConvertNodes(sOSMPath, sNodePath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

        CHECK(result.nRead == fileNodes.size());
        CHECK(result.nDuplicates == 1);
        CHECK(result.nUnsorted == fileNodes.size() - 100000);
        REQUIRE(result.header.nNodeCount == model.size());
        CHECK(result.header.nMinId == model.begin()->first);
        CHECK(result.header.nMaxId == model.rbegin()->first);

        COSMBPTreeNodeFile nodeFile;
        nodeFile.Open(sNodePath);
        CHECK(nodeFile.Header().nNodeCount == model.size());
        nodeFile.Tree().CheckTreeStructure();

        auto mit = model.begin();
        size_t n = 0;
        for (auto it = nodeFile.Tree().begin(); !it.IsNull(); it.Next(), ++mit, ++n)
        {
            REQUIRE(mit != model.end());
            REQUIRE(it.Key() == mit->first);
            REQUIRE(it.Value() == mit->second);
        }
        REQUIRE(n == model.size());

        SOSMPointE7 point;
        REQUIRE(nodeFile.Get(model.begin()->first, point));
        CHECK(point == model.begin()->second);
        CHECK_FALSE(nodeFile.Get(nId + 1, point));
        nodeFile.Close();
    }

    std::filesystem::remove(std::filesystem::u8path(sOSMPath));
    std::filesystem::remove(std::filesystem::u8path(sNodePath));
}

namespace
{
    // cancels after so many checks
    class CCancelAfter : public Display::ITrackCancel
    {
    public:
        explicit CCancelAfter(int nChecks) : m_nChecks(nChecks) {}
        virtual void Cancel() override { m_nChecks = 0; }
        virtual bool Continue() override { return --m_nChecks > 0; }
        virtual void Reset() override {}
    private:
        int m_nChecks;
    };
}

TEST_CASE("BPTree node conversion is canceled", "[osm][bptree]")
{
    std::filesystem::path dir = BPTreeDir();
    std::string sOSMPath = (dir / "bptree_cancel.osm").u8string();
    std::string sNodePath = (dir / "bptree_cancel.osmnodes").u8string();
    {
        std::ofstream file(std::filesystem::u8path(sOSMPath), std::ios::binary);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"test\">\n";
        for (int i = 1; i <= 50000; ++i)
            file << "  <node id=\"" << i << "\" lat=\"50.0\" lon=\"14.0\"/>\n";
        file << "</osm>\n";
    }

    SOSMBPTreeSettings settings;
    settings.nBatchNodes = 1024;
    settings.nQueueBatches = 1;
    for (int nChecks : { 1, 3, 10 })
    {
        COSMBPTreeConvertor convertor(settings);
        bool bThrown = false;
        try
        {
            convertor.ConvertNodes(sOSMPath, sNodePath, IProgressUpdaterPtr(), std::make_shared<CCancelAfter>(nChecks));
        }
        catch (std::exception&)
        {
            bThrown = true;
        }
        CHECK(bThrown);
    }
    std::filesystem::remove(std::filesystem::u8path(sOSMPath));
    std::filesystem::remove(std::filesystem::u8path(sNodePath));
}

TEST_CASE("BPTree node tree: BulkLoad with the size estimate skipped (CompressRatio)", "[osm][bptree]")
{
    std::string sPath = (BPTreeDir() / "bptree_nodes_ratio.btdb").u8string();
    const uint32_t nPageSize = 8192;

    // random jumps compress worse than the ratio: the leaves overflow and must be split before they are encoded
    std::mt19937_64 rng(21);
    std::map<int64_t, SOSMPointE7> model;
    SOSMPointE7 prev(500000000, 140000000);
    int64_t nId = 0;
    while (model.size() < 300000)
    {
        nId += 1 + (int64_t)(rng() % 3);
        prev = model.size() % 100000 < 50000 ? TestPoint(rng, prev)
            : SOSMPointE7((int32_t)((int64_t)(rng() % 1800000001ull) - 900000000), (int32_t)((int64_t)(rng() % 3600000001ull) - 1800000000));
        model.emplace(nId, prev);
    }

    struct SSource
    {
        std::map<int64_t, SOSMPointE7>::const_iterator it, end;
        bool Next(int64_t& nKey, SOSMPointE7& point)
        {
            if (it == end)
                return false;
            nKey = it->first;
            point = it->second;
            ++it;
            return true;
        }
    };

    for (int32_t nRatio : { 0, 3, 20 })
    {
        CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));
        auto ptrStorage = std::make_shared<bptreedb::storage::CFileStorage>(ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr());
        ptrStorage->Open(sPath.c_str(), true, 0, nPageSize);
        auto ptrPager = std::make_shared<bptreedb::storage::CFilePager>(ptrAlloc, ptrStorage, 100);
        int64_t nInfo = -1;
        {
            TNodeTree tree(-1, ptrPager, ptrAlloc, 50, nPageSize, true);
            tree.InnitTree(CreateCompressParams(nRatio), true, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
            nInfo = tree.GetPageBTreeInfo();
            SSource source{ model.begin(), model.end() };
            REQUIRE(tree.BulkLoad(source) == model.size());
            tree.Flush();
        }
        ptrPager->Flush();
        {
            TNodeTree tree(nInfo, ptrPager, ptrAlloc, 50, nPageSize);
            tree.CheckTreeStructure();
            auto mit = model.begin();
            size_t n = 0;
            for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++mit, ++n)
            {
                REQUIRE(mit != model.end());
                REQUIRE(it.Key() == mit->first);
                REQUIRE(it.Value() == mit->second);
            }
            REQUIRE(n == model.size());
        }
        ptrStorage->Close();
    }
    std::filesystem::remove(std::filesystem::u8path(sPath));
}

TEST_CASE("BPTree tag tree of the nodes", "[osm][bptree][tags]")
{
    std::filesystem::path dir = BPTreeDir();
    std::string sOSMPath = (dir / "bptree_tags.osm").u8string();
    std::string sNodePath = (dir / "bptree_tags.osmnodes").u8string();
    std::string sTagPath = TagFilePath(sNodePath);
    CHECK(sTagPath == (dir / "bptree_tags.osmtags").u8string());

    typedef std::vector<std::pair<std::string, std::string> > TTags;
    std::map<int64_t, TTags> model;
    std::vector<int64_t> order;
    for (int64_t nId = 2; nId <= 60000; nId += 2)
    {
        order.push_back(nId);
        if (nId % 6 == 0)
            model[nId] = TTags{ { "amenity", "cafe" }, { "name", "Node " + std::to_string(nId) } };
    }
    // a node with tags over the maximum (cut at a tag boundary), tagged nodes out of the order
    TTags longTags;
    for (int i = 0; i < 400; ++i)
        longTags.emplace_back("name:l" + std::to_string(i), std::string(30, (char)('a' + i % 26)));
    model[30000] = longTags;
    order.push_back(7);
    model[7] = TTags{ { "highway", "crossing" } };
    order.push_back(1001);
    model[1001] = TTags{ { "shop", "bakery" }, { "opening_hours", "Mo-Fr 07:00-18:00" } };

    {
        std::ofstream file(std::filesystem::u8path(sOSMPath), std::ios::binary);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"test\">\n";
        for (int64_t nId : order)
        {
            file << "  <node id=\"" << nId << "\" lat=\"50." << nId % 1000 << "\" lon=\"14." << nId % 997 << "\"";
            auto it = model.find(nId);
            if (it == model.end())
            {
                file << "/>\n";
                continue;
            }
            file << ">";
            for (auto& tag : it->second)
                file << "<tag k=\"" << tag.first << "\" v=\"" << tag.second << "\"/>";
            file << "</node>\n";
        }
        file << "</osm>\n";
    }

    for (int nThreads : { 1, 4 })
    {
        SOSMBPTreeSettings settings;
        settings.nBuildThreads = nThreads;
        settings.nChunkNodes = 5000;
        settings.nTagBatchBytes = 4096;     // many batches
        settings.nTagQueueBatches = 2;
        COSMBPTreeConvertor convertor(settings);
        SOSMBPTreeNodeResult result = convertor.ConvertNodes(sOSMPath, sNodePath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

        REQUIRE(result.sTagFile == sTagPath);
        CHECK(result.tagHeader.nNodeCount == model.size());
        CHECK(result.tagHeader.nTruncated == 1);
        CHECK(result.nTagUnsorted == 2);
        CHECK(result.nTagDuplicates == 0);

        COSMBPTreeTagFile tagFile;
        tagFile.Open(sTagPath, settings.nTagPageSize);
        tagFile.Tree().CheckTreeStructure();
        CHECK(tagFile.Header().nNodeCount == model.size());

        size_t n = 0;
        TTags tags;
        for (auto it = tagFile.Tree().begin(); !it.IsNull(); it.Next(), ++n)
        {
            REQUIRE(model.count(it.Key()) == 1);
            const bptreedb::StringValue& value = it.Value();
            DecodeTags((const char*)value.CStr(), value.StrLen(), tags);
            const TTags& expected = model[it.Key()];
            if (it.Key() == 30000)
            {
                // the first tags that fit in the maximum
                REQUIRE(tags.size() < expected.size());
                REQUIRE(value.StrLen() <= settings.nTagMaxLen);
                REQUIRE(tags.size() > 100);
                for (size_t i = 0; i < tags.size(); ++i)
                    REQUIRE(tags[i] == expected[i]);
            }
            else
                REQUIRE(tags == expected);
        }
        REQUIRE(n == model.size());

        REQUIRE(tagFile.Get(1001, tags));
        CHECK(tags == model[1001]);
        CHECK_FALSE(tagFile.Get(4, tags));       // a node without tags
        CHECK_FALSE(tagFile.Get(3, tags));       // no such node
        tagFile.Close();
    }

    std::filesystem::remove(std::filesystem::u8path(sOSMPath));
    std::filesystem::remove(std::filesystem::u8path(sNodePath));
    std::filesystem::remove(std::filesystem::u8path(sTagPath));
}

TEST_CASE("BPTree ways: shapes and tags", "[osm][bptree][ways]")
{
    std::filesystem::path dir = BPTreeDir();
    std::string sOSMPath = (dir / "bptree_ways.osm").u8string();
    std::string sNodePath = (dir / "bptree_ways.osmnodes").u8string();

    // the nodes 1..20000 (every 50th is missing in the file)
    std::mt19937_64 rng(31);
    std::map<int64_t, SOSMPointE7> nodes;
    SOSMPointE7 prev(500000000, 140000000);
    for (int64_t nId = 1; nId <= 20000; ++nId)
    {
        prev = TestPoint(rng, prev);
        if (nId % 50 != 0)
            nodes[nId] = prev;
    }

    // the ways: random runs of nodes, closed ways, a long way (a shape in the stream pages), ways without enough nodes
    typedef std::vector<std::pair<std::string, std::string> > TTags;
    struct SWay
    {
        std::vector<int64_t> refs;
        TTags tags;
    };
    std::map<int64_t, SWay> ways;
    std::vector<int64_t> wayOrder;
    for (int64_t nWay = 1; nWay <= 3000; ++nWay)
    {
        SWay way;
        int64_t nStart = 1 + (int64_t)(rng() % 19000);
        size_t nCount = nWay == 1500 ? 1500 : 2 + rng() % 30;
        for (size_t i = 0; i < nCount; ++i)
            way.refs.push_back((std::min)((int64_t)20000, nStart + (int64_t)i * (1 + (int64_t)(rng() % 3))));
        if (nWay % 7 == 0)
            way.refs.push_back(way.refs.front());                      // closed
        if (nWay % 11 == 0)
            way.refs.insert(way.refs.begin() + 1, 900000 + nWay);       // a node which isn't in the file
        if (nWay % 13 == 0)
            way.tags = TTags{ { "highway", "residential" }, { "name", "Street " + std::to_string(nWay) } };
        ways[nWay] = way;
        wayOrder.push_back(nWay);
    }
    ways[2] = SWay{ { 50, 100 }, TTags{ { "barrier", "fence" } } };       // both nodes missing: no shape
    ways[3] = SWay{ { 5, 999999 }, TTags() };                             // one node: no shape
    wayOrder.erase(std::find(wayOrder.begin(), wayOrder.end(), 10));     // a way out of the id order (moved to the end)
    wayOrder.push_back(10);

    {
        std::ofstream file(std::filesystem::u8path(sOSMPath), std::ios::binary);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"test\">\n";
        char szLine[256];
        for (auto& node : nodes)
        {
            snprintf(szLine, sizeof(szLine), "  <node id=\"%lld\" lat=\"%.7f\" lon=\"%.7f\"/>\n", (long long)node.first, node.second.Lat(), node.second.Lon());
            file << szLine;
        }
        for (int64_t nWay : wayOrder)
        {
            const SWay& way = ways[nWay];
            file << "  <way id=\"" << nWay << "\">";
            for (int64_t nRef : way.refs)
                file << "<nd ref=\"" << nRef << "\"/>";
            for (auto& tag : way.tags)
                file << "<tag k=\"" << tag.first << "\" v=\"" << tag.second << "\"/>";
            file << "</way>\n";
        }
        file << "</osm>\n";
    }

    for (int nThreads : { 1, 4 })
    {
        SOSMBPTreeSettings settings;
        settings.nResolveThreads = nThreads;
        settings.nWayBatchRefs = 3000;          // many jobs: the shapes are written in the order of the jobs
        settings.nNodeCacheLeaves = 8;          // the shared leaf cache is trimmed (64 shards: 1 leaf a shard)
        settings.nTagBatchBytes = 2048;
        COSMBPTreeConvertor convertor(settings);
        SOSMBPTreeResult result = convertor.Convert(sOSMPath, sNodePath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());

        // the expected shapes: the nodes of the way that are in the file, at least 2
        uint64_t nWithShape = 0, nRefs = 0, nMissing = 0;
        for (auto& way : ways)
        {
            size_t nFound = 0;
            for (int64_t nRef : way.second.refs)
            {
                ++nRefs;
                if (nodes.count(nRef))
                    ++nFound;
                else
                    ++nMissing;
            }
            if (nFound >= 2)
                ++nWithShape;
        }
        const SOSMBPTreeWayHeader& h = result.ways.header;
        CHECK(result.ways.nRead == ways.size());
        CHECK(h.nWayCount == nWithShape);
        CHECK(h.nWaysWithoutShape == ways.size() - nWithShape);
        CHECK(h.nRefCount == nRefs);
        CHECK(h.nMissingRefs == nMissing);
        CHECK(result.ways.nUnsorted == 1);
        CHECK(h.nMinId == 1);
        CHECK(h.nMaxId == 3000);

        COSMBPTreeWayFile wayFile;
        wayFile.Open(result.ways.sWayFile, settings.nWayPageSize);
        wayFile.Tree().CheckTreeStructure();
        std::vector<SOSMPointE7> points;
        uint64_t nChecked = 0;
        for (auto& way : ways)
        {
            std::vector<SOSMPointE7> expected;
            for (int64_t nRef : way.second.refs)
            {
                auto it = nodes.find(nRef);
                if (it != nodes.end())
                    expected.push_back(it->second);
            }
            bool bShape = wayFile.GetPoints(way.first, points);
            REQUIRE(bShape == (expected.size() >= 2));
            if (bShape)
            {
                INFO("way " << way.first);
                REQUIRE(points == expected);
                ++nChecked;
            }
        }
        CHECK(nChecked == nWithShape);
        wayFile.Close();

        // the tags of the ways
        COSMBPTreeTagFile tagFile;
        tagFile.Open(result.ways.sTagFile, settings.nTagPageSize);
        TTags tags;
        uint64_t nTagged = 0;
        for (auto& way : ways)
        {
            bool bTags = tagFile.Get(way.first, tags);
            REQUIRE(bTags == !way.second.tags.empty());
            if (bTags)
            {
                REQUIRE(tags == way.second.tags);
                ++nTagged;
            }
        }
        CHECK(result.ways.tagHeader.nNodeCount == nTagged);
        tagFile.Close();
    }

    std::filesystem::remove(std::filesystem::u8path(sOSMPath));
    for (const std::string& sFile : ConversionFiles(sNodePath))
        std::filesystem::remove(std::filesystem::u8path(sFile));
}

namespace
{
    // the parts of a compressed shape (the points in 1e-7 degrees)
    void DecodeShapeParts(const std::vector<byte_t>& blob, CommonLib::eShapeType& type, std::vector<std::vector<SOSMPointE7> >& parts)
    {
        GeometryCompression::CCompressedShapeReader reader;
        reader.Open(blob.data(), blob.size());
        type = reader.GeneralType();
        parts.clear();
        std::vector<uint32_t> sizes;
        for (uint32_t i = 0; i < reader.PartCount(); ++i)
            sizes.push_back(reader.PartSize(i));
        if (sizes.empty())
            sizes.push_back(reader.PointCount());
        CommonLib::GisXYPoint pt;
        for (uint32_t nSize : sizes)
        {
            parts.emplace_back();
            for (uint32_t i = 0; i < nSize && reader.NextPoint(pt); ++i)
                parts.back().push_back(SOSMPointE7((int32_t)std::llround(pt.y * SOSMPointE7::Scale), (int32_t)std::llround(pt.x * SOSMPointE7::Scale)));
        }
    }

    double RingArea(const std::vector<SOSMPointE7>& ring)
    {
        double dArea = 0.;
        for (size_t i = 0; i + 1 < ring.size(); ++i)
            dArea += (double)ring[i].lon * ring[i + 1].lat - (double)ring[i + 1].lon * ring[i].lat;
        return dArea / 2.;
    }

    // the ring as a set of points (the start and the direction aside)
    std::set<std::pair<int32_t, int32_t> > RingPoints(const std::vector<SOSMPointE7>& ring)
    {
        std::set<std::pair<int32_t, int32_t> > points;
        for (const SOSMPointE7& point : ring)
            points.emplace(point.lat, point.lon);
        return points;
    }
}

TEST_CASE("BPTree relations: records, shapes and tags", "[osm][bptree][relations]")
{
    std::filesystem::path dir = BPTreeDir();
    std::string sOSMPath = (dir / "bptree_relations.osm").u8string();
    std::string sNodePath = (dir / "bptree_relations.osmnodes").u8string();

    // the nodes of a grid 0..20 x 0..20 (0.01 degree), then random nodes
    auto gridId = [](int x, int y) { return (int64_t)(1 + x * 21 + y); };
    auto gridPoint = [](int x, int y) { return SOSMPointE7(y * 100000 + 500000000, x * 100000 + 100000000); };
    std::map<int64_t, SOSMPointE7> nodes;
    for (int x = 0; x <= 20; ++x)
        for (int y = 0; y <= 20; ++y)
            nodes[gridId(x, y)] = gridPoint(x, y);
    std::mt19937_64 rng(77);
    SOSMPointE7 prev(510000000, 110000000);
    for (int64_t nId = 1000; nId < 6000; ++nId)
    {
        prev = TestPoint(rng, prev);
        nodes[nId] = prev;
    }

    // the ways
    std::map<int64_t, std::vector<int64_t> > ways;
    auto gridWay = [&](int64_t nId, std::vector<std::pair<int, int> > xy)
    {
        for (auto& p : xy)
            ways[nId].push_back(gridId(p.first, p.second));
    };
    gridWay(101, { { 0, 0 }, { 10, 0 }, { 10, 10 } });                     // the outer ring of 1: two open ways
    gridWay(102, { { 0, 0 }, { 0, 10 }, { 10, 10 } });
    gridWay(103, { { 2, 2 }, { 2, 4 }, { 4, 4 }, { 4, 2 }, { 2, 2 } });   // the hole of 1 (role "outer": the nesting decides)
    gridWay(104, { { 12, 0 }, { 15, 0 }, { 15, 3 }, { 12, 3 }, { 12, 0 } });   // relation 2: an island
    gridWay(105, { { 12, 12 }, { 18, 12 }, { 18, 18 }, { 12, 18 }, { 12, 12 } });   // relation 2: an island with a hole
    gridWay(106, { { 13, 13 }, { 13, 15 } });                              // the hole: three ways
    gridWay(107, { { 13, 15 }, { 15, 15 }, { 15, 13 } });
    gridWay(108, { { 13, 13 }, { 15, 13 } });
    for (int64_t nWay = 200; nWay < 2200; ++nWay)                          // the random ways
    {
        int64_t nStart = 1000 + (int64_t)(rng() % 4900);
        size_t nCount = 2 + rng() % 8;
        for (size_t i = 0; i < nCount; ++i)
            ways[nWay].push_back(nStart + (int64_t)i);
    }

    // the relations
    typedef std::vector<std::pair<std::string, std::string> > TTags;
    struct SRelation
    {
        std::vector<SRelationMember> members;
        TTags tags;
    };
    auto member = [](uint8_t type, int64_t nId, const char* pszRole) { SRelationMember m; m.type = type; m.id = nId; m.role = pszRole; return m; };
    std::map<int64_t, SRelation> relations;
    relations[1] = SRelation{ { member(RelationMemberWay, 101, "outer"), member(RelationMemberWay, 103, "outer"), member(RelationMemberWay, 102, "outer") },
                              TTags{ { "type", "multipolygon" }, { "landuse", "forest" } } };
    relations[2] = SRelation{ { member(RelationMemberWay, 104, "outer"), member(RelationMemberWay, 105, "outer"), member(RelationMemberWay, 107, "inner"),
                                member(RelationMemberWay, 106, "inner"), member(RelationMemberWay, 108, "inner") },
                              TTags{ { "type", "boundary" }, { "name", "Islands" } } };
    relations[3] = SRelation{ { member(RelationMemberWay, 101, ""), member(RelationMemberNode, gridId(1, 1), "stop"), member(RelationMemberWay, 99999, ""),
                                member(RelationMemberWay, 104, "") },
                              TTags{ { "type", "route" } } };
    relations[4] = SRelation{ { member(RelationMemberNode, gridId(5, 5), "a"), member(RelationMemberNode, 99999, "b"), member(RelationMemberNode, gridId(6, 6), "c") },
                              TTags() };
    relations[5] = SRelation{ { member(RelationMemberWay, 101, "outer") }, TTags{ { "type", "multipolygon" } } };   // not closed
    relations[6] = SRelation{ { member(RelationMemberRelation, 1, ""), member(RelationMemberRelation, 2, "") }, TTags{ { "type", "collection" } } };
    for (int64_t nRel = 1000; nRel < 3000; ++nRel)
    {
        SRelation relation;
        size_t nCount = 1 + rng() % 6;
        for (size_t i = 0; i < nCount; ++i)
        {
            if (rng() % 4 == 0)
                relation.members.push_back(member(RelationMemberNode, 1000 + (int64_t)(rng() % 5000), "stop"));
            else
                relation.members.push_back(member(RelationMemberWay, 200 + (int64_t)(rng() % 2000), i % 2 ? "forward" : ""));
        }
        if (nRel % 3 == 0)
            relation.tags = TTags{ { "type", "route" }, { "ref", std::to_string(nRel) } };
        relations[nRel] = relation;
    }
    std::vector<int64_t> relOrder;
    for (auto& relation : relations)
        if (relation.first != 1500)
            relOrder.push_back(relation.first);
    relOrder.push_back(1500);     // out of the id order

    {
        std::ofstream file(std::filesystem::u8path(sOSMPath), std::ios::binary);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<osm version=\"0.6\" generator=\"test\">\n";
        char szLine[256];
        for (auto& node : nodes)
        {
            snprintf(szLine, sizeof(szLine), "  <node id=\"%lld\" lat=\"%.7f\" lon=\"%.7f\"/>\n", (long long)node.first, node.second.Lat(), node.second.Lon());
            file << szLine;
        }
        for (auto& way : ways)
        {
            file << "  <way id=\"" << way.first << "\">";
            for (int64_t nRef : way.second)
                file << "<nd ref=\"" << nRef << "\"/>";
            file << "</way>\n";
        }
        const char* types[] = { "node", "way", "relation" };
        for (int64_t nRel : relOrder)
        {
            const SRelation& relation = relations[nRel];
            file << "  <relation id=\"" << nRel << "\">";
            for (const SRelationMember& m : relation.members)
                file << "<member type=\"" << types[m.type] << "\" ref=\"" << m.id << "\" role=\"" << m.role << "\"/>";
            for (auto& tag : relation.tags)
                file << "<tag k=\"" << tag.first << "\" v=\"" << tag.second << "\"/>";
            file << "</relation>\n";
        }
        file << "</osm>\n";
    }

    for (int nThreads : { 1, 4 })
    {
        SOSMBPTreeSettings settings;
        settings.nResolveThreads = nThreads;
        settings.nWayBatchRefs = 2000;
        settings.nRelBatchMembers = 100;        // many jobs
        settings.nRelNodeCacheMB = 1;
        settings.nRelWayCacheMB = 1;
        settings.nTagBatchBytes = 1024;
        COSMBPTreeConvertor convertor(settings);
        SOSMBPTreeResult result = convertor.Convert(sOSMPath, sNodePath, IProgressUpdaterPtr(), Display::ITrackCancelPtr());
        const SOSMBPTreeRelationResult& rels = result.relations;
        const SOSMBPTreeRelationHeader& h = rels.header;
        CHECK(rels.nRead == relations.size());
        CHECK(h.nRelationCount == relations.size());
        CHECK(rels.nUnsorted == 1);
        CHECK(h.nMinId == 1);
        CHECK(h.nMaxId == 2999);
        CHECK(h.nPolygons == 2);
        CHECK(h.nAreasWithoutRings == 1);
        CHECK(h.nUnclosedWays == 1);
        CHECK(h.nRelationMembers == 2);

        // the way shapes (the points of the way members)
        COSMBPTreeWayFile wayFile;
        wayFile.Open(result.ways.sWayFile, settings.nWayPageSize);

        COSMBPTreeRelationFile relFile;
        relFile.Open(rels.sRelationFile, settings.nRelPageSize);
        relFile.Tree().CheckTreeStructure();
        SRelationRecord record;
        CommonLib::eShapeType type = CommonLib::shape_type_null;
        std::vector<std::vector<SOSMPointE7> > parts;
        uint64_t nMembers = 0, nMissingNodes = 0, nMissingWays = 0, nWithShape = 0;
        for (auto& relation : relations)
        {
            INFO("relation " << relation.first);
            REQUIRE(relFile.GetRelation(relation.first, record));
            REQUIRE(record.members.size() == relation.second.members.size());
            for (size_t i = 0; i < record.members.size(); ++i)
            {
                const SRelationMember& m = relation.second.members[i];
                REQUIRE(record.members[i].type == m.type);
                REQUIRE(record.members[i].id == m.id);
                REQUIRE(record.members[i].role == m.role);
                ++nMembers;
                if (m.type == RelationMemberNode && !nodes.count(m.id))
                    ++nMissingNodes;
                if (m.type == RelationMemberWay && !ways.count(m.id))
                    ++nMissingWays;
            }
            if (!record.shape.empty())
                ++nWithShape;
            if (relation.first < 1000)
                continue;

            // a random relation: a polyline of its ways, otherwise a multipoint of its nodes
            std::vector<std::vector<SOSMPointE7> > expected;
            std::vector<SOSMPointE7> points;
            for (const SRelationMember& m : relation.second.members)
                if (m.type == RelationMemberWay && wayFile.GetPoints(m.id, points))
                    expected.push_back(points);
            CommonLib::eShapeType expectedType = CommonLib::shape_type_general_polyline;
            if (expected.empty())
            {
                expected.emplace_back();
                for (const SRelationMember& m : relation.second.members)
                    if (m.type == RelationMemberNode)
                        expected.back().push_back(nodes[m.id]);
                expectedType = CommonLib::shape_type_general_multipoint;
            }
            REQUIRE(!record.shape.empty());
            DecodeShapeParts(record.shape, type, parts);
            REQUIRE(type == expectedType);
            REQUIRE(parts == expected);
        }
        CHECK(h.nMemberCount == nMembers);
        CHECK(h.nMissingNodes == nMissingNodes);
        CHECK(h.nMissingWays == nMissingWays);
        CHECK(h.nWithShape == nWithShape);

        // 1: the outer square (two ways joined, one reversed) clockwise, the hole counterclockwise
        REQUIRE(relFile.GetRelation(1, record));
        DecodeShapeParts(record.shape, type, parts);
        REQUIRE(type == CommonLib::shape_type_general_polygon);
        REQUIRE(parts.size() == 2);
        CHECK(RingArea(parts[0]) < 0.);
        CHECK(RingArea(parts[1]) > 0.);
        CHECK(RingPoints(parts[0]) == RingPoints({ gridPoint(0, 0), gridPoint(10, 0), gridPoint(10, 10), gridPoint(0, 10) }));
        CHECK(RingPoints(parts[1]) == RingPoints({ gridPoint(2, 2), gridPoint(2, 4), gridPoint(4, 4), gridPoint(4, 2) }));
        CHECK(parts[0].size() == 5);
        CHECK(parts[0].front() == parts[0].back());

        // 2: the big island, its hole of three ways, the small island
        REQUIRE(relFile.GetRelation(2, record));
        DecodeShapeParts(record.shape, type, parts);
        REQUIRE(type == CommonLib::shape_type_general_polygon);
        REQUIRE(parts.size() == 3);
        CHECK(RingPoints(parts[0]) == RingPoints({ gridPoint(12, 12), gridPoint(18, 12), gridPoint(18, 18), gridPoint(12, 18) }));
        CHECK(RingPoints(parts[1]) == RingPoints({ gridPoint(13, 13), gridPoint(13, 15), gridPoint(15, 15), gridPoint(15, 13) }));
        CHECK(RingPoints(parts[2]) == RingPoints({ gridPoint(12, 0), gridPoint(15, 0), gridPoint(15, 3), gridPoint(12, 3) }));
        CHECK(RingArea(parts[0]) < 0.);
        CHECK(RingArea(parts[1]) > 0.);
        CHECK(RingArea(parts[2]) < 0.);

        // 3: a route: the ways 101 and 104 (99999 is missing), the node isn't in the shape
        REQUIRE(relFile.GetRelation(3, record));
        DecodeShapeParts(record.shape, type, parts);
        CHECK(type == CommonLib::shape_type_general_polyline);
        CHECK(parts.size() == 2);
        // 4: the nodes
        REQUIRE(relFile.GetRelation(4, record));
        DecodeShapeParts(record.shape, type, parts);
        CHECK(type == CommonLib::shape_type_general_multipoint);
        std::vector<std::vector<SOSMPointE7> > expectedNodes(1, std::vector<SOSMPointE7>{ gridPoint(5, 5), gridPoint(6, 6) });
        CHECK(parts == expectedNodes);
        // 5: not closed, 6: relations only - no shape
        REQUIRE(relFile.GetRelation(5, record));
        CHECK(record.shape.empty());
        REQUIRE(relFile.GetRelation(6, record));
        CHECK(record.shape.empty());
        CHECK(!relFile.GetRelation(7, record));
        relFile.Close();
        wayFile.Close();

        // the tags of the relations
        COSMBPTreeTagFile tagFile;
        tagFile.Open(rels.sTagFile, settings.nTagPageSize);
        TTags tags;
        uint64_t nTagged = 0;
        for (auto& relation : relations)
        {
            bool bTags = tagFile.Get(relation.first, tags);
            REQUIRE(bTags == !relation.second.tags.empty());
            if (bTags)
            {
                REQUIRE(tags == relation.second.tags);
                ++nTagged;
            }
        }
        CHECK(rels.tagHeader.nNodeCount == nTagged);
        tagFile.Close();
    }

    std::filesystem::remove(std::filesystem::u8path(sOSMPath));
    for (const std::string& sFile : ConversionFiles(sNodePath))
        std::filesystem::remove(std::filesystem::u8path(sFile));
}
