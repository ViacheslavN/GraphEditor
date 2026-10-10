#include "OSMWayStore.h"
#include "../../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <limits>

namespace GraphEngine {
    namespace Convertors {

        COSMWayStore::COSMWayStore(const std::string& sTempDir, uint64_t nCacheBytes) :
                m_sTempDir(sTempDir), m_nPoints(0), m_nCount(0), m_nLastId((std::numeric_limits<int64_t>::min)()),
                m_bSorted(true), m_bFinished(false)
        {
            m_ptrPoints.reset(new COSMTempFile(sTempDir, "_ways.bin", 4 * 1024 * 1024));
            m_ptrIndex.reset(new COSMTempFile(sTempDir, "_ways_index.bin"));
            size_t nSlots = (size_t)(std::max)((uint64_t)64, nCacheBytes / (sizeof(SEntry) * IndexBlockSize));
            m_cache.resize((std::min)(nSlots, (size_t)65536));
        }

        COSMWayStore::~COSMWayStore()
        {
        }

        void COSMWayStore::WriteEntry(const SEntry& entry)
        {
            if(m_nCount % IndexBlockSize == 0)
                m_vecBlockFirstIds.push_back(entry.nId);
            m_ptrIndex->WriteValue(entry);
            ++m_nCount;
        }

        void COSMWayStore::Add(int64_t nId, const TOSMPoints& points)
        {
            if(m_bFinished)
                throw CommonLib::CExcBase("Way store: a way is added after Finish");
            if(points.empty() || nId == m_nLastId)
                return;
            if(nId < m_nLastId)
                m_bSorted = false;
            m_nLastId = nId;

            SEntry entry;
            entry.nId = nId;
            entry.nOffset = m_nPoints;
            entry.nCount = (uint32_t)points.size();
            m_ptrPoints->Write(points.data(), points.size() * sizeof(CommonLib::GisXYPoint));
            m_nPoints += points.size();
            WriteEntry(entry);
        }

        void COSMWayStore::Finish()
        {
            if(m_bFinished)
                return;
            m_ptrPoints->Flush();
            m_ptrIndex->Flush();

            if(!m_bSorted)
            {
                // an unsorted file (an editor's file, usually small): the index is sorted in memory, the first entry of an id stays
                std::vector<SEntry> entries((size_t)m_nCount);
                if(m_nCount)
                    m_ptrIndex->ReadAt(0, entries.data(), entries.size() * sizeof(SEntry));
                std::stable_sort(entries.begin(), entries.end(), [](const SEntry& a, const SEntry& b) { return a.nId < b.nId; });
                entries.erase(std::unique(entries.begin(), entries.end(), [](const SEntry& a, const SEntry& b) { return a.nId == b.nId; }), entries.end());

                m_ptrIndex.reset(new COSMTempFile(m_sTempDir, "_ways_index.bin"));
                m_vecBlockFirstIds.clear();
                m_nCount = 0;
                for(size_t i = 0; i < entries.size(); ++i)
                    WriteEntry(entries[i]);
                m_ptrIndex->Flush();
                m_bSorted = true;
            }
            m_bFinished = true;
        }

        const std::vector<COSMWayStore::SEntry>& COSMWayStore::Block(uint64_t nBlock)
        {
            SCachedBlock& cached = m_cache[(size_t)(nBlock % m_cache.size())];
            if(cached.nBlock != nBlock)
            {
                uint64_t nFirst = nBlock * IndexBlockSize;
                size_t nEntries = (size_t)(std::min)((uint64_t)IndexBlockSize, m_nCount - nFirst);
                cached.entries.resize(nEntries);
                cached.nBlock = UINT64_MAX;
                m_ptrIndex->ReadAt(nFirst * sizeof(SEntry), cached.entries.data(), nEntries * sizeof(SEntry));
                cached.nBlock = nBlock;
            }
            return cached.entries;
        }

        bool COSMWayStore::Get(int64_t nId, TOSMPoints& points)
        {
            if(!m_bFinished)
                throw CommonLib::CExcBase("Way store: Get is called before Finish");
            if(m_vecBlockFirstIds.empty() || nId < m_vecBlockFirstIds.front())
                return false;

            auto itBlock = std::upper_bound(m_vecBlockFirstIds.begin(), m_vecBlockFirstIds.end(), nId);
            uint64_t nBlock = (uint64_t)(itBlock - m_vecBlockFirstIds.begin()) - 1;
            const std::vector<SEntry>& entries = Block(nBlock);
            auto it = std::lower_bound(entries.begin(), entries.end(), nId, [](const SEntry& e, int64_t id) { return e.nId < id; });
            if(it == entries.end() || it->nId != nId)
                return false;

            points.resize(it->nCount);
            m_ptrPoints->ReadAt(it->nOffset * sizeof(CommonLib::GisXYPoint), points.data(), it->nCount * sizeof(CommonLib::GisXYPoint));
            return true;
        }
    }
}
