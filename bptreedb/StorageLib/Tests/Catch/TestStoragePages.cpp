#include <catch2/catch_test_macros.hpp>

#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../FileStorage.h"
#include "../../FilePager.h"
#include "../../WriteStreamPage.h"
#include "../../ReadStreamPage.h"
#include "../../FreeSpaceMap.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <set>

using namespace bptreedb::storage;

namespace
{
	const uint32_t nPageSize = 8192;

	std::string TestPath(const char* pszName)
	{
		return (std::filesystem::temp_directory_path() / pszName).u8string();
	}

	struct STestFile
	{
		CommonLib::IAllocPtr ptrAlloc{ new CommonLib::CSimpleAlloc(false) };
		std::shared_ptr<CFileStorage> ptrStorage;
		std::shared_ptr<CFilePager> ptrPager;

		STestFile(const std::string& sPath, bool bCreate, uint32_t nCachePages = 16)
		{
			ptrStorage = std::make_shared<CFileStorage>(ptrAlloc, 1, IStorageCipherPtr());
			ptrStorage->Open(sPath.c_str(), bCreate, 0, nPageSize);
			ptrPager = std::make_shared<CFilePager>(ptrAlloc, ptrStorage, nCachePages);
		}

		void Close()
		{
			ptrPager->Flush();
			ptrStorage->Close();
		}
	};

	byte_t Pattern(int64_t nRef, size_t i) { return (byte_t)((uint64_t)nRef * 31 + i * 7 + (i >> 9)); }

	void Fill(IFilePagePtr ptrPage)
	{
		for (size_t i = 0; i < ptrPage->GetSize(); ++i)
			ptrPage->GetPayLoadData()[i] = Pattern(ptrPage->GetAddr(), i);
	}

	bool Check(IFilePagePtr ptrPage)
	{
		for (size_t i = 0; i < ptrPage->GetSize(); ++i)
			if (ptrPage->GetPayLoadData()[i] != Pattern(ptrPage->GetAddr(), i))
				return false;
		return true;
	}

	// the pages of a stream: count, bytes
	void ChainStats(IFilePagerPtr ptrPager, int64_t nRef, uint64_t& nPages, uint64_t& nBytes)
	{
		nPages = nBytes = 0;
		while (nRef != -1)
		{
			IFilePagePtr ptrPage = ptrPager->ReadPage(nRef);
			nBytes += ptrPage->GetFullSize();
			++nPages;
			nRef = ptrPage->GetReadStream()->ReadInt64();
		}
	}
}

TEST_CASE("PageRef: the block address and the size of a page", "[storage][pageref]")
{
	REQUIRE(PageRef::Make(12345, 0) == 12345);
	int64_t nRef = PageRef::Make(12345, 7);
	REQUIRE(PageRef::Addr(nRef) == 12345);
	REQUIRE(PageRef::Blocks(nRef) == 7);
	REQUIRE(PageRef::Size(nRef, nPageSize) == 7 * BlockSize);
	REQUIRE(PageRef::Size(12345, nPageSize) == nPageSize);
	REQUIRE(PageRef::Size(-1, nPageSize) == nPageSize);
	REQUIRE(nRef > 0);
	REQUIRE(PageRef::Make(PageRef::AddrMask, PageRef::MaxBlocks) > 0);
	REQUIRE(PageRef::RoundUp(1) == 512);
	REQUIRE(PageRef::RoundUp(512) == 512);
	REQUIRE(PageRef::RoundUp(513) == 1024);
}

