// Test of B+ trees with different key and value types, and with duplicate keys.
//
//   unique keys (TBPMap):
//     int64  -> double / float values      values: TShuffleZlibEncoder (byte planes + zlib)
//     double -> int64,  float -> int64     keys: TOrderedDiffEncoder (differences of the order-preserving bits)
//     string -> int64 (TBPSMap)            keys up to 256 bytes, CZlibStringComp
//   duplicate keys (TBPMultiMap, the tree key is (key, row id), no values - the "FieldType -> rowId" index):
//     int64, double, float                 TOrderedMultiKeyEncoder
//     string (up to 256 bytes)             TStringMultiKeyEncoder
//
// Every tree is checked against a std::map / std::set model after insert, after removing half and after
// re-opening the storage: the full iteration (order and values), every key, lower_bound / upper_bound of
// random probes, a duplicate insert, removes of missing entries, the tree structure.
// Float keys include -0.0 / +0.0, infinities, denormals and NaN (TOrderedComp is a total order).
//
// usage: TestBPTreeTypes [count] [storage file] [node cache]
//   count        - entries per tree, default 100000
//   storage file - default F:\BPTreeTypes.btdb (Windows), /tmp/BPTreeTypes.btdb (other)
//   node cache   - nodes kept in memory, default 50 (smaller than the trees: nodes are saved and loaded)

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>

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

#include "../../BPMap.h"
#include "../../BPSMap.h"
#include "../../BPMultiMap.h"
#include "../../BPTreeInnerNode.h"
#include "../../BPTreeMapLeafNode.h"
#include "../../BaseNodeCompressor.h"
#include "../../BaseEncoder.h"
#include "../../OrderedKey.h"
#include "../../Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../Compressor/StringCompressor/ZLibStringEncoder.h"
#include "../../Compressor/OrderedEncoder/OrderedDiffEncoder.h"
#include "../../Compressor/OrderedEncoder/ShuffleZlibEncoder.h"

using namespace bptreedb;

namespace
{
	const uint32_t nPageSize = 8192;
	const uint32_t nMaxStr = 256;
	uint32_t g_nCache = 50;
	std::wstring g_path;
	CommonLib::IAllocPtr g_pAlloc;

	typedef TBaseValueEncoder<int64_t, CUnsignedNumLenEncoder> TLinkEncoder;

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	struct SResult
	{
		std::string name;
		uint64_t nEntries{ 0 }, nBytes{ 0 }, nRaw{ 0 };
		double dInsert{ 0 };
	};
	std::vector<SResult> g_results;

	TCompressorParamsBasePtr MakeParams()
	{
		TCompressorParamsBasePtr pParams(new CompressorParamsBase());
		for (ECompressParams id : { eInnerKey, eInnerValue, eLeafKey, eLeafValue })
		{
			TCompressorParamsPtr p(new CompressorParams());
			p->SetIntParam("MaxLen", nMaxStr);
			p->SetIntParam("compressLevel", 6);
			p->SetIntParam("compressRate", 3);
			pParams->AddCompressParams(p, id);
		}
		return pParams;
	}

	struct SStorage
	{
		storage::CFileStoragePtr m_pStorage;
		storage::IFilePagerPtr m_pPager;

		explicit SStorage(bool bCreate)
		{
			m_pStorage.reset(new storage::CFileStorage(g_pAlloc, 1, storage::IStorageCipherPtr()));
			m_pStorage->Open(g_path.c_str(), bCreate, 0, nPageSize);
			m_pPager = std::make_shared<storage::CFilePager>(g_pAlloc, m_pStorage, 100);
		}
		~SStorage()
		{
			m_pPager->Flush();
			m_pStorage->Close();
		}
	};

	template <class T>
	bool BitsEqual(const T& a, const T& b) { return memcmp(&a, &b, sizeof(T)) == 0; }

	template <class T>
	std::string ToStr(const T& v) { return std::to_string(v); }
	std::string ToStr(const std::string& v) { return "\"" + v.substr(0, 40) + "\""; }

	//--------------------------------------------------------------------------------------------
	// data

