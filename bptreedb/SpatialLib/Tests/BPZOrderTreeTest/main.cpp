// Test of the Z-order B+ tree index (SpatialLib/ZOrderTree, TBPZOrderIndex).
//
// For points and rects (Z keys of 1, 2 and 4 64-bit limbs):
//   incremental insert -> check -> search -> remove half -> check -> search
//   bulk load          -> check -> search
// Every search is compared with a brute force scan, in all query modes: BIGMIN jumps, rect split (sub-ranges)
// and a scan of the whole Z range (all must give the same objects); the iterator is checked directly too. Some objects share a shape (different ids).
// Every step re-opens the storage.
// Grid test (TBPZOrderGridIndex): double coordinates in degrees over a country-sized extent, grids of
// 1e8 / 1e6 / 1e4 cells; no object that intersects a query may be missed, the candidates are exactly the
// objects whose grid rects intersect the grid query; size and the share of extra candidates are reported.
// Then the same objects and window queries go to the R-tree (TRTree, NumLen) for a size / speed comparison
// (queries of both are timed on a warm node cache: one untimed pass first).
//
// usage: TestBPZOrderTree [count] [storage file] [node cache]
//   count        - objects, default 200000
//   storage file - default F:\BPZOrderTreeTest.btdb (Windows), /tmp/BPZOrderTreeTest.btdb (other)
//   node cache   - nodes kept in memory, default 200

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <cstdlib>
#include <cstdio>
#include <type_traits>

#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../../../CommonLib/log/logger.h"
#include "../../../../CommonLib/log/ConsoleLogWriter.h"
#include "../../../../CommonLib/log/LogInfo.h"
#include "../../../../CommonLib/stream/MemoryStream.h"
#include "../../../../CommonLib/stream/FixedMemoryStream.h"

#include "../../../StorageLib/Storage.h"
#include "../../../StorageLib/FilePager.h"
#include "../../../StorageLib/FileStorage.h"

#include "../../ZOrderTree/BPZOrderIndex.h"
#include "../../ZOrderTree/BPZOrderGridIndex.h"
#include "../../RTree/RTree.h"
#include "../../RTree/RTreeNumLenCompressor.h"

using namespace bptreedb::spatiallib;

namespace
{
	const uint32_t nPageSize = 8192;

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	struct SResult
	{
		std::string name;
		double dInsert{ 0 }, dBulk{ 0 }, dQuery{ 0 }, dQuerySplit{ 0 }, dRQuery{ 0 }, dRBulk{ 0 };
		uint64_t nInsertBytes{ 0 }, nBulkBytes{ 0 }, nRBulkBytes{ 0 };
		uint64_t nRead{ 0 }, nFound{ 0 }, nSeeks{ 0 }, nTreeSeeks{ 0 }, nReadSplit{ 0 }, nSeeksSplit{ 0 };
	};
	std::vector<SResult> g_results;

	struct SStorage
	{
		CommonLib::IAllocPtr m_pAlloc;
		bptreedb::storage::CFileStoragePtr m_pStorage;
		bptreedb::storage::IFilePagerPtr m_pPager;

		SStorage(CommonLib::IAllocPtr pAlloc, const std::wstring& path, bool bCreate) : m_pAlloc(pAlloc)
		{
			m_pStorage.reset(new bptreedb::storage::CFileStorage(pAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
			m_pStorage->Open(path.c_str(), bCreate, 0, nPageSize);
			m_pPager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, m_pStorage, 100);
		}
		~SStorage()
		{
			m_pPager->Flush();
			m_pStorage->Close();
		}
	};

	template<class TTraits>
	class TTester
	{
	public:
		typedef zorder::TBPZOrderIndex<TTraits> TIndex;
		typedef typename TTraits::TCoord TCoord;
		typedef typename TTraits::TShape TShape;
		typedef typename TTraits::TRect TRect;
		typedef std::pair<TShape, int64_t> TObject;
		static constexpr bool IsPoint = std::is_same<TShape, TPoint2D<TCoord> >::value;

		TTester(const std::string& name, double dWorld, const std::wstring& path, uint32_t nCache, bool bExtent) :
			m_name(name), m_dWorld(dWorld), m_path(path), m_nCache(nCache), m_bExtent(bExtent)
		{
			m_pAlloc.reset(new CommonLib::CSimpleAlloc(false));
			m_result.name = name;
		}

