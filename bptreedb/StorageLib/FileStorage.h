#pragma once
#include "../../CommonLib/filesystem/filesystem.h"
#include "FilePage.h"
#include "Storage.h"


namespace bptreedb
{
	namespace storage
	{

		typedef std::shared_ptr<class CFileStorage> CFileStoragePtr;

		// The file of the pages: the address of a page is its block (BlockSize bytes) after the offset, a page takes its size
		// in blocks (a multiple of BlockSize, the default page size - Open). A page is read / written by one file operation;
		// the cipher encrypts the blocks (the block address).
		class CFileStorage : public IStorage
		{
		private:
			CFileStorage(const CFileStorage&);
			CFileStorage(const CFileStorage&&);
			CFileStorage& operator = (const CFileStorage&);
			CFileStorage& operator = (const CFileStorage&&);

		public:
			CFileStorage(CommonLib::IAllocPtr ptrAlloc, int32_t storageId, IStorageCipherPtr ptrCipher);
			virtual ~CFileStorage();

			virtual void Open(const char* pszNameUtf8, bool bCreate, uint64_t offset, uint32_t nPageSize = 8192);
			virtual void Open(const wchar_t* pszName, bool bCreate, uint64_t offset, uint32_t nPageSize = 8192);
			virtual void Close();
			virtual void ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize);
			virtual void WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize);
			virtual void DropData(int64_t nAddr, uint32_t nSize);
			virtual void Flush();
			virtual void SetStoragePerformer(CommonLib::TPrefCounterPtr pStoragePerformer);
			virtual int32_t GetStorageId() const;
			virtual int64_t GetNewFilePageAddr(uint32_t nSize = 0);
			// the block addresses below nEndAddr become writable (the recovery writes pages allocated before a crash)
			void ReserveAddr(int64_t nEndAddr);
			// the blocks of the file (the end of the allocated space)
			int64_t GetEndAddr() const;
			virtual uint32_t GetPageSize() const;
			virtual void Lock();
			virtual void UnLock();
			virtual bool TryLock();
		private:

			void CheckRange(int64_t nAddr, uint32_t nSize) const;

			CommonLib::IAllocPtr m_ptrAlloc;

			int32_t m_storage_id{ -1 };
			uint32_t m_nPageSize{ 8192 };
			int64_t m_lastAddr{ 0 };
			uint64_t m_offset{ 0 };
			IStorageCipherPtr m_ptrStorageCipher;
			CommonLib::file::CFile m_file;


			typedef std::vector<byte_t> TBufferForChiper;
			TBufferForChiper m_bufForChiper;
			CommonLib::TPrefCounterPtr m_ptrStoragePerformer;
			mutable std::recursive_mutex m_mutex;
		};


	}
}