	// float / double keys: normal values, a pool for duplicates, and the special values
	template <class T>
	std::vector<T> SpecialFloats()
	{
		typedef std::numeric_limits<T> L;
		return { T(0.0), -T(0.0), L::infinity(), -L::infinity(), L::denorm_min(), -L::denorm_min(), (L::min)(), (L::max)(), L::lowest(),
			L::quiet_NaN(), -L::quiet_NaN(), T(1.0), T(-1.0), L::epsilon() };
	}

	template <class T>
	T RandomNumber(std::mt19937_64& rng)
	{
		std::uniform_real_distribution<double> u(0.0, 1.0);
		if constexpr (std::is_floating_point<T>::value)
		{
			double r = u(rng);
			if (r < 0.6) return T((u(rng) - 0.3) * 1e6);                           // coordinates, prices
			if (r < 0.8) return T(std::floor(u(rng) * 1000) / 100);                // rounded values
			if (r < 0.95) return T(std::ldexp(u(rng) - 0.5, (int)(u(rng) * 200) - 100));  // wide range
			std::vector<T> sp = SpecialFloats<T>();
			return sp[(size_t)(u(rng) * sp.size()) % sp.size()];
		}
		else
			return (T)(rng() % 2000000000ull) - 1000000000;
	}

	std::string RandomString(std::mt19937_64& rng)
	{
		static const char* words[] = { "street", "main", "north", "river", "park", "lake", "Praha", "Brno", "hotel", "station", "school", "garden" };
		std::uniform_real_distribution<double> u(0.0, 1.0);
		double r = u(rng);
		if (r < 0.01) return std::string();
		size_t len = r < 0.05 ? nMaxStr : (size_t)(u(rng) * u(rng) * nMaxStr);
		std::string s;
		while (s.size() < len)
		{
			s += words[rng() % 12];
			s += (rng() % 3 == 0) ? std::to_string(rng() % 1000) : std::string(" ");
		}
		s.resize(len);
		return s;
	}

	template <class T>
	struct TModelComp
	{
		bool operator()(const T& a, const T& b) const { return TOrderedComp<T>()(a, b); }
	};
	template <>
	struct TModelComp<std::string>
	{
		bool operator()(const std::string& a, const std::string& b) const { return a < b; }
	};

	//--------------------------------------------------------------------------------------------
	// unique keys: TBPMap<K, V>

	template <class K, class V, class TKeyEncoder, class TValueEncoder>
	struct TMapTypes
	{
		typedef BPTreeLeafNodeMap<K, V, TBaseNodeCompressor<K, V, TKeyEncoder, TValueEncoder> > TLeaf;
		typedef BPTreeInnerNode<K, TBaseNodeCompressor<K, int64_t, TKeyEncoder, TLinkEncoder> > TInner;
		typedef TBPMap<K, V, TOrderedComp<K>, storage::IFilePager, TInner, TLeaf> TTree;
	};