		// the data is inside [0, world]; m_bExtent = false: the whole coordinate range (no extent given)
		TRect Extent() const
		{
			if (!m_bExtent)
				return TRect(TCoord(0), TCoord(0), (std::numeric_limits<TCoord>::max)(), (std::numeric_limits<TCoord>::max)());
			return TRect(TCoord(0), TCoord(0), (TCoord)m_dWorld, (TCoord)m_dWorld);
		}

		static TShape MakeShape(double x, double y, double w, double h)
		{
			TShape s;
			if constexpr (IsPoint)
			{
				s.m_x = (TCoord)x;
				s.m_y = (TCoord)y;
			}
			else
				s = TShape((TCoord)x, (TCoord)y, (TCoord)(x + w), (TCoord)(y + h));
			return s;
		}

		static TRect ToRect(const TShape& s)
		{
			if constexpr (IsPoint)
				return TRect(s.m_x, s.m_y, s.m_x, s.m_y);
			else
				return s;
		}

		static bool Intersects(const TShape& s, const TRect& q)
		{
			TRect r = ToRect(s);
			return !(q.m_maxX < r.m_minX) && !(r.m_maxX < q.m_minX) && !(q.m_maxY < r.m_minY) && !(r.m_maxY < q.m_minY);
		}

		// map-like data in [0, world]: clusters of small objects, some long ones; 2% repeat a shape with another id
		void Generate(size_t nCount, uint32_t nSeed)
		{
			std::mt19937_64 rng(nSeed);
			std::uniform_real_distribution<double> u(0.0, 1.0);
			double W = m_dWorld;
			std::vector<std::pair<double, double>> towns(200);
			for (auto& t : towns)
				t = std::make_pair(u(rng) * W * 0.95, u(rng) * W * 0.95);

			m_objects.clear();
			for (size_t i = 0; i < nCount; ++i)
			{
				int64_t nId = (int64_t)i + 1;
				if (i > 0 && u(rng) < 0.02)
				{
					m_objects.push_back(TObject(m_objects[(size_t)(u(rng) * m_objects.size())].first, nId));
					continue;
				}
				const auto& t = towns[(size_t)(std::pow(u(rng), 2.0) * towns.size())];
				double r = W * 0.02 * u(rng) * u(rng), a = u(rng) * 6.283185307;
				double x = (std::min)(W * 0.96, (std::max)(0.0, t.first + r * std::cos(a)));
				double y = (std::min)(W * 0.96, (std::max)(0.0, t.second + r * std::sin(a)));
				double w = W * 0.0002 * u(rng), h = W * 0.0002 * u(rng);
				if (u(rng) < 0.05) { w = W * 0.03 * u(rng); h = W * 0.003 * u(rng); }
				m_objects.push_back(TObject(MakeShape(x, y, w, h), nId));
			}
		}

		std::vector<TRect> Queries(uint32_t nCount, uint32_t nSeed)
		{
			std::mt19937_64 rng(nSeed);
			std::uniform_real_distribution<double> u(0.0, 1.0);
			std::vector<TRect> queries;
			for (uint32_t i = 0; i < nCount; ++i)
			{
				double x = u(rng) * m_dWorld * 0.95, y = u(rng) * m_dWorld * 0.95, kind = u(rng);
				if (kind < 0.1 && !m_present.empty())
					queries.push_back(ToRect(m_present[(size_t)(u(rng) * m_present.size()) % m_present.size()].first));   // exactly an object
				else if (kind < 0.2)
					queries.push_back(TRect((TCoord)x, (TCoord)y, (TCoord)x, (TCoord)y));
				else
				{
					double s = m_dWorld * (0.001 + u(rng) * 0.02);
					queries.push_back(TRect((TCoord)x, (TCoord)y, (TCoord)(x + s), (TCoord)(y + s * 0.6)));
				}
			}
			return queries;
		}

		void Step(const char* pszName, bool bCreate, const std::function<void(TIndex&)>& step)
		{
			auto start = std::chrono::steady_clock::now();
			{
				SStorage storage(m_pAlloc, m_path, bCreate);
				TIndex index(bCreate ? -1 : m_nInfoPage, storage.m_pPager, m_pAlloc, m_nCache, nPageSize, Extent());
				if (bCreate)
					index.Create();
				m_nInfoPage = index.GetPageInfo();
				if (step)
					step(index);
				index.Flush();
			}
			Log.InfoT("{0} {1}: {2} s", m_name, std::string(pszName), Seconds(start));
		}