TEST_CASE("Pages of different sizes in one file", "[storage][pages]")
{
	std::string sPath = TestPath("bptreedb_storage_pages.btdb");
	const std::vector<uint32_t> sizes = { 0, 512, 1536, 8192, 20000, 65536, 3000, 300 * 1024 };
	std::vector<int64_t> refs;
	{
		STestFile file(sPath, true, 4);    // the cache holds 32 KB: the big pages pass it, the small ones are evicted
		for (int nRound = 0; nRound < 3; ++nRound)
		{
			for (uint32_t nSize : sizes)
			{
				int64_t nRef = file.ptrPager->GetNewFilePageRef(nSize);
				uint32_t nExpected = nSize == 0 ? nPageSize : PageRef::RoundUp(nSize);
				REQUIRE(file.ptrPager->GetPageSize(nRef) == nExpected);
				// the default size has a plain address
				REQUIRE(PageRef::IsDefaultSize(nRef) == (nExpected == nPageSize));

				IFilePagePtr ptrPage = file.ptrPager->GetNewPage(nRef);
				REQUIRE(ptrPage->GetFullSize() == nExpected);
				Fill(ptrPage);
				file.ptrPager->SavePage(ptrPage);
				refs.push_back(nRef);
			}
		}
		REQUIRE(file.ptrPager->GetCachedBytes() <= (uint64_t)300 * 1024);

		// the blocks follow each other
		uint64_t nBlocks = 0;
		for (int64_t nRef : refs)
			nBlocks += file.ptrPager->GetPageSize(nRef) / BlockSize;
		REQUIRE((uint64_t)file.ptrStorage->GetEndAddr() == nBlocks);

		// read back through the cache, one page object resized for every size
		IFilePagePtr ptrPage = file.ptrPager->GetNewPage(-1);
		for (int64_t nRef : refs)
		{
			file.ptrPager->ReadPage(ptrPage, nRef);
			REQUIRE(ptrPage->GetFullSize() == file.ptrPager->GetPageSize(nRef));
			REQUIRE(Check(ptrPage));
		}
		file.Close();
	}

	// the file size: the sum of the pages
	uint64_t nTotal = 0;
	for (int64_t nRef : refs)
		nTotal += PageRef::Size(nRef, nPageSize);
	REQUIRE(std::filesystem::file_size(std::filesystem::u8path(sPath)) == nTotal);

	{
		STestFile file(sPath, false, 64);
		for (size_t i = refs.size(); i-- > 0;)
		{
			IFilePagePtr ptrPage = file.ptrPager->ReadPage(refs[i]);
			REQUIRE(Check(ptrPage));
		}

		// a page read with a wrong size fails (the crc / the reference)
		IFilePagePtr ptrPage = file.ptrPager->GetNewPage(refs[2]);
		bool bThrown = false;
		try { ptrPage->Read(file.ptrPager->GetStorageIO(), refs[1]); }
		catch (std::exception&) { bThrown = true; }
		REQUIRE(bThrown);
		file.Close();
	}
	std::filesystem::remove(std::filesystem::u8path(sPath));
}

TEST_CASE("Page cache: the limit in bytes, the dirty pages written back", "[storage][cache]")
{
	std::string sPath = TestPath("bptreedb_storage_cache.btdb");
	std::vector<int64_t> refs;
	{
		STestFile file(sPath, true, 3);    // 24 KB
		for (int i = 0; i < 200; ++i)
		{
			int64_t nRef = file.ptrPager->GetNewFilePageRef(512 * (1 + i % 20));
			IFilePagePtr ptrPage = file.ptrPager->GetNewPage(nRef);
			Fill(ptrPage);
			file.ptrPager->SavePage(ptrPage);
			refs.push_back(nRef);
			REQUIRE(file.ptrPager->GetCachedBytes() <= 3 * nPageSize);
		}
		// rewritten while some of them are cached
		for (size_t i = 0; i < refs.size(); i += 3)
		{
			IFilePagePtr ptrPage = file.ptrPager->ReadPage(refs[i]);
			REQUIRE(Check(ptrPage));
			file.ptrPager->SavePage(ptrPage);
		}
		file.Close();
	}
	{
		STestFile file(sPath, false, 0);   // no cache
		for (int64_t nRef : refs)
			REQUIRE(Check(file.ptrPager->ReadPage(nRef)));
		file.Close();
	}
	std::filesystem::remove(std::filesystem::u8path(sPath));
}

