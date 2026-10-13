#pragma once
#include "../../BPtreeLib/BPMap.h"
#include "../../BPtreeLib/BPTreeInnerNode.h"
#include "../../BPtreeLib/BPTreeMapLeafNode.h"
#include "../../BPtreeLib/BaseNodeCompressor.h"
#include "../../BPtreeLib/BaseEncoder.h"
#include "../../StorageLib/Storage.h"
#include "ZOrderKey.h"
#include "ZOrderKeyEncoder.h"
#include <vector>
#include <functional>
#include <algorithm>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace zorder
	{
		struct SZOrderQueryStats
		{
			uint64_t m_nFound{ 0 };      // keys in the query
			uint64_t m_nRead{ 0 };       // keys looked at
			uint64_t m_nSeeks{ 0 };      // jumps (BIGMIN or to the next sub-query)
			uint64_t m_nTreeSeeks{ 0 };  // ... that went through the tree (the rest stayed in the leaf)
			uint64_t m_nSubQueries{ 0 }; // rect split: sub-queries made
		};

		enum EZOrderQueryMode
		{
			eZQueryBigMin,      // a key outside the query: jump to the next Z value inside the range (BIGMIN)
			eZQueryRectSplit,   // the Z range is split into sub-ranges (halves of the query box) down to a leaf;
			                    // the keys of a sub-range are read one by one, between sub-ranges the scan jumps
			eZQueryFullScan     // every key from the lower to the upper Z value of the range (for tests)
		};

		// Spatial query over TBPZOrderIndex: the objects that intersect the query rect, in Z order.
		//   for (auto it = index.spatialQuery(rect); !it.IsNull(); it.Next()) { it.Shape(); it.Id(); }
		// The iterator keeps the current leaf (a shared pointer), the tree must outlive it and not change
		// while it is used.
		template <class _TIndex>
		class TZOrderSpatialIterator
		{
		public:
			typedef _TIndex TIndex;
			typedef typename TIndex::TTraits TTraits;
			typedef typename TIndex::TTree TTree;
			typedef typename TIndex::TKey TKey;
			typedef typename TIndex::ZOrder ZOrder;
			typedef typename TIndex::TShape TShape;
			typedef typename TIndex::TRect TRect;
			typedef typename TTree::iterator TTreeIterator;

			TZOrderSpatialIterator() {}

			TZOrderSpatialIterator(TTree* pTree, const TRect& rect, const TRect& extent, EZOrderQueryMode mode) :
				m_pTree(pTree), m_rect(rect), m_mode(mode)
			{
				// the query clipped to the extent (every object is inside it)
				TRect q((std::max)(rect.m_minX, extent.m_minX), (std::max)(rect.m_minY, extent.m_minY),
					(std::min)(rect.m_maxX, extent.m_maxX), (std::min)(rect.m_maxY, extent.m_maxY));
				if (rect.m_maxX < rect.m_minX || rect.m_maxY < rect.m_minY || q.m_maxX < q.m_minX || q.m_maxY < q.m_minY)
				{
					m_bEnd = true;
					return;
				}

				TTraits::Range(q, extent, m_zMin, m_zMax);
				m_cur.m_zMin = m_zMin;
				m_cur.m_zMax = m_zMax;
				m_cur.m_nBits = m_zMin.getBits();
				m_it = m_pTree->lower_bound(LowKey(m_zMin));
				Find();
			}

			bool IsNull() const { return m_bEnd; }

			bool Next()
			{
				if (m_bEnd)
					return false;
				m_it.Next();
				return Find();
			}

			const TKey& Key() const { return m_it.Key(); }
			int64_t Id() const { return m_it.Key().m_nId; }
			TShape Shape() const { return TTraits::GetShape(m_it.Key().m_z); }
			const SZOrderQueryStats& Stats() const { return m_stats; }

		private:
			struct SSubQuery
			{
				ZOrder m_zMin;
				ZOrder m_zMax;
				short m_nBits{ 0 };
			};

			static TKey LowKey(const ZOrder& z) { return TKey(z, (std::numeric_limits<int64_t>::min)()); }
			static TKey HighKey(const ZOrder& z) { return TKey(z, (std::numeric_limits<int64_t>::max)()); }

			// moves to the next key in the query starting from the current position; false at the end
			bool Find()
			{
				switch (m_mode)
				{
				case eZQueryBigMin:    return FindBigMin();
				case eZQueryRectSplit: return FindRectSplit();
				default:               return FindFullScan();
				}
			}

			bool Stop()
			{
				m_bEnd = true;
				return false;
			}

			bool Found()
			{
				++m_stats.m_nFound;
				return true;
			}

			bool FindFullScan()
			{
				const TKey keyMax = HighKey(m_zMax);
				for (; !m_it.IsNull(); m_it.Next())
				{
					const TKey& key = m_it.Key();
					if (keyMax < key)
						break;
					++m_stats.m_nRead;
					if (TTraits::InQuery(key.m_z, m_rect))
						return Found();
				}
				return Stop();
			}

			bool FindBigMin()
			{
				const TKey keyMax = HighKey(m_zMax);
				while (!m_it.IsNull())
				{
					const TKey& key = m_it.Key();
					if (keyMax < key)
						break;
					++m_stats.m_nRead;
					if (TTraits::InQuery(key.m_z, m_rect))
						return Found();

					ZOrder zNext;
					if (!TTraits::NextInRange(key.m_z, m_zMin, m_zMax, zNext))
						break;
					Seek(LowKey(zNext));
				}
				return Stop();
			}

			// After EmbDB TBPTSpatialRectSplitIterator: the current sub-range is halved at its highest
			// differing bit (the upper half goes to the stack) until it ends inside the current leaf; its keys
			// are read in order; then the next sub-range (the smallest on the stack) is sought.
			bool FindRectSplit()
			{
				while (!m_it.IsNull())
				{
					SplitToLeaf();

					const TKey& key = m_it.Key();
					if (HighKey(m_cur.m_zMax) < key)
					{
						// the current sub-range is done: the next one (or the end); empty ones are skipped
						bool bNext = false;
						while (!m_queries.empty())
						{
							m_cur = m_queries.back();
							m_queries.pop_back();
							if (!(HighKey(m_cur.m_zMax) < key))
							{
								bNext = true;
								break;
							}
						}
						if (!bNext)
							break;
						if (key < LowKey(m_cur.m_zMin))
							Seek(LowKey(m_cur.m_zMin));
						continue;
					}

					++m_stats.m_nRead;
					if (TTraits::InQuery(key.m_z, m_rect))
						return Found();
					m_it.Next();
				}
				return Stop();
			}

			void SplitToLeaf()
			{
				const auto& pNode = m_it.m_pCurNode;
				const TKey lastKey = pNode->Key(pNode->Count() - 1);
				while (lastKey < HighKey(m_cur.m_zMax) && !(m_cur.m_zMin == m_cur.m_zMax))
				{
					ZOrder qMin = m_cur.m_zMin, qMax = m_cur.m_zMax;
					while ((qMin.getBit(m_cur.m_nBits) & 1) == (qMax.getBit(m_cur.m_nBits) & 1))
					{
						if (--m_cur.m_nBits < 0)
							throw CommonLib::CExcBase("[TZOrderSpatialIterator] rect split: no differing bit");
					}

					SSubQuery upper;
					upper.m_zMin = m_cur.m_zMin;
					upper.m_zMin.clearLowBits(m_cur.m_nBits);     // the first value of the upper half
					upper.m_zMax = m_cur.m_zMax;
					m_cur.m_zMax.setLowBits(m_cur.m_nBits);        // the last value of the lower half
					upper.m_nBits = --m_cur.m_nBits;
					m_queries.push_back(upper);
					++m_stats.m_nSubQueries;
				}
			}

			// moves to the first key >= key: in the current leaf if it is there, otherwise from the root
			void Seek(const TKey& key)
			{
				++m_stats.m_nSeeks;
				auto& pNode = m_it.m_pCurNode;
				int32_t nCount = (int32_t)pNode->Count();
				if (nCount != 0 && !(pNode->Key(nCount - 1) < key))
				{
					int32_t lo = m_it.m_nIndex, hi = nCount - 1;      // the answer is in [lo, hi]
					while (lo < hi)
					{
						int32_t mid = lo + (hi - lo) / 2;
						if (pNode->Key(mid) < key)
							lo = mid + 1;
						else
							hi = mid;
					}
					m_it.m_nIndex = lo;
					return;
				}

				++m_stats.m_nTreeSeeks;
				m_it = m_pTree->lower_bound(key);
			}

		private:
			TTree* m_pTree{ nullptr };
			TRect m_rect;
			EZOrderQueryMode m_mode{ eZQueryBigMin };
			ZOrder m_zMin, m_zMax;
			TTreeIterator m_it;
			bool m_bEnd{ false };
			SSubQuery m_cur;
			std::vector<SSubQuery> m_queries;     // stack: the smallest sub-range on top
			SZOrderQueryStats m_stats;
		};

		// Spatial index on the B+ tree of BPtreeLib, keyed by the Z-order (Morton) value of the shape.
		//   TTraits: TZOrderPointTraits<ZOrderPoint2DU16 / U32 / U64> or TZOrderRectTraits<ZOrderRect2DU16 / U32 / U64>
		//   (coordinates are unsigned: shift / scale the geo coordinates into the range)
		//
		// The key is (Z value, id): unique, two objects with the same shape are two keys.
		// Keys are compressed by TZOrderKeyEncoder (differences of the Z values and ids, NumLen), the map has
		// no values (TNullValueEncoder).
		//
		// spatialQuery(rect) / search(rect): walks the keys from the Z value of the lower corner of the query range
		// to the upper one (TZOrderSpatialIterator), skipping the keys outside the query by BIGMIN jumps or by
		// splitting the range into sub-ranges (EZOrderQueryMode). Jumps stay inside the current leaf when they can
		// (binary search), otherwise they go through the tree (lower_bound).
		// A rect key is the 4D point (minX, minY, maxX, maxY); "intersects" is a 4D range, so the same scan works.
		// Rects and points are closed: touching counts.
		template <class _TTraits>
		class TBPZOrderIndex
		{
		public:
			typedef _TTraits TTraits;
			typedef typename TTraits::ZOrder ZOrder;
			typedef typename TTraits::TCoord TCoord;
			typedef typename TTraits::TShape TShape;
			typedef typename TTraits::TRect TRect;

			typedef TZOrderKey<ZOrder> TKey;
			typedef TZOrderKeyComp<TKey> TComp;
			typedef uint8_t TValue;                       // not used: the id is in the key
			typedef TZOrderKeyEncoder<TKey> TKeyEncoder;
			typedef TBaseValueEncoder<int64_t, CUnsignedNumLenEncoder> TLinkEncoder;

			typedef BPTreeInnerNode<TKey, TBaseNodeCompressor<TKey, int64_t, TKeyEncoder, TLinkEncoder> > TInnerNode;
			typedef BPTreeLeafNodeMap<TKey, TValue, TBaseNodeCompressor<TKey, TValue, TKeyEncoder, TNullValueEncoder<TValue> > > TLeafNode;
			typedef TBPMap<TKey, TValue, TComp, storage::IFilePager, TInnerNode, TLeafNode> TTree;
			typedef typename TTree::iterator iterator;

			// return false to stop
			typedef std::function<bool(const TShape& shape, int64_t nId)> TSearchCallback;

			// extent: every object must be inside it (insert throws otherwise); queries are bounded by it.
			// The default is the whole coordinate range. Give the extent of the data (as UniGIS fullExtent):
			// a rect query ranges up to the extent's max corner, so a tight extent makes the Z range much smaller.
			// The extent isn't stored in the tree: open an index with the extent it was built with.
			TBPZOrderIndex(int64_t nPageInfo, storage::IFilePagerPtr pStorage, CommonLib::IAllocPtr pAlloc, uint32_t nCacheSize, uint32_t nPageSize,
				const TRect& extent = TRect(TCoord(0), TCoord(0), (std::numeric_limits<TCoord>::max)(), (std::numeric_limits<TCoord>::max)())) :
				m_tree(nPageInfo, pStorage, pAlloc, nCacheSize, nPageSize, false), m_extent(extent)
			{
				if (m_extent.m_maxX < m_extent.m_minX || m_extent.m_maxY < m_extent.m_minY)
					throw CommonLib::CExcBase("[TBPZOrderIndex] wrong extent");
			}

			const TRect& Extent() const { return m_extent; }

			// a new index: writes the info page (GetPageInfo() gives its address to open the index later)
			void Create()
			{
				TCompressorParamsBasePtr pParams(new CompressorParamsBase());
				for (ECompressParams id : { eInnerKey, eInnerValue, eLeafKey, eLeafValue })
					pParams->AddCompressParams(TCompressorParamsPtr(new CompressorParams()), id);
				m_tree.InnitTree(pParams, false, 0, storage::eEmptyType, 0, storage::eEmptyType);
			}

			int64_t GetPageInfo() const { return m_tree.GetPageBTreeInfo(); }

			void insert(const TShape& shape, int64_t nId)
			{
				CheckExtent(shape, nId);
				m_tree.insert(TKey(TTraits::MakeZ(shape), nId), TValue());
			}

			bool remove(const TShape& shape, int64_t nId)
			{
				return m_tree.remove(TKey(TTraits::MakeZ(shape), nId));
			}

			// an empty index: builds it from the objects (any order, (shape, id) pairs must be unique)
			template<class TObjects>
			uint64_t BulkLoad(const TObjects& objects)
			{
				std::vector<TKey> keys;
				keys.reserve(objects.size());
				for (const auto& obj : objects)
				{
					CheckExtent(obj.first, obj.second);
					keys.push_back(TKey(TTraits::MakeZ(obj.first), obj.second));
				}
				std::sort(keys.begin(), keys.end());
				for (size_t i = 1; i < keys.size(); ++i)
					if (keys[i] == keys[i - 1])
						throw CommonLib::CExcBase("[TBPZOrderIndex] BulkLoad: duplicate object, id {0}", keys[i].m_nId);

				struct SSource
				{
					const std::vector<TKey>& m_keys;
					size_t m_nPos;
					bool Next(TKey& key, TValue& value)
					{
						if (m_nPos == m_keys.size())
							return false;
						key = m_keys[m_nPos++];
						value = TValue();
						return true;
					}
				} source{ keys, 0 };
				return m_tree.BulkLoad(source);
			}

			typedef TZOrderSpatialIterator<TBPZOrderIndex> TSpatialIterator;

			// iterator over the objects that intersect rect
			TSpatialIterator spatialQuery(const TRect& rect, EZOrderQueryMode mode = eZQueryBigMin)
			{
				return TSpatialIterator(&m_tree, rect, m_extent, mode);
			}

			// calls func for the objects that intersect rect (func returns false to stop)
			SZOrderQueryStats search(const TRect& rect, const TSearchCallback& func, EZOrderQueryMode mode = eZQueryBigMin)
			{
				TSpatialIterator it = spatialQuery(rect, mode);
				for (; !it.IsNull(); it.Next())
				{
					if (!func(it.Shape(), it.Id()))
						break;
				}
				return it.Stats();
			}

			void search(const TRect& rect, std::vector<std::pair<TShape, int64_t> >& result)
			{
				search(rect, [&result](const TShape& s, int64_t id) { result.emplace_back(s, id); return true; });
			}

			void Flush() { m_tree.Flush(); }
			void CheckTreeStructure() { m_tree.CheckTreeStructure(); }
			TTree& Tree() { return m_tree; }

		private:
			void CheckExtent(const TShape& shape, int64_t nId) const
			{
				if (!TTraits::InExtent(shape, m_extent))
					throw CommonLib::CExcBase("[TBPZOrderIndex] object {0} is outside the extent of the index", nId);
			}

		private:
			TTree m_tree;
			TRect m_extent;
		};
	}
	}
}
