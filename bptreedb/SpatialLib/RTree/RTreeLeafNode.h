#pragma once
#include "RTreeNode.h"

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		// Leaf node of TRTree (level 0): the entries are (rect of the object, id of the object).
		template <class _TCoord, class _TCompressor>
		class TRTreeLeafNode : public TRTreeNode<_TCoord, _TCompressor>
		{
		public:
			typedef TRTreeNode<_TCoord, _TCompressor> TBase;
			typedef typename TBase::TEntry TEntry;
			typedef typename TBase::TRect TRect;

			explicit TRTreeLeafNode(int64_t nAddr) : TBase(nAddr, 0) {}

			int64_t Id(size_t i) const { return this->m_entries[i].m_nValue; }

			// the index of the entry with this rect and id, -1 if there is none
			int32_t Find(const TRect& rect, int64_t nId) const
			{
				for (size_t i = 0; i < this->m_entries.size(); ++i)
				{
					const TEntry& e = this->m_entries[i];
					if (e.m_nValue == nId && e.m_rect == rect)
						return (int32_t)i;
				}
				return -1;
			}

			// calls func(rect, id) for the entries that intersect rect; true if func asked to stop (returned false)
			template <class TFunc>
			bool Search(const TRect& rect, const TFunc& func, uint64_t& nFound) const
			{
				for (const TEntry& e : this->m_entries)
				{
					if (!RectMath::Intersects(e.m_rect, rect))
						continue;
					++nFound;
					if (!func(e.m_rect, e.m_nValue))
						return true;
				}
				return false;
			}
		};
	}
	}
}
