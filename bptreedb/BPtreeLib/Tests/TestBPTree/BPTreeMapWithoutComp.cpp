#include "pch.h"
#include <chrono>
#include "../../../UtilsLib/DataGenerator/TestDataGenerator.h"
#include "../../../UtilsLib/DataGenerator/Random/IntegerValueGenerator.h"
#include "../../../UtilsLib/DataGenerator/Random/RandomIntegerValueGenerator.h"
#include "../../Compressor/EntropyEncoder/SignedNumLenEncoder.h"
#include "../../Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../BaseEncoder.h"

#include "PerfLog/PerfLog.h"
#include <functional>
#include <algorithm>
#include <vector>
#include "TestRestrictedValueEncoder.h"

typedef bptreedb::TEmptyValueEncoder<int64_t> TKeyEncoder;
typedef bptreedb::TEmptyValueEncoder<int64_t> TLinkEncoder;
typedef bptreedb::TEmptyValueEncoder<int64_t> TValueEncoder;

//typedef bptreedb::TValueDiffEncoder<int64_t, int64_t, TInt64Encoder> TValueEncoder;
typedef bptreedb::BPTreeLeafNodeMap<int64_t, int64_t,  bptreedb::TBaseNodeCompressor<int64_t, int64_t, TKeyEncoder, TValueEncoder > > TLeafNode;
typedef bptreedb::BPTreeInnerNode<int64_t, bptreedb::TBaseNodeCompressor<int64_t, int64_t, TKeyEncoder, TLinkEncoder > > TInnerNode;
typedef bptreedb::TBPMap<int64_t, int64_t, comp<int64_t>, 	bptreedb::storage::IFilePager, TInnerNode, TLeafNode> TBPMapNoComp;

typedef bptreedb::utils::TDataGenerator<int64_t, bptreedb::utils::TRandomInegerGenerator<int64_t> > TTestRandomDataGenerator;
typedef std::shared_ptr<TTestRandomDataGenerator> TTestRandomDataGeneratorPtr;


typedef bptreedb::utils::TDataGenerator<int64_t, bptreedb::utils::TInegerGenerator<int64_t> > TOrderTestDataGenerator;
typedef std::shared_ptr<TOrderTestDataGenerator> TOrderTestDataGeneratorPtr;

typedef std::shared_ptr<bptreedb::utils::IDataGenerator<int64_t>> TDataGeneratorPtr;


typedef bptreedb::TValueDiffEncoder<int64_t, int64_t, bptreedb::CUnsignedNumLenEncoder> TKeyNumLenEncoder;
typedef bptreedb::TBaseValueEncoder<int64_t,  bptreedb::CUnsignedNumLenEncoder> TLinkNumLenEncoder;
typedef bptreedb::TValueDiffEncoder<int64_t, int64_t, bptreedb::CSignedNumLenEncoder> TValueNumLenEncoder;

typedef bptreedb::BPTreeLeafNodeMap<int64_t, int64_t,  bptreedb::TBaseNodeCompressor<int64_t, int64_t, TKeyNumLenEncoder, TValueNumLenEncoder > > TLeafNodeNumLen;
typedef bptreedb::BPTreeInnerNode<int64_t, bptreedb::TBaseNodeCompressor<int64_t, int64_t, TKeyNumLenEncoder, TLinkNumLenEncoder > > TInnerNodeNumLen;
typedef bptreedb::TBPMap<int64_t, int64_t, comp<int64_t>, bptreedb::storage::IFilePager, TInnerNodeNumLen, TLeafNodeNumLen> TBPMapNumLen;


static const int _nCount = 20;
typedef bptreedb::TTestRestrictedValueEncoder<int64_t, _nCount> TRestrictedKeyEncoder;
typedef bptreedb::TTestRestrictedValueEncoder<int64_t, _nCount> TRestrictedLinkEncoder;
typedef bptreedb::TTestRestrictedValueEncoder<int64_t, _nCount> TRestrictedValueEncoder;