		uint64_t TreeBytes(TIndex& index)
		{
			auto stat = index.Tree().GetStatistics();
			stat->CalcNodesInTree();
			if (stat->m_nKeyCount != m_present.size())
				throw CommonLib::CExcBase("tree has {0} keys, expected {1}", stat->m_nKeyCount, (uint64_t)m_present.size());
			uint64_t nBytes = (stat->m_nInnerNodeCount + stat->m_nLeafNodeCount) * nPageSize;
			Log.InfoT("{0}: {1} inner nodes, {2} leaves, {3} bytes", m_name, stat->m_nInnerNodeCount, stat->m_nLeafNodeCount, nBytes);
			return nBytes;
		}

		void CheckSearch(TIndex& index, uint32_t nQueries, uint32_t nSeed)
		{
			const zorder::EZOrderQueryMode modes[] = { zorder::eZQueryBigMin, zorder::eZQueryRectSplit, zorder::eZQueryFullScan };
			const char* names[] = { "BIGMIN", "rect split", "full Z range" };
			uint64_t nFound = 0, nRead[3] = { 0, 0, 0 };
			for (const TRect& q : Queries(nQueries, nSeed))
			{
				std::vector<int64_t> expected;
				for (const TObject& obj : m_present)
					if (Intersects(obj.first, q))
						expected.push_back(obj.second);
				std::sort(expected.begin(), expected.end());

				for (int m = 0; m < 3; ++m)
				{
					std::vector<int64_t> found;
					zorder::SZOrderQueryStats st = index.search(q, [&found, &q](const TShape& s, int64_t id)
					{
						if (!Intersects(s, q))
							throw CommonLib::CExcBase("search returned an object outside the query, id {0}", id);
						found.push_back(id);
						return true;
					}, modes[m]);
					std::sort(found.begin(), found.end());
					if (found != expected)
						throw CommonLib::CExcBase("search {0}: found {1}, brute force {2}", std::string(names[m]), (uint64_t)found.size(), (uint64_t)expected.size());
					nRead[m] += st.m_nRead;
				}

				// the iterator directly: ascending keys, the same objects
				std::vector<int64_t> found;
				typename TIndex::TKey prev;
				for (auto it = index.spatialQuery(q, zorder::eZQueryRectSplit); !it.IsNull(); it.Next())
				{
					if (!found.empty() && !(prev < it.Key()))
						throw CommonLib::CExcBase("iterator: keys not ascending, id {0}", it.Id());
					prev = it.Key();
					found.push_back(it.Id());
				}
				std::sort(found.begin(), found.end());
				if (found != expected)
					throw CommonLib::CExcBase("iterator: found {0}, brute force {1}", (uint64_t)found.size(), (uint64_t)expected.size());
				nFound += expected.size();
			}
			Log.InfoT("{0}: {1} queries ok, {2} found", m_name, nQueries, nFound);
			Log.InfoT("{0}: keys read: BIGMIN {1}, rect split {2}, full Z range {3}", m_name, nRead[0], nRead[1], nRead[2]);
		}

		double Bench(TIndex& index, const std::vector<TRect>& queries, zorder::EZOrderQueryMode mode)
		{
			for (const TRect& q : queries)      // warm up: both indexes are timed with the nodes in the cache
				index.search(q, [](const TShape&, int64_t) { return true; }, mode);

			zorder::SZOrderQueryStats total;
			auto start = std::chrono::steady_clock::now();
			for (const TRect& q : queries)
			{
				zorder::SZOrderQueryStats st = index.search(q, [](const TShape&, int64_t) { return true; }, mode);
				total.m_nFound += st.m_nFound; total.m_nRead += st.m_nRead; total.m_nSeeks += st.m_nSeeks; total.m_nTreeSeeks += st.m_nTreeSeeks;
			}
			double d = Seconds(start);
			if (mode == zorder::eZQueryBigMin)
			{
				m_result.nFound = total.m_nFound; m_result.nRead = total.m_nRead; m_result.nSeeks = total.m_nSeeks; m_result.nTreeSeeks = total.m_nTreeSeeks;
			}
			else
			{
				m_result.nReadSplit = total.m_nRead; m_result.nSeeksSplit = total.m_nSeeks;
			}
			Log.InfoT("{0}: {1} window queries {2} s, found {3}", m_name, (uint64_t)queries.size(), d, total.m_nFound);
			Log.InfoT("{0}: read {1}, seeks {2}, through the tree {3}", m_name, total.m_nRead, total.m_nSeeks, total.m_nTreeSeeks);
			return d;
		}

