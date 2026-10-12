// Test of the B+ tree with int64 keys and string values (TBPStringValueMap, values up to 1024 bytes).
//
// Leaves: keys with the NumLen diff encoder, values with CZlibStringComp. Inner nodes: NumLen keys and links.
// Values are generated from the key (no copy in memory): length 0..1024, every 97th value exactly 1024,
// every 101st empty; text-like content (words + numbers) so zlib has something to do.
// Every step re-opens the storage, so it also checks that the tree was persisted correctly.

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
#include "../../../../CommonLib/log/LogInfo.h"
#include "../../../../CommonLib/filesystem/File.h"
#include "../../../../CommonLib/stream/MemoryStream.h"
#include "../../../../CommonLib/stream/FixedMemoryStream.h"

#include "../../../StorageLib/Storage.h"
#include "../../../StorageLib/FilePager.h"
#include "../../../StorageLib/FileStorage.h"

#include "../../BPStringValueMap.h"
#include "../../BPTreeInnerNode.h"
#include "../../BPTreeMapLeafNode.h"
#include "../../BaseDIffEncoder.h"
#include "../../BaseNodeCompressor.h"
#include "../../BaseEncoder.h"
#include "../../Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../Compressor/StringCompressor/ZLibStringEncoder.h"

namespace
{
	struct CompInt64
	{
		bool LE(int64_t l, int64_t r) const { return l < r; }
		bool EQ(int64_t l, int64_t r) const { return l == r; }
		bool operator()(int64_t l, int64_t r) const { return l < r; }
	};

	typedef bptreedb::StringValue TString;
	typedef bptreedb::TValueDiffEncoder<int64_t, int64_t, bptreedb::CUnsignedNumLenEncoder> TKeyEncoder;
	typedef bptreedb::TBaseValueEncoder<int64_t, bptreedb::CUnsignedNumLenEncoder> TLinkEncoder;

	typedef bptreedb::BPTreeLeafNodeMap<int64_t, TString, bptreedb::TBaseNodeCompressor<int64_t, TString, TKeyEncoder, bptreedb::CZlibStringComp> > TLeafNode;
	typedef bptreedb::BPTreeInnerNode<int64_t, bptreedb::TBaseNodeCompressor<int64_t, int64_t, TKeyEncoder, TLinkEncoder> > TInnerNode;
	typedef bptreedb::TBPStringValueMap<int64_t, CompInt64, bptreedb::storage::IFilePager, TInnerNode, TLeafNode> TValueTree;

	const uint32_t nPageSize = 8192;
	const uint32_t nMaxValue = 1024;

	uint64_t Mix(uint64_t x)
	{
		x += 0x9E3779B97F4A7C15ull;
		x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
		x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
		return x ^ (x >> 31);
	}

	const char* g_words[] = { "alpha", "street", "river", "north", "house", "green", "data", "value", "tree",
		"page", "node", "map", "road", "city", "lake", "stone", "field", "bridge", "tower", "garden" };
	const size_t g_nWords = sizeof(g_words) / sizeof(g_words[0]);