typedef bptreedb::BPTreeLeafNodeMap<int64_t, int64_t,  bptreedb::TBaseNodeCompressor<int64_t, int64_t, TRestrictedKeyEncoder, TRestrictedValueEncoder > > TRestrictedLeafNode;
typedef bptreedb::BPTreeInnerNode<int64_t, bptreedb::TBaseNodeCompressor<int64_t, int64_t, TRestrictedKeyEncoder, TRestrictedLinkEncoder > > TRestrictedInnerNode;
typedef bptreedb::TBPMap<int64_t, int64_t, comp<int64_t>, 	bptreedb::storage::IFilePager, TRestrictedInnerNode, TRestrictedLeafNode> TRestrictedBPMap;



template <class TBPTree, class TStorage>
uint64_t CreateBPTreeMap(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, TStorage pStorage, bool minSplit)
{
	try
	{
		CommonLib::CLogInfo info(Log, "CreateBPTreeMap");

		TBPTree tree(-1, pStorage, pAlloc, 10, nPageSize, minSplit);


		int nMaxCount = getenv("MAXCOUNT") ? atoi(getenv("MAXCOUNT")) : 8192 * 8;

		bptreedb::TCompressorParamsBasePtr pCompParmas(new bptreedb::CompressorParamsBase());
		bptreedb::TCompressorParamsPtr pCompInnerKey(new bptreedb::CompressorParams());
		bptreedb::TCompressorParamsPtr pCompInnerValue(new bptreedb::CompressorParams());
		bptreedb::TCompressorParamsPtr pCompLeafKey(new bptreedb::CompressorParams());
		bptreedb::TCompressorParamsPtr pCompLeafValuey(new bptreedb::CompressorParams());

		pCompInnerKey->SetIntParam("compressLevel", 3);
		pCompInnerKey->SetIntParam("compressRate", 4);
		pCompInnerKey->SetIntParam("MaxCount", nMaxCount);

		pCompParmas->AddCompressParams(pCompInnerKey, bptreedb::eInnerKey);

		pCompInnerValue->SetIntParam("compressLevel", 3);
		pCompInnerValue->SetIntParam("compressRate", 4);
		pCompInnerValue->SetIntParam("MaxCount", nMaxCount);

		pCompParmas->AddCompressParams(pCompInnerValue, bptreedb::eInnerValue);

		pCompLeafKey->SetIntParam("compressLevel", 3);
		pCompLeafKey->SetIntParam("compressRate", 4);
		pCompLeafKey->SetIntParam("MaxCount", nMaxCount);
		if (getenv("COUNTMODE"))
			pCompLeafKey->SetIntParam("CountLimitMode", 1);
		if (getenv("SPLITFIT"))
			pCompLeafKey->SetIntParam("SplitAtFit", 1);
		if (getenv("PENDING"))
			pCompLeafKey->SetIntParam("PendingLimit", atoi(getenv("PENDING")));

		pCompParmas->AddCompressParams(pCompLeafKey, bptreedb::eLeafKey);

		pCompLeafValuey->SetIntParam("compressLevel", 3);
		pCompLeafValuey->SetIntParam("compressRate", 4);
		pCompLeafValuey->SetIntParam("MaxCount", nMaxCount);

		pCompParmas->AddCompressParams(pCompLeafValuey, bptreedb::eLeafValue);

		tree.InnitTree(pCompParmas, minSplit, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
		tree.Flush();

		info.Complete();
		return tree.GetPageBTreeInfo();
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed create BPTreeSet", exe);
		throw;
	}
}





template <class TBPTree, class TStorage>
void InsertBPTreeMap(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage, uint32_t nNodeCache, int64_t nBegin, int64_t nEnd, bool minSplit, TDataGeneratorPtr pDataGenerator)
{
	try
	{
		int64_t nCount = abs(nEnd - nBegin);
		int64_t nStep = nCount / 100;

		if (nStep == 0)
			nStep = 1;

		CommonLib::CLogInfo info(Log, "InsertBPTreeMap count: {0}", nCount);

		TBPTree tree(nBPTreePage, pStorage, pAlloc, nNodeCache, nPageSize, minSplit);
		tree.SetBPTreePerfCounter(pPerf);

		for (int64_t i = nBegin; i < nEnd; i++)
		{
			int64_t val = (int64_t)pDataGenerator->GetValue((uint32_t)i);
			tree.insert(val, i);
			if (i % nStep == 0)
				std::cout << i << "  " << ((i - nBegin) * 100) / nCount << " %" << '\r';
		}

		tree.Flush();
		info.Complete("InsertBPTreeMap finish");
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed to insert BPTreeMap", exe);
		throw;
	}
}


// Bulk load of [nBegin, nEnd): pairs (key, idx) sorted by key, then TBPMap::BulkLoad
template <class TBPTree, class TStorage>
void BulkLoadBPTreeMap(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage, uint32_t nNodeCache, int64_t nBegin, int64_t nEnd, bool minSplit, TDataGeneratorPtr pDataGenerator)
{
	try
	{
		int64_t nCount = abs(nEnd - nBegin);
		CommonLib::CLogInfo info(Log, "BulkLoadBPTreeMap count: {0}", nCount);

		std::vector<std::pair<int64_t, int64_t>> data;
		data.reserve((size_t)nCount);
		for (int64_t i = nBegin; i < nEnd; i++)
			data.emplace_back((int64_t)pDataGenerator->GetValue((uint32_t)i), i);

		auto t0 = std::chrono::steady_clock::now();
		std::sort(data.begin(), data.end());
		auto t1 = std::chrono::steady_clock::now();

		struct TSource
		{
			const std::vector<std::pair<int64_t, int64_t>>& m_data;
			size_t m_nPos{ 0 };
			bool Next(int64_t& key, int64_t& value)
			{
				if (m_nPos >= m_data.size())
					return false;
				key = m_data[m_nPos].first;
				value = m_data[m_nPos].second;
				++m_nPos;
				return true;
			}
		} source{ data };

		TBPTree tree(nBPTreePage, pStorage, pAlloc, nNodeCache, nPageSize, minSplit);
		tree.SetBPTreePerfCounter(pPerf);
		uint64_t nLoaded = tree.BulkLoad(source);
		auto t2 = std::chrono::steady_clock::now();

		Log.InfoT("bulk load: sort {0} s, build {1} s, elements {2}",
			std::chrono::duration<double>(t1 - t0).count(), std::chrono::duration<double>(t2 - t1).count(), nLoaded);
		if (getenv("CHECKTREE"))
			tree.CheckTreeStructure();
		info.Complete("BulkLoadBPTreeMap finish");
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed to bulk load BPTreeMap", exe);
		throw;
	}
}


// bMustExist = true : every key of [nBegin, nEnd) must be found with value == idx
// bMustExist = false: none of the keys of [nBegin, nEnd) may be found (they were removed)
template <class TBPTree, class TStorage>
void FindBPTreeMap(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage, int64_t nBegin, int64_t nEnd, bool bMustExist, TDataGeneratorPtr pDataGenerator)
{
	try
	{
		int64_t nCount = abs(nEnd - nBegin);
		int64_t nStep = nCount / 100;
		if (nStep == 0)
			nStep = 1;

		CommonLib::CLogInfo info(Log, "FindBPTreeMap count: {0}, must exist: {1}", nCount, std::string(bMustExist ? "true" : "false"));
		TBPTree tree(nBPTreePage, pStorage, pAlloc, 10, nPageSize, false);
		tree.SetBPTreePerfCounter(pPerf);

		for (int64_t i = nBegin; i < nEnd; ++i)
		{
			int64_t val = (int64_t)pDataGenerator->GetValue((uint32_t)i);
			auto it = tree.find(val);

			if (bMustExist)
			{
				if (it.IsNull())
					throw CommonLib::CExcBase("Key {0} not found, idx {1}", val, i);

				if (it.Key() != val)
					throw CommonLib::CExcBase("Wrong key {0}, expected {1}, idx {2}", it.Key(), val, i);

				if (it.Value() != i)
					throw CommonLib::CExcBase("Wrong value {0}, expected {1}, key {2}", it.Value(), i, val);
			}
			else if (!it.IsNull())
			{
				throw CommonLib::CExcBase("Removed key {0} still found (value {1}), idx {2}", val, it.Value(), i);
			}

			if (i % nStep == 0)
				std::cout << i << "  " << ((i - nBegin) * 100) / nCount << " %" << '\r';
		}

		tree.Flush();
		info.Complete("FindBPTreeMap finish");
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed FindBPTreeMap", exe);
		throw;
	}
}


// Keys that were never inserted must not be found.
// The random generator produces uint32 values, so negative keys can't be in the tree.
template <class TBPTree, class TStorage>
void FindMissingBPTreeMap(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage, int64_t nCount)
{
	try
	{
		CommonLib::CLogInfo info(Log, "FindMissingBPTreeMap count: {0}", nCount);
		TBPTree tree(nBPTreePage, pStorage, pAlloc, 10, nPageSize, false);
		tree.SetBPTreePerfCounter(pPerf);

		for (int64_t i = 0; i < nCount; ++i)
		{
			int64_t val = -1 - i * 7919;
			auto it = tree.find(val);
			if (!it.IsNull())
				throw CommonLib::CExcBase("Missing key {0} was found, returned key {1}", val, it.Key());
		}

		info.Complete("FindMissingBPTreeMap finish");
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed FindMissingBPTreeMap", exe);
		throw;
	}
}


// bMustExist = true : every remove must return true
// bMustExist = false: keys are already removed, remove must return false
template <class TBPTree, class TStorage>
void DeleteFromBPTreeMap(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage, uint32_t nNodeCache, int64_t nBegin, int64_t nEnd, bool minSplit, bool bMustExist, TDataGeneratorPtr pDataGenerator)
{
	try
	{
		int64_t nCount = abs(nEnd - nBegin);
		int64_t nStep = nCount / 100;
		if (nStep == 0)
			nStep = 1;

		CommonLib::CLogInfo info(Log, "DeleteFromBPTreeMap count: {0}, must exist: {1}", nCount, std::string(bMustExist ? "true" : "false"));

		TBPTree tree(nBPTreePage, pStorage, pAlloc, nNodeCache, nPageSize, minSplit);
		tree.SetBPTreePerfCounter(pPerf);

		for (int64_t i = nBegin; i < nEnd; ++i)
		{
			int64_t val = (int64_t)pDataGenerator->GetValue((uint32_t)i);
			bool bRemoved = tree.remove(val);

			// CHECKEVERY=N: validate the whole tree after every N-th remove (slow, for debugging)
			static int64_t s_nCheckEvery = getenv("CHECKEVERY") ? atoll(getenv("CHECKEVERY")) : 0;
			if (s_nCheckEvery && (i - nBegin) % s_nCheckEvery == 0)
			{
				try { tree.CheckTreeStructure(); }
				catch (std::exception& e) { CommonLib::CExcBase::RegenExcT("tree check failed after remove idx {0} key {1}", i, val, e); throw; }
			}

			if (bRemoved != bMustExist)
				throw CommonLib::CExcBase("remove({0}) returned {1}, idx {2}", val, std::string(bRemoved ? "true" : "false"), i);

			if (i % nStep == 0)
				std::cout << i << "  " << ((i - nBegin) * 100) / nCount << " %" << '\r';
		}

		tree.Flush();
		info.Complete("DeleteFromBPTreeMap finish");
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed to delete from BPTreeMap", exe);
		throw;
	}
}


// Walks the whole tree from begin() and compares it with the expected content:
// the generator keys of [nValidBegin, nValidEnd) sorted by key, value == generator index.
// Checks order, keys, values and count in a single linear pass.
template <class TBPTree, class TStorage>
uint64_t TravelTree(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage, int64_t nValidBegin, int64_t nValidEnd, TDataGeneratorPtr pDataGenerator)
{
	try
	{
		std::vector<std::pair<int64_t, int64_t>> expected;
		expected.reserve((size_t)(nValidEnd - nValidBegin));
		for (int64_t i = nValidBegin; i < nValidEnd; ++i)
			expected.emplace_back((int64_t)pDataGenerator->GetValue((uint32_t)i), i);
		std::sort(expected.begin(), expected.end());

		CommonLib::CLogInfo info(Log, "TravelTree expected: {0}", (uint64_t)expected.size());

		TBPTree tree(nBPTreePage, pStorage, pAlloc, 10, nPageSize, false);
		tree.SetBPTreePerfCounter(pPerf);

		auto it = tree.begin();
		uint64_t nCount = 0;
		while (!it.IsNull())
		{
			if (nCount >= expected.size())
				throw CommonLib::CExcBase("Tree has more elements than expected {0}, extra key {1}", (uint64_t)expected.size(), it.Key());

			int64_t key = it.Key();
			int64_t value = it.Value();
			const auto& exp = expected[(size_t)nCount];

			if (key != exp.first)
				throw CommonLib::CExcBase("Wrong key at pos {0}: {1}, expected {2}", nCount, key, exp.first);

			if (value != exp.second)
				throw CommonLib::CExcBase("Wrong value at pos {0}: {1}, expected {2}, key {3}", nCount, value, exp.second, key);

			nCount += 1;
			it.Next();
		}

		if (nCount != expected.size())
			throw CommonLib::CExcBase("Travel count {0}, expected {1}", nCount, (uint64_t)expected.size());

		info.Complete("TravelTree elements {0}", nCount);
		return nCount;
	}
	catch (std::exception& exe)
	{
		CommonLib::CExcBase::RegenExc("Failed TravelTree", exe);
		throw;
	}
}




template <class TBPTree, class TStorage>
void InfoBPTree(CommonLib::IAllocPtr pAlloc, uint32_t nPageSize, CommonLib::TPrefCounterPtr pPerf, TStorage pStorage, int64_t nBPTreePage)
{

	Log.Info("InfoBPTree");


	TBPTree tree(nBPTreePage, pStorage, pAlloc, 10, nPageSize, false);
	tree.SetBPTreePerfCounter(pPerf);


	auto treeStat = tree.GetStatistics();
	treeStat->CalcNodesInTree();

	Log.InfoT("Inner Nodes: {0}, Leaf Nodes: {1}, Keys: {2}", treeStat->m_nInnerNodeCount, treeStat->m_nLeafNodeCount, treeStat->m_nKeyCount);


	if (treeStat->m_setHeights.size() == 1)
		Log.InfoT("Height: {0}, ", *treeStat->m_setHeights.begin());
	else
	{
		Log.Info(" Heights:");
		auto it = treeStat->m_setHeights.begin();
		auto end = treeStat->m_setHeights.end();

		for (; it != end; ++it)
		{
			Log.InfoT("{0}", *it);
		}
	}
}




// Per-generator data file name: every generator needs its own data file,
// TDataGenerator reuses an existing file if it has enough elements.
template<class TGenerator> struct TDataGeneratorName;
template<> struct TDataGeneratorName<TTestRandomDataGenerator> { static const wchar_t* Get() { return L"Random"; } };
template<> struct TDataGeneratorName<TOrderTestDataGenerator> { static const wchar_t* Get() { return L"Order"; } };


// Full BPMap test: create, insert, search, travel, remove, re-insert.
// TGenerator - key generator (key of element i is GetValue(i), value is i)
// nPageSize  - storage and node page size (multiple of 8192)
// nCount     - count of elements
template< class TBPMapType>
int TestBPMap(uint32_t nPageSize, int64_t nCount, TDataGeneratorPtr pDataGenerator, const std::string& testName)
{

	try {
#ifdef _WIN32
		std::wstring storagePath = L"F:\\BPMapTest.btdb";

#else
		std::wstring storagePath = L"/home/slava/BPMapTest.btdb";

#endif
		CommonLib::CLogInfo testInfo(Log, "TestBPMap generator: {0}, page size: {1}, count: {2}", testName, nPageSize, nCount);

		int64_t nBPTreePage = -1;
		uint32_t nNodeCache = 10000;
		int32_t storageId = 1;
		int64_t nBegin = 0;
		int64_t nEnd = nBegin + nCount;
		int64_t nMid = nBegin + (nEnd - nBegin) / 2;
		bool minSplit = false;


#ifdef NDEBUG
		CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false)); // no guard bytes / counters in perf runs
#else
		CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(true));
