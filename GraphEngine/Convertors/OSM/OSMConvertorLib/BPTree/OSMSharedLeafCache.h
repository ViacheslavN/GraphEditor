#pragma once

// The parts of the readers of a B+ tree used by many threads at once (CSharedNodeReader, CSharedWayReader):
//   - BuildLeafIndex: the first keys and the addresses of all the leaves, read once from the inner levels of the tree;
//   - TSharedLeafCache: the decoded leaves shared by the threads, in shards (every shard with its own lock); a leaf being
//     decoded by one thread is waited for by the others (decoded once); a full shard replaces a random leaf (the batches
//     sweep the keys again and again: an LRU smaller than a sweep would drop every leaf before it is used again).
// TBPMap itself isn't thread safe: find changes its node cache.

#include "OSMBPTreeUtils.h"
#include "../../../../../CommonLib/CommonLib.h"
#include "../../../../../CommonLib/exception/exc_base.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the leaves of a tree in the key order: keys[i] - the first key of the range of leaf i (keys[0] - the smallest
            // int64), addrs[i] - its page. TTree: TBPMap with int64 keys.
            template <class TTree>
            void BuildLeafIndex(TTree& tree, std::vector<int64_t>& keys, std::vector<int64_t>& addrs)
            {
                typedef typename TTree::TBPTreeNodePtr TNodePtr;
                const int64_t nMinKey = (std::numeric_limits<int64_t>::min)();
                keys.clear();
                addrs.clear();
                TNodePtr pRoot = tree.Root();
                if (pRoot->IsLeaf())
                {
                    keys.push_back(nMinKey);
                    addrs.push_back(pRoot->GetAddr());
                    return;
                }

                // the depth of the leaves (they are all at one depth): the leftmost path
                uint32_t nLevels = 1;
                {
                    TNodePtr pNode = pRoot;
                    while (!pNode->IsLeaf())
                    {
                        pNode = tree.GetNode(pNode->Less());
                        ++nLevels;
                    }
                    tree.CheckCache();
                }

                // a child covers [its key, the key of the next one), Less - from the lower bound of the parent
                std::function<void(const TNodePtr&, int64_t, uint32_t)> visit = [&](const TNodePtr& pNode, int64_t nLowKey, uint32_t nLevel)
                {
                    int32_t nCount = (int32_t)pNode->Count();
                    if (nLevel + 1 == nLevels)
                    {
                        keys.push_back(nLowKey);
                        addrs.push_back(pNode->Less());
                        for (int32_t i = 0; i < nCount; ++i)
                        {
                            keys.push_back(pNode->Key(i));
                            addrs.push_back(pNode->Link(i));
                        }
                        return;
                    }
                    // the children (the node may leave the cache of the tree: they are copied)
                    std::vector<std::pair<int64_t, int64_t> > children;
                    children.emplace_back(nLowKey, pNode->Less());
                    for (int32_t i = 0; i < nCount; ++i)
                        children.emplace_back(pNode->Key(i), pNode->Link(i));
                    for (auto& child : children)
                    {
                        TNodePtr pChild = tree.GetNode(child.second);
                        if (!pChild.get())
                            throw CommonLib::CExcBase("node {0} of the tree isn't found", child.second);
                        visit(pChild, child.first, nLevel + 1);
                        tree.CheckCache();
                    }
                };
                visit(pRoot, nMinKey, 1);
                keys[0] = nMinKey;
            }

            // the index entry of a key, false - the key is before all the leaves
            inline bool FindLeaf(const std::vector<int64_t>& keys, int64_t nKey, size_t& nLeaf)
            {
                size_t n = (size_t)(std::upper_bound(keys.begin(), keys.end(), nKey) - keys.begin());
                if (n == 0)
                    return false;
                nLeaf = n - 1;
                return true;
            }

            inline bool InLeaf(const std::vector<int64_t>& keys, size_t nLeaf, int64_t nKey)
            {
                return nLeaf < keys.size() && nKey >= keys[nLeaf] && (nLeaf + 1 == keys.size() || nKey < keys[nLeaf + 1]);
            }

            template <class TLeaf>
            class TSharedLeafCache
            {
            public:
                typedef std::shared_ptr<const TLeaf> TLeafPtr;
                static const size_t Shards = 64;

                TSharedLeafCache() : m_shards(new SShard[Shards]) {}

                void SetCapacity(uint64_t nLeaves) { m_nShardLeaves = (std::max)((size_t)1, (size_t)((nLeaves + Shards - 1) / Shards)); }
                uint64_t Capacity() const { return (uint64_t)m_nShardLeaves * Shards; }

                // the leaf of the page nAddr: the cache, the leaf being decoded by another thread, otherwise fnDecode()
                // (TLeafPtr fnDecode(), on this thread)
                template <class TDecode>
                TLeafPtr Get(int64_t nAddr, TDecode fnDecode)
                {
                    SShard& shard = m_shards[(uint64_t)nAddr % Shards];
                    std::shared_ptr<std::promise<TLeafPtr> > ptrPromise;
                    std::shared_future<TLeafPtr> future;
                    {
                        std::lock_guard<std::mutex> lock(shard.mutex);
                        auto it = shard.map.find(nAddr);
                        if (it != shard.map.end())
                        {
                            m_nHits++;
                            return shard.leaves[it->second].second;
                        }
                        auto itPending = shard.pending.find(nAddr);
                        if (itPending != shard.pending.end())
                            future = itPending->second;
                        else
                        {
                            ptrPromise = std::make_shared<std::promise<TLeafPtr> >();
                            future = ptrPromise->get_future().share();
                            shard.pending.emplace(nAddr, future);
                        }
                    }
                    if (!ptrPromise)
                    {
                        m_nWaits++;
                        return future.get();     // the error of the other thread is thrown here too
                    }

                    TLeafPtr ptrLeaf;
                    try
                    {
                        auto start = TClock::now();
                        ptrLeaf = fnDecode();
                        m_nDecodeNs += NanosecondsSince(start);
                        m_nDecodes++;
                    }
                    catch (...)
                    {
                        ptrPromise->set_exception(std::current_exception());
                        std::lock_guard<std::mutex> lock(shard.mutex);
                        shard.pending.erase(nAddr);
                        throw;
                    }
                    ptrPromise->set_value(ptrLeaf);

                    std::lock_guard<std::mutex> lock(shard.mutex);
                    shard.pending.erase(nAddr);
                    if (shard.leaves.size() < m_nShardLeaves)
                    {
                        shard.map[nAddr] = shard.leaves.size();
                        shard.leaves.emplace_back(nAddr, ptrLeaf);
                    }
                    else
                    {
                        // full: a random leaf is replaced (xorshift)
                        shard.nRandom ^= shard.nRandom << 13;
                        shard.nRandom ^= shard.nRandom >> 7;
                        shard.nRandom ^= shard.nRandom << 17;
                        size_t nDrop = (size_t)(shard.nRandom % shard.leaves.size());
                        shard.map.erase(shard.leaves[nDrop].first);
                        shard.leaves[nDrop] = std::make_pair(nAddr, ptrLeaf);
                        shard.map[nAddr] = nDrop;
                    }
                    return ptrLeaf;
                }

                uint64_t Decodes() const { return m_nDecodes.load(); }   // the leaves decoded (by all the threads)
                uint64_t Hits() const { return m_nHits.load(); }         // found in the cache
                uint64_t Waits() const { return m_nWaits.load(); }       // a thread waited for the leaf decoded by another one
                uint64_t DecodeNs() const { return m_nDecodeNs.load(); }

            private:
                TSharedLeafCache(const TSharedLeafCache&);
                TSharedLeafCache& operator=(const TSharedLeafCache&);

                struct SShard
                {
                    std::mutex mutex;
                    std::vector<std::pair<int64_t, TLeafPtr> > leaves;
                    std::unordered_map<int64_t, size_t> map;    // the address -> the index in leaves
                    std::unordered_map<int64_t, std::shared_future<TLeafPtr> > pending;   // being decoded
                    uint64_t nRandom = 0x9E3779B97F4A7C15ull;    // the state of the random drop
                };

                std::unique_ptr<SShard[]> m_shards;
                size_t m_nShardLeaves = 1;
                std::atomic<uint64_t> m_nDecodes{0};
                std::atomic<uint64_t> m_nHits{0};
                std::atomic<uint64_t> m_nWaits{0};
                std::atomic<uint64_t> m_nDecodeNs{0};
            };

            // the leaves the memory holds: all of them if they fit in nMB (0 - nDefaultBytes), nLeafBytes - a decoded leaf
            inline uint64_t CacheLeavesFor(size_t nLeafCount, uint64_t nLeafBytes, uint32_t nMB, uint64_t nDefaultBytes)
            {
                uint64_t nMemory = nMB ? (uint64_t)nMB * 1024 * 1024 : nDefaultBytes;
                return (std::min)((uint64_t)nLeafCount + 256, nMemory / (std::max)((uint64_t)1, nLeafBytes));
            }
        }
    }
}