	template <class K, class V, class TKeyEncoder, class TValueEncoder>
	void TestMap(const std::string& name, size_t nCount)
	{
		typedef typename TMapTypes<K, V, TKeyEncoder, TValueEncoder>::TTree TTree;
		typedef std::map<K, V, TModelComp<K> > TModel;
		CommonLib::CLogInfo info(Log, "map {0}", name);
		SResult res;
		res.name = name;

		std::mt19937_64 rng(77);
		TModel model;
		std::vector<K> keys;
		while (model.size() < nCount)
		{
			K k = RandomNumber<K>(rng);
			V v = RandomNumber<V>(rng);
			if (model.emplace(k, v).second)
				keys.push_back(k);
		}

		int64_t nInfo = -1;
		auto open = [&](bool bCreate, const std::function<void(TTree&)>& step)
		{
			SStorage storage(bCreate);
			TTree tree(bCreate ? -1 : nInfo, storage.m_pPager, g_pAlloc, g_nCache, nPageSize, false);
			if (bCreate)
				tree.InnitTree(MakeParams(), false, 0, storage::eEmptyType, 0, storage::eEmptyType);
			nInfo = tree.GetPageBTreeInfo();
			step(tree);
			tree.Flush();
		};

		auto check = [&](TTree& tree)
		{
			tree.CheckTreeStructure();
			auto mit = model.begin();
			size_t n = 0;
			for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++mit, ++n)
			{
				if (mit == model.end() || !BitsEqual(it.Key(), mit->first) || !BitsEqual(it.Value(), mit->second))
					throw CommonLib::CExcBase("{0}: iteration differs at {1}", name, (uint64_t)n);
			}
			if (mit != model.end())
				throw CommonLib::CExcBase("{0}: iteration ended at {1} of {2}", name, (uint64_t)n, (uint64_t)model.size());

			for (const auto& kv : model)
			{
				auto it = tree.find(kv.first);
				if (it.IsNull() || !BitsEqual(it.Value(), kv.second))
					throw CommonLib::CExcBase("{0}: find {1}", name, ToStr(kv.first));
			}

			std::mt19937_64 r(5);
			for (int i = 0; i < 2000; ++i)
			{
				K probe = (i % 2 && !keys.empty()) ? keys[r() % keys.size()] : RandomNumber<K>(r);
				auto mlo = model.lower_bound(probe);
				auto mup = model.upper_bound(probe);
				auto lo = tree.lower_bound(probe);
				auto up = tree.upper_bound(probe);
				if ((mlo == model.end()) != lo.IsNull() || (mlo != model.end() && !BitsEqual(lo.Key(), mlo->first)))
					throw CommonLib::CExcBase("{0}: lower_bound {1}", name, ToStr(probe));
				if ((mup == model.end()) != up.IsNull() || (mup != model.end() && !BitsEqual(up.Key(), mup->first)))
					throw CommonLib::CExcBase("{0}: upper_bound {1}", name, ToStr(probe));
			}
		};

