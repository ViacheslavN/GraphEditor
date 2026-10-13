// Test of the B+ tree with int64 keys and blob values (TBPBlobValueMap).
//
// Blob sizes are generated from the key: empty, small (inline, < 1024), exactly 1024 (last inline size),
// 1025 (first stream size), multi-page and a few big ones (100K+). Some blobs are inserted from a stream.
// The content is read back through IBlob in random chunk sizes, so reads cross the stream page borders.
// A pager decorator records dropped pages: every page of a removed blob's chain must be dropped.
// Every step re-opens the storage, so it also checks that the tree and the blobs were persisted.
//
// usage: TestBPTreeBlobs [count] [storage file] [node cache]
//   count        - number of key/blob pairs, default 20000
//   storage file - default F:\BPBlobTest.btdb (Windows), /tmp/BPBlobTest.btdb (other)
//   node cache   - nodes kept in memory, default 1000

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
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
#include "../../../../CommonLib/stream/MemoryStream.h"
#include "../../../../CommonLib/stream/FixedMemoryStream.h"

#include "../../../StorageLib/Storage.h"
#include "../../../StorageLib/FilePager.h"
#include "../../../StorageLib/FileStorage.h"

#include "../../BPBlobValueMap.h"
#include "../../BPTreeInnerNode.h"
#include "../../BPTreeMapLeafNode.h"
#include "../../BaseDIffEncoder.h"
#include "../../BaseNodeCompressor.h"
#include "../../BaseEncoder.h"
#include "../../Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../Compressor/BlobCompressor/BlobValueEncoder.h"

namespace
{
	struct CompInt64
	{
		bool LE(int64_t l, int64_t r) const { return l < r; }
		bool EQ(int64_t l, int64_t r) const { return l == r; }
		bool operator()(int64_t l, int64_t r) const { return l < r; }
	};

	typedef bptreedb::TValueDiffEncoder<int64_t, int64_t, bptreedb::CUnsignedNumLenEncoder> TKeyEncoder;
	typedef bptreedb::TBaseValueEncoder<int64_t, bptreedb::CUnsignedNumLenEncoder> TLinkEncoder;

	typedef bptreedb::BPTreeLeafNodeMap<int64_t, bptreedb::BlobValue, bptreedb::TBaseNodeCompressor<int64_t, bptreedb::BlobValue, TKeyEncoder, bptreedb::CBlobValueEncoder> > TLeafNode;
	typedef bptreedb::BPTreeInnerNode<int64_t, bptreedb::TBaseNodeCompressor<int64_t, int64_t, TKeyEncoder, TLinkEncoder> > TInnerNode;
	typedef bptreedb::TBPBlobValueMap<int64_t, CompInt64, bptreedb::storage::IFilePager, TInnerNode, TLeafNode> TBlobTree;

	const uint32_t nPageSize = 8192;
	const uint32_t nMaxInline = 1024;

	// forwards to CFilePager, records the dropped pages
	class CDropRecordingPager : public bptreedb::storage::IFilePager
	{
	public:
		CDropRecordingPager(bptreedb::storage::IFilePagerPtr pPager, std::set<int64_t>& dropped) : m_pPager(pPager), m_dropped(dropped) {}

		virtual int64_t GetNewFilePageAddr() { return m_pPager->GetNewFilePageAddr(); }
		virtual bptreedb::storage::IFilePagePtr GetNewPage(int64_t nAddr) { return m_pPager->GetNewPage(nAddr); }
		virtual bptreedb::storage::IFilePagePtr ReadPage(int64_t nAddr) { return m_pPager->ReadPage(nAddr); }
		virtual void ReadPage(bptreedb::storage::IFilePagePtr ptrPage, int64_t nAddr) { m_pPager->ReadPage(ptrPage, nAddr); }
		virtual void SavePage(bptreedb::storage::IFilePagePtr ptrPage) { m_pPager->SavePage(ptrPage); }
		virtual void DropFilePage(int64_t nAddr) { m_dropped.insert(nAddr); m_pPager->DropFilePage(nAddr); }
		virtual void Flush() { m_pPager->Flush(); }

	private:
		bptreedb::storage::IFilePagerPtr m_pPager;
		std::set<int64_t>& m_dropped;
	};

	uint64_t Mix(uint64_t x)
	{
		x += 0x9E3779B97F4A7C15ull;
		x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
		x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
		return x ^ (x >> 31);
	}