		void RunRTree(const std::vector<TRect>& queries)
		{
			typedef rtree::TRTree<TCoord, rtree::TRTreeNumLenCompressor<TCoord> > TRTree;
			SStorage storage(m_pAlloc, m_path, true);
			TRTree tree(-1, storage.m_pPager, m_nCache);
			tree.Open();
			std::vector<typename TRTree::TEntry> entries;
			for (const TObject& obj : m_objects)
				entries.push_back(typename TRTree::TEntry(ToRect(obj.first), obj.second));
			auto start = std::chrono::steady_clock::now();
			tree.BulkLoad(entries);
			m_result.dRBulk = Seconds(start);
			rtree::SRTreeStatistics stat;
			tree.GetStatistics(stat);
			for (uint64_t n : stat.m_nodesByLevel)
				m_result.nRBulkBytes += n * nPageSize;

			for (const TRect& q : queries)      // warm up, as for the Z-order index
				tree.search(q, [](const TRect&, int64_t) { return true; });

			uint64_t nFound = 0;
			start = std::chrono::steady_clock::now();
			for (const TRect& q : queries)
				nFound += tree.search(q, [](const TRect&, int64_t) { return true; });
			m_result.dRQuery = Seconds(start);
			if (nFound != m_result.nFound)
				throw CommonLib::CExcBase("R-tree found {0}, Z-order index {1}", nFound, m_result.nFound);
			tree.Flush();
		}

		void Run(size_t nCount)
		{
			Generate(nCount, 4242);
			std::vector<TObject> order = m_objects;
			std::shuffle(order.begin(), order.end(), std::mt19937(5));
			size_t nHalf = order.size() / 2;

			Step("create", true, nullptr);
			Step("insert all", false, [&](TIndex& index) {
				auto start = std::chrono::steady_clock::now();
				for (const TObject& obj : order)
					index.insert(obj.first, obj.second);
				m_result.dInsert = Seconds(start);
				m_present = m_objects;
			});
			Step("check", false, [&](TIndex& index) { index.CheckTreeStructure(); m_result.nInsertBytes = TreeBytes(index); CheckSearch(index, 200, 1); });

			Step("remove half", false, [&](TIndex& index) {
				for (size_t i = 0; i < nHalf; ++i)
					if (!index.remove(order[i].first, order[i].second))
						throw CommonLib::CExcBase("remove of id {0} failed", order[i].second);
				if (index.remove(order[0].first, order[0].second) || index.remove(order[nHalf].first, -7))
					throw CommonLib::CExcBase("remove of a missing object returned true");
				m_present.assign(order.begin() + nHalf, order.end());
			});
			Step("check half", false, [&](TIndex& index) { index.CheckTreeStructure(); TreeBytes(index); CheckSearch(index, 200, 2); });

			Step("create for bulk", true, nullptr);
			Step("bulk load", false, [&](TIndex& index) {
				auto start = std::chrono::steady_clock::now();
				index.BulkLoad(m_objects);
				m_result.dBulk = Seconds(start);
				m_present = m_objects;
			});
			Step("check bulk", false, [&](TIndex& index) { index.CheckTreeStructure(); m_result.nBulkBytes = TreeBytes(index); CheckSearch(index, 200, 3); });

			std::vector<TRect> queries = Queries(5000, 77);
			Step("bench", false, [&](TIndex& index) {
				m_result.dQuery = Bench(index, queries, zorder::eZQueryBigMin);
				m_result.dQuerySplit = Bench(index, queries, zorder::eZQueryRectSplit);
			});

			RunRTree(queries);
			g_results.push_back(m_result);
		}