#endif
		CommonLib::TPrefCounterPtr pStoragePerformer(new CommonLib::CPerfCounter(20));
		CommonLib::TPrefCounterPtr pBPTreePerf(new CommonLib::CPerfCounter(20));

		// Opens the storage, runs one test step on a fresh pager, flushes and closes it.
		// Every step re-opens the file, so it also checks the tree was persisted correctly.
		auto runStep = [&](bool bCreate, const std::function<void(bptreedb::storage::IFilePagerPtr)>& step)
		{
			bptreedb::storage::CFileStoragePtr pStorage(new bptreedb::storage::CFileStorage(pAlloc, storageId, bptreedb::storage::IStorageCipherPtr()));
			pStorage->Open(storagePath.c_str(), bCreate, 0, nPageSize);
			pStorage->SetStoragePerformer(pStoragePerformer);
			pStoragePerformer->Reset();
			pBPTreePerf->Reset();

			bptreedb::storage::IFilePagerPtr ptrFilePager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, pStorage, 100);
			step(ptrFilePager);
			ptrFilePager->Flush();
			pStorage->Close();
		};

		// 1. create
		runStep(true, [&](bptreedb::storage::IFilePagerPtr pager) {
			nBPTreePage = CreateBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pager, minSplit);
		});

		// 2. insert [nBegin, nEnd)
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			if (getenv("BULK"))
				BulkLoadBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nNodeCache, nBegin, nEnd, minSplit, pDataGenerator);
			else
				InsertBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nNodeCache, nBegin, nEnd, minSplit, pDataGenerator);
		});
		{
			CommonLib::file::TFilePtr file = CommonLib::file::CFileCreator::OpenFile(storagePath.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
			Log.InfoT("Storage size: {0}", file->GetFileSize());
		}


		CPerfLog::LogBPtreeInsertPerf(pBPTreePerf);
		CPerfLog::LogBPtreePerf(pBPTreePerf);
		CPerfLog::LogPerfStorage(pStoragePerformer);

		// Info
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			InfoBPTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage);
		});


		// 3. search: all inserted keys are found with correct values
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			FindBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, nEnd, true, pDataGenerator);
		});
		CPerfLog::LogBPtreeFindPerf(pBPTreePerf);
		CPerfLog::LogPerfStorage(pStoragePerformer);

		// 4. search: keys never inserted are not found
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			FindMissingBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, 100000);
		});

		// 5. travel: full tree, sorted, count == N
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TravelTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, nEnd, pDataGenerator);
		});

		if (getenv("STOPAFTERTRAVEL"))
		{
			testInfo.Complete("TestBPMap stopped after travel, generator: {0}", testName);
			return 0;
		}

		// 6. remove first half [nBegin, nMid)
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			DeleteFromBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nNodeCache, nBegin, nMid, minSplit, true, pDataGenerator);
		});
		CPerfLog::LogBPtreeRemovePerf(pBPTreePerf);
		CPerfLog::LogPerfStorage(pStoragePerformer);

		// 7. removed keys are gone, remaining keys are intact
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			FindBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, nMid, false, pDataGenerator);
			FindBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nMid, nEnd, true, pDataGenerator);
		});

		// Info after removing
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			InfoBPTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage);
		});


		// 8. travel after partial remove: only [nMid, nEnd) left
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TravelTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nMid, nEnd, pDataGenerator);
		});

		// 9. removing already removed keys returns false and changes nothing
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			DeleteFromBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nNodeCache, nBegin, (std::min)(nMid, nBegin + 100000), minSplit, false, pDataGenerator);
		});

		// 10. remove the rest -> empty tree
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			DeleteFromBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nNodeCache, nMid, nEnd, minSplit, true, pDataGenerator);
		});
		CPerfLog::LogBPtreeRemovePerf(pBPTreePerf);
		CPerfLog::LogPerfStorage(pStoragePerformer);

		// 11. empty tree: travel sees nothing, nothing is found
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TravelTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, nBegin, pDataGenerator);
			FindBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, (std::min)(nEnd, nBegin + 100000), false, pDataGenerator);
		});

		// 12. tree is reusable after being emptied
		int64_t nReinsertEnd = (std::min)(nEnd, nBegin + 100000);
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			InsertBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nNodeCache, nBegin, nReinsertEnd, minSplit, pDataGenerator);
		});

		// Info
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			InfoBPTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage);
		});


		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			FindBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, nReinsertEnd, true, pDataGenerator);
			TravelTree<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pBPTreePerf, pager, nBPTreePage, nBegin, nReinsertEnd, pDataGenerator);
		});

		testInfo.Complete("TestBPMap passed, generator: {0}, page size: {1}, count: {2}", testName, nPageSize, nCount);
		return 0;
	}
	catch (std::exception& exe)
	{
		std::cout << "TestBPMap failed, generator: " << testName << ", page size: " << nPageSize << ", count: " << nCount << "\n";

		std::vector<std::string> msgChain = CommonLib::CExcBase::GetChainFromExc(exe);
		for (size_t i = 0, sz = msgChain.size(); i < sz; ++i)
		{
			if (i != 0)
				std::cout << "\r\n";

			std::cout << msgChain[i];
		}
		std::cout << std::endl;
		return 1;
	}
}


