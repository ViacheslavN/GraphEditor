// Test of the B+ tree with string keys and string values.
//
// Keys and values are compressed with CZlibStringComp (leaves: keys and values, inner nodes: keys;
// links with the NumLen encoder). Every step re-opens the storage, so it also checks that the tree
// was persisted correctly.
//
// The second part (StringValueTest.cpp) tests TBPStringValueMap: int64 keys, string values up to 1024 bytes.
//
// usage: TestBPTreeStrings [count] [storage file] [zlib level] [node cache] [value tree count]
//   count        - number of key/value pairs, default 1000000
//   storage file - default F:\BPStringTest.btdb (Windows), /tmp/BPStringTest.btdb (other)
//   zlib level   - 1..9, default 6
//   node cache   - nodes kept in memory, default 10000
//   value tree count - pairs of the int64 -> string(<=1024) test, default 200000, 0 - skip

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <functional>
#include <cstdlib>
#include <cstring>

#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../../../CommonLib/log/logger.h"
#include "../../../../CommonLib/log/ConsoleLogWriter.h"
#include "../../../../CommonLib/log/LogInfo.h"
#include "../../../../CommonLib/perf/PerfCount.h"
#include "../../../../CommonLib/filesystem/File.h"
#include "../../../../CommonLib/stream/MemoryStream.h"
#include "../../../../CommonLib/stream/FixedMemoryStream.h"

#include "../../../StorageLib/Storage.h"
#include "../../../StorageLib/FilePager.h"
#include "../../../StorageLib/FileStorage.h"

#include "../../BPSMap.h"
#include "../../BPTStringMapLeafNode.h"
#include "../../BPTStringMapInnerNode.h"
#include "../../BaseNodeCompressor.h"
#include "../../BaseEncoder.h"
#include "../../Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../Compressor/StringCompressor/ZLibStringEncoder.h"


typedef bptreedb::StringValue TString;
typedef bptreedb::CZlibStringComp TStringEncoder;
typedef bptreedb::TBaseValueEncoder<int64_t, bptreedb::CUnsignedNumLenEncoder> TLinkEncoder;

typedef bptreedb::TBPTreeStringMapLeafNode<TString, bptreedb::TBaseNodeCompressor<TString, TString, TStringEncoder, TStringEncoder> > TLeafNode;
typedef bptreedb::TBPTreeStringMapInnerNode<bptreedb::TBaseNodeCompressor<TString, int64_t, TStringEncoder, TLinkEncoder> > TInnerNode;
typedef bptreedb::TBPSMap<TString, bptreedb::CompStringValue, bptreedb::storage::IFilePager, TInnerNode, TLeafNode> TStringTree;

static const uint32_t nPageSize = 8192;
static const uint32_t nMaxStr = 256;
static uint32_t nNodeCache = 10000;      // nodes in memory: with fewer nodes than leaves random inserts
                                        // recompress an evicted leaf on almost every insert
static int32_t nCompressLevel = 6;       // zlib level of keys and values


//------------------------------------------------------------------------------------------------
// test data: unique keys that look like names (words + number + unique suffix), values of variable
// length (some empty, some long)

static uint64_t SplitMix(uint64_t x)
{
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return x ^ (x >> 31);
}

static const char* g_words[] = {
	"Main", "Street", "Avenue", "Park", "Old", "New", "North", "South", "East", "West", "Lake", "River",
	"Hill", "Bridge", "Station", "Market", "Church", "School", "Garden", "Road", "Lane", "Square", "Castle", "Mill",
	"Forest", "Green", "Spring", "Valley", "Harbor", "Central", "Royal", "Little", "Upper", "Lower", "King", "Queen"
};
static const size_t g_nWords = sizeof(g_words) / sizeof(g_words[0]);

static std::string Base36(uint64_t v)
{
	static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	std::string s;
	do { s.push_back(digits[v % 36]); v /= 36; } while (v);
	std::reverse(s.begin(), s.end());
	return s;
}

struct STestData
{
	std::vector<std::string> keys;
	std::vector<std::string> values;
	std::vector<uint32_t> sortedOrder;     // indexes sorted by key

