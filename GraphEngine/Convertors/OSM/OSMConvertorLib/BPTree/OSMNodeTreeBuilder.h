#pragma once

// CNodeTreeBuilder: the node tree with the leaves written by the parallel build (CParallelLeafBuilder),
// the inner levels are built from the written leaves as in TBPMap::BulkLoad.

#include "OSMBPTreeTypes.h"

#include <memory>
#include <utility>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class CNodeTreeBuilder : public TNodeTree
            {
            public:
                typedef std::pair<int64_t, int64_t> TLevelEntry;     // the first key, the address of the node

                CNodeTreeBuilder(int64_t nPageBTreeInfo, std::shared_ptr<bptreedb::storage::IFilePager> pStorage, CommonLib::IAllocPtr pAlloc,
                                 uint32_t nCacheSize, uint32_t nNodesPageSize, bool bMinSplit) :
                        TNodeTree(nPageBTreeInfo, pStorage, pAlloc, nCacheSize, nNodesPageSize, bMinSplit)
                {}

                // the payload of a page of a node (after InnitTree / LoadTree)
                uint32_t NodePagePayload() const { return this->GetNodePageSize(); }
                bptreedb::TCompressorParamsBasePtr CompressParams() const { return this->m_pCompressParams; }

                // the leaves are written in the key order and linked (next / prev), level - their first keys and addresses
                // (used for the inner levels, changed); the tree must be empty. One leaf: its elements go to the root.
                void BuildInner(std::vector<TLevelEntry>& level);

            private:
                bool FitsInOneInnerNode(const std::vector<TLevelEntry>& level);
            };
        }
    }
}
