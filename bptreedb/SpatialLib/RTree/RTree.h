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
		//   - one node is one page: uint16 level (0 - leaf), uint32 count, the entries by the compressor;
		//   - the root has a fixed address (as node 1 of SQLite), the tree info page keeps it and the height;
		//   - insert: ChooseSubtree by the least area enlargement (ties: the least area), an overflowing node
		//     first reinserts 30% of its entries farthest from the center (once per level and insert, R*),
		//     then it is split by the R* split (axis by the least margin sum, distribution by the least overlap);
		//   - remove(rect, id): the leaf is found by the rect, nodes below the minimum fill are dissolved and
		//     their entries reinserted (condense tree). No id -> leaf map like SQLite's _rowid table: the caller
		//     gives the rect of the entry;
		//   - BulkLoad: STR (sort-tile-recursive) packing of an empty tree.
		//
		// A node is full when its entries don't fit in the page by the compressed size (or more than MaxCount).
		// Every node keeps an upper estimate of its encoded size: the real size after load / save / a real
		// check, plus TCompressor::MaxGrowth for every entry added, removed or changed since. The compressor
		// runs only when the estimate is over the limit. Decisions use the page size minus a reserve.
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
					m_nPageMax = nPageSize - NodeHeadSize;
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
						m_pRoot = std::make_shared<TNode>(m_nRootAddr, 0);
						m_pRoot->m_bDirty = true;
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
			uint32_t height() const { return m_pRoot->m_nLevel + 1; }

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
						CheckCache();      // read only: nodes may leave the cache during the search
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

			void search(const TRect& rect, std::vector<TEntry>& result)
			{
				search(rect, [&result](const TRect& r, int64_t v) { result.emplace_back(r, v); return true; });
			}

			// STR bulk load of an empty tree; takes the entries (the vector is left empty)
			void BulkLoad(std::vector<TEntry>& entries)
			{
				try
				{
					if (m_nCount != 0 || m_pRoot->m_nLevel != 0 || !m_pRoot->m_entries.empty())
						throw CommonLib::CExcBase("the tree isn't empty");

					for (const TEntry& e : entries)
						CheckRect(e.m_rect);

					uint64_t nCount = entries.size();
					uint32_t nLimit = std::max<uint32_t>(TCompressor::HeadSize + TCompressor::RawEntrySize,
						(uint32_t)(m_nPageLimit * (std::min)(1.0, m_params.m_dBulkFill)));

					std::vector<TEntry> level;
					level.swap(entries);
					uint32_t nLevel = 0;
					for (;;)
					{
						if (level.size() <= m_params.m_nMaxCount && m_compressor.Fits(level.data(), level.size(), m_nPageLimit))
						{
							m_pRoot->Reset(std::move(level));
							m_pRoot->m_nLevel = nLevel;
							break;
						}

						std::vector<TEntry> upper;
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

			void Flush()
			{
				try
				{
					for (auto& item : m_cache)
					{
						if (item.second.m_pNode->m_bDirty)
							SaveNode(*item.second.m_pNode);
					}

					if (m_pRoot->m_bDirty)
						SaveNode(*m_pRoot);

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
				stat.m_nodesByLevel.assign(m_pRoot->m_nLevel + 1, 0);
				stat.m_entriesByLevel.assign(m_pRoot->m_nLevel + 1, 0);

				std::vector<int64_t> stack;
				auto visit = [&](const TNodePtr& pNode)
				{
					stat.m_nodesByLevel[pNode->m_nLevel] += 1;
					stat.m_entriesByLevel[pNode->m_nLevel] += pNode->m_entries.size();
					if (pNode->m_nLevel == 0)
						stat.m_nEntries += pNode->m_entries.size();
					else
						for (const TEntry& e : pNode->m_entries)
							stack.push_back(e.m_nValue);
				};

				visit(m_pRoot);
				while (!stack.empty())
				{
					int64_t nAddr = stack.back();
					stack.pop_back();
					visit(GetNode(nAddr));
					CheckCache();
				}
			}

			// checks the structure: levels, MBRs of the children, the entry count; throws on an error
			void Check()
			{
				uint64_t nEntries = 0;
				if (m_pRoot->m_nLevel != 0 && m_pRoot->m_entries.empty())
					throw CommonLib::CExcBase("[TRTree] check: empty inner root");
				CheckNode(m_pRoot, nullptr, nEntries);
				if (nEntries != m_nCount)
					throw CommonLib::CExcBase("[TRTree] check: {0} entries, count {1}", nEntries, m_nCount);
			}

		private:
			static constexpr uint32_t NodeHeadSize = sizeof(uint16_t) + sizeof(uint32_t);
			static constexpr uint32_t MaxLevels = 64;
			static constexpr uint32_t InfoMagic = 0x45525452;   // "RTRE"
			static constexpr uint32_t InfoVersion = 1;

			struct TNode
			{
				TNode(int64_t nAddr, uint32_t nLevel) : m_nAddr(nAddr), m_nLevel(nLevel) {}

				TRect Mbr() const { return RectMath::Mbr(m_entries.begin(), m_entries.end()); }

				int32_t FindChild(int64_t nAddr) const
				{
					for (size_t i = 0; i < m_entries.size(); ++i)
						if (m_entries[i].m_nValue == nAddr)
							return (int32_t)i;
					throw CommonLib::CExcBase("[TRTree] node {0} has no child {1}", m_nAddr, nAddr);
				}

				// all changes of the entries go through these: they keep m_nEstSize an upper estimate
				void Add(const TEntry& e)
				{
					m_entries.push_back(e);
					Grow();
				}

				void SetRect(size_t i, const TRect& rect)
				{
					m_entries[i].m_rect = rect;
					Grow();
				}

				void Erase(size_t i)
				{
					m_entries.erase(m_entries.begin() + i);    // a removed entry can make the encoding bigger too (deltas)
					Grow();
				}

				void Reset(std::vector<TEntry>&& entries)
				{
					m_entries = std::move(entries);
					m_nEstSize = 0;
					m_bDirty = true;
				}

				void Grow()
				{
					if (m_nEstSize != 0)
						m_nEstSize += TCompressor::MaxGrowth(m_entries.size());
					m_bDirty = true;
				}

				int64_t m_nAddr;
				uint32_t m_nLevel;
				std::vector<TEntry> m_entries;
				bool m_bDirty{ false };
				uint32_t m_nEstSize{ 0 };     // upper estimate of the encoded size, 0 - unknown
			};
			typedef std::shared_ptr<TNode> TNodePtr;

			struct SCacheItem
			{
				TNodePtr m_pNode;
				std::list<int64_t>::iterator m_lru;
			};

			//------------------------------------------------------------------------------------------
			// storage

			static void CheckRect(const TRect& rect)
			{
				if (!RectMath::IsValid(rect))
					throw CommonLib::CExcBase("[TRTree] wrong rect: min is above max");
			}

			bool Fits(TNode& node)
			{
				size_t n = node.m_entries.size();
				if (n > m_params.m_nMaxCount)
					return false;
				if (TCompressor::HeadSize + n * TCompressor::RawEntrySize <= m_nPageLimit)
					return true;
				if (node.m_nEstSize != 0 && node.m_nEstSize <= m_nPageLimit)
					return true;

				bool bFits = m_compressor.Prepare(node.m_entries.data(), n, m_nPageLimit);
				node.m_nEstSize = m_compressor.EncodedSize();
				return bFits;
			}

			void SaveNode(TNode& node)
			{
				try
				{
					if (!m_compressor.Prepare(node.m_entries.data(), node.m_entries.size(), m_nPageMax))
						throw CommonLib::CExcBase("{0} entries don't fit in the page", (uint64_t)node.m_entries.size());

					m_cachePage->SetAddr(node.m_nAddr);
					CommonLib::CFxMemoryWriteStream stream;
					stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
					stream.Write((uint16_t)node.m_nLevel);
					stream.Write((uint32_t)node.m_entries.size());
					m_compressor.Write(&stream);
					m_pStorage->SavePage(m_cachePage);
					node.m_nEstSize = m_compressor.EncodedSize();
					node.m_bDirty = false;
				}
				catch (std::exception& exc)
				{
					CommonLib::CExcBase::RegenExcT("[TRTree] failed to save node {0}, level {1}", node.m_nAddr, node.m_nLevel, exc);
					throw;
				}
			}

			TNodePtr LoadNode(int64_t nAddr)
			{
				try
				{
					m_pStorage->ReadPage(m_cachePage, nAddr);
					CommonLib::CReadMemoryStream stream;
					stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());

					uint32_t nLevel = stream.Readintu16();
					uint32_t nCount = stream.ReadIntu32();
					if (nLevel >= MaxLevels || nCount > m_params.m_nMaxCount)
						throw CommonLib::CExcBase("wrong node header, level {0}, count {1}", nLevel, nCount);

					TNodePtr pNode = std::make_shared<TNode>(nAddr, nLevel);
					m_compressor.Read(nCount, pNode->m_entries, &stream);
					pNode->m_nEstSize = m_compressor.EncodedSize();
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
				m_lru.push_front(pNode->m_nAddr);
				m_cache[pNode->m_nAddr] = SCacheItem{ pNode, m_lru.begin() };
			}

			TNodePtr NewNode(uint32_t nLevel)
			{
				TNodePtr pNode = std::make_shared<TNode>(m_pStorage->GetNewFilePageAddr(), nLevel);
				pNode->m_bDirty = true;
				AddToCache(pNode);
				return pNode;
			}

			void DeleteNode(const TNodePtr& pNode)
			{
				auto it = m_cache.find(pNode->m_nAddr);
				if (it != m_cache.end())
				{
					m_lru.erase(it->second.m_lru);
					m_cache.erase(it);
				}
				pNode->m_bDirty = false;
				m_pStorage->DropFilePage(pNode->m_nAddr);
			}

			void CheckCache()
			{
				while (m_cache.size() > m_nCacheSize && !m_lru.empty())
				{
					int64_t nAddr = m_lru.back();
					auto it = m_cache.find(nAddr);
					if (it->second.m_pNode->m_bDirty)
						SaveNode(*it->second.m_pNode);
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
				while (pNode->m_nLevel > nLevel)
				{
					if (pNode->m_entries.empty())
						throw CommonLib::CExcBase("[TRTree] empty inner node {0}", pNode->m_nAddr);

					size_t nBest = 0;
					double dBestGrow = 0, dBestArea = 0;
					for (size_t i = 0; i < pNode->m_entries.size(); ++i)
					{
						const TRect& r = pNode->m_entries[i].m_rect;
						double dArea = RectMath::Area(r);
						double dGrow = RectMath::Area(RectMath::Union(r, rect)) - dArea;
						if (i == 0 || dGrow < dBestGrow || (dGrow == dBestGrow && dArea < dBestArea))
						{
							nBest = i;
							dBestGrow = dGrow;
							dBestArea = dArea;
						}
					}

					pNode = GetNode(pNode->m_entries[nBest].m_nValue);
					path.push_back(pNode);
				}
			}

			// inserts an entry into a node of nLevel (0 - leaf entry, > 0 - a subtree of level nLevel - 1)
			void InsertEntry(const TEntry& entry, uint32_t nLevel, std::vector<bool>& reinserted)
			{
				if (nLevel > m_pRoot->m_nLevel)
				{
					// a subtree from a dissolved node is higher than the tree now: insert its entries
					TNodePtr pChild = GetNode(entry.m_nValue);
					std::vector<TEntry> entries = pChild->m_entries;
					DeleteNode(pChild);
					for (const TEntry& e : entries)
						InsertEntry(e, nLevel - 1, reinserted);
					return;
				}

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
						if (j != 0 && pReinserted != nullptr && !(*pReinserted)[pNode->m_nLevel])
						{
							(*pReinserted)[pNode->m_nLevel] = true;
							std::vector<TEntry> removed;
							TakeFarthest(*pNode, removed);
							PropagateUp(path, j, nullptr);       // the node fits now, shrink the rects above
							for (const TEntry& e : removed)
								InsertEntry(e, pNode->m_nLevel, *pReinserted);
							return;
						}

						if (j == 0)
						{
							SplitRoot();
							return;
						}

						TNodePtr pNew = Split(pNode);
						TNodePtr pParent = path[j - 1];
						pParent->SetRect(pParent->FindChild(pNode->m_nAddr), pNode->Mbr());
						pParent->Add(TEntry(pNew->Mbr(), pNew->m_nAddr));
						continue;
					}

					if (j == 0)
						return;

					TNodePtr pParent = path[j - 1];
					int32_t nChild = pParent->FindChild(pNode->m_nAddr);
					TRect mbr = pNode->Mbr();
					if (pParent->m_entries[nChild].m_rect == mbr)
						return;          // nothing changes above
					pParent->SetRect(nChild, mbr);
				}
			}

			// R*: removes the part of the entries farthest from the center of the node; closest first
			void TakeFarthest(TNode& node, std::vector<TEntry>& removed)
			{
				size_t n = node.m_entries.size();
				size_t p = std::max<size_t>(1, (size_t)(n * m_params.m_dReinsert));
				TRect center = node.Mbr();

				std::vector<std::pair<double, size_t>> dist(n);
				for (size_t i = 0; i < n; ++i)
					dist[i] = std::make_pair(RectMath::CenterDist2(node.m_entries[i].m_rect, center), i);
				std::sort(dist.begin(), dist.end(), [](const std::pair<double, size_t>& a, const std::pair<double, size_t>& b) { return a.first > b.first; });

				std::vector<bool> take(n, false);
				for (size_t i = 0; i < p; ++i)
					take[dist[i].second] = true;

				for (size_t i = p; i-- > 0; )
					removed.push_back(node.m_entries[dist[i].second]);

				std::vector<TEntry> keep;
				keep.reserve(n - p);
				for (size_t i = 0; i < n; ++i)
					if (!take[i])
						keep.push_back(node.m_entries[i]);
				node.Reset(std::move(keep));
			}

			// R* split: pNode keeps the first group, the second goes to a new node of the same level
			TNodePtr Split(const TNodePtr& pNode)
			{
				std::vector<TEntry>& entries = pNode->m_entries;
				size_t n = entries.size();
				if (n < 2)
					throw CommonLib::CExcBase("[TRTree] one entry doesn't fit in a node");

				size_t m = std::max<size_t>(1, (size_t)(n * m_params.m_dSplitMin));
				if (m * 2 > n)
					m = n / 2;

				// sorts: axis 0 - x, 1 - y; by min (then max) and by max (then min)
				auto sortBy = [](std::vector<TEntry>& v, int axis, bool byMax)
				{
					std::sort(v.begin(), v.end(), [axis, byMax](const TEntry& a, const TEntry& b)
					{
						double a1 = axis == 0 ? (double)(byMax ? a.m_rect.m_maxX : a.m_rect.m_minX) : (double)(byMax ? a.m_rect.m_maxY : a.m_rect.m_minY);
						double b1 = axis == 0 ? (double)(byMax ? b.m_rect.m_maxX : b.m_rect.m_minX) : (double)(byMax ? b.m_rect.m_maxY : b.m_rect.m_minY);
						if (a1 != b1)
							return a1 < b1;
						double a2 = axis == 0 ? (double)(byMax ? a.m_rect.m_minX : a.m_rect.m_maxX) : (double)(byMax ? a.m_rect.m_minY : a.m_rect.m_maxY);
						double b2 = axis == 0 ? (double)(byMax ? b.m_rect.m_minX : b.m_rect.m_maxX) : (double)(byMax ? b.m_rect.m_minY : b.m_rect.m_maxY);
						return a2 < b2;
					});
				};

				std::vector<TRect> prefix(n), suffix(n);
				auto calcBounds = [&](const std::vector<TEntry>& v)
				{
					prefix[0] = v[0].m_rect;
					for (size_t i = 1; i < n; ++i)
						prefix[i] = RectMath::Union(prefix[i - 1], v[i].m_rect);
					suffix[n - 1] = v[n - 1].m_rect;
					for (size_t i = n - 1; i-- > 0; )
						suffix[i] = RectMath::Union(suffix[i + 1], v[i].m_rect);
				};

				// choose the axis: the least sum of the margins of all distributions
				std::vector<TEntry> sorted = entries;
				int nBestAxis = 0;
				double dBestMargin = 0;
				for (int axis = 0; axis < 2; ++axis)
				{
					double dMargin = 0;
					for (int byMax = 0; byMax < 2; ++byMax)
					{
						sortBy(sorted, axis, byMax != 0);
						calcBounds(sorted);
						for (size_t k = m; k <= n - m; ++k)
							dMargin += RectMath::Margin(prefix[k - 1]) + RectMath::Margin(suffix[k]);
					}
					if (axis == 0 || dMargin < dBestMargin)
					{
						nBestAxis = axis;
						dBestMargin = dMargin;
					}
				}

				// choose the distribution on the axis: the least overlap, then the least area
				bool bBestByMax = false;
				size_t nBestK = m;
				double dBestOverlap = 0, dBestArea = 0;
				bool bFirst = true;
				for (int byMax = 0; byMax < 2; ++byMax)
				{
					sortBy(sorted, nBestAxis, byMax != 0);
					calcBounds(sorted);
					for (size_t k = m; k <= n - m; ++k)
					{
						double dOverlap = RectMath::OverlapArea(prefix[k - 1], suffix[k]);
						double dArea = RectMath::Area(prefix[k - 1]) + RectMath::Area(suffix[k]);
						if (bFirst || dOverlap < dBestOverlap || (dOverlap == dBestOverlap && dArea < dBestArea))
						{
							bFirst = false;
							bBestByMax = byMax != 0;
							nBestK = k;
							dBestOverlap = dOverlap;
							dBestArea = dArea;
						}
					}
				}

				sortBy(sorted, nBestAxis, bBestByMax);
				TNodePtr pNew = NewNode(pNode->m_nLevel);
				pNew->Reset(std::vector<TEntry>(sorted.begin() + nBestK, sorted.end()));
				pNode->Reset(std::vector<TEntry>(sorted.begin(), sorted.begin() + nBestK));

				if (!Fits(*pNode) || !Fits(*pNew))
					throw CommonLib::CExcBase("[TRTree] split of node {0}: a half doesn't fit ({1} + {2} entries)", pNode->m_nAddr, (uint64_t)entries.size(), (uint64_t)pNew->m_entries.size());

				return pNew;
			}

			// the root keeps its address: its entries go to two new nodes, the root becomes their parent
			void SplitRoot()
			{
				if (m_pRoot->m_nLevel + 1 >= MaxLevels)
					throw CommonLib::CExcBase("[TRTree] too many levels");

				TNodePtr pLeft = NewNode(m_pRoot->m_nLevel);
				pLeft->Reset(std::move(m_pRoot->m_entries));
				TNodePtr pRight = Split(pLeft);

				std::vector<TEntry> rootEntries;
				rootEntries.push_back(TEntry(pLeft->Mbr(), pLeft->m_nAddr));
				rootEntries.push_back(TEntry(pRight->Mbr(), pRight->m_nAddr));
				m_pRoot->Reset(std::move(rootEntries));
				m_pRoot->m_nLevel += 1;
			}

			//------------------------------------------------------------------------------------------
			// remove

			bool FindLeaf(const TNodePtr& pNode, const TRect& rect, int64_t nValue, std::vector<TNodePtr>& path, int32_t& nIndex)
			{
				path.push_back(pNode);
				if (pNode->m_nLevel == 0)
				{
					for (size_t i = 0; i < pNode->m_entries.size(); ++i)
					{
						const TEntry& e = pNode->m_entries[i];
						if (e.m_nValue == nValue && e.m_rect == rect)
						{
							nIndex = (int32_t)i;
							return true;
						}
					}
				}
				else
				{
					for (size_t i = 0; i < pNode->m_entries.size(); ++i)
					{
						if (RectMath::Contains(pNode->m_entries[i].m_rect, rect) &&
							FindLeaf(GetNode(pNode->m_entries[i].m_nValue), rect, nValue, path, nIndex))
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
					TNodePtr pParent = path[j - 1];
					int32_t nChild = pParent->FindChild(pNode->m_nAddr);

					if (pNode->m_entries.size() < m_nMinCount)
					{
						for (const TEntry& e : pNode->m_entries)
							orphans.emplace_back(e, pNode->m_nLevel);
						pParent->Erase(nChild);
						DeleteNode(pNode);
					}
					else
						pParent->SetRect(nChild, pNode->Mbr());
				}

				if (m_pRoot->m_nLevel != 0 && m_pRoot->m_entries.empty())
				{
					m_pRoot->m_nLevel = 0;      // everything below was dissolved
					m_pRoot->Reset(std::vector<TEntry>());
				}

				// higher subtrees first: they don't depend on the leaves
				std::stable_sort(orphans.begin(), orphans.end(), [](const std::pair<TEntry, uint32_t>& a, const std::pair<TEntry, uint32_t>& b) { return a.second > b.second; });
				for (const auto& orphan : orphans)
				{
					std::vector<bool> reinserted(MaxLevels, false);
					InsertEntry(orphan.first, orphan.second, reinserted);
				}

				// a root with one child: the child becomes the root
				while (m_pRoot->m_nLevel != 0 && m_pRoot->m_entries.size() == 1)
				{
					TNodePtr pChild = GetNode(m_pRoot->m_entries[0].m_nValue);
					m_pRoot->Reset(std::vector<TEntry>(pChild->m_entries));
					m_pRoot->m_nLevel = pChild->m_nLevel;
					DeleteNode(pChild);
				}
			}

			//------------------------------------------------------------------------------------------
			// search

			// returns true if func asked to stop
			bool SearchNode(const TNodePtr& pNode, const TRect& rect, const TSearchCallback& func, std::vector<int64_t>& stack, uint64_t& nFound)
			{
				for (const TEntry& e : pNode->m_entries)
				{
					if (!RectMath::Intersects(e.m_rect, rect))
						continue;

					if (pNode->m_nLevel == 0)
					{
						++nFound;
						if (!func(e.m_rect, e.m_nValue))
							return true;
					}
					else
						stack.push_back(e.m_nValue);
				}
				return false;
			}

			//------------------------------------------------------------------------------------------
			// bulk load

			// the most entries from nStart (up to nEnd) that fit in a node of nLimit bytes
			size_t FitCount(const std::vector<TEntry>& v, size_t nStart, size_t nEnd, uint32_t nLimit)
			{
				size_t nMax = std::min<size_t>(nEnd - nStart, m_params.m_nMaxCount);
				const TEntry* p = v.data() + nStart;
				if (m_compressor.Fits(p, nMax, nLimit))
					return nMax;
				if (!m_compressor.Fits(p, 1, nLimit))
					throw CommonLib::CExcBase("[TRTree] one entry doesn't fit in a node");

				size_t lo = 1, hi = nMax;        // lo fits, hi doesn't
				while (hi - lo > 1)
				{
					size_t mid = lo + (hi - lo) / 2;
					if (m_compressor.Fits(p, mid, nLimit))
						lo = mid;
					else
						hi = mid;
				}
				return lo;
			}

			// STR: packs the entries of a level into nodes, upper gets an entry per node
			void PackLevel(std::vector<TEntry>& level, uint32_t nLevel, uint32_t nLimit, std::vector<TEntry>& upper)
			{
				auto byX = [](const TEntry& a, const TEntry& b) { return RectMath::CenterX(a.m_rect) < RectMath::CenterX(b.m_rect); };
				auto byY = [](const TEntry& a, const TEntry& b) { return RectMath::CenterY(a.m_rect) < RectMath::CenterY(b.m_rect); };

				size_t n = level.size();
				std::sort(level.begin(), level.end(), byX);

				// node capacity by the first entries: the number of vertical slices
				size_t nCap = FitCount(level, 0, n, nLimit);
				size_t nNodes = (n + nCap - 1) / nCap;
				size_t nSlices = (size_t)std::ceil(std::sqrt((double)nNodes));
				size_t nSliceSize = nSlices * nCap;

				TNode node(-1, nLevel);
				for (size_t nSlice = 0; nSlice < n; nSlice += nSliceSize)
				{
					size_t nSliceEnd = (std::min)(n, nSlice + nSliceSize);
					std::sort(level.begin() + nSlice, level.begin() + nSliceEnd, byY);

					for (size_t i = nSlice; i < nSliceEnd; )
					{
						size_t k = FitCount(level, i, nSliceEnd, nLimit);
						node.m_nAddr = m_pStorage->GetNewFilePageAddr();
						node.Reset(std::vector<TEntry>(level.begin() + i, level.begin() + i + k));
						SaveNode(node);           // written at once, not cached
						upper.push_back(TEntry(node.Mbr(), node.m_nAddr));
						i += k;
					}
				}
			}

			//------------------------------------------------------------------------------------------
			// check

			void CheckNode(const TNodePtr& pNode, const TRect* pParentRect, uint64_t& nEntries)
			{
				if (pNode != m_pRoot && pNode->m_entries.empty())
					throw CommonLib::CExcBase("[TRTree] check: empty node {0}", pNode->m_nAddr);
				if (!pNode->m_entries.empty() && pParentRect != nullptr && !(pNode->Mbr() == *pParentRect))
					throw CommonLib::CExcBase("[TRTree] check: the parent rect of node {0} isn't its MBR", pNode->m_nAddr);
				if (!m_compressor.Prepare(pNode->m_entries.data(), pNode->m_entries.size(), m_nPageMax))
					throw CommonLib::CExcBase("[TRTree] check: node {0} doesn't fit", pNode->m_nAddr);
				if (pNode->m_nEstSize != 0 && m_compressor.EncodedSize() > pNode->m_nEstSize)
					throw CommonLib::CExcBase("[TRTree] check: node {0} encoded size {1} is above the estimate {2}", pNode->m_nAddr, m_compressor.EncodedSize(), pNode->m_nEstSize);

				if (pNode->m_nLevel == 0)
				{
					nEntries += pNode->m_entries.size();
					return;
				}

				std::vector<TEntry> entries = pNode->m_entries;     // the node may leave the cache below
				for (const TEntry& e : entries)
				{
					TNodePtr pChild = GetNode(e.m_nValue);
					if (pChild->m_nLevel + 1 != pNode->m_nLevel)
						throw CommonLib::CExcBase("[TRTree] check: node {0} level {1}, parent level {2}", pChild->m_nAddr, pChild->m_nLevel, pNode->m_nLevel);
					CheckNode(pChild, &e.m_rect, nEntries);
				}
				CheckCache();
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

			TNodePtr m_pRoot;
			std::unordered_map<int64_t, SCacheItem> m_cache;
			std::list<int64_t> m_lru;
		};
	}
	}
}