	void Generate(uint32_t nCount)
	{
		keys.resize(nCount);
		values.resize(nCount);
		for (uint32_t i = 0; i < nCount; ++i)
		{
			uint64_t h = SplitMix(i);
			std::string key = g_words[h % g_nWords];
			key += ' ';
			key += g_words[(h >> 8) % g_nWords];
			if ((h >> 16) % 3 == 0)
			{
				key += ' ';
				key += g_words[(h >> 24) % g_nWords];
			}
			key += ' ';
			key += std::to_string((h >> 32) % 1000);
			key += '#';
			key += Base36(i);           // unique
			keys[i] = key;

			uint64_t h2 = SplitMix(h);
			std::string value;
			if (h2 % 1000 != 0)         // 0.1% empty values
			{
				value = "id=" + std::to_string(i) + ";name=" + key;
				size_t nTags = (h2 >> 8) % 6;
				for (size_t t = 0; t < nTags; ++t)
				{
					value += ";tag";
					value += std::to_string(t);
					value += '=';
					value += g_words[(h2 >> (12 + t * 4)) % g_nWords];
				}
				if ((h2 >> 40) % 100 == 0)
					value.append(120, 'x');   // some long values
			}
			values[i] = value;
		}

		sortedOrder.resize(nCount);
		std::iota(sortedOrder.begin(), sortedOrder.end(), 0u);
		std::sort(sortedOrder.begin(), sortedOrder.end(), [this](uint32_t a, uint32_t b) { return keys[a] < keys[b]; });
	}
};


//------------------------------------------------------------------------------------------------

static double Seconds(std::chrono::steady_clock::time_point start)
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

static std::string ToStd(const TString& str)
{
	return std::string((const char*)str.CStr());
}

static void SetStringParams(bptreedb::TCompressorParamsBasePtr pParams, bptreedb::ECompressParams type)
{
	bptreedb::TCompressorParamsPtr p(new bptreedb::CompressorParams());
	p->SetIntParam("compressLevel", nCompressLevel);
	p->SetIntParam("compressRate", 3);       // start estimate, then the measured ratio of every node
	p->SetIntParam("MaxLen", nMaxStr);
	pParams->AddCompressParams(p, type);
}

