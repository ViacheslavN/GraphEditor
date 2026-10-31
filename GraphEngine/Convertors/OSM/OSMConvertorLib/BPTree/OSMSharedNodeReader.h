#pragma once

// CSharedNodeReader: the node file read by many threads at once (the resolving of the way / relation refs).
//
//   - the leaf index (BuildLeafIndex) and the shared cache of the decoded leaves (TSharedLeafCache, ~35 KB a leaf);
//   - a lookup (CLookup, one per thread): the leaf by the index (or the last leaf of the thread), the node by a binary
//     search in the decoded ids. A missing leaf is read through the pager of the thread (the storage is shared, it locks
//     its reads) and decoded by the encoders of the thread (TNodeIdEncoder, TPointCompressor - the format of TNodeLeaf).

#include "OSMBPTreeTypes.h"
#include "OSMBPTreeSettings.h"
#include "OSMSharedLeafCache.h"

#include <memory>
#include <vector>

namespace bptreedb {
    namespace storage {
        class CFileStorage;
        class CFilePager;
    }
}

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class CSharedNodeReader
            {
            public:
                // a decoded leaf
                struct SLeaf
                {
                    std::vector<int64_t>     ids;
                    std::vector<SOSMPointE7> points;
                };
                typedef TSharedLeafCache<SLeaf> TCache;
                typedef TCache::TLeafPtr TLeafPtr;

                // nCacheLeaves - the decoded leaves kept for all the threads, 0 - all of them if they fit in nCacheMB
                // (0 - nDefaultMemory, 0 - half of the physical memory), otherwise as many as fit
                CSharedNodeReader(std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage, const SOSMBPTreeNodeHeader& header,
                                  uint32_t nCacheLeaves, uint32_t nCacheMB = 0, uint64_t nDefaultMemory = 0);
                ~CSharedNodeReader();

                // the lookups of one thread
                class CLookup
                {
                public:
                    explicit CLookup(CSharedNodeReader& reader);
                    ~CLookup();

                    // false - no such node
                    bool Get(int64_t nId, SOSMPointE7& point);

                private:
                    CLookup(const CLookup&);
                    CLookup& operator=(const CLookup&);

                    // the leaf of the page nAddr (the pager and the encoders of the thread)
                    TLeafPtr Decode(int64_t nAddr);

                private:
                    CSharedNodeReader& m_reader;
                    CommonLib::IAllocPtr m_ptrAlloc;
                    std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;
                    std::unique_ptr<TNodeIdEncoder> m_ptrKeys;
                    std::unique_ptr<TPointCompressor> m_ptrValues;
                    size_t   m_nLeaf = (size_t)-1;     // the last leaf: the index entry, the decoded leaf
                    TLeafPtr m_ptrLeaf;
                };

                size_t   LeafCount() const { return m_keys.size(); }
                const TCache& Cache() const { return m_cache; }
                // compatibility of the progress
                uint64_t Decodes() const { return m_cache.Decodes(); }
                uint64_t CacheHits() const { return m_cache.Hits(); }
                uint64_t CacheWaits() const { return m_cache.Waits(); }
                uint64_t DecodeNs() const { return m_cache.DecodeNs(); }
                uint64_t CacheLeaves() const { return m_cache.Capacity(); }

            private:
                CSharedNodeReader(const CSharedNodeReader&);
                CSharedNodeReader& operator=(const CSharedNodeReader&);

            private:
                std::shared_ptr<bptreedb::storage::CFileStorage> m_ptrStorage;
                SOSMBPTreeNodeHeader m_header;
                bptreedb::TCompressorParamsBasePtr m_ptrParams;
                std::vector<int64_t> m_keys;    // the first keys of the leaves (the first one - the smallest int64)
                std::vector<int64_t> m_addrs;   // their addresses
                TCache m_cache;
            };
        }
    }
}