		open(true, [&](TTree& tree) {
			auto start = std::chrono::steady_clock::now();
			for (const K& k : keys)
				tree.insert(k, model[k]);
			res.dInsert = Seconds(start);
			bool bThrown = false;
			try { tree.insert(keys[0], V()); } catch (std::exception&) { bThrown = true; }
			if (!bThrown)
				throw CommonLib::CExcBase("{0}: duplicate key accepted", name);
		});
		open(false, [&](TTree& tree) {
			check(tree);
			auto stat = tree.GetStatistics();
			stat->CalcNodesInTree();
			res.nEntries = model.size();
			res.nBytes = (stat->m_nInnerNodeCount + stat->m_nLeafNodeCount) * nPageSize;
			res.nRaw = model.size() * (sizeof(K) + sizeof(V));
		});
		open(false, [&](TTree& tree) {
			std::shuffle(keys.begin(), keys.end(), std::mt19937(3));
			for (size_t i = 0; i < keys.size() / 2; ++i)
			{
				if (!tree.remove(keys[i]))
					throw CommonLib::CExcBase("{0}: remove {1}", name, ToStr(keys[i]));
				model.erase(keys[i]);
			}
			if (tree.remove(keys[0]))
				throw CommonLib::CExcBase("{0}: removed key removed again", name);
			keys.erase(keys.begin(), keys.begin() + keys.size() / 2);
		});
		open(false, [&](TTree& tree) { check(tree); });
		g_results.push_back(res);
		info.Complete("map {0} ok", name);
	}

	//--------------------------------------------------------------------------------------------
	// unique string keys: TBPSMap<int64 value>

	void TestStringMap(size_t nCount)
	{
		typedef BPTreeLeafNodeMap<StringValue, int64_t, TBaseNodeCompressor<StringValue, int64_t, CZlibStringComp, TShuffleZlibEncoder<int64_t> > > TLeaf;
		typedef BPTreeInnerNode<StringValue, TBaseNodeCompressor<StringValue, int64_t, CZlibStringComp, TLinkEncoder> > TInner;
		typedef TBPSMap<int64_t, CompStringValue, storage::IFilePager, TInner, TLeaf> TTree;
		const std::string name = "string -> int64";
		CommonLib::CLogInfo info(Log, "map {0}", name);
		SResult res;
		res.name = name;

		std::mt19937_64 rng(99);
		std::map<std::string, int64_t> model;
		std::vector<std::string> keys;
		uint64_t nRaw = 0;
		while (model.size() < nCount)
		{
			std::string k = RandomString(rng);
			if (model.emplace(k, (int64_t)rng()).second)
			{
				keys.push_back(k);
				nRaw += k.size() + sizeof(int64_t);
			}
		}

		int64_t nInfo = -1;
		auto open = [&](bool bCreate, const std::function<void(TTree&)>& step)
		{
			SStorage storage(bCreate);
			TTree tree(bCreate ? -1 : nInfo, storage.m_pPager, g_pAlloc, g_nCache, nPageSize, nMaxStr, false);
			if (bCreate)
				tree.InnitTree(MakeParams(), false, 0, storage::eEmptyType, 0, storage::eEmptyType);
			nInfo = tree.GetPageBTreeInfo();
			step(tree);
			tree.Flush();
		};

		auto check = [&](TTree& tree)
		{
			tree.CheckTreeStructure();
			auto mit = model.begin();
			size_t n = 0;
			for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++mit, ++n)
			{
				if (mit == model.end() || std::string((const char*)it.Key().CStr()) != mit->first || it.Value() != mit->second)
					throw CommonLib::CExcBase("{0}: iteration differs at {1}", name, (uint64_t)n);
			}
			if (mit != model.end())
				throw CommonLib::CExcBase("{0}: iteration ended at {1}", name, (uint64_t)n);
			for (const auto& kv : model)
			{
				auto it = tree.find(kv.first);
				if (it.IsNull() || it.Value() != kv.second)
					throw CommonLib::CExcBase("{0}: find {1}", name, ToStr(kv.first));
			}
		};

		open(true, [&](TTree& tree) {
			auto start = std::chrono::steady_clock::now();
			for (const std::string& k : keys)
				tree.insert(k.c_str(), (uint32_t)k.size(), model[k]);
			res.dInsert = Seconds(start);
			bool bThrown = false;
			std::string tooLong(nMaxStr + 1, 'x');
			try { tree.insert(tooLong.c_str(), (uint32_t)tooLong.size(), 1); } catch (std::exception&) { bThrown = true; }
			if (!bThrown)
				throw CommonLib::CExcBase("{0}: a key of {1} bytes accepted", name, nMaxStr + 1);
			bThrown = false;
			try { tree.insert(keys[0].c_str(), (uint32_t)keys[0].size(), 1); } catch (std::exception&) { bThrown = true; }
			if (!bThrown)
				throw CommonLib::CExcBase("{0}: duplicate key accepted", name);
		});
		open(false, [&](TTree& tree) {
			check(tree);
			auto stat = tree.GetStatistics();
			stat->CalcNodesInTree();
			res.nEntries = model.size();
			res.nBytes = (stat->m_nInnerNodeCount + stat->m_nLeafNodeCount) * nPageSize;
			res.nRaw = nRaw;
		});
		open(false, [&](TTree& tree) {
			std::shuffle(keys.begin(), keys.end(), std::mt19937(3));
			for (size_t i = 0; i < keys.size() / 2; ++i)
			{
				if (!tree.remove(keys[i]))
					throw CommonLib::CExcBase("{0}: remove {1}", name, ToStr(keys[i]));
				model.erase(keys[i]);
			}
		});
		open(false, [&](TTree& tree) { check(tree); });
		g_results.push_back(res);
		info.Complete("map {0} ok", name);
	}

	//--------------------------------------------------------------------------------------------
	// duplicate keys: TBPMultiMap<K> (key, row id), no values

	template <class K>
	struct TMultiTypes
	{
		typedef TMultiKey<K> TMKey;
		typedef TOrderedMultiKeyEncoder<K> TKeyEncoder;
		typedef TOrderedComp<K> TComp;
		typedef K TModelKey;
		static K ToKey(const K& k, TOrderedComp<K>*) { return k; }
	};

	template <>
	struct TMultiTypes<StringValue>
	{
		typedef TMultiKey<StringValue> TMKey;
		typedef TStringMultiKeyEncoder TKeyEncoder;
		typedef CompStringValue TComp;
		typedef std::string TModelKey;
	};

	template <class K>
	void TestMultiMap(const std::string& name, size_t nCount, size_t nDistinct)
	{
		typedef TMultiTypes<K> TTypes;
		typedef typename TTypes::TMKey TMKey;
		typedef typename TTypes::TModelKey TModelKey;
		typedef BPTreeLeafNodeMap<TMKey, uint8_t, TBaseNodeCompressor<TMKey, uint8_t, typename TTypes::TKeyEncoder, TNullValueEncoder<uint8_t> > > TLeaf;
		typedef BPTreeInnerNode<TMKey, TBaseNodeCompressor<TMKey, int64_t, typename TTypes::TKeyEncoder, TLinkEncoder> > TInner;
		typedef TBPMultiMap<K, typename TTypes::TComp, uint8_t, storage::IFilePager, TInner, TLeaf> TTree;
		constexpr bool IsString = std::is_same<K, StringValue>::value;

		struct SEntryComp
		{
			bool operator()(const std::pair<TModelKey, int64_t>& a, const std::pair<TModelKey, int64_t>& b) const
			{
				TModelComp<TModelKey> c;
				if (c(a.first, b.first)) return true;
				if (c(b.first, a.first)) return false;
				return a.second < b.second;
			}
		};
		typedef std::set<std::pair<TModelKey, int64_t>, SEntryComp> TModel;

		CommonLib::CLogInfo info(Log, "multimap {0}", name);
		SResult res;
		res.name = name;

		// a pool of distinct keys, entries pick keys from it (a few keys very often)
		std::mt19937_64 rng(1234);
		std::uniform_real_distribution<double> u(0.0, 1.0);
		std::set<TModelKey, TModelComp<TModelKey> > poolSet;
		while (poolSet.size() < nDistinct)
		{
			if constexpr (IsString) poolSet.insert(RandomString(rng));
			else poolSet.insert(RandomNumber<K>(rng));
		}
		std::vector<TModelKey> pool(poolSet.begin(), poolSet.end());
		std::shuffle(pool.begin(), pool.end(), std::mt19937(9));
		TModel model;
		std::vector<std::pair<TModelKey, int64_t> > entries;
		uint64_t nRaw = 0;
		for (size_t i = 0; i < nCount; ++i)
		{
			const TModelKey& k = pool[(size_t)(std::pow(u(rng), 3.0) * pool.size())];
			int64_t row = (int64_t)i * 7 + 1;      // row ids
			model.emplace(k, row);
			entries.emplace_back(k, row);
			if constexpr (IsString) nRaw += k.size() + sizeof(int64_t);
			else nRaw += sizeof(K) + sizeof(int64_t);
		}
		std::shuffle(entries.begin(), entries.end(), std::mt19937(11));

		int64_t nInfo = -1;
		auto open = [&](bool bCreate, const std::function<void(TTree&)>& step)
		{
			SStorage storage(bCreate);
			TTree tree(bCreate ? -1 : nInfo, storage.m_pPager, g_pAlloc, g_nCache, nPageSize, nMaxStr);
			if (bCreate)
				tree.InnitTree(MakeParams(), false, 0, storage::eEmptyType, 0, storage::eEmptyType);
			nInfo = tree.GetPageBTreeInfo();
			step(tree);
			tree.Flush();
		};

		auto key = [](TTree& tree, const TModelKey& k) -> K
		{
			if constexpr (IsString) return tree.MakeKey(k);
			else return k;
		};
		auto modelKey = [](const K& k) -> TModelKey
		{
			if constexpr (IsString) return std::string((const char*)k.CStr());
			else return k;
		};
		auto sameKey = [](const TModelKey& a, const TModelKey& b)
		{
			TModelComp<TModelKey> c;
			return !c(a, b) && !c(b, a) && (IsString || memcmp(&a, &b, sizeof(TModelKey)) == 0);
		};

		auto check = [&](TTree& tree)
		{
			tree.CheckTreeStructure();
			auto mit = model.begin();
			size_t n = 0;
			for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++mit, ++n)
			{
				if (mit == model.end() || !sameKey(modelKey(it.Key().m_key), mit->first) || it.Key().m_nSub != mit->second)
					throw CommonLib::CExcBase("{0}: iteration differs at {1}", name, (uint64_t)n);
			}
			if (mit != model.end())
				throw CommonLib::CExcBase("{0}: iteration ended at {1} of {2}", name, (uint64_t)n, (uint64_t)model.size());

			// every key of the pool: all its rows, in row order; lower / upper bound of the key
			for (const TModelKey& k : pool)
			{
				auto lo = model.lower_bound(std::make_pair(k, (std::numeric_limits<int64_t>::min)()));
				std::vector<int64_t> expected;
				for (auto m = lo; m != model.end() && sameKey(m->first, k); ++m)
					expected.push_back(m->second);
				std::vector<int64_t> rows;
				K tk = key(tree, k);
				tree.ForEach(tk, [&rows](int64_t row, const uint8_t&) { rows.push_back(row); return true; });
				if (rows != expected)
					throw CommonLib::CExcBase("{0}: key {1}: {2} rows, expected {3}", name, ToStr(k), (uint64_t)rows.size(), (uint64_t)expected.size());
				if (tree.count(tk) != expected.size())
					throw CommonLib::CExcBase("{0}: count {1}", name, ToStr(k));

				auto up = model.upper_bound(std::make_pair(k, (std::numeric_limits<int64_t>::max)()));
				auto tlo = tree.lower_bound_key(tk);
				auto tup = tree.upper_bound_key(tk);
				if ((lo == model.end()) != tlo.IsNull() || (lo != model.end() && (!sameKey(modelKey(tlo.Key().m_key), lo->first) || tlo.Key().m_nSub != lo->second)))
					throw CommonLib::CExcBase("{0}: lower_bound_key {1}", name, ToStr(k));
				if ((up == model.end()) != tup.IsNull() || (up != model.end() && (!sameKey(modelKey(tup.Key().m_key), up->first) || tup.Key().m_nSub != up->second)))
					throw CommonLib::CExcBase("{0}: upper_bound_key {1}", name, ToStr(k));
				if (!expected.empty() && (!tree.contains(tk, expected[0]) || tree.contains(tk, expected[0] + 3)))
					throw CommonLib::CExcBase("{0}: contains {1}", name, ToStr(k));
			}
		};

		open(true, [&](TTree& tree) {
			auto start = std::chrono::steady_clock::now();
			for (const auto& e : entries)
				tree.insert(key(tree, e.first), e.second);
			res.dInsert = Seconds(start);
			bool bThrown = false;
			try { tree.insert(key(tree, entries[0].first), entries[0].second); } catch (std::exception&) { bThrown = true; }
			if (!bThrown)
				throw CommonLib::CExcBase("{0}: duplicate (key, row) accepted", name);
			if constexpr (IsString)
			{
				bThrown = false;
				try { tree.MakeKey(std::string(nMaxStr + 1, 'y')); } catch (std::exception&) { bThrown = true; }
				if (!bThrown)
					throw CommonLib::CExcBase("{0}: a key of {1} bytes accepted", name, nMaxStr + 1);
			}
		});
		open(false, [&](TTree& tree) {
			check(tree);
			auto stat = tree.GetStatistics();
			stat->CalcNodesInTree();
			res.nEntries = model.size();
			res.nBytes = (stat->m_nInnerNodeCount + stat->m_nLeafNodeCount) * nPageSize;
			res.nRaw = nRaw;
		});
		open(false, [&](TTree& tree) {
			for (size_t i = 0; i < entries.size() / 2; ++i)
			{
				if (!tree.remove(key(tree, entries[i].first), entries[i].second))
					throw CommonLib::CExcBase("{0}: remove {1} row {2}", name, ToStr(entries[i].first), entries[i].second);
				model.erase(entries[i]);
			}
			if (tree.remove(key(tree, entries[0].first), entries[0].second) || tree.remove(key(tree, entries.back().first), -1))
				throw CommonLib::CExcBase("{0}: remove of a missing entry returned true", name);
		});
		open(false, [&](TTree& tree) { check(tree); });

		// bulk load into a new tree: the same content
		open(true, [&](TTree& tree) {
			std::vector<TMKey> sorted;
			for (const auto& e : model)
				sorted.emplace_back(key(tree, e.first), e.second);
			struct SSource
			{
				const std::vector<TMKey>& v;
				size_t i;
				bool Next(TMKey& k, uint8_t& val) { if (i == v.size()) return false; k = v[i++]; val = 0; return true; }
			} source{ sorted, 0 };
			tree.BulkLoad(source);
		});
		open(false, [&](TTree& tree) { check(tree); });

		g_results.push_back(res);
		info.Complete("multimap {0} ok", name);
	}

	template <class TFunc>
	int Run(const char* pszName, TFunc func)
	{
		try
		{
			func();
			return 0;
		}
		catch (std::exception& exc)
		{
			Log.Exc(exc);
			std::cout << "failed: " << pszName << std::endl;
			return 1;
		}
	}
}