// Regression test for the value sign encoding (CBoolEncoder, PosBits mode):
// keys are ordered, values mostly decrease with a rare jump up, so the value diffs are
// mostly negative with a small positive minority. With a large page a leaf holds more
// than 32768 entries, so sign positions above int16 range are stored too.
template< class TBPMapType>
int TestBPMapSignPattern(uint32_t nPageSize, int64_t nCount)
{
	auto valueOf = [nCount](int64_t i) -> int64_t { return (i % 32 == 0) ? 3 * nCount + i : nCount - i; };

	try
	{
#ifdef _WIN32
		std::wstring storagePath = L"F:\\BPMapSignTest.btdb";
#else
		std::wstring storagePath = L"/home/slava/BPMapSignTest.btdb";
#endif
		CommonLib::CLogInfo testInfo(Log, "TestBPMapSignPattern page size: {0}, count: {1}", nPageSize, nCount);

		CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false));
		int32_t storageId = 1;
		int64_t nBPTreePage = -1;

		auto runStep = [&](bool bCreate, const std::function<void(bptreedb::storage::IFilePagerPtr)>& step)
		{
			bptreedb::storage::CFileStoragePtr pStorage(new bptreedb::storage::CFileStorage(pAlloc, storageId, bptreedb::storage::IStorageCipherPtr()));
			pStorage->Open(storagePath.c_str(), bCreate, 0, nPageSize);
			bptreedb::storage::IFilePagerPtr ptrFilePager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, pStorage, 100);
			step(ptrFilePager);
			ptrFilePager->Flush();
			pStorage->Close();
		};

		runStep(true, [&](bptreedb::storage::IFilePagerPtr pager) {
			nBPTreePage = CreateBPTreeMap<TBPMapType, bptreedb::storage::IFilePagerPtr>(pAlloc, nPageSize, pager, true);
		});

		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TBPMapType tree(nBPTreePage, pager, pAlloc, 1000, nPageSize, true); // min split: left leaves stay full (~65K entries)
			for (int64_t i = 0; i < nCount; ++i)
				tree.insert(i, valueOf(i));
			tree.Flush();
		});

		// small node cache: every leaf is decoded from disk
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TBPMapType tree(nBPTreePage, pager, pAlloc, 10, nPageSize, false);
			int64_t i = 0;
			for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++i)
			{
				if (it.Key() != i)
					throw CommonLib::CExcBase("Sign test: wrong key {0}, expected {1}", it.Key(), i);
				if (it.Value() != valueOf(i))
					throw CommonLib::CExcBase("Sign test: wrong value {0}, expected {1}, key {2}", it.Value(), valueOf(i), i);
			}
			if (i != nCount)
				throw CommonLib::CExcBase("Sign test: travel count {0}, expected {1}", i, nCount);
		});

		// remove every 3rd key (re-encodes and rebalances leaves), then check again
		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TBPMapType tree(nBPTreePage, pager, pAlloc, 1000, nPageSize, false);
			for (int64_t i = 0; i < nCount; i += 3)
				if (!tree.remove(i))
					throw CommonLib::CExcBase("Sign test: remove({0}) returned false", i);
			tree.Flush();
		});

		runStep(false, [&](bptreedb::storage::IFilePagerPtr pager) {
			TBPMapType tree(nBPTreePage, pager, pAlloc, 10, nPageSize, false);
			int64_t i = 1, n = 0;
			for (auto it = tree.begin(); !it.IsNull(); it.Next(), ++n)
			{
				if (i % 3 == 0)
					++i;
				if (it.Key() != i || it.Value() != valueOf(i))
					throw CommonLib::CExcBase("Sign test after remove: key {0} value {1}, expected key {2} value {3}", it.Key(), it.Value(), i, valueOf(i));
				++i;
			}
			int64_t nExpected = nCount - (nCount + 2) / 3;
			if (n != nExpected)
				throw CommonLib::CExcBase("Sign test after remove: count {0}, expected {1}", n, nExpected);
		});

		testInfo.Complete("TestBPMapSignPattern passed, page size: {0}, count: {1}", nPageSize, nCount);
		return 0;
	}
	catch (std::exception& exe)
	{
		std::cout << "TestBPMapSignPattern failed, page size: " << nPageSize << ", count: " << nCount << "\n";
		std::vector<std::string> msgChain = CommonLib::CExcBase::GetChainFromExc(exe);
		for (size_t i = 0, sz = msgChain.size(); i < sz; ++i)
		{
			if (i != 0)
				std::cout << "\r\n";
			std::cout << msgChain[i];
		}
		std::cout << std::endl;
		return 1;
	}
}