int64_t CreateTree(CommonLib::IAllocPtr pAlloc, bptreedb::storage::IFilePagerPtr pStorage)
{
	CommonLib::CLogInfo info(Log, "Create string tree");

	TStringTree tree(-1, pStorage, pAlloc, 10, nPageSize, nMaxStr, false);

	bptreedb::TCompressorParamsBasePtr pParams(new bptreedb::CompressorParamsBase());
	SetStringParams(pParams, bptreedb::eInnerKey);
	SetStringParams(pParams, bptreedb::eLeafKey);
	SetStringParams(pParams, bptreedb::eLeafValue);
	bptreedb::TCompressorParamsPtr pLinks(new bptreedb::CompressorParams());
	pParams->AddCompressParams(pLinks, bptreedb::eInnerValue);

	tree.InnitTree(pParams, false, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
	tree.Flush();

	info.Complete();
	return tree.GetPageBTreeInfo();
}

void InsertRange(TStringTree& tree, const STestData& data, const std::vector<uint32_t>& order, size_t nBegin, size_t nEnd)
{
	for (size_t n = nBegin; n < nEnd; ++n)
	{
		uint32_t i = order[n];
		tree.insert(data.keys[i], data.values[i]);
	}
}

// every key of idxs must be found with its value (bMustExist) or must not be found
void FindAll(TStringTree& tree, const STestData& data, const std::vector<uint32_t>& idxs, bool bMustExist)
{
	for (uint32_t i : idxs)
	{
		auto it = tree.find(data.keys[i]);
		if (bMustExist)
		{
			if (it.IsNull())
				throw CommonLib::CExcBase("key not found: {0}, idx {1}", data.keys[i], i);
			if (ToStd(it.Key()) != data.keys[i])
				throw CommonLib::CExcBase("wrong key: {0}, expected {1}", ToStd(it.Key()), data.keys[i]);
			if (ToStd(it.Value()) != data.values[i])
				throw CommonLib::CExcBase("wrong value for key {0}: '{1}', expected '{2}'", data.keys[i], ToStd(it.Value()), data.values[i]);
		}
		else if (!it.IsNull())
			throw CommonLib::CExcBase("removed key found: {0}, idx {1}", data.keys[i], i);
	}
}

// walks the whole tree: keys in order, values, count == the expected (sorted) set
uint64_t Travel(TStringTree& tree, const STestData& data, const std::vector<bool>& present)
{
	auto it = tree.begin();
	size_t nPos = 0;
	uint64_t nCount = 0;
	while (!it.IsNull())
	{
		while (nPos < data.sortedOrder.size() && !present[data.sortedOrder[nPos]])
			++nPos;
		if (nPos >= data.sortedOrder.size())
			throw CommonLib::CExcBase("tree has more elements than expected, extra key {0}", ToStd(it.Key()));

		uint32_t i = data.sortedOrder[nPos];
		std::string key = ToStd(it.Key());
		if (key != data.keys[i])
			throw CommonLib::CExcBase("travel: wrong key at {0}: {1}, expected {2}", nCount, key, data.keys[i]);
		if (ToStd(it.Value()) != data.values[i])
			throw CommonLib::CExcBase("travel: wrong value for key {0}", key);

		++nPos;
		++nCount;
		it.Next();
	}

	uint64_t nExpected = (uint64_t)std::count(present.begin(), present.end(), true);
	if (nCount != nExpected)
		throw CommonLib::CExcBase("travel count {0}, expected {1}", nCount, nExpected);
	return nCount;
}

void RemoveAll(TStringTree& tree, const STestData& data, const std::vector<uint32_t>& idxs, bool bMustExist)
{
	for (uint32_t i : idxs)
	{
		bool bRemoved = tree.remove(data.keys[i]);
		if (bRemoved != bMustExist)
			throw CommonLib::CExcBase("remove({0}) returned {1}", data.keys[i], std::string(bRemoved ? "true" : "false"));
	}
}

void TreeInfo(TStringTree& tree)
{
	auto stat = tree.GetStatistics();
	stat->CalcNodesInTree();
	Log.InfoT("inner nodes: {0}, leaf nodes: {1}, keys: {2}", stat->m_nInnerNodeCount, stat->m_nLeafNodeCount, stat->m_nKeyCount);
}


// one full test: insert in the given order, search, travel, remove, re-insert
int TestStringTree(const STestData& data, const std::vector<uint32_t>& insertOrder, const std::string& testName, const std::wstring& storagePath)
{
	try
	{
		CommonLib::CLogInfo testInfo(Log, "TestStringTree {0}, count: {1}", testName, (uint64_t)data.keys.size());

		CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false));
		int32_t storageId = 1;
		int64_t nTreePage = -1;
		const size_t nCount = insertOrder.size();
		const size_t nMid = nCount / 2;

		std::vector<uint32_t> firstHalf(insertOrder.begin(), insertOrder.begin() + nMid);
		std::vector<uint32_t> secondHalf(insertOrder.begin() + nMid, insertOrder.end());
		std::vector<bool> present(data.keys.size(), false);

		auto runStep = [&](const char* name, bool bCreate, const std::function<void(TStringTree*)>& step)
		{
			auto start = std::chrono::steady_clock::now();
			bptreedb::storage::CFileStoragePtr pStorage(new bptreedb::storage::CFileStorage(pAlloc, storageId, bptreedb::storage::IStorageCipherPtr()));
			pStorage->Open(storagePath.c_str(), bCreate, 0, nPageSize);
			bptreedb::storage::IFilePagerPtr pPager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, pStorage, 100);

			if (bCreate)
				nTreePage = CreateTree(pAlloc, pPager);
			else
			{
				TStringTree tree(nTreePage, pPager, pAlloc, nNodeCache, nPageSize, nMaxStr, false);
				step(&tree);
				tree.Flush();
			}

			pPager->Flush();
			pStorage->Close();
			Log.InfoT("{0}: {1} s", std::string(name), Seconds(start));
		};

		runStep("create", true, nullptr);

		runStep("insert all", false, [&](TStringTree* tree) {
			InsertRange(*tree, data, insertOrder, 0, nCount);
			for (uint32_t i : insertOrder) present[i] = true;
		});
		{
			CommonLib::file::TFilePtr file = CommonLib::file::CFileCreator::OpenFile(storagePath.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
			uint64_t nRaw = 0;
			for (size_t i = 0; i < data.keys.size(); ++i)
				nRaw += data.keys[i].size() + data.values[i].size();
			Log.InfoT("storage size: {0}, raw strings: {1}", (uint64_t)file->GetFileSize(), nRaw);
		}

		runStep("info", false, [&](TStringTree* tree) { TreeInfo(*tree); });
		runStep("find all", false, [&](TStringTree* tree) { FindAll(*tree, data, insertOrder, true); });

		runStep("find missing", false, [&](TStringTree* tree) {
			for (size_t n = 0; n < std::min<size_t>(nCount, 100000); ++n)
			{
				std::string key = data.keys[insertOrder[n]] + "~missing";
				if (!tree->find(key).IsNull())
					throw CommonLib::CExcBase("missing key found: {0}", key);
			}
		});

		runStep("travel", false, [&](TStringTree* tree) { Travel(*tree, data, present); });

		runStep("remove first half", false, [&](TStringTree* tree) {
			RemoveAll(*tree, data, firstHalf, true);
			for (uint32_t i : firstHalf) present[i] = false;
		});
		runStep("find after remove", false, [&](TStringTree* tree) {
			FindAll(*tree, data, firstHalf, false);
			FindAll(*tree, data, secondHalf, true);
		});
		runStep("travel half", false, [&](TStringTree* tree) { Travel(*tree, data, present); });
		runStep("remove removed", false, [&](TStringTree* tree) { RemoveAll(*tree, data, firstHalf, false); });

		runStep("remove rest", false, [&](TStringTree* tree) {
			RemoveAll(*tree, data, secondHalf, true);
			for (uint32_t i : secondHalf) present[i] = false;
		});
		runStep("travel empty", false, [&](TStringTree* tree) {
			if (Travel(*tree, data, present) != 0)
				throw CommonLib::CExcBase("tree isn't empty");
		});

		// the emptied tree is reusable
		std::vector<uint32_t> reinsert(insertOrder.begin(), insertOrder.begin() + std::min<size_t>(nCount, 100000));
		runStep("re-insert", false, [&](TStringTree* tree) {
			InsertRange(*tree, data, reinsert, 0, reinsert.size());
			for (uint32_t i : reinsert) present[i] = true;
		});
		runStep("find re-inserted", false, [&](TStringTree* tree) {
			FindAll(*tree, data, reinsert, true);
			Travel(*tree, data, present);
		});

		testInfo.Complete("TestStringTree passed: {0}", testName);
		return 0;
	}
	catch (std::exception& exc)
	{
		Log.Exc(exc);
		std::cout << "TestStringTree failed: " << testName << std::endl;
		return 1;
	}
}