	private:
		std::string m_name;
		double m_dWorld;
		std::wstring m_path;
		bool m_bExtent;
		uint32_t m_nCache;
		CommonLib::IAllocPtr m_pAlloc;
		int64_t m_nInfoPage{ -1 };
		std::vector<TObject> m_objects;
		std::vector<TObject> m_present;
		SResult m_result;
	};


	struct SGridResult
	{
		std::string name;
		uint64_t nCells{ 0 };
		double dQuery{ 0 };
		uint64_t nBytes{ 0 }, nExact{ 0 }, nCandidates{ 0 };
	};
	std::vector<SGridResult> g_gridResults;

	// TTraits: point or rect traits of uint32 keys; objects in degrees inside extent
	template<class TTraits>
	void RunGridTest(const std::string& name, uint64_t nCells, size_t nCount, const std::wstring& path)
	{
		typedef zorder::TBPZOrderGridIndex<TTraits> TIndex;
		typedef typename TIndex::TShape TShape;
		typedef typename TIndex::TRectD TRectD;
		typedef typename TIndex::TGrid TGrid;
		static constexpr bool IsPoint = std::is_same<TShape, TPoint2D<double> >::value;

		const TRectD extent(12.09, 48.55, 18.86, 51.06);
		auto toRect = [](const TShape& s) -> TRectD
		{
			if constexpr (IsPoint)
				return TRectD(s.m_x, s.m_y, s.m_x, s.m_y);
			else
				return s;
		};
		auto intersects = [](const TRectD& a, const TRectD& b)
		{
			return !(b.m_maxX < a.m_minX) && !(a.m_maxX < b.m_minX) && !(b.m_maxY < a.m_minY) && !(a.m_maxY < b.m_minY);
		};

		// towns of small objects (buildings ~1e-4 deg), some roads ~1e-2 deg
		std::mt19937_64 rng(31);
		std::uniform_real_distribution<double> u(0.0, 1.0);
		std::vector<std::pair<double, double>> towns(300);
		for (auto& t : towns)
			t = std::make_pair(extent.m_minX + 0.05 + u(rng) * (extent.m_maxX - extent.m_minX - 0.1), extent.m_minY + 0.05 + u(rng) * (extent.m_maxY - extent.m_minY - 0.1));
		std::vector<std::pair<TShape, int64_t> > objects;
		for (size_t i = 0; i < nCount; ++i)
		{
			const auto& t = towns[(size_t)(std::pow(u(rng), 2.0) * towns.size())];
			double r = 0.05 * u(rng) * u(rng), a = u(rng) * 6.283185307;
			double x = t.first + r * std::cos(a), y = t.second + r * std::sin(a);
			double w = 2e-4 * u(rng), h = 2e-4 * u(rng);
			if (u(rng) < 0.05) { w = 0.02 * u(rng); h = 0.002 * u(rng); }
			TShape s;
			if constexpr (IsPoint) { s.m_x = x; s.m_y = y; }
			else s = TRectD(x, y, (std::min)(x + w, extent.m_maxX), (std::min)(y + h, extent.m_maxY));
			objects.emplace_back(s, (int64_t)i + 1);
		}

		std::vector<TRectD> queries;
		for (int i = 0; i < 2000; ++i)
		{
			const auto& t = towns[(size_t)(u(rng) * towns.size())];
			double x = t.first + (u(rng) - 0.5) * 0.1, y = t.second + (u(rng) - 0.5) * 0.1, sz = 0.001 + u(rng) * 0.02;
			if (i % 10 == 0)
				queries.push_back(toRect(objects[(size_t)(u(rng) * objects.size())].first));      // exactly an object
			else
				queries.push_back(TRectD(x, y, x + sz, y + sz * 0.6));
		}

		TGrid grid(extent, nCells);
		SGridResult res;
		res.name = name;
		res.nCells = nCells;

		CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false));
		SStorage storage(pAlloc, path, true);
		TIndex index(-1, storage.m_pPager, pAlloc, 2000, nPageSize, grid);
		index.Create();

		// a part by insert / remove, the rest by bulk load is not possible (bulk needs an empty tree):
		// insert everything, remove 10%, insert them again
		index.BulkLoad(objects);
		for (size_t i = 0; i < objects.size(); i += 10)
			if (!index.remove(objects[i].first, objects[i].second))
				throw CommonLib::CExcBase("grid: remove of {0} failed", objects[i].second);
		for (size_t i = 0; i < objects.size(); i += 10)
			index.insert(objects[i].first, objects[i].second);

