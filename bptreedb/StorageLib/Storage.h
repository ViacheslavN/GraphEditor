#pragma once
#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/exception/exc_base.h"
#include "../../CommonLib/perf/PerfCount.h"
#include "../../CommonLib/stream/FixedMemoryStream.h"
#include "PageMemoryBuffer.h"

namespace bptreedb
{
	namespace storage
	{
		typedef std::shared_ptr<class IStorageIO> IStorageIOPtr;
		typedef std::shared_ptr<class IFilePage> IFilePagePtr;
		typedef std::shared_ptr<class IFilePager> IFilePagerPtr;

		// The addresses of a storage are in blocks of BlockSize bytes (IStorageIO): a page of N bytes (a multiple of the
		// block size) takes N / BlockSize consecutive blocks, so the pages of one file can have different sizes (the tree
		// nodes - the default page size of the storage, the blobs and the long strings - the size of their data).
		//
		// A page reference (int64) is "the address of a page" for the pagers and for everything stored on the pages
		// (the links of the trees, the page chains of the streams, the blob values):
		//   bits 0..47   the block address of the page
		//   bits 48..62  the size of the page in blocks; 0 - the default page size of the storage (GetPageSize())
		//   -1           no page
		// A page of the default size has a plain block address as its reference.
		static const uint32_t BlockSize = 512;

		struct PageRef
		{
			static const int AddrBits = 48;
			static const int64_t AddrMask = ((int64_t)1 << AddrBits) - 1;
			static const uint32_t MaxBlocks = 0x7FFF;     // a page up to 16 MB - 512

			// nBlocks = 0 - the default page size
			static int64_t Make(int64_t nAddr, uint32_t nBlocks)
			{
				if (nAddr < 0 || nAddr > AddrMask)
					throw CommonLib::CExcBase("PageRef: wrong block address {0}", nAddr);
				if (nBlocks > MaxBlocks)
					throw CommonLib::CExcBase("PageRef: too big page, {0} blocks", nBlocks);
				return nAddr | ((int64_t)nBlocks << AddrBits);
			}

			static int64_t Addr(int64_t nRef) { return nRef & AddrMask; }
			static uint32_t Blocks(int64_t nRef) { return nRef < 0 ? 0 : (uint32_t)(nRef >> AddrBits) & MaxBlocks; }
			static bool IsDefaultSize(int64_t nRef) { return Blocks(nRef) == 0; }

			// the size of the referenced page (a negative reference - a page of the default size)
			static uint32_t Size(int64_t nRef, uint32_t nDefaultPageSize)
			{
				uint32_t nBlocks = Blocks(nRef);
				return nBlocks ? nBlocks * BlockSize : nDefaultPageSize;
			}

			// the blocks of a page of nSize bytes (rounded up)
			static uint32_t BlocksFor(uint32_t nSize) { return (nSize + BlockSize - 1) / BlockSize; }
			static uint32_t RoundUp(uint32_t nSize) { return BlocksFor(nSize) * BlockSize; }
			static uint32_t MaxPageSize() { return MaxBlocks * BlockSize; }
		};

		class IFilePager
		{
		public:

			IFilePager() {}
			virtual ~IFilePager() {}

			// the default page size (the tree nodes)
			virtual uint32_t GetPageSize() const = 0;

			// a new page of the default size: its reference (a block address)
			virtual int64_t GetNewFilePageAddr() = 0;
			// a new page of nSize bytes (rounded up to the blocks; 0 - the default size): its reference
			virtual int64_t GetNewFilePageRef(uint32_t nSize) = 0;

			// the pages by their references (a negative reference - an unsaved page of the default size)
			virtual IFilePagePtr GetNewPage(int64_t nRef) = 0;
			virtual IFilePagePtr ReadPage(int64_t nRef) = 0;
			// reads into ptrPage (resized if the referenced page has another size)
			virtual void ReadPage(IFilePagePtr ptrPage, int64_t nRef) = 0;
			virtual void SavePage(IFilePagePtr ptrPage) = 0;
			virtual void DropFilePage(int64_t nRef) = 0;
			virtual void Flush() = 0;

			// the size of the referenced page
			uint32_t GetPageSize(int64_t nRef) const { return PageRef::Size(nRef, GetPageSize()); }
		};

		class IFilePage
		{
		public:

			IFilePage() {}
			virtual ~IFilePage() {}

