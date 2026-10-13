#pragma once
#include "../SpatialKey.h"
#include <algorithm>
#include <cmath>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace rtree
	{
		// An entry of an R-tree node: the rect and
		//   leaf node  - the id of the object (like the rowid of SQLite rtree)
		//   inner node - the address of the child node (its rect is the MBR of the child)
		template <class _TCoord>
		struct TRTreeEntry
		{
			typedef _TCoord TCoord;
			typedef spatiallib::TRect2D<TCoord> TRect;

			TRTreeEntry() {}
			TRTreeEntry(const TRect& rect, int64_t nValue) : m_rect(rect), m_nValue(nValue) {}

			TRect m_rect;
			int64_t m_nValue{ 0 };
		};

		// Rect math of the R-tree. Rects are closed: [min, max] on both axes, so a point is a rect with
		// min == max and touching rects intersect (as in SQLite). Areas and margins are calculated in double
		// (int64 coordinates would overflow).
		struct RectMath
		{
			template<class TRect>
			static bool IsValid(const TRect& r)
			{
				return !(r.m_maxX < r.m_minX) && !(r.m_maxY < r.m_minY);
			}

			template<class TRect>
			static double Width(const TRect& r) { return (double)r.m_maxX - (double)r.m_minX; }

			template<class TRect>
			static double Height(const TRect& r) { return (double)r.m_maxY - (double)r.m_minY; }

			template<class TRect>
			static double Area(const TRect& r) { return Width(r) * Height(r); }

			template<class TRect>
			static double Margin(const TRect& r) { return Width(r) + Height(r); }

			template<class TRect>
			static void Extend(TRect& r, const TRect& add)
			{
				if (add.m_minX < r.m_minX) r.m_minX = add.m_minX;
				if (add.m_minY < r.m_minY) r.m_minY = add.m_minY;
				if (r.m_maxX < add.m_maxX) r.m_maxX = add.m_maxX;
				if (r.m_maxY < add.m_maxY) r.m_maxY = add.m_maxY;
			}

			template<class TRect>
			static TRect Union(const TRect& a, const TRect& b)
			{
				TRect r = a;
				Extend(r, b);
				return r;
			}

			template<class TRect>
			static bool Intersects(const TRect& a, const TRect& b)
			{
				return !(a.m_maxX < b.m_minX) && !(b.m_maxX < a.m_minX) &&
					!(a.m_maxY < b.m_minY) && !(b.m_maxY < a.m_minY);
			}

			// a contains b
			template<class TRect>
			static bool Contains(const TRect& a, const TRect& b)
			{
				return !(b.m_minX < a.m_minX) && !(a.m_maxX < b.m_maxX) &&
					!(b.m_minY < a.m_minY) && !(a.m_maxY < b.m_maxY);
			}

			template<class TRect>
			static double OverlapArea(const TRect& a, const TRect& b)
			{
				double w = (std::min)((double)a.m_maxX, (double)b.m_maxX) - (std::max)((double)a.m_minX, (double)b.m_minX);
				double h = (std::min)((double)a.m_maxY, (double)b.m_maxY) - (std::max)((double)a.m_minY, (double)b.m_minY);
				return (w > 0 && h > 0) ? w * h : 0.0;
			}

			template<class TRect>
			static double Enlargement(const TRect& r, const TRect& add)
			{
				return Area(Union(r, add)) - Area(r);
			}

			template<class TRect>
			static double CenterX(const TRect& r) { return ((double)r.m_minX + (double)r.m_maxX) * 0.5; }

			template<class TRect>
			static double CenterY(const TRect& r) { return ((double)r.m_minY + (double)r.m_maxY) * 0.5; }

			template<class TRect>
			static double CenterDist2(const TRect& a, const TRect& b)
			{
				double dx = CenterX(a) - CenterX(b);
				double dy = CenterY(a) - CenterY(b);
				return dx * dx + dy * dy;
			}

			// MBR of entries [begin, end), the range must not be empty
			template<class TIt>
			static auto Mbr(TIt begin, TIt end) -> decltype(begin->m_rect)
			{
				auto r = begin->m_rect;
				for (++begin; begin != end; ++begin)
					Extend(r, begin->m_rect);
				return r;
			}
		};
	}
	}
}