		auto stat = index.Index().Tree().GetStatistics();
		stat->CalcNodesInTree();
		if (stat->m_nKeyCount != objects.size())
			throw CommonLib::CExcBase("grid: {0} keys, expected {1}", stat->m_nKeyCount, (uint64_t)objects.size());
		res.nBytes = (stat->m_nInnerNodeCount + stat->m_nLeafNodeCount) * nPageSize;

		for (const TRectD& q : queries)
		{
			std::vector<int64_t> found;
			index.search(q, [&](const TRectD& area, int64_t id)
			{
				const TRectD obj = toRect(objects[(size_t)id - 1].first);
				if (obj.m_minX < area.m_minX - 1e-9 || obj.m_minY < area.m_minY - 1e-9 || area.m_maxX + 1e-9 < obj.m_maxX || area.m_maxY + 1e-9 < obj.m_maxY)
					throw CommonLib::CExcBase("grid: the grid area doesn't cover object {0}", id);
				found.push_back(id);
				return true;
			});
			std::sort(found.begin(), found.end());

			std::vector<int64_t> exact, byGrid;
			typename TGrid::TGridRect gq = grid.ToGrid(q);
			for (const auto& obj : objects)
			{
				if (intersects(toRect(obj.first), q))
					exact.push_back(obj.second);
				typename TGrid::TGridRect go;
				if constexpr (IsPoint)
				{
					auto gp = grid.ToGrid(obj.first);
					go = typename TGrid::TGridRect(gp.m_x, gp.m_y, gp.m_x, gp.m_y);
				}
				else
					go = grid.ToGrid(obj.first);
				if (!(gq.m_maxX < go.m_minX) && !(go.m_maxX < gq.m_minX) && !(gq.m_maxY < go.m_minY) && !(go.m_maxY < gq.m_minY))
					byGrid.push_back(obj.second);
			}
			if (!std::includes(found.begin(), found.end(), exact.begin(), exact.end()))
				throw CommonLib::CExcBase("grid {0} cells: an object that intersects the query is missing", nCells);
			if (found != byGrid)
				throw CommonLib::CExcBase("grid {0} cells: {1} candidates, by the grid rects {2}", nCells, (uint64_t)found.size(), (uint64_t)byGrid.size());
			res.nExact += exact.size();
			res.nCandidates += found.size();
		}