TEST_CASE("Page streams sized for their data", "[storage][stream]")
{
	typedef std::vector<uint32_t> TPlan;
	const uint32_t K = 1024, M = 1024 * 1024;
	REQUIRE(TWriteStreamPage::MaxClass(64 * K) == 64 * K);
	REQUIRE(TWriteStreamPage::MaxClass(100 * K) == 64 * K);
	REQUIRE((TWriteStreamPage::PlanPages(1, M) == TPlan{ 512 }));
	REQUIRE((TWriteStreamPage::PlanPages(1500, M) == TPlan{ 2 * K }));                 // one page: it loses less than 1 KB
	REQUIRE((TWriteStreamPage::PlanPages(9 * K + 8, M) == TPlan{ 8 * K, 2 * K }));     // 16K would lose 7 KB
	REQUIRE((TWriteStreamPage::PlanPages(200 * K + 8, M) == TPlan{ 128 * K, 64 * K, 16 * K }));
	REQUIRE((TWriteStreamPage::PlanPages(3 * M + 8, M) == TPlan{ M, M, M, 512 }));
	REQUIRE((TWriteStreamPage::PlanPages(15 * K, M) == TPlan{ 16 * K }));               // an eighth: up to 1.9 KB lost

	std::string sPath = TestPath("bptreedb_storage_streams.btdb");
	const std::vector<uint64_t> sizes = { 1, 100, 492, 493, 1000, 8000, 65516, 65517, 70000, 300000, 2500000 };
	const uint32_t nMaxPage = 1024 * 1024;
	std::vector<int64_t> refs;
	{
		STestFile file(sPath, true, 16);
		for (uint64_t nSize : sizes)
		{
			std::vector<byte_t> data((size_t)nSize);
			for (size_t i = 0; i < data.size(); ++i)
				data[i] = (byte_t)(i * 13 + nSize);

			TWriteStreamPage stream(file.ptrPager);
			int64_t nRef = stream.Create(nSize, nMaxPage);
			// a page over an eighth of the cache isn't cached
			REQUIRE(file.ptrPager->GetCachedBytes() <= 16 * nPageSize);
			stream.WriteBytes(data.data(), data.size() / 3);           // in parts: the page sizes follow the bytes left
			stream.WriteBytes(data.data() + data.size() / 3, data.size() - data.size() / 3);
			stream.Close();
			refs.push_back(nRef);

			// the pages as planned (the size classes), at most the budget lost
			uint64_t nPages = 0, nBytes = 0;
			ChainStats(file.ptrPager, nRef, nPages, nBytes);
			std::vector<uint32_t> plan = TWriteStreamPage::PlanPages(nSize, nMaxPage);
			uint64_t nPlanBytes = 0;
			for (uint32_t nPage : plan)
			{
				REQUIRE((nPage & (nPage - 1)) == 0);
				nPlanBytes += nPage;
			}
			REQUIRE(nPages == plan.size());
			REQUIRE(nBytes == nPlanBytes);
			REQUIRE(nBytes >= nSize + nPages * TWriteStreamPage::PageOverhead);
			REQUIRE(nBytes <= nSize + nPages * TWriteStreamPage::PageOverhead + TWriteStreamPage::WasteBudget(nSize));
		}
		file.Close();
	}
	{
		STestFile file(sPath, false, 16);
		for (size_t k = 0; k < sizes.size(); ++k)
		{
			std::vector<byte_t> data((size_t)sizes[k]);
			TReadStreamPage stream(file.ptrPager);
			stream.Open(refs[k]);
			stream.ReadBytes(data.data(), data.size());
			bool bSame = true;
			for (size_t i = 0; i < data.size(); ++i)
				bSame = bSame && data[i] == (byte_t)(i * 13 + sizes[k]);
			REQUIRE(bSame);
		}

		// a stream of default pages, then rewritten in place and grown (the new pages - of the stream's page size)
		int64_t nRef = file.ptrPager->GetNewFilePageAddr();
		{
			TWriteStreamPage stream(file.ptrPager);
			stream.Open(nRef, true);
			for (int64_t i = 0; i < 5000; ++i)
				stream.Write(i);
			stream.Close();
		}
		{
			TWriteStreamPage stream(file.ptrPager, 2048);
			stream.Open(nRef, false);
			for (int64_t i = 0; i < 9000; ++i)
				stream.Write(i * 2);
			stream.Close();
		}
		TReadStreamPage stream(file.ptrPager);
		stream.Open(nRef);
		bool bSame = true;
		for (int64_t i = 0; i < 9000; ++i)
			bSame = bSame && stream.ReadInt64() == i * 2;
		REQUIRE(bSame);
		file.Close();
	}
	std::filesystem::remove(std::filesystem::u8path(sPath));
}

