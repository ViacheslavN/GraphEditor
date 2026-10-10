#pragma once
#include "OSMGeometry.h"
#include "OSMTempFile.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // A set of OSM ids (the member ways of the multipolygons): a bitmap of 64K-id pages allocated when an id of the page
        // is added, 8 KB per page (all the ways of the planet ~ 170 MB, an extract - the pages of its ids).
        class COSMIdSet
        {
        public:
            void Add(int64_t nId)
            {
                if(nId < 0)
                {
                    m_negative.push_back(nId);
                    m_bNegativeSorted = false;
                    ++m_nCount;
                    return;
                }
                uint64_t nPage = (uint64_t)nId >> PageBits;
                if(nPage >= m_pages.size())
                    m_pages.resize((size_t)nPage + 1);
                if(!m_pages[(size_t)nPage])
                    m_pages[(size_t)nPage].reset(new uint64_t[PageWords]());
                uint64_t nBit = (uint64_t)nId & (PageIds - 1);
                uint64_t& word = m_pages[(size_t)nPage][nBit >> 6];
                uint64_t nMask = (uint64_t)1 << (nBit & 63);
                if(!(word & nMask))
                {
                    word |= nMask;
                    ++m_nCount;
                }
            }

            // after all the ids are added: Contains doesn't change the set (several threads)
            void Seal()
            {
                if(!m_bNegativeSorted)
                {
                    std::sort(m_negative.begin(), m_negative.end());
                    m_negative.erase(std::unique(m_negative.begin(), m_negative.end()), m_negative.end());
                    m_bNegativeSorted = true;
                }
            }

            bool Contains(int64_t nId)
            {
                Seal();
                return ContainsSealed(nId);
            }

            // thread safe after Seal
            bool ContainsSealed(int64_t nId) const
            {
                if(nId < 0)
                    return std::binary_search(m_negative.begin(), m_negative.end(), nId);
                uint64_t nPage = (uint64_t)nId >> PageBits;
                if(nPage >= m_pages.size() || !m_pages[(size_t)nPage])
                    return false;
                uint64_t nBit = (uint64_t)nId & (PageIds - 1);
                return (m_pages[(size_t)nPage][nBit >> 6] >> (nBit & 63)) & 1;
            }

            bool     IsEmpty() const { return m_nCount == 0; }
            uint64_t Count() const { return m_nCount; }    // the negative ids can be counted twice

        private:
            static const uint32_t PageBits = 16;
            static const uint64_t PageIds = (uint64_t)1 << PageBits;
            static const size_t   PageWords = PageIds / 64;

            std::vector<std::unique_ptr<uint64_t[]> > m_pages;
            std::vector<int64_t> m_negative;
            bool     m_bNegativeSorted = true;
            uint64_t m_nCount = 0;
        };

        // Geometry of the ways which are members of multipolygons (needed after all the ways are read), on the disk:
        //  - the points are appended to a temporary file;
        //  - the index (id, offset, count) is another file, sorted by id (the ways of an OSM file are sorted, an unsorted
        //    index is sorted by Finish in memory), the first ids of its blocks are in memory, the blocks are cached.
        // Add ... Add, Finish, Get ... Get.
        class COSMWayStore
        {
        public:
            // nCacheBytes - the memory for the cached index blocks
            COSMWayStore(const std::string& sTempDir, uint64_t nCacheBytes);
            ~COSMWayStore();

            void Add(int64_t nId, const TOSMPoints& points) { Add(nId, points.data(), points.size()); }
            void Add(int64_t nId, const CommonLib::GisXYPoint* pPoints, size_t nCount);
            void Finish();
            bool Get(int64_t nId, TOSMPoints& points);
            uint64_t Count() const { return m_nCount; }

            static const uint32_t IndexBlockSize = 1024;   // entries

        private:
            COSMWayStore(const COSMWayStore&);
            COSMWayStore& operator=(const COSMWayStore&);

#pragma pack(push, 4)
            struct SEntry
            {
                int64_t  nId;
                uint64_t nOffset;    // in points
                uint32_t nCount;
            };
#pragma pack(pop)

            struct SCachedBlock
            {
                uint64_t nBlock = UINT64_MAX;
                std::vector<SEntry> entries;
            };

            void WriteEntry(const SEntry& entry);
            const std::vector<SEntry>& Block(uint64_t nBlock);

        private:
            std::string m_sTempDir;
            std::unique_ptr<COSMTempFile> m_ptrPoints;
            std::unique_ptr<COSMTempFile> m_ptrIndex;
            uint64_t    m_nPoints;
            uint64_t    m_nCount;
            int64_t     m_nLastId;
            bool        m_bSorted;
            bool        m_bFinished;
            std::vector<int64_t> m_vecBlockFirstIds;
            std::vector<SCachedBlock> m_cache;    // direct mapped by the block number
        };
    }
}