		for (int pass = 0; pass < 2; ++pass)      // warm up, then timed
		{
			auto start = std::chrono::steady_clock::now();
			for (const TRectD& q : queries)
				index.search(q, [](const TRectD&, int64_t) { return true; });
			res.dQuery = Seconds(start);
		}
		index.Flush();
		Log.InfoT("grid {0}, {1} cells: ok, {2} bytes", name, nCells, res.nBytes);
		g_gridResults.push_back(res);
	}

	template<class TTraits>
	int RunTest(const std::string& name, double dWorld, size_t nCount, const std::wstring& path, uint32_t nCache, bool bExtent = true)
	{
		try
		{
			CommonLib::CLogInfo testInfo(Log, "TestBPZOrderTree {0}, count: {1}", name, (uint64_t)nCount);
			TTester<TTraits> tester(name, dWorld, path, nCache, bExtent);
			tester.Run(nCount);
			testInfo.Complete("TestBPZOrderTree passed: {0}", name);
			return 0;
		}
		catch (std::exception& exc)
		{
			Log.Exc(exc);
			std::cout << "TestBPZOrderTree failed: " << name << std::endl;
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
	std::wstring storagePath = L"F:\\BPZOrderTreeTest.btdb";
#else
	std::wstring storagePath = L"/tmp/BPZOrderTreeTest.btdb";
#endif
	if (argc > 2)
	{
		std::string path = argv[2];
		storagePath.assign(path.begin(), path.end());
	}
	uint32_t nCache = argc > 3 ? (uint32_t)std::strtoul(argv[3], nullptr, 10) : 200;

	// world 1e8: int32 fixed point of degrees * 1e7 / 2 (about 2 cm), shifted to unsigned
	int nFailed = 0;
	nFailed += RunTest<zorder::TZOrderPointTraits<ZOrderPoint2DU32> >("point u32", 1e8, nCount, storagePath, nCache);
	nFailed += RunTest<zorder::TZOrderRectTraits<ZOrderRect2DU32> >("rect u32", 1e8, nCount, storagePath, nCache);
	nFailed += RunTest<zorder::TZOrderRectTraits<ZOrderRect2DU32> >("rect u32 *", 1e8, nCount, storagePath, nCache, false);
	nFailed += RunTest<zorder::TZOrderRectTraits<ZOrderRect2DU16> >("rect u16", 65000, nCount / 4, storagePath, nCache);
	nFailed += RunTest<zorder::TZOrderPointTraits<ZOrderPoint2DU64> >("point u64", 1e15, nCount / 4, storagePath, nCache);
	nFailed += RunTest<zorder::TZOrderRectTraits<ZOrderRect2DU64> >("rect u64", 1e15, nCount / 4, storagePath, nCache);

	try
	{
		for (uint64_t nCells : { (uint64_t)100000000, (uint64_t)1000000, (uint64_t)10000 })
		{
			RunGridTest<zorder::TZOrderPointTraits<ZOrderPoint2DU32> >("point u32", nCells, nCount, storagePath);
			RunGridTest<zorder::TZOrderRectTraits<ZOrderRect2DU32> >("rect u32", nCells, nCount, storagePath);
		}
	}
	catch (std::exception& exc)
	{
		Log.Exc(exc);
		std::cout << "grid test failed" << std::endl;
		++nFailed;
	}

	char line[512];
	std::cout << std::endl << "count " << nCount << " (u16 / u64: " << nCount / 4 << "), node cache " << nCache << ", page " << nPageSize << ", 5000 window queries" << std::endl;
	snprintf(line, sizeof(line), "%-10s | %8s %10s | %7s %10s %8s | %9s %8s %8s | %9s %8s %8s | %8s %10s %8s\n", "", "insert s", "insert B", "bulk s", "bulk B", "B/obj",
		"BIGMIN ms", "read/fnd", "seeks", "split ms", "read/fnd", "seeks", "R bulk s", "R bulk B", "R ms");
	std::cout << line;
	for (const SResult& r : g_results)
	{
		snprintf(line, sizeof(line), "%-10s | %8.2f %10llu | %7.2f %10llu %8.2f | %9.1f %8.2f %8llu | %9.1f %8.2f %8llu | %8.2f %10llu %8.1f\n", r.name.c_str(),
			r.dInsert, (unsigned long long)r.nInsertBytes, r.dBulk, (unsigned long long)r.nBulkBytes,
			(double)r.nBulkBytes / (r.name.find("u32") != std::string::npos ? nCount : nCount / 4),
			r.dQuery * 1000, r.nFound ? (double)r.nRead / r.nFound : 0.0, (unsigned long long)r.nSeeks,
			r.dQuerySplit * 1000, r.nFound ? (double)r.nReadSplit / r.nFound : 0.0, (unsigned long long)r.nSeeksSplit,
			r.dRBulk, (unsigned long long)r.nRBulkBytes, r.dRQuery * 1000);
		std::cout << line;
	}
	std::cout << "* - without an extent (the whole coordinate range)" << std::endl << std::endl;

	std::cout << "grid index (degrees, extent 6.8 x 2.5 deg), 2000 window queries" << std::endl;
	snprintf(line, sizeof(line), "%-10s %10s %10s %10s %9s %10s %12s %8s\n", "", "cells", "cell, m", "bytes", "B/object", "query ms", "candidates", "extra");
	std::cout << line;
	for (const SGridResult& r : g_gridResults)
	{
		snprintf(line, sizeof(line), "%-10s %10llu %10.3f %10llu %9.2f %10.1f %12llu %7.2f%%\n", r.name.c_str(), (unsigned long long)r.nCells,
			6.77 / r.nCells * 111000.0 * 0.66, (unsigned long long)r.nBytes, (double)r.nBytes / nCount, r.dQuery * 1000, (unsigned long long)r.nCandidates,
			r.nExact ? 100.0 * (r.nCandidates - r.nExact) / r.nExact : 0.0);
		std::cout << line;
	}
	std::cout << (nFailed ? std::to_string(nFailed) + " test(s) failed" : std::string("all tests passed")) << std::endl;
	return nFailed;
}
