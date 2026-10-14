#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/exception/exc_base.h"
#include "../../../CommonLib/stream/MemoryStream.h"
#include "../../../CommonLib/stream/FixedMemoryStream.h"
#include "../../StorageLib/Storage.h"
#include "../../StorageLib/ReadStreamPage.h"
#include "../../StorageLib/WriteStreamPage.h"
#include "RTreeEntry.h"
#include "RTreeZlibCompressor.h"
#include "RTreeNode.h"
#include "RTreeLeafNode.h"
#include "RTreeInnerNode.h"

#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <functional>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		struct SRTreeParams
		{
			int32_t  m_nCompressLevel{ 6 };   // zlib level of the nodes
			uint32_t m_nMaxCount{ 4096 };     // entries in a node at most (the page size limits it too)
			double   m_dReinsert{ 0.3 };      // R*: part of an overflowing node that is reinserted (once per level and insert)
			double   m_dSplitMin{ 0.4 };      // R*: the smaller group of a split has at least this part of the entries
			double   m_dMinFill{ 0.3 };       // a node with fewer entries than this part of the raw page capacity is dissolved on delete
			double   m_dBulkFill{ 1.0 };      // bulk load fills the nodes up to this part of the page
		};

		struct SRTreeStatistics
		{
			uint64_t m_nEntries{ 0 };                  // leaf entries
			std::vector<uint64_t> m_nodesByLevel;      // [0] - leaves
			std::vector<uint64_t> m_entriesByLevel;
		};

		// R*-tree on the pages of IFilePager, after SQLite rtree:
		//   - one node is one page: uint16 level (0 - leaf), uint32 count, the entries by the compressor
		//     (TRTreeLeafNode: rect -> id, TRTreeInnerNode: MBR -> child address, TRTreeNode - the common part);
		//   - the root has a fixed address (as node 1 of SQLite), the tree info page keeps it and the height;
		//   - insert: ChooseSubtree by the least area enlargement (ties: the least area), an overflowing node
		//     first reinserts 30% of its entries farthest from the center (once per level and insert, R*),
		//     then it is split by the R* split (axis by the least margin sum, distribution by the least overlap);
		//   - remove(rect, id): the leaf is found by the rect, nodes below the minimum fill are dissolved and
		//     their entries reinserted (condense tree). No id -> leaf map like SQLite's _rowid table: the caller
		//     gives the rect of the entry;
		//   - BulkLoad: STR (sort-tile-recursive) packing of an empty tree.
		//
		// A node is full when its entries don't fit in the page by the compressed size (or more than MaxCount);
		// the node keeps an upper estimate of its encoded size, the compressor runs only when the estimate is
		// over the limit (TRTreeNode::Fits). Decisions use the page size minus a reserve.
		//
		// TCompressor: TRTreeZlibCompressor (any coordinates) or TRTreeNumLenCompressor (integer coordinates):
		// RawEntrySize, HeadSize, MaxGrowth, Fits, Prepare, EncodedSize, Write, Read.
		template <class _TCoord, class _TCompressor = TRTreeZlibCompressor<_TCoord> >
		class TRTree
		{
		public:
			typedef _TCoord TCoord;
			typedef _TCompressor TCompressor;
			typedef TRTreeEntry<TCoord> TEntry;
			typedef typename TEntry::TRect TRect;
			typedef std::vector<TEntry> TEntries;

			typedef TRTreeNode<TCoord, TCompressor> TNode;
			typedef TRTreeLeafNode<TCoord, TCompressor> TLeafNode;
			typedef TRTreeInnerNode<TCoord, TCompressor> TInnerNode;
			typedef std::shared_ptr<TNode> TNodePtr;

			// search callback: return false to stop
			typedef std::function<bool(const TRect& rect, int64_t nValue)> TSearchCallback;

			TRTree(int64_t nPageInfo, storage::IFilePagerPtr pStorage, uint32_t nCacheSize, const SRTreeParams& params = SRTreeParams()) :
				m_nPageInfo(nPageInfo), m_pStorage(pStorage), m_nCacheSize(nCacheSize), m_params(params), m_compressor(params.m_nCompressLevel)
			{
			}

			~TRTree()
			{
			}

			// creates the tree if nPageInfo was -1, otherwise loads it
			void Open()
			{
				try
				{
					m_cachePage = m_pStorage->GetNewPage(-1);
					uint32_t nPageSize = m_cachePage->GetSize();
					m_nPageMax = nPageSize - TNode::HeadSize;
					m_nPageLimit = m_nPageMax - std::max<uint32_t>(16, nPageSize / 64);

					uint32_t nRawCap = m_nPageLimit / TCompressor::RawEntrySize;
					if (nRawCap < 4)
						throw CommonLib::CExcBase("page size {0} is too small", nPageSize);
					m_nMinCount = std::max<uint32_t>(2, (uint32_t)(nRawCap * m_params.m_dMinFill));

					if (m_nPageInfo == -1)
					{
						m_nPageInfo = m_pStorage->GetNewFilePageAddr();
						m_nRootAddr = m_pStorage->GetNewFilePageAddr();
						m_nCount = 0;
						m_pRoot = CreateNode(m_nRootAddr, 0);
						m_pRoot->SetDirty(true);
						m_bInfoDirty = true;
						Flush();
					}
					else
					{
						LoadInfo();
						m_pRoot = LoadNode(m_nRootAddr);
					}
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to open, info page: {0}", m_nPageInfo, exc);
					throw;
				}
			}

			int64_t GetPageInfo() const { return m_nPageInfo; }
			uint64_t size() const { return m_nCount; }
			uint32_t height() const { return m_pRoot->Level() + 1; }

			void insert(const TRect& rect, int64_t nValue)
			{
				try
				{
					CheckRect(rect);
					std::vector<bool> reinserted(MaxLevels, false);
					InsertEntry(TEntry(rect, nValue), 0, reinserted);
					++m_nCount;
					m_bInfoDirty = true;
					CheckCache();
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to insert, value: {0}", nValue, exc);
					throw;
				}
			}

			// removes the entry with this rect and value; false if there is no such entry
			bool remove(const TRect& rect, int64_t nValue)
			{
				try
				{
					CheckRect(rect);
					std::vector<TNodePtr> path;
					int32_t nIndex = -1;
					if (!FindLeaf(m_pRoot, rect, nValue, path, nIndex))
						return false;

					path.back()->Erase(nIndex);
					--m_nCount;
					m_bInfoDirty = true;

					CondenseTree(path);
					CheckCache();
					return true;
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to remove, value: {0}", nValue, exc);
					throw;
				}
			}

			// calls func for every entry that intersects rect (touching counts); returns the number of calls
			uint64_t search(const TRect& rect, const TSearchCallback& func)
			{
				try
				{
					uint64_t nFound = 0;
					if (!RectMath::IsValid(rect))
						return 0;

					std::vector<int64_t> stack;
					if (SearchNode(m_pRoot, rect, func, stack, nFound))
						return nFound;

					while (!stack.empty())
					{
						int64_t nAddr = stack.back();
						stack.pop_back();
						TNodePtr pNode = GetNode(nAddr);
						bool bStop = SearchNode(pNode, rect, func, stack, nFound);
						pNode.reset();
						CheckCache(false);    // nodes may leave the cache, the tree mustn't change during the search
						if (bStop)
							break;
					}
					return nFound;
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] search failed", exc);
					throw;
				}
			}

			void search(const TRect& rect, TEntries& result)
			{
				search(rect, [&result](const TRect& r, int64_t v) { result.emplace_back(r, v); return true; });
			}

			// STR bulk load of an empty tree; takes the entries (the vector is left empty)
			void BulkLoad(TEntries& entries)
			{
				try
				{
					if (m_nCount != 0 || !m_pRoot->IsLeaf() || !m_pRoot->Empty())
						throw CommonLib::CExcBase("the tree isn't empty");

					for (const TEntry& e : entries)
						CheckRect(e.m_rect);

					uint64_t nCount = entries.size();
					uint32_t nLimit = std::max<uint32_t>(TCompressor::HeadSize + TCompressor::RawEntrySize,
						(uint32_t)(m_nPageLimit * (std::min)(1.0, m_params.m_dBulkFill)));

					TEntries level;
					level.swap(entries);
					uint32_t nLevel = 0;
					for (;;)
					{
						if (level.size() <= m_params.m_nMaxCount && m_compressor.Fits(level.data(), level.size(), m_nPageLimit))
						{
							m_pRoot = CreateNode(m_nRootAddr, nLevel);
							m_pRoot->Reset(std::move(level));
							break;
						}

						TEntries upper;
						PackLevel(level, nLevel, nLimit, upper);
						level.swap(upper);
						++nLevel;
						if (nLevel >= MaxLevels)
							throw CommonLib::CExcBase("too many levels");
					}

					m_nCount = nCount;
					m_bInfoDirty = true;
					Flush();
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] bulk load failed", exc);
					throw;
				}
			}

			// writes the changed nodes; a node that turns out bigger than the page is split first
			void Flush()
			{
				try
				{
					for (;;)
					{
						std::vector<TNodePtr> dirty;
						for (auto& item : m_cache)
							if (item.second.m_pNode->IsDirty())
								dirty.push_back(item.second.m_pNode);
						if (m_pRoot->IsDirty())
							dirty.push_back(m_pRoot);

						bool bSplit = false;
						for (const TNodePtr& pNode : dirty)
						{
							if (pNode->IsDirty() && !SaveDirtyNode(pNode, true))
							{
								bSplit = true;     // the tree changed: collect the dirty nodes again
								break;
							}
						}
						if (!bSplit)
							break;
					}

					if (m_bInfoDirty)
						SaveInfo();
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] flush failed", exc);
					throw;
				}
			}

			void GetStatistics(SRTreeStatistics& stat)
			{
				stat = SRTreeStatistics();
				stat.m_nodesByLevel.assign(m_pRoot->Level() + 1, 0);
				stat.m_entriesByLevel.assign(m_pRoot->Level() + 1, 0);

				std::vector<int64_t> stack;
				auto visit = [&](const TNodePtr& pNode)
				{
					stat.m_nodesByLevel[pNode->Level()] += 1;
					stat.m_entriesByLevel[pNode->Level()] += pNode->Count();
					if (pNode->IsLeaf())
						stat.m_nEntries += pNode->Count();
					else
						for (const TEntry& e : pNode->Entries())
							stack.push_back(e.m_nValue);
				};

				visit(m_pRoot);
				while (!stack.empty())
				{
					int64_t nAddr = stack.back();
					stack.pop_back();
					visit(GetNode(nAddr));
					CheckCache(false);
				}
			}

			// The pages of the tree without reading the leaves: the inner nodes are read, the leaves are the entries
			// of the nodes of level 1. nLevels - with the leaves.
			void GetPages(uint64_t& nInner, uint64_t& nLeaves, uint32_t& nLevels)
			{
				nInner = nLeaves = 0;
				nLevels = m_pRoot->Level() + 1;
				if (m_pRoot->IsLeaf())
				{
					nLeaves = 1;
					return;
				}

				std::vector<int64_t> stack;
				auto visit = [&](const TNodePtr& pNode)
				{
					nInner += 1;
					if (pNode->Level() == 1)
						nLeaves += pNode->Count();
					else
						for (const TEntry& e : pNode->Entries())
							stack.push_back(e.m_nValue);
				};

				visit(m_pRoot);
				while (!stack.empty())
				{
					int64_t nAddr = stack.back();
					stack.pop_back();
					visit(GetNode(nAddr));
					CheckCache(false);
				}
			}

			// checks the structure: levels, MBRs of the children, the entry count; throws on an error
			void Check()
			{
				uint64_t nEntries = 0;
				if (!m_pRoot->IsLeaf() && m_pRoot->Empty())
					throw CommonLib::CExcBase("[TRTree] check: empty inner root");
				CheckNode(m_pRoot, nullptr, nEntries);
				if (nEntries != m_nCount)
					throw CommonLib::CExcBase("[TRTree] check: {0} entries, count {1}", nEntries, m_nCount);
			}

		private:
			static constexpr uint32_t MaxLevels = 64;
			static constexpr uint32_t InfoMagic = 0x45525452;   // "RTRE"
			static constexpr uint32_t InfoVersion = 1;

			struct SCacheItem
			{
				TNodePtr m_pNode;
				std::list<int64_t>::iterator m_lru;
			};

			//------------------------------------------------------------------------------------------
			// nodes

			static TNodePtr CreateNode(int64_t nAddr, uint32_t nLevel)
			{
				if (nLevel == 0)
					return std::make_shared<TLeafNode>(nAddr);
				return std::make_shared<TInnerNode>(nAddr, nLevel);
			}

			static TLeafNode& Leaf(const TNodePtr& pNode)
			{
				if (!pNode->IsLeaf())
					throw CommonLib::CExcBase("[TRTree] node {0} isn't a leaf", pNode->Addr());
				return static_cast<TLeafNode&>(*pNode);
			}

			static TInnerNode& Inner(const TNodePtr& pNode)
			{
				if (pNode->IsLeaf())
					throw CommonLib::CExcBase("[TRTree] node {0} isn't an inner node", pNode->Addr());
				return static_cast<TInnerNode&>(*pNode);
			}

			bool Fits(TNode& node)
			{
				return node.Fits(m_compressor, m_nPageLimit, m_params.m_nMaxCount);
			}

			//------------------------------------------------------------------------------------------
			// storage

			static void CheckRect(const TRect& rect)
			{
				if (!RectMath::IsValid(rect))
					throw CommonLib::CExcBase("[TRTree] wrong rect: min is above max");
			}

			// writes a node known to fit in the page (a new root, the nodes of a bulk load sized by FitCount)
			void SaveNode(TNode& node)
			{
				try
				{
					m_cachePage->SetAddr(node.Addr());
					CommonLib::CFxMemoryWriteStream stream;
					stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
					node.Write(m_compressor, m_nPageMax, &stream);
					m_pStorage->SavePage(m_cachePage);
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to save node {0}, level {1}", node.Addr(), node.Level(), exc);
					throw;
				}
			}

			// Writes a changed node (it leaves the cache or is flushed). The real encoded size decides: the size
			// estimate of a node is a heuristic (zlib can grow by more than MaxGrowth for one entry), so a node can
			// pass the checks of the inserts and still not fit in the page. Such a node is split first when the
			// tree may change (bCanSplit: no operation in progress), otherwise it isn't written and stays in memory.
			// Returns true if the node was written; false - not written (split: the tree changed, try again).
			bool SaveDirtyNode(const TNodePtr& pNode, bool bCanSplit)
			{
				try
				{
					if (pNode->Prepare(m_compressor, m_nPageMax))
					{
						m_cachePage->SetAddr(pNode->Addr());
						CommonLib::CFxMemoryWriteStream stream;
						stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
						pNode->WritePrepared(m_compressor, &stream);     // the block of this Prepare: compressed once
						m_pStorage->SavePage(m_cachePage);
						return true;
					}

					if (bCanSplit)
						SplitOverflow(pNode);
					return false;
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to save node {0}, level {1}", pNode->Addr(), pNode->Level(), exc);
					throw;
				}
			}

			// a node bigger than the page (found when it was written): split by the path from the root
			void SplitOverflow(const TNodePtr& pNode)
			{
				std::vector<TNodePtr> path;
				if (pNode == m_pRoot)
					path.push_back(m_pRoot);
				else if (pNode->Empty() || !FindPath(m_pRoot, pNode->Addr(), pNode->Level(), pNode->Mbr(), path))
					throw CommonLib::CExcBase("[TRTree] node {0} of level {1} isn't in the tree", pNode->Addr(), pNode->Level());

				PropagateUp(path, path.size() - 1, nullptr);     // split (no forced reinsert), the rects above
			}

			// the path from pFrom to the node nAddr of nLevel with the MBR mbr (the rect of a node in its parent
			// is its MBR, so the node is below the children that contain it)
			bool FindPath(const TNodePtr& pFrom, int64_t nAddr, uint32_t nLevel, const TRect& mbr, std::vector<TNodePtr>& path)
			{
				path.push_back(pFrom);
				if (pFrom->Addr() == nAddr)
					return true;

				if (pFrom->Level() > nLevel)
				{
					std::vector<int64_t> children;
					Inner(pFrom).ContainingChildren(mbr, children);
					for (int64_t nChild : children)
					{
						if (nChild == nAddr || pFrom->Level() - 1 > nLevel)
						{
							if (FindPath(GetNode(nChild), nAddr, nLevel, mbr, path))
								return true;
						}
					}
				}
				path.pop_back();
				return false;
			}

			TNodePtr LoadNode(int64_t nAddr)
			{
				try
				{
					m_pStorage->ReadPage(m_cachePage, nAddr);
					CommonLib::CReadMemoryStream stream;
					stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());

					uint32_t nLevel = 0, nCount = 0;
					TNode::ReadHead(&stream, MaxLevels, m_params.m_nMaxCount, nLevel, nCount);
					TNodePtr pNode = CreateNode(nAddr, nLevel);
					pNode->ReadEntries(m_compressor, nCount, &stream);
					return pNode;
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to load node {0}", nAddr, exc);
					throw;
				}
			}

			void SaveInfo()
			{
				storage::TWriteStreamPage stream(m_pStorage);
				stream.Open(m_nPageInfo, true);
				stream.Write(InfoMagic);
				stream.Write(InfoVersion);
				stream.Write((uint32_t)sizeof(TCoord));
				stream.Write(m_nRootAddr);
				stream.Write(m_nCount);
				stream.Close();
				m_bInfoDirty = false;
			}

			void LoadInfo()
			{
				storage::TReadStreamPage stream(m_pStorage);
				stream.Open(m_nPageInfo);
				uint32_t nMagic = stream.ReadIntu32();
				uint32_t nVersion = stream.ReadIntu32();
				uint32_t nCoordSize = stream.ReadIntu32();
				if (nMagic != InfoMagic || nVersion != InfoVersion)
					throw CommonLib::CExcBase("not an R-tree info page, magic {0}, version {1}", nMagic, nVersion);
				if (nCoordSize != sizeof(TCoord))
					throw CommonLib::CExcBase("coordinate size {0}, the tree type has {1}", nCoordSize, (uint32_t)sizeof(TCoord));

				m_nRootAddr = stream.ReadInt64();
				m_nCount = stream.ReadIntu64();
				m_bInfoDirty = false;
			}

			//------------------------------------------------------------------------------------------
			// node cache (LRU). Nodes leave the cache only in CheckCache, called when no operation that
			// changes the tree is in progress, so the nodes of a path stay the cached ones.
			// The root isn't in the cache: it is m_pRoot (a root that changes its kind is a new object).

			TNodePtr GetNode(int64_t nAddr)
			{
				if (nAddr == m_nRootAddr)
					return m_pRoot;

				auto it = m_cache.find(nAddr);
				if (it != m_cache.end())
				{
					m_lru.splice(m_lru.begin(), m_lru, it->second.m_lru);
					return it->second.m_pNode;
				}

				TNodePtr pNode = LoadNode(nAddr);
				AddToCache(pNode);
				return pNode;
			}

			void AddToCache(const TNodePtr& pNode)
			{
				m_lru.push_front(pNode->Addr());
				m_cache[pNode->Addr()] = SCacheItem{ pNode, m_lru.begin() };
			}

			TNodePtr NewNode(uint32_t nLevel)
			{
				TNodePtr pNode = CreateNode(m_pStorage->GetNewFilePageAddr(), nLevel);
				pNode->SetDirty(true);
				AddToCache(pNode);
				return pNode;
			}

			void DeleteNode(const TNodePtr& pNode)
			{
				auto it = m_cache.find(pNode->Addr());
				if (it != m_cache.end())
				{
					m_lru.erase(it->second.m_lru);
					m_cache.erase(it);
				}
				pNode->SetDirty(false);
				m_pStorage->DropFilePage(pNode->Addr());
			}

			// evicts the least recently used nodes over the cache size; the changed ones are written.
			// bCanSplit: no operation is in progress, a node bigger than the page is split (SaveDirtyNode);
			// otherwise (a search) such a node stays in the cache until the next insert / remove / Flush.
			void CheckCache(bool bCanSplit = true)
			{
				size_t nKept = 0;
				while (m_cache.size() > m_nCacheSize + nKept && !m_lru.empty())
				{
					int64_t nAddr = m_lru.back();
					auto it = m_cache.find(nAddr);
					TNodePtr pNode = it->second.m_pNode;
					if (pNode->IsDirty() && !SaveDirtyNode(pNode, bCanSplit))
					{
						if (!bCanSplit)
						{
							m_lru.splice(m_lru.begin(), m_lru, it->second.m_lru);     // keep it
							if (++nKept >= m_cache.size())
								break;
						}
						continue;    // split: the node changed, look at the cache again
					}
					m_lru.pop_back();
					m_cache.erase(it);
				}
			}

			//------------------------------------------------------------------------------------------
			// insert

			// path from the root to the node of nLevel where rect goes
			void ChooseSubtree(const TRect& rect, uint32_t nLevel, std::vector<TNodePtr>& path)
			{
				TNodePtr pNode = m_pRoot;
				path.push_back(pNode);
				while (pNode->Level() > nLevel)
				{
					const TInnerNode& inner = Inner(pNode);
					pNode = GetNode(inner.ChildAddr(inner.ChooseSubtree(rect)));
					path.push_back(pNode);
				}
			}

			// inserts an entry into a node of nLevel (0 - a leaf entry, > 0 - a subtree of level nLevel - 1);
			// the tree is at least that high (see ReinsertSubtree)
			void InsertEntry(const TEntry& entry, uint32_t nLevel, std::vector<bool>& reinserted)
			{
				if (nLevel > m_pRoot->Level())
					throw CommonLib::CExcBase("[TRTree] an entry of level {0} in a tree of height {1}", nLevel, m_pRoot->Level() + 1);

				std::vector<TNodePtr> path;
				ChooseSubtree(entry.m_rect, nLevel, path);
				path.back()->Add(entry);
				PropagateUp(path, path.size() - 1, &reinserted);
			}

			// path[nIndex] has changed: handles overflows (reinsert / split) and updates the rects up to the root.
			// pReinserted == nullptr: no forced reinsert (split only)
			void PropagateUp(std::vector<TNodePtr>& path, size_t nIndex, std::vector<bool>* pReinserted)
			{
				for (size_t j = nIndex; ; --j)
				{
					TNodePtr pNode = path[j];
					if (!Fits(*pNode))
					{
						if (j != 0 && pReinserted != nullptr && !(*pReinserted)[pNode->Level()])
						{
							(*pReinserted)[pNode->Level()] = true;
							TEntries removed;
							pNode->TakeFarthest(m_params.m_dReinsert, removed);
							PropagateUp(path, j, nullptr);       // the node fits now, shrink the rects above
							for (const TEntry& e : removed)
								InsertEntry(e, pNode->Level(), *pReinserted);
							return;
						}

						if (j == 0)
						{
							SplitRoot();
							return;
						}

						TNodePtr pNew = Split(pNode);
						TInnerNode& parent = Inner(path[j - 1]);
						parent.SetRect(parent.FindChild(pNode->Addr()), pNode->Mbr());
						parent.Add(TEntry(pNew->Mbr(), pNew->Addr()));
						continue;
					}

					if (j == 0)
						return;

					TInnerNode& parent = Inner(path[j - 1]);
					int32_t nChild = parent.FindChild(pNode->Addr());
					TRect mbr = pNode->Mbr();
					if (parent.Entry(nChild).m_rect == mbr)
						return;          // nothing changes above
					parent.SetRect(nChild, mbr);
				}
			}

			// R* split: pNode keeps the first group, the second goes to a new node of the same level
			TNodePtr Split(const TNodePtr& pNode)
			{
				TNodePtr pNew = NewNode(pNode->Level());
				pNew->Reset(pNode->SplitOff(m_params.m_dSplitMin));

				if (!Fits(*pNode) || !Fits(*pNew))
					throw CommonLib::CExcBase("[TRTree] split of node {0}: a half doesn't fit ({1} + {2} entries)", pNode->Addr(), (uint64_t)pNode->Count(), (uint64_t)pNew->Count());
				return pNew;
			}

			// the root keeps its address: its entries go to two new nodes, the root becomes their parent
			void SplitRoot()
			{
				if (m_pRoot->Level() + 1 >= MaxLevels)
					throw CommonLib::CExcBase("[TRTree] too many levels");

				TNodePtr pLeft = NewNode(m_pRoot->Level());
				pLeft->Reset(m_pRoot->TakeEntries());
				TNodePtr pRight = Split(pLeft);

				TEntries rootEntries;
				rootEntries.push_back(TEntry(pLeft->Mbr(), pLeft->Addr()));
				rootEntries.push_back(TEntry(pRight->Mbr(), pRight->Addr()));
				m_pRoot = CreateNode(m_nRootAddr, m_pRoot->Level() + 1);
				m_pRoot->Reset(std::move(rootEntries));
			}

			//------------------------------------------------------------------------------------------
			// remove

			bool FindLeaf(const TNodePtr& pNode, const TRect& rect, int64_t nValue, std::vector<TNodePtr>& path, int32_t& nIndex)
			{
				path.push_back(pNode);
				if (pNode->IsLeaf())
				{
					nIndex = Leaf(pNode).Find(rect, nValue);
					if (nIndex >= 0)
						return true;
				}
				else
				{
					std::vector<int64_t> children;
					Inner(pNode).ContainingChildren(rect, children);
					for (int64_t nChild : children)
					{
						if (FindLeaf(GetNode(nChild), rect, nValue, path, nIndex))
							return true;
					}
				}
				path.pop_back();
				return false;
			}

			// after a remove from path.back(): dissolves underfull nodes, reinserts their entries, shortens the tree
			void CondenseTree(std::vector<TNodePtr>& path)
			{
				std::vector<std::pair<TEntry, uint32_t>> orphans;     // entry, level of its node

				for (size_t j = path.size() - 1; j > 0; --j)
				{
					TNodePtr pNode = path[j];
					TInnerNode& parent = Inner(path[j - 1]);
					int32_t nChild = parent.FindChild(pNode->Addr());

					if (pNode->Count() < m_nMinCount)
					{
						for (const TEntry& e : pNode->Entries())
							orphans.emplace_back(e, pNode->Level());
						parent.Erase(nChild);
						DeleteNode(pNode);
					}
					else
						parent.SetRect(nChild, pNode->Mbr());
				}

				if (!m_pRoot->IsLeaf() && m_pRoot->Empty())
				{
					m_pRoot = CreateNode(m_nRootAddr, 0);      // everything below was dissolved
					m_pRoot->SetDirty(true);
				}

				// higher subtrees first: they don't depend on the leaves
				std::stable_sort(orphans.begin(), orphans.end(), [](const std::pair<TEntry, uint32_t>& a, const std::pair<TEntry, uint32_t>& b) { return a.second > b.second; });
				for (const auto& orphan : orphans)
				{
					std::vector<bool> reinserted(MaxLevels, false);
					if (orphan.second > m_pRoot->Level())
						ReinsertSubtree(orphan.first, orphan.second, reinserted);
					else
						InsertEntry(orphan.first, orphan.second, reinserted);
				}

				// a root with one child: the child becomes the root
				while (!m_pRoot->IsLeaf() && m_pRoot->Count() == 1)
				{
					TNodePtr pChild = GetNode(Inner(m_pRoot).ChildAddr(0));
					TNodePtr pRoot = CreateNode(m_nRootAddr, pChild->Level());
					pRoot->Reset(pChild->TakeEntries());
					DeleteNode(pChild);
					m_pRoot = pRoot;
				}
			}

			// an orphan subtree (entry of a node of nLevel) higher than the tree after the condense: its top node
			// is taken apart, its entries go one level lower (recursively) and its page is freed
			void ReinsertSubtree(const TEntry& entry, uint32_t nLevel, std::vector<bool>& reinserted)
			{
				if (nLevel <= m_pRoot->Level())
				{
					InsertEntry(entry, nLevel, reinserted);
					return;
				}

				TNodePtr pChild = GetNode(entry.m_nValue);
				TEntries entries = pChild->TakeEntries();
				DeleteNode(pChild);
				for (const TEntry& e : entries)
					ReinsertSubtree(e, nLevel - 1, reinserted);
			}

			//------------------------------------------------------------------------------------------
			// search

			// returns true if func asked to stop
			bool SearchNode(const TNodePtr& pNode, const TRect& rect, const TSearchCallback& func, std::vector<int64_t>& stack, uint64_t& nFound)
			{
				if (pNode->IsLeaf())
					return Leaf(pNode).Search(rect, func, nFound);
				Inner(pNode).IntersectingChildren(rect, stack);
				return false;
			}

			//------------------------------------------------------------------------------------------
			// bulk load

			// the most entries from nStart (up to nEnd) that fit in a node of nLimit bytes
			// The number of entries from nStart that fit in a node (bulk load). Every probe of a compressed size is
			// a compression of the entries, so the search starts from the count of the previous node (the nodes of a
			// level compress alike) and gallops from it: 2..3 compressions per node instead of a binary search over
			// 1..MaxCount (13 compressions, the first of MaxCount entries - the most of the bulk load time).
			size_t FitCount(const TEntries& v, size_t nStart, size_t nEnd, uint32_t nLimit)
			{
				size_t nMax = std::min<size_t>(nEnd - nStart, m_params.m_nMaxCount);
				const TEntry* p = v.data() + nStart;

				// all of them uncompressed: no compression
				if (TCompressor::HeadSize + nMax * TCompressor::RawEntrySize <= nLimit)
					return nMax;

				auto fits = [&](size_t nCount) { return m_compressor.Fits(p, nCount, nLimit); };

				size_t nGuess = m_nBulkFit != 0 ? m_nBulkFit : (size_t)(nLimit / TCompressor::RawEntrySize);   // the raw capacity
				nGuess = (std::max<size_t>)(1, (std::min)(nGuess, nMax));

				size_t lo, hi;   // lo fits, hi doesn't (hi == nMax + 1: unknown)
				if (fits(nGuess))
				{
					lo = nGuess;
					hi = nMax + 1;
					size_t nStep = (std::max<size_t>)(1, lo / 16);
					while (lo < nMax)
					{
						size_t nNext = (std::min)(nMax, lo + nStep);
						if (!fits(nNext))
						{
							hi = nNext;
							break;
						}
						lo = nNext;
						nStep *= 2;
					}
				}
				else
				{
					hi = nGuess;
					lo = 0;
					size_t nStep = (std::max<size_t>)(1, hi / 16);
					while (hi > 1)
					{
						size_t nNext = hi > nStep ? hi - nStep : 1;
						if (fits(nNext))
						{
							lo = nNext;
							break;
						}
						hi = nNext;
						nStep *= 2;
					}
					if (lo == 0)
						throw CommonLib::CExcBase("[TRTree] one entry doesn't fit in a node");
				}

				// between lo and hi; a node a few entries short is good enough (1/64 of the count)
				size_t nTolerance = (std::max<size_t>)(1, lo / 64);
				while (hi <= nMax && hi - lo > nTolerance)
				{
					size_t mid = lo + (hi - lo) / 2;
					if (fits(mid))
						lo = mid;
					else
						hi = mid;
				}

				if (lo < nMax || nMax == m_params.m_nMaxCount)
					m_nBulkFit = lo;     // the last node of a slice (nMax small) doesn't tell the capacity
				return lo;
			}

			// STR: packs the entries of a level into nodes, upper gets an entry per node
			void PackLevel(TEntries& level, uint32_t nLevel, uint32_t nLimit, TEntries& upper)
			{
				auto byX = [](const TEntry& a, const TEntry& b) { return RectMath::CenterX(a.m_rect) < RectMath::CenterX(b.m_rect); };
				auto byY = [](const TEntry& a, const TEntry& b) { return RectMath::CenterY(a.m_rect) < RectMath::CenterY(b.m_rect); };

				size_t n = level.size();
				std::sort(level.begin(), level.end(), byX);
				m_nBulkFit = 0;     // the nodes of another level compress differently

				// node capacity by the first entries: the number of vertical slices
				size_t nCap = FitCount(level, 0, n, nLimit);
				size_t nNodes = (n + nCap - 1) / nCap;
				size_t nSlices = (size_t)std::ceil(std::sqrt((double)nNodes));
				size_t nSliceSize = nSlices * nCap;

				TNodePtr pNode = CreateNode(-1, nLevel);
				for (size_t nSlice = 0; nSlice < n; nSlice += nSliceSize)
				{
					size_t nSliceEnd = (std::min)(n, nSlice + nSliceSize);
					std::sort(level.begin() + nSlice, level.begin() + nSliceEnd, byY);

					for (size_t i = nSlice; i < nSliceEnd; )
					{
						size_t k = FitCount(level, i, nSliceEnd, nLimit);
						pNode->SetAddr(m_pStorage->GetNewFilePageAddr());
						pNode->Reset(TEntries(level.begin() + i, level.begin() + i + k));
						SaveNode(*pNode);           // written at once, not cached
						upper.push_back(TEntry(pNode->Mbr(), pNode->Addr()));
						i += k;
					}
				}
			}

			//------------------------------------------------------------------------------------------
			// check

			void CheckNode(const TNodePtr& pNode, const TRect* pParentRect, uint64_t& nEntries)
			{
				if (pNode != m_pRoot && pNode->Empty())
					throw CommonLib::CExcBase("[TRTree] check: empty node {0}", pNode->Addr());
				if (!pNode->Empty() && pParentRect != nullptr && !(pNode->Mbr() == *pParentRect))
					throw CommonLib::CExcBase("[TRTree] check: the parent rect of node {0} isn't its MBR", pNode->Addr());
				if (!m_compressor.Prepare(pNode->Entries().data(), pNode->Count(), m_nPageMax))
					throw CommonLib::CExcBase("[TRTree] check: node {0} doesn't fit", pNode->Addr());

				if (pNode->IsLeaf())
				{
					nEntries += pNode->Count();
					return;
				}

				TEntries entries = pNode->Entries();     // the node may leave the cache below
				for (const TEntry& e : entries)
				{
					TNodePtr pChild = GetNode(e.m_nValue);
					if (pChild->Level() + 1 != pNode->Level())
						throw CommonLib::CExcBase("[TRTree] check: node {0} level {1}, parent level {2}", pChild->Addr(), pChild->Level(), pNode->Level());
					CheckNode(pChild, &e.m_rect, nEntries);
				}
				CheckCache(false);
			}

		private:
			int64_t m_nPageInfo{ -1 };
			int64_t m_nRootAddr{ -1 };
			uint64_t m_nCount{ 0 };
			bool m_bInfoDirty{ false };

			storage::IFilePagerPtr m_pStorage;
			storage::IFilePagePtr m_cachePage;
			uint32_t m_nCacheSize;
			SRTreeParams m_params;
			TCompressor m_compressor;

			uint32_t m_nPageMax{ 0 };     // bytes for the entries in a page
			uint32_t m_nPageLimit{ 0 };   // the same minus a reserve: the limit for "the node is full"
			uint32_t m_nMinCount{ 2 };
			size_t m_nBulkFit{ 0 };       // bulk load: the entries of the last full node (the guess of FitCount)

			TNodePtr m_pRoot;
			std::unordered_map<int64_t, SCacheItem> m_cache;
			std::list<int64_t> m_lru;
		};
	}
	}
}