	int64_t BlobSizeOf(int64_t key)
	{
		uint64_t h = Mix((uint64_t)key ^ 0xB10B);
		switch (h % 100)
		{
		case 0: case 1: case 2:            return 0;
		case 3: case 4: case 5: case 6:    return nMaxInline;          // the biggest inline blob
		case 7: case 8: case 9: case 10:   return nMaxInline + 1;      // the smallest stream blob
		case 11:                           return 100000 + (int64_t)((h >> 8) % 200000);
		default: break;
		}
		if (h % 100 < 60)
			return 1 + (int64_t)((h >> 8) % (nMaxInline - 1));     // inline
		return nMaxInline + 1 + (int64_t)((h >> 8) % 30000);     // 1..4 stream pages
	}

	void BlobOf(int64_t key, std::vector<byte_t>& data)
	{
		data.resize((size_t)BlobSizeOf(key));
		uint64_t r = Mix((uint64_t)key);
		for (size_t i = 0; i < data.size(); ++i)
		{
			if ((i & 7) == 0)
				r = Mix(r + i);
			data[i] = (byte_t)(r >> ((i & 7) * 8));
		}
	}

	bool FromStream(int64_t key) { return (Mix((uint64_t)key ^ 0x57) % 7) == 0; }

	void InsertBlob(TBlobTree& tree, int64_t key)
	{
		std::vector<byte_t> data;
		BlobOf(key, data);
		if (FromStream(key))
		{
			CommonLib::CReadMemoryStream src;
			if (!data.empty())
				src.AttachBuffer(data.data(), data.size());
			tree.insert(key, &src, (int64_t)data.size());
		}
		else
			tree.insert(key, data);
	}

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	void SetParams(bptreedb::TCompressorParamsBasePtr pParams)
	{
		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eLeafValue);
		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eLeafKey);
		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eInnerKey);
		pParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), bptreedb::eInnerValue);
	}

	// reads the blob in chunks of different sizes and compares with the expected content
	void CheckBlob(bptreedb::IBlobPtr pBlob, int64_t key)
	{
		if (pBlob.get() == nullptr)
			throw CommonLib::CExcBase("key not found: {0}", key);

		std::vector<byte_t> expected;
		BlobOf(key, expected);

		if (pBlob->Size() != (int64_t)expected.size())
			throw CommonLib::CExcBase("key {0}: blob size {1}, expected {2}", key, pBlob->Size(), (uint64_t)expected.size());

		bool bInline = expected.size() <= nMaxInline;
		if (pBlob->IsInline() != bInline)
			throw CommonLib::CExcBase("key {0}: size {1}, inline: {2}", key, (uint64_t)expected.size(), (int)pBlob->IsInline());

		CommonLib::ReadStreamPtr pStream = pBlob->GetReadStream();
		pBlob.reset();     // the stream keeps the blob alive

		std::vector<byte_t> data(expected.size());
		uint64_t r = Mix((uint64_t)key ^ 0xC4);
		size_t nPos = 0;
		while (nPos < data.size())
		{
			r = Mix(r);
			size_t nChunk = std::min<size_t>(data.size() - nPos, 1 + (size_t)(r % 5000));
			pStream->ReadBytes(data.data() + nPos, nChunk);
			nPos += nChunk;
		}
		if (data != expected)
			throw CommonLib::CExcBase("key {0}: wrong blob content, size {1}", key, (uint64_t)expected.size());

		// past the end of an inline blob (a page stream doesn't know the blob size)
		if (bInline)
		{
			bool bThrown = false;
			try { byte_t b; pStream->ReadBytes(&b, 1); }
			catch (std::exception&) { bThrown = true; }
			if (!bThrown)
				throw CommonLib::CExcBase("key {0}: read past the end of the blob", key);
		}
	}

	// GetReadStream() again rewinds the blob's stream
	void CheckRewind(bptreedb::IBlobPtr pBlob, int64_t key)
	{
		std::vector<byte_t> expected;
		BlobOf(key, expected);
		if (expected.size() < 2)
			return;

		byte_t b[2];
		CommonLib::ReadStreamPtr pStream = pBlob->GetReadStream();
		pStream->ReadBytes(b, 2);
		pStream = pBlob->GetReadStream();
		pStream->ReadBytes(b, 1);
		if (b[0] != expected[0])
			throw CommonLib::CExcBase("key {0}: GetReadStream didn't rewind", key);
	}

	void FindAll(TBlobTree& tree, const std::vector<int64_t>& keys, bool bMustExist)
	{
		for (int64_t key : keys)
		{
			bptreedb::IBlobPtr pBlob = tree.GetBlob(key);
			if (bMustExist)
			{
				CheckRewind(pBlob, key);
				CheckBlob(pBlob, key);
			}
			else if (pBlob.get() != nullptr)
				throw CommonLib::CExcBase("removed key found: {0}", key);
		}
	}

	void Travel(TBlobTree& tree, const std::vector<int64_t>& sortedPresent)
	{
		auto it = tree.begin();
		size_t n = 0;
		while (!it.IsNull())
		{
			if (n >= sortedPresent.size())
				throw CommonLib::CExcBase("tree has more elements than expected, extra key {0}", it.Key());
			if (it.Key() != sortedPresent[n])
				throw CommonLib::CExcBase("wrong key: {0}, expected {1}", it.Key(), sortedPresent[n]);
			CheckBlob(tree.GetBlob(it), it.Key());
			++n;
			it.Next();
		}
		if (n != sortedPresent.size())
			throw CommonLib::CExcBase("travel count {0}, expected {1}", (uint64_t)n, (uint64_t)sortedPresent.size());
	}

	// the pages of a blob's chain (page header: int64 next, int64 prev)
	std::vector<int64_t> ChainOf(bptreedb::storage::IFilePagerPtr pPager, int64_t nAddr)
	{
		std::vector<int64_t> pages;
		while (nAddr != -1)
		{
			pages.push_back(nAddr);
			bptreedb::storage::IFilePagePtr pPage = pPager->ReadPage(nAddr);
			nAddr = pPage->GetReadStream()->ReadInt64();
			if (pages.size() > 1000000)
				throw CommonLib::CExcBase("page chain loop");
		}
		return pages;
	}

	void RemoveAll(TBlobTree& tree, bptreedb::storage::IFilePagerPtr pPager, const std::set<int64_t>& dropped, const std::vector<int64_t>& keys, bool bMustExist)
	{
		for (int64_t key : keys)
		{
			std::vector<int64_t> chain;
			if (bMustExist)
			{
				auto it = tree.find(key);
				if (it.IsNull())
					throw CommonLib::CExcBase("key to remove not found: {0}", key);
				if (!it.Value().IsInline())
				{
					chain = ChainOf(pPager, it.Value().Addr());
					int64_t nPayload = (int64_t)pPager->ReadPage(chain[0])->GetSize() - 2 * sizeof(int64_t);   // page header: next, prev
					int64_t nStreamSize = tree.GetBlob(it)->Size() + (int64_t)sizeof(int64_t);                  // size header + bytes
					size_t nExpected = (size_t)((nStreamSize + nPayload - 1) / nPayload);
					if (chain.size() != nExpected)
						throw CommonLib::CExcBase("key {0}: {1} pages in the chain, expected {2}", key, (uint64_t)chain.size(), (uint64_t)nExpected);
				}
			}

			if (tree.remove(key) != bMustExist)
				throw CommonLib::CExcBase("remove({0}) returned {1}", key, std::string(bMustExist ? "false" : "true"));

			for (int64_t nPage : chain)
				if (dropped.find(nPage) == dropped.end())
					throw CommonLib::CExcBase("key {0}: blob page {1} wasn't dropped", key, nPage);
		}
	}

	std::vector<int64_t> Sorted(std::vector<int64_t> v)
	{
		std::sort(v.begin(), v.end());
		return v;
	}

	int RunTest(const std::vector<int64_t>& insertOrder, const std::string& testName, const std::wstring& storagePath, uint32_t nNodeCache)
	{
		try
		{
			CommonLib::CLogInfo testInfo(Log, "TestBlobValueTree {0}, count: {1}", testName, (uint64_t)insertOrder.size());

			CommonLib::IAllocPtr pAlloc(new CommonLib::CSimpleAlloc(false));
			int64_t nTreePage = -1;
			const size_t nCount = insertOrder.size();
			std::vector<int64_t> firstHalf(insertOrder.begin(), insertOrder.begin() + nCount / 2);
			std::vector<int64_t> secondHalf(insertOrder.begin() + nCount / 2, insertOrder.end());
			std::set<int64_t> dropped;

			auto runStep = [&](const char* name, bool bCreate, const std::function<void(TBlobTree&, bptreedb::storage::IFilePagerPtr)>& step)
			{
				auto start = std::chrono::steady_clock::now();
				bptreedb::storage::CFileStoragePtr pStorage(new bptreedb::storage::CFileStorage(pAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
				pStorage->Open(storagePath.c_str(), bCreate, 0, nPageSize);
				bptreedb::storage::IFilePagerPtr pFilePager = std::make_shared<bptreedb::storage::CFilePager>(pAlloc, pStorage, 100);
				bptreedb::storage::IFilePagerPtr pPager = std::make_shared<CDropRecordingPager>(pFilePager, dropped);

				if (bCreate)
				{
					TBlobTree tree(-1, pPager, pAlloc, 10, nPageSize, nMaxInline);
					bptreedb::TCompressorParamsBasePtr pParams(new bptreedb::CompressorParamsBase());
					SetParams(pParams);
					tree.InnitTree(pParams, false, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
					tree.Flush();
					nTreePage = tree.GetPageBTreeInfo();
				}
				else
				{
					TBlobTree tree(nTreePage, pPager, pAlloc, nNodeCache, nPageSize, nMaxInline);
					step(tree, pPager);
					tree.Flush();
				}

				pPager->Flush();
				pStorage->Close();
				Log.InfoT("{0}: {1} s", std::string(name), Seconds(start));
			};

			runStep("create", true, nullptr);

			runStep("insert all", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) {
				for (int64_t key : insertOrder)
					InsertBlob(tree, key);
			});

			runStep("duplicate key", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) {
				// an existing key: rejected, the pages of the new blob are dropped, the old blob is intact
				std::vector<byte_t> big(3 * nPageSize, 0xAB);   // 4 stream pages (a page has less than nPageSize for data)
				size_t nDropped = dropped.size();
				bool bThrown = false;
				try { tree.insert(insertOrder[0], big); }
				catch (std::exception&) { bThrown = true; }
				if (!bThrown)
					throw CommonLib::CExcBase("duplicate key was accepted");
				if (dropped.size() != nDropped + 4)
					throw CommonLib::CExcBase("duplicate key: {0} pages dropped, expected 4", (uint64_t)(dropped.size() - nDropped));
				CheckBlob(tree.GetBlob(insertOrder[0]), insertOrder[0]);
			});

			runStep("find all", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) { FindAll(tree, insertOrder, true); });
			runStep("travel", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) { Travel(tree, Sorted(insertOrder)); });

			runStep("remove first half", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr pPager) { RemoveAll(tree, pPager, dropped, firstHalf, true); });
			runStep("find after remove", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) {
				FindAll(tree, firstHalf, false);
				FindAll(tree, secondHalf, true);
			});
			runStep("travel half", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) { Travel(tree, Sorted(secondHalf)); });
			runStep("remove removed", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr pPager) { RemoveAll(tree, pPager, dropped, firstHalf, false); });
			runStep("remove rest", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr pPager) { RemoveAll(tree, pPager, dropped, secondHalf, true); });
			runStep("travel empty", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) { Travel(tree, std::vector<int64_t>()); });

			std::vector<int64_t> reinsert(insertOrder.begin(), insertOrder.begin() + std::min<size_t>(nCount, 5000));
			runStep("re-insert", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) {
				for (int64_t key : reinsert)
					InsertBlob(tree, key);
			});
			runStep("find re-inserted", false, [&](TBlobTree& tree, bptreedb::storage::IFilePagerPtr) {
				FindAll(tree, reinsert, true);
				Travel(tree, Sorted(reinsert));
			});

			testInfo.Complete("TestBlobValueTree passed: {0}", testName);
			return 0;
		}
		catch (std::exception& exc)
		{
			Log.Exc(exc);
			std::cout << "TestBlobValueTree failed: " << testName << std::endl;
			return 1;
		}
	}
}

