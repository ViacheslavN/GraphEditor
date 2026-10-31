#pragma once

#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/stream/FixedMemoryStream.h"
#include "../../CommonLib/stream/MemoryStream.h"
#include "../../CommonLib/stream/StreamBaseEmpty.h"
#include "FilePage.h"
#include <vector>

namespace bptreedb
{
	namespace storage
	{
		// A stream over a chain of pages: a page starts with int64 next, int64 prev (the page references, -1 - none).
		// The pages of a stream may have different sizes (PageRef):
		//   - Open(nRef, true) - a stream on the page nRef, the next pages of nPageSize (the constructor, 0 - the default);
		//   - Create(nTotalBytes, nMaxPageSize) - a new stream of a known size in pages of the size classes: 512 bytes
		//     times a power of two up to nMaxPageSize (a few sizes: the free pages are reused by size, CFreeSpaceMap).
		//     The pages for the data left: one page of the smallest class holding it if it loses at most the budget
		//     (an eighth of the stream, at least 1 KB), otherwise the biggest class it fills and the rest the same way:
		//     9 KB -> 8K + 2K, 200 KB -> 128K + 64K + 16K, 3 MB with 1 MB pages -> 3 x 1M + 512.
		class TWriteStreamPage : public CommonLib::TMemoryStreamBaseEmpty<CommonLib::IMemoryWriteStream>
		{
		public:
			static const uint32_t PageHeaderSize = 2 * sizeof(int64_t);       // next, prev
			static const uint32_t PageOverhead = PageHeaderSize + sizeof(uint32_t);   // + the crc of the page

			// nPageSize - the size of the new pages (0 - the default page size)
			TWriteStreamPage(IFilePagerPtr ptrIFilePager, uint32_t nPageSize = 0);
			virtual ~TWriteStreamPage();

			void Open(int64_t nPageAddr, bool bCreate);
			// a new stream of nTotalBytes: its first page is allocated (bNonZero - a page with the reference 0 is skipped:
			// 0 means "no stream" for the caller), returns its reference
			int64_t Create(uint64_t nTotalBytes, uint32_t nMaxPageSize, bool bNonZero = false);
			virtual std::streamsize WriteBytes(const byte_t* buffer, size_t size);
			virtual std::streamsize WriteInverse(const byte_t* buffer, size_t size);
			virtual bool IsEnoughSpace(size_t size) const;
			virtual void Close();

			// the biggest size class up to nMaxPageSize
			static uint32_t MaxClass(uint32_t nMaxPageSize);
			// the bytes a stream of nTotalBytes may lose in its last page
			static uint64_t WasteBudget(uint64_t nTotalBytes);
			// the size of the next page for nLeft bytes of the stream
			static uint32_t NextPageSize(uint64_t nLeft, uint64_t nBudget, uint32_t nMaxClass);
			// the pages of a stream of nTotalBytes
			static std::vector<uint32_t> PlanPages(uint64_t nTotalBytes, uint32_t nMaxPageSize);

		private:
			void write_bytes_impl(const byte_t* buffer, size_t size, bool bInvers);
			void OpenNextPage(int64_t nPageAddr, bool bCreate);
			int64_t NewPageRef();

		private:
			int64_t m_nNextAddr{ -1 };
			int64_t m_nPrevAddr{ -1 };
			uint32_t m_nPageSize{ 0 };
			uint32_t m_nMaxClass{ 0 };         // Create: the pages of the size classes for the data left
			uint64_t m_nBudget{ 0 };
			uint64_t m_nLeft{ 0 };             // Create: the bytes still to write

			IFilePagerPtr m_ptrIFilePager;
			IFilePagePtr m_ptrCurrentPage;
			CommonLib::IMemoryWriteStreamPtr m_ptrStream;
		};
	}
}
