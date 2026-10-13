#pragma once

// Internal header of the database: common includes, the shared state of an open database, helpers.
// Not part of the SDK (bptreedb.h).

#include "bptreedb.h"
#include "../../CommonLib/exception/exc_base.h"
#include "../../CommonLib/alloc/alloc.h"
#include "../../CommonLib/stream/stream.h"
#include "../../CommonLib/stream/MemoryStream.h"
#include "../../CommonLib/stream/FixedMemoryStream.h"
#include "../StorageLib/Storage.h"
#include "../StorageLib/FilePager.h"
#include "../BPtreeLib/Compressor/CompressorParams.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

namespace bptreedb
{
	namespace db
	{
		// limits
		const uint32_t MaxNameLength = 256;           // tables, fields
		const uint32_t MaxIndexStringLength = 256;    // indexed string values

		// Shared state of an open database: what the tables, trees and cursors need.
		struct SDBContext
		{
			CommonLib::IAllocPtr m_ptrAlloc;
			storage::CFilePagerPtr m_ptrPager;           // the committed pages (the shared page cache of the file)
			std::wstring m_sPath;                        // the database file
			uint32_t m_nPageSize{ 8192 };
			uint32_t m_nTreeCacheSize{ 64 };
			uint32_t m_nSpatialCacheSize{ 1000 };
			uint32_t m_nCompressLevel{ 6 };
			uint32_t m_nTranMemoryPages{ 4096 };
			std::atomic<bool> m_bOpen{ false };
			std::atomic<uint64_t> m_nCommits{ 0 };       // commits since open: the shared trees of the tables follow it

			void CheckOpen() const
			{
				if (!m_bOpen)
					throw CommonLib::CExcBase("database is closed");
			}

			// compressor params of a new tree; nMaxLen - the maximum length of the string keys / values
			TCompressorParamsBasePtr MakeTreeParams(uint32_t nMaxLen) const;
		};
		typedef std::shared_ptr<SDBContext> SDBContextPtr;

		// Checks a name of a table or a field: 1..MaxNameLength bytes.
		void CheckName(const std::string& sName, const char* pszWhat);

		const char* DataTypeName(eDataTypes type);
		const char* OpTypeName(OpType op);
	}
}
