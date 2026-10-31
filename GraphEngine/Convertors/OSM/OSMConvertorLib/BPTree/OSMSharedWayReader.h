#pragma once

// CSharedWayReader: the way shape file read by many threads at once (the resolving of the relation members), as
// CSharedNodeReader the node file:
//   - the leaf index of the shape tree (BuildLeafIndex) and the shared cache of the decoded leaves (TSharedLeafCache):
//     a decoded leaf keeps the ids and the compressed shapes of its ways (the shapes in the stream pages are read when
//     the leaf is decoded);
//   - a lookup (CLookup, one per thread): the leaf by the index (or the last leaf of the thread), the way by a binary search.
//     A missing leaf is read through the pager of the thread and parsed in the format of TShapeLeaf (CBlobValueEncoder).

#include "OSMWayTreeTypes.h"
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

            class CSharedWayReader
            {
            public:
                // a decoded leaf: the shape of way i - data[offsets[i], offsets[i + 1]) (the last one - to the end)
                struct SLeaf
                {
                    std::vector<int64_t>  ids;
                    std::vector<uint32_t> offsets;
                    std::vector<byte_t>   data;

                    uint32_t Size(size_t i) const { return (uint32_t)((i + 1 < offsets.size() ? offsets[i + 1] : data.size()) - offsets[i]); }
                };
                typedef TSharedLeafCache<SLeaf> TCache;
                typedef TCache::TLeafPtr TLeafPtr;

                // nCacheMB - the memory of the decoded leaves (0 - nDefaultMemory, 0 - a quarter of the physical memory):
                // all the leaves if they fit
                CSharedWayReader(std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage, const SOSMBPTreeWayHeader& header,
                                 uint32_t nCacheMB = 0, uint64_t nDefaultMemory = 0);
                ~CSharedWayReader();

                class CLookup
                {
                public:
                    explicit CLookup(CSharedWayReader& reader);
                    ~CLookup();

                    // the compressed shape of a way (valid until the next Get), false - no such way (or no shape)
                    bool GetBlob(int64_t nId, const byte_t*& pData, uint32_t& nSize);
                    // the points of the shape of a way
                    bool GetPoints(int64_t nId, std::vector<SOSMPointE7>& points);

                private:
                    CLookup(const CLookup&);
                    CLookup& operator=(const CLookup&);

                    TLeafPtr Decode(int64_t nAddr);

                private:
                    CSharedWayReader& m_reader;
                    CommonLib::IAllocPtr m_ptrAlloc;
                    std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;
                    std::unique_ptr<TNodeIdEncoder> m_ptrKeys;
                    size_t   m_nLeaf = (size_t)-1;
                    TLeafPtr m_ptrLeaf;
                };

                size_t   LeafCount() const { return m_keys.size(); }
                const TCache& Cache() const { return m_cache; }

            private:
                CSharedWayReader(const CSharedWayReader&);
                CSharedWayReader& operator=(const CSharedWayReader&);

            private:
                std::shared_ptr<bptreedb::storage::CFileStorage> m_ptrStorage;
                SOSMBPTreeWayHeader m_header;
                bptreedb::TCompressorParamsBasePtr m_ptrParams;
                std::vector<int64_t> m_keys;
                std::vector<int64_t> m_addrs;
                TCache m_cache;
            };
        }
    }
}
