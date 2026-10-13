#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/exception/exc_base.h"
#include "../../../CommonLib/stream/stream.h"
#include "RTreeEntry.h"

#include <vector>
#include <memory>
#include <algorithm>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		// Base of the nodes of TRTree: one node is one page.
		//   page: uint16 level (0 - leaf), uint32 count, the entries by the compressor
		// The entries are (rect, value): in a leaf the value is the id of the object, in an inner node the
		// address of the child (its rect is the MBR of the child). TRTreeLeafNode / TRTreeInnerNode add what
		// depends on the kind; the base has what doesn't: the entries and their changes, the size in the page,
		// the encoding and the R* algorithms that only rearrange the entries (forced reinsert, split).
		//
		// The node keeps an estimate of its encoded size: the real size after load / save / a real check, plus
		// TCompressor::MaxGrowth for every entry added, removed or changed since. Fits runs the compressor only
		// when the estimate is over the limit. The estimate isn't a guaranteed bound (zlib can grow by more than
		// MaxGrowth for one entry): TRTree checks the real size before it writes a node (Prepare / WritePrepared).
		template <class _TCoord, class _TCompressor>
		class TRTreeNode
		{
		public:
			typedef _TCoord TCoord;
			typedef _TCompressor TCompressor;
			typedef TRTreeEntry<TCoord> TEntry;
			typedef typename TEntry::TRect TRect;
			typedef std::vector<TEntry> TEntries;

			static constexpr uint32_t HeadSize = sizeof(uint16_t) + sizeof(uint32_t);

			TRTreeNode(int64_t nAddr, uint32_t nLevel) : m_nAddr(nAddr), m_nLevel(nLevel) {}
			virtual ~TRTreeNode() {}

			bool IsLeaf() const { return m_nLevel == 0; }
			int64_t Addr() const { return m_nAddr; }
			void SetAddr(int64_t nAddr) { m_nAddr = nAddr; }
			uint32_t Level() const { return m_nLevel; }
			size_t Count() const { return m_entries.size(); }
			bool Empty() const { return m_entries.empty(); }
			const TEntries& Entries() const { return m_entries; }
			const TEntry& Entry(size_t i) const { return m_entries[i]; }
			bool IsDirty() const { return m_bDirty; }
			void SetDirty(bool bDirty) { m_bDirty = bDirty; }
			uint32_t EstSize() const { return m_nEstSize; }

			TRect Mbr() const { return RectMath::Mbr(m_entries.begin(), m_entries.end()); }

			// all changes of the entries go through these: they keep the estimate an upper one
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

			void Reset(TEntries&& entries)
			{
				m_entries = std::move(entries);
				m_nEstSize = 0;
				m_bDirty = true;
			}

			// takes the entries away (the node is empty and dirty)
			TEntries TakeEntries()
			{
				TEntries entries;
				entries.swap(m_entries);
				m_nEstSize = 0;
				m_bDirty = true;
				return entries;
			}

			//------------------------------------------------------------------------------------------
			// size in the page

			// do the entries fit in nPageLimit bytes (and no more than nMaxCount entries)
			bool Fits(TCompressor& compressor, uint32_t nPageLimit, uint32_t nMaxCount)
			{
				size_t n = m_entries.size();
				if (n > nMaxCount)
					return false;
				if (TCompressor::HeadSize + n * TCompressor::RawEntrySize <= nPageLimit)
					return true;
				if (m_nEstSize != 0 && m_nEstSize <= nPageLimit)
					return true;

				bool bFits = compressor.Prepare(m_entries.data(), n, nPageLimit);
				m_nEstSize = compressor.EncodedSize();
				return bFits;
			}

			//------------------------------------------------------------------------------------------
			// encoding

			// encodes the entries into the compressor (the real size): true if they fit in nMaxSize bytes.
			// The block stays in the compressor for WritePrepared until the compressor encodes something else.
			bool Prepare(TCompressor& compressor, uint32_t nMaxSize)
			{
				bool bFits = compressor.Prepare(m_entries.data(), m_entries.size(), nMaxSize);
				m_nEstSize = compressor.EncodedSize();
				return bFits;
			}

			// writes the node (header and the block of the last Prepare of this node)
			void WritePrepared(TCompressor& compressor, CommonLib::IWriteStream* pStream)
			{
				pStream->Write((uint16_t)m_nLevel);
				pStream->Write((uint32_t)m_entries.size());
				compressor.Write(pStream);
				m_bDirty = false;
			}

			// writes the node; throws if it doesn't fit in nPageMax bytes
			void Write(TCompressor& compressor, uint32_t nPageMax, CommonLib::IWriteStream* pStream)
			{
				if (!Prepare(compressor, nPageMax))
					throw CommonLib::CExcBase("{0} entries don't fit in the page", (uint64_t)m_entries.size());
				WritePrepared(compressor, pStream);
			}

			// the header of a page: the level and the count (nMaxLevels / nMaxCount - the sanity limits)
			static void ReadHead(CommonLib::IReadStream* pStream, uint32_t nMaxLevels, uint32_t nMaxCount, uint32_t& nLevel, uint32_t& nCount)
			{
				nLevel = pStream->Readintu16();
				nCount = pStream->ReadIntu32();
				if (nLevel >= nMaxLevels || nCount > nMaxCount)
					throw CommonLib::CExcBase("wrong node header, level {0}, count {1}", nLevel, nCount);
			}

			// the entries after the header
			void ReadEntries(TCompressor& compressor, uint32_t nCount, CommonLib::IReadStream* pStream)
			{
				compressor.Read(nCount, m_entries, pStream);
				m_nEstSize = compressor.EncodedSize();
				m_bDirty = false;
			}

			//------------------------------------------------------------------------------------------
			// R* algorithms on the entries

			// forced reinsert: removes the part dPart of the entries farthest from the center of the node,
			// closest first in removed
			void TakeFarthest(double dPart, TEntries& removed)
			{
				size_t n = m_entries.size();
				size_t p = std::max<size_t>(1, (size_t)(n * dPart));
				TRect center = Mbr();

				std::vector<std::pair<double, size_t>> dist(n);
				for (size_t i = 0; i < n; ++i)
					dist[i] = std::make_pair(RectMath::CenterDist2(m_entries[i].m_rect, center), i);
				std::sort(dist.begin(), dist.end(), [](const std::pair<double, size_t>& a, const std::pair<double, size_t>& b) { return a.first > b.first; });

				std::vector<bool> take(n, false);
				for (size_t i = 0; i < p; ++i)
					take[dist[i].second] = true;

				for (size_t i = p; i-- > 0; )
					removed.push_back(m_entries[dist[i].second]);

				TEntries keep;
				keep.reserve(n - p);
				for (size_t i = 0; i < n; ++i)
					if (!take[i])
						keep.push_back(m_entries[i]);
				Reset(std::move(keep));
			}

			// R* split: the axis by the least sum of the margins, the distribution by the least overlap (then area).
			// The node keeps the first group, the second is returned. dSplitMin - the smaller group has at least
			// this part of the entries.
			TEntries SplitOff(double dSplitMin)
			{
				size_t n = m_entries.size();
				if (n < 2)
					throw CommonLib::CExcBase("[TRTree] one entry doesn't fit in a node");

				size_t m = std::max<size_t>(1, (size_t)(n * dSplitMin));
				if (m * 2 > n)
					m = n / 2;

				// sorts: axis 0 - x, 1 - y; by min (then max) and by max (then min)
				auto sortBy = [](TEntries& v, int axis, bool byMax)
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
				auto calcBounds = [&](const TEntries& v)
				{
					prefix[0] = v[0].m_rect;
					for (size_t i = 1; i < n; ++i)
						prefix[i] = RectMath::Union(prefix[i - 1], v[i].m_rect);
					suffix[n - 1] = v[n - 1].m_rect;
					for (size_t i = n - 1; i-- > 0; )
						suffix[i] = RectMath::Union(suffix[i + 1], v[i].m_rect);
				};

				// the axis: the least sum of the margins of all distributions
				TEntries sorted = m_entries;
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

				// the distribution on the axis: the least overlap, then the least area
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
				TEntries second(sorted.begin() + nBestK, sorted.end());
				sorted.resize(nBestK);
				Reset(std::move(sorted));
				return second;
			}

		protected:
			void Grow()
			{
				if (m_nEstSize != 0)
					m_nEstSize += TCompressor::MaxGrowth(m_entries.size());
				m_bDirty = true;
			}

		protected:
			int64_t m_nAddr;
			uint32_t m_nLevel;
			TEntries m_entries;
			bool m_bDirty{ false };
			uint32_t m_nEstSize{ 0 };     // upper estimate of the encoded size, 0 - unknown
		};
	}
	}
}
