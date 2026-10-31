#pragma once
#include "../../CommonLib/CommonLib.h"
#include "Storage.h"

#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace bptreedb
{
	namespace storage
	{
		typedef std::shared_ptr<class CFreeSpaceMap> CFreeSpaceMapPtr;

		// The free pages of a storage: the dropped pages by their sizes (in blocks). A new page of a size takes a free page
		// of exactly that size (the lowest address: the file stays compact), so the pages keep a few sizes (the default page
		// size, the size classes of the stream pages - TWriteStreamPage).
		//
		// The map is kept by its owner: Save / Load write it to a stream of pages (TWriteStreamPage) from a root page of
		// the default size; the stream pages aren't taken from the map (Save marks the map as being saved: Take gives
		// nothing meanwhile).
		//   uint32 magic, uint32 version, uint32 count of the sizes; a size: uint32 blocks, uint64 count, varint deltas
		//   of the sorted block addresses
		class CFreeSpaceMap
		{
		public:
			CFreeSpaceMap();
			~CFreeSpaceMap();

			// a free page of nBlocks, false - none
			bool Take(uint32_t nBlocks, int64_t& nAddr);
			// a dropped page (a page freed twice throws)
			void Add(int64_t nAddr, uint32_t nBlocks);
			void Clear();

			uint64_t PageCount() const;
			uint64_t FreeBlocks() const;
			// the end of the last free page (the storage must not give these blocks again: CFileStorage::ReserveAddr)
			int64_t EndAddr() const;

			// the map and the pages in extra (dropped, not in the map yet) into the stream at nRootRef (the root page exists)
			void Save(IFilePagerPtr ptrPager, int64_t nRootRef, const std::vector<std::pair<int64_t, uint32_t> >& extra = std::vector<std::pair<int64_t, uint32_t> >());
			// a new root page with an empty map
			static void WriteEmpty(IFilePagerPtr ptrPager, int64_t nRootRef);
			void Load(IFilePagerPtr ptrPager, int64_t nRootRef);

		private:
			CFreeSpaceMap(const CFreeSpaceMap&);
			CFreeSpaceMap& operator=(const CFreeSpaceMap&);

			static const uint32_t Magic = 0x50465042;     // "BPFP"
			static const uint32_t Version = 1;

		private:
			mutable std::recursive_mutex m_mutex;
			std::map<uint32_t, std::set<int64_t> > m_free;    // blocks -> addresses
			uint64_t m_nPages{ 0 };
			uint64_t m_nBlocks{ 0 };
			bool m_bSaving{ false };
		};
	}
}