TEST_CASE("Free space map: the dropped pages are taken again by size", "[storage][freespace]")
{
	std::string sPath = TestPath("bptreedb_storage_free.btdb");
	{
		STestFile file(sPath, true, 16);
		int64_t nRoot = file.ptrPager->GetNewFilePageAddr();
		CFreeSpaceMap::WriteEmpty(file.ptrPager, nRoot);

		CFreeSpaceMapPtr ptrFree = std::make_shared<CFreeSpaceMap>();
		file.ptrPager->SetFreeSpaceMap(ptrFree);

		// pages of three sizes, every other one dropped
		std::vector<int64_t> refs, dropped;
		for (int i = 0; i < 300; ++i)
		{
			uint32_t nSize = i % 3 == 0 ? 0 : (i % 3 == 1 ? 2048 : 65536);
			int64_t nRef = file.ptrPager->GetNewFilePageRef(nSize);
			IFilePagePtr ptrPage = file.ptrPager->GetNewPage(nRef);
			Fill(ptrPage);
			file.ptrPager->SavePage(ptrPage);
			refs.push_back(nRef);
		}
		int64_t nEnd = file.ptrStorage->GetEndAddr();
		for (size_t i = 0; i < refs.size(); i += 2)
		{
			file.ptrPager->DropFilePage(refs[i]);
			dropped.push_back(refs[i]);
		}
		REQUIRE(ptrFree->PageCount() == dropped.size());
		bool bThrown = false;
		try { file.ptrPager->DropFilePage(refs[0]); }
		catch (std::exception&) { bThrown = true; }
		REQUIRE(bThrown);    // freed twice

		// saved and loaded
		ptrFree->Save(file.ptrPager, nRoot);
		CFreeSpaceMap loaded;
		loaded.Load(file.ptrPager, nRoot);
		REQUIRE(loaded.PageCount() == ptrFree->PageCount());
		REQUIRE(loaded.FreeBlocks() == ptrFree->FreeBlocks());
		REQUIRE(loaded.EndAddr() == ptrFree->EndAddr());

		// the new pages of the same sizes take the dropped ones (the lowest address first), the file doesn't grow
		std::set<int64_t> free(dropped.begin(), dropped.end());
		nEnd = file.ptrStorage->GetEndAddr();
		for (int64_t nOld : dropped)
		{
			int64_t nRef = file.ptrPager->GetNewFilePageRef(file.ptrPager->GetPageSize(nOld));
			REQUIRE(free.erase(nRef) == 1);
			IFilePagePtr ptrPage = file.ptrPager->GetNewPage(nRef);
			Fill(ptrPage);
			file.ptrPager->SavePage(ptrPage);
		}
		REQUIRE(free.empty());
		REQUIRE(ptrFree->PageCount() == 0);
		REQUIRE(file.ptrStorage->GetEndAddr() == nEnd);
		for (int64_t nRef : refs)
			REQUIRE(Check(file.ptrPager->ReadPage(nRef)));

		// a size without free pages: the end of the file
		int64_t nRef = file.ptrPager->GetNewFilePageRef(4096);
		REQUIRE(PageRef::Addr(nRef) == nEnd);
		file.Close();
	}
	std::filesystem::remove(std::filesystem::u8path(sPath));
}