	// the value of a key: deterministic, length 0..nMaxValue
	std::string ValueOf(int64_t key)
	{
		uint64_t h = Mix((uint64_t)key ^ 0x5555);
		uint32_t len;
		if (h % 97 == 0)
			len = nMaxValue;
		else if (h % 101 == 0)
			len = 0;
		else
			len = (uint32_t)((h >> 8) % (nMaxValue + 1));

		std::string s;
		s.reserve(len + 16);
		uint64_t r = h;
		while (s.size() < len)
		{
			r = Mix(r);
			if (r % 4 == 0)
				s += std::to_string(r % 100000);
			else
				s += g_words[r % g_nWords];
			s += ' ';
		}
		s.resize(len);
		return s;
	}

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	void SetParams(bptreedb::TCompressorParamsBasePtr pParams, int32_t nCompressLevel)
	{
		bptreedb::TCompressorParamsPtr pValues(new bptreedb::CompressorParams());
		pValues->SetIntParam("compressLevel", nCompressLevel);
		pValues->SetIntParam("compressRate", 3);
		pValues->SetIntParam("MaxLen", nMaxValue);
		pParams->AddCompressParams(pValues, bptreedb::eLeafValue);

		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eLeafKey);
		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eInnerKey);
		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eInnerValue);
	}

	void CheckValue(TValueTree::iterator& it, int64_t key)
	{
		if (it.IsNull())
			throw CommonLib::CExcBase("key not found: {0}", key);
		if (it.Key() != key)
			throw CommonLib::CExcBase("wrong key: {0}, expected {1}", it.Key(), key);

		const TString& val = it.Value();
		std::string expected = ValueOf(key);
		if (val.StrLen() != expected.size() || memcmp(val.CStr(), expected.data(), expected.size()) != 0)
			throw CommonLib::CExcBase("wrong value for key {0}: length {1}, expected {2}", key, val.StrLen(), (uint32_t)expected.size());
	}

	void FindAll(TValueTree& tree, const std::vector<int64_t>& keys, bool bMustExist)
	{
		for (int64_t key : keys)
		{
			auto it = tree.find(key);
			if (bMustExist)
				CheckValue(it, key);
			else if (!it.IsNull())
				throw CommonLib::CExcBase("removed key found: {0}", key);
		}
	}

	// walks the whole tree; sortedPresent = the keys expected in the tree, ascending
	void Travel(TValueTree& tree, const std::vector<int64_t>& sortedPresent)
	{
		auto it = tree.begin();
		size_t n = 0;
		while (!it.IsNull())
		{
			if (n >= sortedPresent.size())
				throw CommonLib::CExcBase("tree has more elements than expected, extra key {0}", it.Key());
			CheckValue(it, sortedPresent[n]);
			++n;
			it.Next();
		}
		if (n != sortedPresent.size())
			throw CommonLib::CExcBase("travel count {0}, expected {1}", (uint64_t)n, (uint64_t)sortedPresent.size());
	}

	void RemoveAll(TValueTree& tree, const std::vector<int64_t>& keys, bool bMustExist)
	{
		for (int64_t key : keys)
		{
			if (tree.remove(key) != bMustExist)
				throw CommonLib::CExcBase("remove({0}) returned {1}", key, std::string(bMustExist ? "false" : "true"));
		}
	}

	std::vector<int64_t> Sorted(std::vector<int64_t> v)
	{
		std::sort(v.begin(), v.end());
		return v;
	}

	int RunTest(const std::vector<int64_t>& insertOrder, const std::string& testName, const std::wstring& storagePath, int32_t nCompressLevel, uint32_t nNodeCache)
	{
		try
		{
			CommonLib::CLogInfo testInfo(Log, "TestStringValueTree {0}, count: {1}", testName, (uint64_t)insertOrder.size());

			CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false));
			int64_t nTreePage = -1;
			const size_t nCount = insertOrder.size();
			std::vector<int64_t> firstHalf(insertOrder.begin(), insertOrder.begin() + nCount / 2);
			std::vector<int64_t> secondHalf(insertOrder.begin() + nCount / 2, insertOrder.end());

			auto runStep = [&](const char* name, bool bCreate, const std::function<void(TValueTree&)>& step)
			{
				auto start = std::chrono::steady_clock::now();
				bptreedb::storage::CFileStoragePtr pStorage(new bptreedb::storage::CFileStorage(pAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
				pStorage->Open(storagePath.c_str(), bCreate, 0, nPageSize);
				bptreedb::storage::IFilePagerPtr pPager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, pStorage, 100);

				if (bCreate)
				{
					TValueTree tree(-1, pPager, pAlloc, 10, nPageSize, nMaxValue);
					bptreedb::TCompressorParamsBasePtr pParams(new bptreedb::CompressorParamsBase());
					SetParams(pParams, nCompressLevel);
					tree.InnitTree(pParams, false, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
					tree.Flush();
					nTreePage = tree.GetPageBTreeInfo();
				}
				else
				{
					TValueTree tree(nTreePage, pPager, pAlloc, nNodeCache, nPageSize, nMaxValue);
					step(tree);
					tree.Flush();
				}

				pPager->Flush();
				pStorage->Close();
				Log.InfoT("{0}: {1} s", std::string(name), Seconds(start));
			};

			runStep("create", true, nullptr);

			runStep("insert all", false, [&](TValueTree& tree) {
				for (int64_t key : insertOrder)
					tree.insert(key, ValueOf(key));
			});
			{
				CommonLib::file::TFilePtr file = CommonLib::file::CFileCreator::OpenFile(storagePath.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
				uint64_t nRaw = 0;
				for (int64_t key : insertOrder)
					nRaw += sizeof(int64_t) + ValueOf(key).size();
				Log.InfoT("storage size: {0}, raw data: {1}", (uint64_t)file->GetFileSize(), nRaw);
			}

			runStep("limits", false, [&](TValueTree& tree) {
				// longer than the maximum: rejected, the tree is unchanged
				std::string tooLong(nMaxValue + 1, 'x');
				bool bThrown = false;
				try { tree.insert(insertOrder[0], tooLong); }
				catch (std::exception&) { bThrown = true; }
				if (!bThrown)
					throw CommonLib::CExcBase("value of {0} bytes was accepted", (uint32_t)tooLong.size());
				auto it = tree.find(insertOrder[0]);
				CheckValue(it, insertOrder[0]);
			});

			runStep("info", false, [&](TValueTree& tree) {
				auto stat = tree.GetStatistics();
				stat->CalcNodesInTree();
				Log.InfoT("inner nodes: {0}, leaf nodes: {1}, keys: {2}", stat->m_nInnerNodeCount, stat->m_nLeafNodeCount, stat->m_nKeyCount);
				if (stat->m_nKeyCount != nCount)
					throw CommonLib::CExcBase("statistics: {0} keys, expected {1}", stat->m_nKeyCount, (uint64_t)nCount);
			});

			runStep("find all", false, [&](TValueTree& tree) { FindAll(tree, insertOrder, true); });
			runStep("find missing", false, [&](TValueTree& tree) {
				for (size_t n = 0; n < std::min<size_t>(nCount, 100000); ++n)
				{
					int64_t key = insertOrder[n] + 1;       // keys are even
					if (!tree.find(key).IsNull())
						throw CommonLib::CExcBase("missing key found: {0}", key);
				}
			});
			runStep("travel", false, [&](TValueTree& tree) { Travel(tree, Sorted(insertOrder)); });

			runStep("remove first half", false, [&](TValueTree& tree) { RemoveAll(tree, firstHalf, true); });
			runStep("find after remove", false, [&](TValueTree& tree) {
				FindAll(tree, firstHalf, false);
				FindAll(tree, secondHalf, true);
			});
			runStep("travel half", false, [&](TValueTree& tree) { Travel(tree, Sorted(secondHalf)); });
			runStep("remove removed", false, [&](TValueTree& tree) { RemoveAll(tree, firstHalf, false); });
			runStep("remove rest", false, [&](TValueTree& tree) { RemoveAll(tree, secondHalf, true); });
			runStep("travel empty", false, [&](TValueTree& tree) { Travel(tree, std::vector<int64_t>()); });

			std::vector<int64_t> reinsert(insertOrder.begin(), insertOrder.begin() + std::min<size_t>(nCount, 100000));
			runStep("re-insert", false, [&](TValueTree& tree) {
				for (int64_t key : reinsert)
					tree.insert(key, ValueOf(key));
			});
			runStep("find re-inserted", false, [&](TValueTree& tree) {
				FindAll(tree, reinsert, true);
				Travel(tree, Sorted(reinsert));
			});

			testInfo.Complete("TestStringValueTree passed: {0}", testName);
			return 0;
		}
		catch (std::exception& exc)
		{
			Log.Exc(exc);
			std::cout << "TestStringValueTree failed: " << testName << std::endl;
			return 1;
		}
	}
}

// returns the number of failed tests
int TestStringValueTrees(uint32_t nCount, const std::wstring& storagePath, int32_t nCompressLevel, uint32_t nNodeCache)
{
	// even keys in random order (odd keys stay free for "find missing"); 62 random bits, collisions are negligible
	std::vector<int64_t> randomOrder(nCount);
	for (uint32_t i = 0; i < nCount; ++i)
		randomOrder[i] = (int64_t)((Mix(i) >> 2) << 1);

	int nFailed = 0;
	nFailed += RunTest(randomOrder, "random order", storagePath, nCompressLevel, nNodeCache);
	nFailed += RunTest(Sorted(randomOrder), "sorted order", storagePath, nCompressLevel, nNodeCache);
	return nFailed;
}