int main(int argc, char* argv[])
{
	std::shared_ptr<CommonLib::IlogWriter> pLogWriter(new CommonLib::CConsoleLogWriter());
	Log.InnitLog(std::shared_ptr<CommonLib::IlogLineFormat>(), pLogWriter);

	uint32_t nCount = argc > 1 ? (uint32_t)std::strtoul(argv[1], nullptr, 10) : 20000;
#ifdef _WIN32
	std::wstring storagePath = L"F:\\BPBlobTest.btdb";
#else
	std::wstring storagePath = L"/tmp/BPBlobTest.btdb";
#endif
	if (argc > 2)
	{
		std::string path = argv[2];
		storagePath.assign(path.begin(), path.end());
	}
	uint32_t nNodeCache = argc > 3 ? (uint32_t)std::strtoul(argv[3], nullptr, 10) : 1000;

	// even keys in random order
	std::vector<int64_t> randomOrder(nCount);
	for (uint32_t i = 0; i < nCount; ++i)
		randomOrder[i] = (int64_t)((Mix(i) >> 2) << 1);

	int nFailed = 0;
	nFailed += RunTest(randomOrder, "random order", storagePath, nNodeCache);
	nFailed += RunTest(Sorted(randomOrder), "sorted order", storagePath, nNodeCache);

	if (nFailed != 0)
		std::cout << nFailed << " test(s) failed" << std::endl;
	else
		std::cout << "all tests passed" << std::endl;

	return nFailed;
}