int TestBPMapBase()
{
	const int64_t nCount = 10000000; // 10000000 for the perf run (debug build is slow)
	int nFailed = 0;

	int64_t nBegin = 0;
	int64_t nEnd = nBegin + nCount;
#ifdef _WIN32
	std::wstring dataPath = std::wstring(L"F:\\BPMapTestData");
#else
	std::wstring dataPath = std::wstring(L"/home/slava/BPMapTestData");
#endif

	TDataGeneratorPtr pDataGenerator(new TOrderTestDataGenerator((uint32_t)nEnd, dataPath+ TDataGeneratorName<TOrderTestDataGenerator>::Get() + L".data"));
	pDataGenerator->Open();

	TDataGeneratorPtr pDataRandomGenerator(new TTestRandomDataGenerator((uint32_t)nEnd, dataPath+ TDataGeneratorName<TTestRandomDataGenerator>::Get() + L".data"));
	pDataRandomGenerator->Open();

	if(getenv("FULL") && (!getenv("ONLYRESTR") || getenv("SIGN"))) nFailed += TestBPMapSignPattern<TBPMapNumLen>(32768, 2000000);




	for (uint32_t nPageSize : { 8192u })
	{

		if (!getenv("NOCOMPONLY"))
		{
			CommonLib::CLogInfo info(Log, "Test  for correctness");
			if(getenv("FULL")) nFailed += TestBPMap<TRestrictedBPMap>(nPageSize, getenv("CNT") ? atoi(getenv("CNT")) : 1000000, pDataRandomGenerator, "random");
			if(getenv("ONLYRESTR")) return nFailed;
			if(getenv("FULL")) nFailed += TestBPMap<TRestrictedBPMap>(nPageSize, 1000000, pDataGenerator, "order");

			info.Complete();
		}

		if (!getenv("NOCOMPONLY"))
		{
			CommonLib::CLogInfo info(Log, "Test with compression");
			if(!getenv("ORDERONLY")) nFailed += TestBPMap<TBPMapNumLen>(nPageSize, nCount, pDataRandomGenerator, "random");
			nFailed += TestBPMap<TBPMapNumLen>(nPageSize, nCount, pDataGenerator, "order");

			info.Complete();
		}

		{
			CommonLib::CLogInfo info(Log, "Test without compression");
			if(!getenv("NONOCOMP")) nFailed += TestBPMap< TBPMapNoComp >(nPageSize, nCount, pDataRandomGenerator, "random");
			if(!getenv("NONOCOMP")) nFailed += TestBPMap< TBPMapNoComp>(nPageSize, nCount, pDataGenerator, "order");

			info.Complete();
		}
	}

	if (nFailed != 0)
		std::cout << "TestBPMapBase: " << nFailed << " test(s) failed" << std::endl;

	return nFailed == 0 ? 0 : 1;
}
