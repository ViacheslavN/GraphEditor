// Test of the R-tree (SpatialLib/RTree, TRTree with the zlib node compressor).
//
// Data looks like map features: clusters ("towns") of points and small rects, some long rects (roads).
// Every search is compared with a brute force scan of the entries that should be in the tree, and the
// tree structure is checked (TRTree::Check: levels, parent rects are the MBRs of the children, counts).
// Every step re-opens the storage, so it also checks that the tree was persisted.
//
// Runs for int32 (fixed point) with the zlib and the NumLen node compressors, and for double with zlib:
//   incremental insert -> search -> remove half -> search -> remove the rest
//   bulk load (STR)    -> search -> insert / remove on top of the packed tree -> search
// and prints a comparison: time and node pages of insert / bulk load, time of window queries
// (tree search only, node cache smaller than the tree, so nodes are decoded again and again).
//
// usage: TestBPRTree [count] [storage file] [node cache]
//   count        - number of rects, default 200000
//   storage file - default F:\BPRTreeTest.btdb (Windows), /tmp/BPRTreeTest.btdb (other)
//   node cache   - nodes kept in memory, default 200

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <cstdlib>
#include <cstdio>

#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../../../CommonLib/log/logger.h"
#include "../../../../CommonLib/log/ConsoleLogWriter.h"
#include "../../../../CommonLib/log/LogInfo.h"
#include "../../../../CommonLib/filesystem/File.h"
#include "../../../../CommonLib/stream/MemoryStream.h"
#include "../../../../CommonLib/stream/FixedMemoryStream.h"

#include "../../../StorageLib/Storage.h"
#include "../../../StorageLib/FilePager.h"
#include "../../../StorageLib/FileStorage.h"

#include "../../RTree/RTree.h"
#include "../../RTree/RTreeNumLenCompressor.h"

namespace
{
	const uint32_t nPageSize = 8192;

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	// rects in "world" units 0..1e6 (int32: 1 unit, double: the same values as doubles + a fraction)
	template<class TCoord>
	struct TData
	{
		typedef bptreedb::spatiallib::rtree::TRTreeEntry<TCoord> TEntry;
		typedef typename TEntry::TRect TRect;

		std::vector<TEntry> entries;          // value = index + 1

		static TCoord C(double v) { return (TCoord)v; }

		void Generate(size_t nCount, uint32_t nSeed)
		{
			std::mt19937_64 rng(nSeed);
			std::uniform_real_distribution<double> u(0.0, 1.0);

			const size_t nTowns = 200;
			std::vector<std::pair<double, double>> towns(nTowns);
			for (auto& t : towns)
				t = std::make_pair(u(rng) * 1e6, u(rng) * 1e6);

			entries.resize(nCount);
			for (size_t i = 0; i < nCount; ++i)
			{
				const auto& t = towns[(size_t)(std::pow(u(rng), 2.0) * nTowns)];   // some towns are bigger
				double r = 20000.0 * u(rng) * u(rng);
				double a = u(rng) * 6.283185307;
				double x = (std::min)(1e6, (std::max)(0.0, t.first + r * std::cos(a)));
				double y = (std::min)(1e6, (std::max)(0.0, t.second + r * std::sin(a)));
				double kind = u(rng);
				double w = 0, h = 0;
				if (kind < 0.35)
				{
					// point
				}
				else if (kind < 0.95)
				{
					w = 5 + u(rng) * 200;    // building
					h = 5 + u(rng) * 200;
				}
				else
				{
					w = u(rng) * 30000;     // road
					h = u(rng) * 3000;
				}
				if (std::is_floating_point<TCoord>::value)
				{
					x += u(rng);
					y += u(rng);
				}
				entries[i] = TEntry(TRect(C(x), C(y), C(x + w), C(y + h)), (int64_t)i + 1);
			}
		}
	};

	struct SMetrics
	{
		std::string name;
		double dInsert{ 0 }, dBulk{ 0 }, dRemove{ 0 };
		double dQueryInsert{ 0 }, dQueryBulk{ 0 };
		uint64_t nInsertBytes{ 0 }, nBulkBytes{ 0 }, nRawBytes{ 0 }, nQueryResults{ 0 };
		uint32_t nQueries{ 0 };
	};
	std::vector<SMetrics> g_metrics;

