#pragma once
#include "RTreeNode.h"

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		// Inner node of TRTree (level > 0): the entries are (MBR of the child, address of the child).
		template <class _TCoord, class _TCompressor>
		class TRTreeInnerNode : public TRTreeNode<_TCoord, _TCompressor>
		{
		public:
			typedef TRTreeNode<_TCoord, _TCompressor> TBase;
			typedef typename TBase::TEntry TEntry;
			typedef typename TBase::TRect TRect;

			TRTreeInnerNode(int64_t nAddr, uint32_t nLevel) : TBase(nAddr, nLevel)
			{
				if (nLevel == 0)
					throw CommonLib::CExcBase("[TRTree] an inner node of level 0, addr {0}", nAddr);
			}

			int64_t ChildAddr(size_t i) const { return this->m_entries[i].m_nValue; }

			// the index of the child, throws if it isn't a child of this node
			int32_t FindChild(int64_t nAddr) const
			{
				for (size_t i = 0; i < this->m_entries.size(); ++i)
					if (this->m_entries[i].m_nValue == nAddr)
						return (int32_t)i;
				throw CommonLib::CExcBase("[TRTree] node {0} has no child {1}", this->m_nAddr, nAddr);
			}

			// the child where rect goes: the least enlargement of the area, ties - the least area
			size_t ChooseSubtree(const TRect& rect) const
			{
				if (this->m_entries.empty())
					throw CommonLib::CExcBase("[TRTree] empty inner node {0}", this->m_nAddr);

				size_t nBest = 0;
				double dBestGrow = 0, dBestArea = 0;
				for (size_t i = 0; i < this->m_entries.size(); ++i)
				{
					const TRect& r = this->m_entries[i].m_rect;
					double dArea = RectMath::Area(r);
					double dGrow = RectMath::Area(RectMath::Union(r, rect)) - dArea;
					if (i == 0 || dGrow < dBestGrow || (dGrow == dBestGrow && dArea < dBestArea))
					{
						nBest = i;
						dBestGrow = dGrow;
						dBestArea = dArea;
					}
				}
				return nBest;
			}

			// the children that intersect rect (search)
			void IntersectingChildren(const TRect& rect, std::vector<int64_t>& children) const
			{
				for (const TEntry& e : this->m_entries)
					if (RectMath::Intersects(e.m_rect, rect))
						children.push_back(e.m_nValue);
			}

			// the children whose MBR contains rect (the leaf of an entry is below one of them)
			void ContainingChildren(const TRect& rect, std::vector<int64_t>& children) const
			{
				for (const TEntry& e : this->m_entries)
					if (RectMath::Contains(e.m_rect, rect))
						children.push_back(e.m_nValue);
			}
		};
	}
	}
}
