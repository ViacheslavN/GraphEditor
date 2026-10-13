#pragma once

// The page IO of a write transaction: the pages it wrote are private (memory; above a limit they spill to a
// temporary file), the other pages are read from the committed pages (the shared page cache). The database
// file isn't touched until the commit applies the private pages (CRedoLog).

#include "../DBCommon.h"
#include "../../../CommonLib/filesystem/File.h"
#include <unordered_map>
#include <map>
#include <list>

namespace bptreedb
{
	namespace db
	{
		class CTranStorageIO : public storage::IStorageIO
		{
		public:
			// nMemoryPages: private pages kept in memory, the others go to sTempPath
			CTranStorageIO(storage::IStorageIOPtr ptrCommitted, uint32_t nMemoryPages, const std::wstring& sTempPath);
			virtual ~CTranStorageIO();

			// IStorageIO
			virtual uint32_t GetPageSize() const;
			virtual int64_t GetNewFilePageAddr(uint32_t nSize = 0);
			virtual void ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize);
			virtual void WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize);
			virtual void DropData(int64_t nAddr, uint32_t nSize);

			size_t PrivatePageCount() const;
			// calls func(addr, data) for every private page in the order of the addresses
			void ForEachPage(const std::function<void(int64_t, const byte_t*)>& func);
			void Clear();          // drops the private pages and the temporary file

		private:
			void CheckSize(uint32_t nSize) const;
			void SpillOne();       // the least recently written page from memory to the file
			void OpenTempFile();

		private:
			storage::IStorageIOPtr m_ptrCommitted;
			uint32_t m_nPageSize;
			uint32_t m_nMemoryPages;
			std::wstring m_sTempPath;

			struct SMemPage
			{
				std::vector<byte_t> m_data;
				std::list<int64_t>::iterator m_lru;
			};
			std::unordered_map<int64_t, SMemPage> m_memory;
			std::list<int64_t> m_lru;                        // front - the last written
			std::unordered_map<int64_t, uint64_t> m_spilled; // page -> slot in the temporary file
			uint64_t m_nSlots{ 0 };
			CommonLib::file::TFilePtr m_ptrTempFile;
			std::vector<byte_t> m_buffer;
			mutable std::recursive_mutex m_mutex;
		};
		typedef std::shared_ptr<CTranStorageIO> CTranStorageIOPtr;
	}
}