	template<class TCoord, class TCompressor = bptreedb::spatiallib::rtree::TRTreeZlibCompressor<TCoord> >
	class TTester
	{
	public:
		typedef bptreedb::spatiallib::rtree::TRTree<TCoord, TCompressor> TTree;
		typedef typename TTree::TEntry TEntry;
		typedef typename TTree::TRect TRect;
		typedef bptreedb::spatiallib::rtree::RectMath RectMath;

		TTester(const std::string& name, const std::wstring& path, uint32_t nCache) : m_name(name), m_path(path), m_nCache(nCache)
		{
			m_metrics.name = name;
			m_pAlloc.reset(new CommonLib::CSimpleAlloc(false));
		}

		// opens the storage and the tree, runs step, flushes and closes
		void Step(const char* pszName, bool bCreate, const std::function<void(TTree&)>& step)
		{
			auto start = std::chrono::steady_clock::now();
			bptreedb::storage::CFileStoragePtr pStorage(new bptreedb::storage::CFileStorage(m_pAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
			pStorage->Open(m_path.c_str(), bCreate, 0, nPageSize);
			bptreedb::storage::IFilePagerPtr pPager = std::make_shared<bptreedb::storage::CFilePager>(m_pAlloc, pStorage, 100);

			TTree tree(bCreate ? -1 : m_nInfoPage, pPager, m_nCache);
			tree.Open();
			m_nInfoPage = tree.GetPageInfo();
			if (step)
				step(tree);
			tree.Flush();

			pPager->Flush();
			pStorage->Close();
			Log.InfoT("{0} {1}: {2} s", m_name, std::string(pszName), Seconds(start));
		}

		void CheckSize(TTree& tree)
		{
			if (tree.size() != m_present.size())
				throw CommonLib::CExcBase("tree size {0}, expected {1}", tree.size(), (uint64_t)m_present.size());
		}

		void CheckSearch(TTree& tree, uint32_t nQueries, uint32_t nSeed)
		{
			std::mt19937_64 rng(nSeed);
			std::uniform_real_distribution<double> u(0.0, 1.0);

			std::vector<TEntry> all;
			all.reserve(m_present.size());
			for (const auto& kv : m_present)
				all.push_back(TEntry(kv.second, kv.first));

			uint64_t nTotal = 0;
			for (uint32_t q = 0; q < nQueries; ++q)
			{
				TRect query;
				double kind = u(rng);
				double x = u(rng) * 1e6, y = u(rng) * 1e6;
				if (kind < 0.1 && !all.empty())
					query = all[(size_t)(u(rng) * all.size()) % all.size()].m_rect;         // exactly an entry (touching edges)
				else if (kind < 0.2)
					query = TRect((TCoord)x, (TCoord)y, (TCoord)x, (TCoord)y);           // point
				else if (kind < 0.9)
				{
					double s = 100 + u(rng) * 20000;                                     // screen-like window
					query = TRect((TCoord)x, (TCoord)y, (TCoord)(x + s), (TCoord)(y + s * 0.6));
				}
				else
					query = TRect((TCoord)0, (TCoord)0, (TCoord)1e6, (TCoord)(u(rng) * 1e6));  // big

				std::vector<int64_t> found;
				tree.search(query, [&found](const TRect&, int64_t v) { found.push_back(v); return true; });

				std::vector<int64_t> expected;
				for (const TEntry& e : all)
					if (RectMath::Intersects(e.m_rect, query))
						expected.push_back(e.m_nValue);

				std::sort(found.begin(), found.end());
				std::sort(expected.begin(), expected.end());
				if (found != expected)
					throw CommonLib::CExcBase("search {0}: found {1}, expected {2}", q, (uint64_t)found.size(), (uint64_t)expected.size());
				nTotal += found.size();
			}

			// stop after the first entry
			uint64_t nCalls = tree.search(TRect((TCoord)0, (TCoord)0, (TCoord)1e6, (TCoord)1e6), [](const TRect&, int64_t) { return false; });
			if (nCalls != (m_present.empty() ? 0u : 1u))
				throw CommonLib::CExcBase("search stop: {0} calls", nCalls);

			Log.InfoT("{0}: {1} queries ok, {2} results", m_name, nQueries, nTotal);
		}

		uint64_t LogStat(TTree& tree)
		{
			bptreedb::spatiallib::rtree::SRTreeStatistics stat;
			tree.GetStatistics(stat);
			uint64_t nNodes = 0;
			std::string levels;
			for (size_t i = 0; i < stat.m_nodesByLevel.size(); ++i)
			{
				nNodes += stat.m_nodesByLevel[i];
				levels += " L" + std::to_string(i) + ": " + std::to_string(stat.m_nodesByLevel[i]) + " nodes, " +
					std::to_string(stat.m_nodesByLevel[i] ? stat.m_entriesByLevel[i] / stat.m_nodesByLevel[i] : 0) + " avg;";
			}
			uint64_t nRaw = stat.m_nEntries * (4 * sizeof(TCoord) + sizeof(int64_t));
			uint64_t nBytes = nNodes * nPageSize;
			Log.InfoT("{0}: height {1}, entries {2},{3}", m_name, tree.height(), stat.m_nEntries, levels);
			Log.InfoT("{0}: node pages {1} bytes, raw entries {2} bytes ({3}%)", m_name, nBytes, nRaw, nRaw ? (uint64_t)(nBytes * 100 / nRaw) : (uint64_t)0);
			m_metrics.nRawBytes = nRaw;
			return nBytes;
		}

		// window queries, only the tree search is timed
		double Bench(TTree& tree, uint32_t nQueries)
		{
			std::mt19937_64 rng(77);
			std::uniform_real_distribution<double> u(0.0, 1.0);
			std::vector<TRect> queries(nQueries);
			for (TRect& q : queries)
			{
				double x = u(rng) * 1e6, y = u(rng) * 1e6, s = 1000 + u(rng) * 10000;
				q = TRect((TCoord)x, (TCoord)y, (TCoord)(x + s), (TCoord)(y + s * 0.6));
			}
			uint64_t nResults = 0;
			auto start = std::chrono::steady_clock::now();
			for (const TRect& q : queries)
				nResults += tree.search(q, [](const TRect&, int64_t) { return true; });
			double d = Seconds(start);
			m_metrics.nQueries = nQueries;
			m_metrics.nQueryResults = nResults;
			Log.InfoT("{0}: {1} window queries {2} s, {3} results", m_name, nQueries, d, nResults);
			return d;
		}

		void Run(size_t nCount)
		{
			TData<TCoord> data;
			data.Generate(nCount, 12345);
			std::vector<TEntry> order = data.entries;
			std::shuffle(order.begin(), order.end(), std::mt19937(7));
			size_t nHalf = order.size() / 2;

			//------------------------- incremental
			Step("create", true, nullptr);
			Step("insert all", false, [&](TTree& tree) {
				auto start = std::chrono::steady_clock::now();
				for (const TEntry& e : order)
				{
					tree.insert(e.m_rect, e.m_nValue);
					m_present[e.m_nValue] = e.m_rect;
				}
				m_metrics.dInsert = Seconds(start);
			});
			Step("check", false, [&](TTree& tree) { CheckSize(tree); tree.Check(); m_metrics.nInsertBytes = LogStat(tree); });
			Step("bench", false, [&](TTree& tree) { m_metrics.dQueryInsert = Bench(tree, 5000); });
			Step("search", false, [&](TTree& tree) { CheckSearch(tree, 300, 1); });

			Step("remove half", false, [&](TTree& tree) {
				auto start = std::chrono::steady_clock::now();
				for (size_t i = 0; i < nHalf; ++i)
				{
					if (!tree.remove(order[i].m_rect, order[i].m_nValue))
						throw CommonLib::CExcBase("remove {0} failed", order[i].m_nValue);
					m_present.erase(order[i].m_nValue);
				}
				m_metrics.dRemove = Seconds(start);
				// not in the tree: removed already, wrong rect, wrong value
				if (tree.remove(order[0].m_rect, order[0].m_nValue))
					throw CommonLib::CExcBase("removed entry removed again");
				const TEntry& e = order[nHalf];
				TRect other(e.m_rect.m_minX, e.m_rect.m_minY, (TCoord)(e.m_rect.m_maxX + 1), e.m_rect.m_maxY);
				if (tree.remove(other, e.m_nValue) || tree.remove(e.m_rect, -5))
					throw CommonLib::CExcBase("remove of a missing entry returned true");
			});
			Step("check half", false, [&](TTree& tree) { CheckSize(tree); tree.Check(); LogStat(tree); CheckSearch(tree, 300, 2); });

			Step("remove rest", false, [&](TTree& tree) {
				for (size_t i = nHalf; i < order.size(); ++i)
				{
					if (!tree.remove(order[i].m_rect, order[i].m_nValue))
						throw CommonLib::CExcBase("remove {0} failed", order[i].m_nValue);
					m_present.erase(order[i].m_nValue);
				}
			});
			Step("check empty", false, [&](TTree& tree) {
				CheckSize(tree);
				tree.Check();
				if (tree.height() != 1)
					throw CommonLib::CExcBase("empty tree height {0}", tree.height());
				CheckSearch(tree, 20, 3);
			});

			//------------------------- bulk load
			Step("create for bulk", true, nullptr);
			Step("bulk load", false, [&](TTree& tree) {
				std::vector<TEntry> entries = data.entries;
				auto start = std::chrono::steady_clock::now();
				tree.BulkLoad(entries);
				m_metrics.dBulk = Seconds(start);
				for (const TEntry& e : data.entries)
					m_present[e.m_nValue] = e.m_rect;
			});
			Step("check bulk", false, [&](TTree& tree) { CheckSize(tree); tree.Check(); m_metrics.nBulkBytes = LogStat(tree); CheckSearch(tree, 300, 4); });
			Step("bench bulk", false, [&](TTree& tree) { m_metrics.dQueryBulk = Bench(tree, 5000); });

			Step("update bulk", false, [&](TTree& tree) {
				// small updates on the packed tree: remove 10%, insert 10% new
				size_t nUpd = nCount / 10;
				for (size_t i = 0; i < nUpd; ++i)
				{
					if (!tree.remove(order[i].m_rect, order[i].m_nValue))
						throw CommonLib::CExcBase("remove {0} failed", order[i].m_nValue);
					m_present.erase(order[i].m_nValue);
				}
				TData<TCoord> more;
				more.Generate(nUpd, 999);
				for (TEntry& e : more.entries)
				{
					e.m_nValue += (int64_t)nCount;
					tree.insert(e.m_rect, e.m_nValue);
					m_present[e.m_nValue] = e.m_rect;
				}
			});
			Step("check updated", false, [&](TTree& tree) { CheckSize(tree); tree.Check(); LogStat(tree); CheckSearch(tree, 300, 5); });

			CommonLib::file::TFilePtr file = CommonLib::file::CFileCreator::OpenFile(m_path.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
			Log.InfoT("{0}: storage file {1} bytes", m_name, (uint64_t)file->GetFileSize());
			g_metrics.push_back(m_metrics);
		}

	private:
		std::string m_name;
		std::wstring m_path;
		uint32_t m_nCache;
		CommonLib::IAllocPtr m_pAlloc;
		int64_t m_nInfoPage{ -1 };
		std::unordered_map<int64_t, TRect> m_present;
		SMetrics m_metrics;
	};

	// Nodes bigger than the page when they are written. The size estimate of a node (the real size + MaxGrowth
	// per changed entry) isn't a bound for zlib: on compressible coordinates level 1 grows by a few hundred bytes
	// for one entry, so a node passes the checks of the inserts and doesn't fit when it leaves the cache (a small
	// cache: it happens all the time) or is flushed. The tree has to split it there. Searches between the
	// inserts mustn't split (the nodes they will visit are on their stack): the node stays in the cache.
	// bSearchEachInsert: a search of the whole extent after every insert (the "keep it during a search" case).
	int TestOverflowSave(const std::wstring& path, size_t nCount, bool bSearchEachInsert)
	{
		typedef bptreedb::spatiallib::rtree::TRTree<double> TTree;
		typedef TTree::TEntry TEntry;
		typedef TTree::TRect TRect;
		typedef bptreedb::spatiallib::rtree::RectMath RectMath;

		try
		{
			CommonLib::CLogInfo testInfo(Log, "TestRTree overflow on save, count: {0}, search after every insert: {1}", (uint64_t)nCount, bSearchEachInsert ? 1 : 0);
			CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false));
			bptreedb::spatiallib::rtree::SRTreeParams params;
			params.m_nCompressLevel = 1;
			const uint32_t nTreeCache = 8;

			auto openStorage = [&](bool bCreate, bptreedb::storage::CFileStoragePtr& pStorage, bptreedb::storage::IFilePagerPtr& pPager)
			{
				pStorage.reset(new bptreedb::storage::CFileStorage(pAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
				pStorage->Open(path.c_str(), bCreate, 0, nPageSize);
				pPager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, pStorage, 100);
			};

			std::vector<TEntry> entries;
			auto check = [&](TTree& tree, std::mt19937_64& rng, int nQueries)
			{
				for (int q = 0; q < nQueries; ++q)
				{
					double x = (double)(rng() % 1000), y = (double)(rng() % 1000), s = (double)(rng() % 200);
					TRect rect(x, y, x + s, y + s);
					std::vector<int64_t> found, expected;
					tree.search(rect, [&found](const TRect&, int64_t v) { found.push_back(v); return true; });
					for (const TEntry& e : entries)
						if (RectMath::Intersects(e.m_rect, rect))
							expected.push_back(e.m_nValue);
					std::sort(found.begin(), found.end());
					std::sort(expected.begin(), expected.end());
					if (found != expected)
						throw CommonLib::CExcBase("search: {0} entries, expected {1}", (uint64_t)found.size(), (uint64_t)expected.size());
				}
			};

			std::mt19937_64 rng(11);
			int64_t nInfo = -1;
			{
				bptreedb::storage::CFileStoragePtr pStorage;
				bptreedb::storage::IFilePagerPtr pPager;
				openStorage(true, pStorage, pPager);
				TTree tree(-1, pPager, nTreeCache, params);
				tree.Open();

				for (size_t i = 0; i < nCount; ++i)
				{
					// points with integer coordinates (as doubles): zlib finds many repeats in the byte columns
					double x = (double)(rng() % 1000), y = (double)(rng() % 1000);
					TEntry e(TRect(x, y, x, y), (int64_t)i * 3 + 1);
					tree.insert(e.m_rect, e.m_nValue);
					entries.push_back(e);
					if (bSearchEachInsert)
					{
						uint64_t nFound = tree.search(TRect(0, 0, 1000, 1000), [](const TRect&, int64_t) { return true; });
						if (nFound != entries.size())
							throw CommonLib::CExcBase("search after insert {0}: {1} entries", (uint64_t)i, nFound);
					}
					else if (i % 2500 == 2499)
						check(tree, rng, 3);     // dirty, maybe too big nodes in the cache during the search
				}
				tree.Check();

				std::vector<TEntry> kept;
				for (size_t i = 0; i < entries.size(); ++i)
				{
					if (i % 3 == 0)
					{
						if (!tree.remove(entries[i].m_rect, entries[i].m_nValue))
							throw CommonLib::CExcBase("remove of entry {0}", entries[i].m_nValue);
					}
					else
						kept.push_back(entries[i]);
				}
				entries.swap(kept);
				tree.Check();
				check(tree, rng, 20);
				tree.Flush();
				nInfo = tree.GetPageInfo();
				pPager->Flush();
				pStorage->Close();
			}

			{
				bptreedb::storage::CFileStoragePtr pStorage;
				bptreedb::storage::IFilePagerPtr pPager;
				openStorage(false, pStorage, pPager);
				TTree tree(nInfo, pPager, nTreeCache, params);
				tree.Open();
				tree.Check();
				if (tree.size() != entries.size())
					throw CommonLib::CExcBase("size after reopen {0}, expected {1}", tree.size(), (uint64_t)entries.size());
				check(tree, rng, 20);
				pStorage->Close();
			}
			testInfo.Complete("TestRTree overflow on save passed");
			return 0;
		}
		catch (std::exception& exc)
		{
			Log.Exc(exc);
			std::cout << "TestRTree overflow on save failed" << std::endl;
			return 1;
		}
	}

	template<class TCoord, class TCompressor = bptreedb::spatiallib::rtree::TRTreeZlibCompressor<TCoord> >
	int RunTest(const std::string& name, size_t nCount, const std::wstring& path, uint32_t nCache)
	{
		try
		{
			CommonLib::CLogInfo testInfo(Log, "TestRTree {0}, count: {1}", name, (uint64_t)nCount);
			TTester<TCoord, TCompressor> tester(name, path, nCache);
			tester.Run(nCount);
			testInfo.Complete("TestRTree passed: {0}", name);
			return 0;
		}
		catch (std::exception& exc)
		{
			Log.Exc(exc);
			std::cout << "TestRTree failed: " << name << std::endl;
			return 1;
		}
	}
}

int main(int argc, char* argv[])
{
	std::shared_ptr<CommonLib::IlogWriter> pLogWriter(new CommonLib::CConsoleLogWriter());
	Log.InnitLog(std::shared_ptr<CommonLib::IlogLineFormat>(), pLogWriter);

	size_t nCount = argc > 1 ? (size_t)std::strtoul(argv[1], nullptr, 10) : 200000;
#ifdef _WIN32
	std::wstring storagePath = L"F:\\BPRTreeTest.btdb";
#else
	std::wstring storagePath = L"/tmp/BPRTreeTest.btdb";
#endif
	if (argc > 2)
	{
		std::string path = argv[2];
		storagePath.assign(path.begin(), path.end());
	}
	uint32_t nCache = argc > 3 ? (uint32_t)std::strtoul(argv[3], nullptr, 10) : 200;

	int nFailed = 0;
	nFailed += TestOverflowSave(storagePath, (std::min)(nCount, (size_t)60000), false);
	nFailed += TestOverflowSave(storagePath, (std::min)(nCount, (size_t)15000), true);
	nFailed += RunTest<int32_t>("int32 zlib", nCount, storagePath, nCache);
	nFailed += RunTest<int32_t, bptreedb::spatiallib::rtree::TRTreeNumLenCompressor<int32_t> >("int32 numlen", nCount, storagePath, nCache);
	nFailed += RunTest<int64_t, bptreedb::spatiallib::rtree::TRTreeNumLenCompressor<int64_t> >("int64 numlen", nCount, storagePath, nCache);
	nFailed += RunTest<double>("double zlib", nCount, storagePath, nCache);

	// comparison
	char line[512];
	std::cout << std::endl << "count " << nCount << ", node cache " << nCache << ", page " << nPageSize << std::endl;
	snprintf(line, sizeof(line), "%-14s %10s %12s %8s %10s %12s %8s %12s %12s %10s\n", "", "insert s", "insert pages", "% raw", "bulk s", "bulk pages", "% raw", "query ms ins", "query ms bulk", "remove s");
	std::cout << line;
	for (const SMetrics& m : g_metrics)
	{
		snprintf(line, sizeof(line), "%-14s %10.2f %12llu %7llu%% %10.2f %12llu %7llu%% %12.1f %12.1f %10.2f\n", m.name.c_str(),
			m.dInsert, (unsigned long long)m.nInsertBytes, (unsigned long long)(m.nRawBytes ? m.nInsertBytes * 100 / m.nRawBytes : 0),
			m.dBulk, (unsigned long long)m.nBulkBytes, (unsigned long long)(m.nRawBytes ? m.nBulkBytes * 100 / m.nRawBytes : 0),
			m.dQueryInsert * 1000, m.dQueryBulk * 1000, m.dRemove);
		std::cout << line;
	}
	std::cout << "(queries: " << (g_metrics.empty() ? 0 : g_metrics[0].nQueries) << " windows per run; raw = count * (4 * sizeof(coord) + 8))" << std::endl << std::endl;

	if (nFailed != 0)
		std::cout << nFailed << " test(s) failed" << std::endl;
	else
		std::cout << "all tests passed" << std::endl;
	return nFailed;
}