int main(int argc, char* argv[])
{
	std::shared_ptr<CommonLib::IlogWriter> pLogWriter(new CommonLib::CConsoleLogWriter());
	Log.InnitLog(std::shared_ptr<CommonLib::IlogLineFormat>(), pLogWriter);
	g_pAlloc.reset(new CommonLib::CSimpleAlloc(false));

	size_t nCount = argc > 1 ? (size_t)std::strtoul(argv[1], nullptr, 10) : 100000;
#ifdef _WIN32
	g_path = L"F:\\BPTreeTypes.btdb";
#else
	g_path = L"/tmp/BPTreeTypes.btdb";
#endif
	if (argc > 2)
	{
		std::string path = argv[2];
		g_path.assign(path.begin(), path.end());
	}
	if (argc > 3)
		g_nCache = (uint32_t)std::strtoul(argv[3], nullptr, 10);

	int nFailed = 0;
	nFailed += Run("int64 -> double", [&] { TestMap<int64_t, double, TOrderedDiffEncoder<int64_t>, TShuffleZlibEncoder<double> >("int64 -> double", nCount); });
	nFailed += Run("int64 -> float", [&] { TestMap<int64_t, float, TOrderedDiffEncoder<int64_t>, TShuffleZlibEncoder<float> >("int64 -> float", nCount); });
	nFailed += Run("double -> int64", [&] { TestMap<double, int64_t, TOrderedDiffEncoder<double>, TShuffleZlibEncoder<int64_t> >("double -> int64", nCount); });
	nFailed += Run("float -> int64", [&] { TestMap<float, int64_t, TOrderedDiffEncoder<float>, TShuffleZlibEncoder<int64_t> >("float -> int64", nCount); });
	nFailed += Run("double -> double", [&] { TestMap<double, double, TOrderedDiffEncoder<double>, TShuffleZlibEncoder<double> >("double -> double", nCount); });
	nFailed += Run("string -> int64", [&] { TestStringMap(nCount); });
	nFailed += Run("multi int64", [&] { TestMultiMap<int64_t>("multi int64", nCount, nCount / 50); });
	nFailed += Run("multi double", [&] { TestMultiMap<double>("multi double", nCount, nCount / 50); });
	nFailed += Run("multi float", [&] { TestMultiMap<float>("multi float", nCount, nCount / 50); });
	nFailed += Run("multi string", [&] { TestMultiMap<StringValue>("multi string", nCount, nCount / 50); });

	char line[256];
	std::cout << std::endl << "entries per tree " << nCount << " (after remove: half), page " << nPageSize << ", node cache " << g_nCache << std::endl;
	snprintf(line, sizeof(line), "%-18s %10s %8s %12s %12s %8s %10s\n", "", "entries", "insert s", "pages B", "raw B", "% raw", "B/entry");
	std::cout << line;
	for (const SResult& r : g_results)
	{
		snprintf(line, sizeof(line), "%-18s %10llu %8.2f %12llu %12llu %7llu%% %10.2f\n", r.name.c_str(), (unsigned long long)r.nEntries, r.dInsert,
			(unsigned long long)r.nBytes, (unsigned long long)r.nRaw, (unsigned long long)(r.nRaw ? r.nBytes * 100 / r.nRaw : 0),
			r.nEntries ? (double)r.nBytes / r.nEntries : 0.0);
		std::cout << line;
	}
	std::cout << (nFailed ? std::to_string(nFailed) + " test(s) failed" : std::string("all tests passed")) << std::endl;
	return nFailed;
}