			// the reference of the page (PageRef)
			virtual int64_t GetAddr() const = 0;
			virtual void SetAddr(int64_t nAddr) = 0;
			// the payload size (the page without its header)
			virtual uint32_t GetSize() const = 0;
			// the size of the page on the storage
			virtual uint32_t GetFullSize() const = 0;
			// a page of another size (the content is lost when it changes)
			virtual void Resize(uint32_t nFullSize) = 0;
			virtual void Erase(byte_t val) = 0;
			virtual byte_t* GetPayLoadData() = 0;
			virtual const byte_t* GetPayLoadData() const = 0;


			// nAddr - the reference (-1 - the reference of the page); the page must have the referenced size
			virtual void Save(IStorageIOPtr ptrStorageIO, int64_t nAddr = -1) = 0;
			virtual void Read(IStorageIOPtr ptrStorageIO, int64_t nAddr = -1) = 0;

			virtual CommonLib::IMemoryWriteStreamPtr GetWriteStream() = 0;
			virtual CommonLib::IMemoryReadStreamPtr GetReadStream() const = 0;
		};

		class IStorageCipher
		{
		public:
			IStorageCipher() {}
			virtual ~IStorageCipher() {}

			virtual uint32_t Encrypt(int64_t nPageAddr, const byte_t* srcBuf, uint32_t bufSize, byte_t* dstBuf, uint32_t dstSize) = 0;
			virtual uint32_t Decrypt(int64_t nPageAddr, const byte_t* srcBuf, uint32_t bufSize, byte_t* dstBuf, uint32_t dstSize) = 0;
			virtual uint32_t Encrypt(int64_t nPageAddr, byte_t* srcBuf, uint32_t bufSize) = 0;
			virtual uint32_t Decrypt(int64_t nPageAddr, byte_t* srcBuf, uint32_t bufSize) = 0;
		};

		// The raw IO of a storage: the addresses are block addresses (BlockSize), the sizes - multiples of the block size;
		// the data of a page is read and written as a whole (the address and the size of the page).
		class IStorageIO
		{
		public:
			IStorageIO() {}
			virtual ~IStorageIO() {}

			// the default page size (a multiple of the block size)
			virtual uint32_t GetPageSize() const = 0;
			uint32_t GetBlockSize() const { return BlockSize; }
			// the block address of nSize new bytes (0 - the default page size)
			virtual int64_t GetNewFilePageAddr(uint32_t nSize = 0) = 0;
			virtual void ReadData(int64_t nAddr, byte_t* pData, uint32_t nSize) = 0;
			virtual void WriteData(int64_t nAddr, const byte_t* pData, uint32_t nSize) = 0;
			virtual void DropData(int64_t nAddr, uint32_t nSize) = 0;
		};
		 
		class IStorage : public IStorageIO
		{
		public:
			enum EPrefOperation
			{
				eReadData,
				eWriteData,
				eEncryptData,
				eDecryptData
			};

			IStorage() {}
			virtual ~IStorage() {}

			virtual int32_t GetStorageId() const = 0;
			// nPageSize - the default page size, a multiple of BlockSize
			virtual void Open(const char* pszNameUtf8, bool bCreate, uint64_t offset, uint32_t nPageSize = 8192) = 0;
			virtual void Open(const wchar_t* pszName, bool bCreate, uint64_t offset, uint32_t nPageSize = 8192) = 0;
			virtual void Close() = 0;
			virtual void Flush() = 0;
			virtual void SetStoragePerformer(CommonLib::TPrefCounterPtr pStoragePerformer) = 0;
			virtual void Lock() = 0;
			virtual void UnLock() = 0;
			virtual bool TryLock() = 0;
		};

		typedef std::shared_ptr<IStorage> IStoragePtr;

		class IStoragesHolder
		{
		public:
			IStoragesHolder() {};
			virtual ~IStoragesHolder() {}

			virtual IStoragePtr GetStorage(int32_t nId) const = 0;
			virtual IStoragePtr GetStorageThrowIfNull(int32_t nId) const = 0;
			virtual void AddStorage(IStoragePtr ptrStorage) = 0;
			virtual void RemoveStorage(int32_t nId) = 0;

		};

		
		typedef std::shared_ptr<IStorageCipher> IStorageCipherPtr;
		typedef std::shared_ptr<IStoragesHolder> IStoragesHolderPtr;
	}
}