int TestStringValueTrees(uint32_t nCount, const std::wstring& storagePath, int32_t nCompressLevel, uint32_t nNodeCache);   // StringValueTest.cpp

int main(int argc, char* argv[])
{
	std::shared_ptr<CommonLib::IlogWriter> pLogWriter(new CommonLib::CConsoleLogWriter());
	Log.InnitLog(std::shared_ptr<CommonLib::IlogLineFormat>(), pLogWriter);

	uint32_t nCount = argc > 1 ? (uint32_t)std::strtoul(argv[1], nullptr, 10) : 1000000;
#ifdef _WIN32
	std::wstring storagePath = L"F:\\BPStringTest.btdb";
#else
	std::wstring storagePath = L"/tmp/BPStringTest.btdb";
#endif
	if (argc > 2)
	{
		std::string path = argv[2];
		storagePath.assign(path.begin(), path.end());
	}
	if (argc > 3)
		nCompressLevel = std::atoi(argv[3]);
	if (argc > 4)
		nNodeCache = (uint32_t)std::strtoul(argv[4], nullptr, 10);
	uint32_t nValueTreeCount = argc > 5 ? (uint32_t)std::strtoul(argv[5], nullptr, 10) : 1000000;
	Log.InfoT("zlib level: {0}, node cache: {1}", nCompressLevel, nNodeCache);

	auto start = std::chrono::steady_clock::now();
	STestData data;
	data.Generate(nCount);
	Log.InfoT("test data: {0} pairs, {1} s", (uint64_t)nCount, Seconds(start));

	std::vector<uint32_t> randomOrder(nCount);
	std::iota(randomOrder.begin(), randomOrder.end(), 0u);    // generation order: keys in random order

	int nFailed = 0;
	nFailed += TestStringTree(data, randomOrder, "random order", storagePath);
	nFailed += TestStringTree(data, data.sortedOrder, "sorted order", storagePath);

	// int64 keys -> string values up to 1024 bytes
	if (nValueTreeCount != 0)
		nFailed += TestStringValueTrees(nValueTreeCount, storagePath, nCompressLevel, nNodeCache);

	if (nFailed != 0)
		std::cout << nFailed << " test(s) failed" << std::endl;
	else
		std::cout << "all tests passed" << std::endl;

	return nFailed == 0 ? 0 : 1;
}
