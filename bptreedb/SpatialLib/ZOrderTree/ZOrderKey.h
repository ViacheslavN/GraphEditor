#pragma once
#include "../ZCurve/Point/SpatialPointQuery.h"
#include "../ZCurve/Rect/SpatialRectQuery.h"
#include <type_traits>
#include <limits>
#include <cstring>

namespace bptreedb
{
	namespace spatiallib
	{
	namespace zorder
	{
		// The Z value of a key as 64 bit limbs, limb 0 is the lowest (ZOrderPoint2DU16 / U32, ZOrderRect2DU16
		// have one value, ZOrderPoint2DU64 / ZOrderRect2DU32 two, ZOrderRect2DU64 four; the last one is compared first)
		template <class ZOrder>
		struct TZLimbs
		{
			typedef typename std::remove_reference<decltype(std::declval<ZOrder>().m_nZValue)>::type TField;
			static constexpr bool IsArray = std::is_array<TField>::value;
			static constexpr size_t Count = IsArray ? std::extent<TField>::value : 1;

			static void Get(const ZOrder& z, uint64_t* pLimbs)
			{
				if constexpr (IsArray)
				{
					for (size_t i = 0; i < Count; ++i)
						pLimbs[i] = z.m_nZValue[i];
				}
				else
					pLimbs[0] = (uint64_t)z.m_nZValue;
			}

			static void Set(ZOrder& z, const uint64_t* pLimbs)
			{
				if constexpr (IsArray)
				{
					for (size_t i = 0; i < Count; ++i)
						z.m_nZValue[i] = pLimbs[i];
				}
				else
					z.m_nZValue = (TField)pLimbs[0];
			}
		};

		// Key of the Z-order B+ tree: the Z value of the shape and the id of the object.
		// The id makes the key unique, so objects with the same shape (two points at one place) are separate keys
		// and a remove takes exactly the given object.
		template <class _ZOrder>
		struct TZOrderKey
		{
			typedef _ZOrder ZOrder;

			TZOrderKey() {}
			TZOrderKey(const ZOrder& z, int64_t nId) : m_z(z), m_nId(nId) {}

			bool operator < (const TZOrderKey& key) const
			{
				if (m_z < key.m_z) return true;
				if (key.m_z < m_z) return false;
				return m_nId < key.m_nId;
			}

			bool operator == (const TZOrderKey& key) const
			{
				return m_z == key.m_z && m_nId == key.m_nId;
			}

			ZOrder m_z;
			int64_t m_nId{ 0 };
		};

		template <class TKey>
		struct TZOrderKeyComp
		{
			bool LE(const TKey& l, const TKey& r) const { return l < r; }
			bool EQ(const TKey& l, const TKey& r) const { return l == r; }
			bool operator()(const TKey& l, const TKey& r) const { return l < r; }
		};

		// Shape of the index: points
		template <class _ZOrder>
		struct TZOrderPointTraits
		{
			typedef _ZOrder ZOrder;
			typedef typename ZOrder::TPointType TCoord;
			typedef TPoint2D<TCoord> TShape;
			typedef TRect2D<TCoord> TRect;

			static ZOrder MakeZ(const TShape& p) { return ZOrder(p.m_x, p.m_y); }
			static TShape GetShape(const ZOrder& z) { TShape p; z.getXY(p.m_x, p.m_y); return p; }

			static bool InExtent(const TShape& p, const TRect& extent)
			{
				return !(p.m_x < extent.m_minX) && !(extent.m_maxX < p.m_x) && !(p.m_y < extent.m_minY) && !(extent.m_maxY < p.m_y);
			}

			// the key range of the query: the corners of the rect (the query is clipped to the extent already)
			static void Range(const TRect& q, const TRect& /*extent*/, ZOrder& zMin, ZOrder& zMax)
			{
				zMin = ZOrder(q.m_minX, q.m_minY);
				zMax = ZOrder(q.m_maxX, q.m_maxY);
			}

			static bool InQuery(const ZOrder& z, const TRect& q)
			{
				TCoord x, y;
				z.getXY(x, y);
				return !(x < q.m_minX) && !(q.m_maxX < x) && !(y < q.m_minY) && !(q.m_maxY < y);
			}

			// the smallest Z value > z inside the range (BIGMIN); z has to be outside the query
			static bool NextInRange(const ZOrder& z, const ZOrder& zMin, const ZOrder& zMax, ZOrder& zNext)
			{
				return FindMinZVal(z, zMin, zMax, zNext);
			}
		};

		// Shape of the index: rects, the key is the 4D point (minX, minY, maxX, maxY)
		template <class _ZOrder>
		struct TZOrderRectTraits
		{
			typedef _ZOrder ZOrder;
			typedef typename ZOrder::TPointType TCoord;
			typedef TRect2D<TCoord> TShape;
			typedef TRect2D<TCoord> TRect;

			static ZOrder MakeZ(const TShape& r) { return ZOrder(r.m_minX, r.m_minY, r.m_maxX, r.m_maxY); }
			static TShape GetShape(const ZOrder& z) { TShape r; z.getXY(r.m_minX, r.m_minY, r.m_maxX, r.m_maxY); return r; }

			static bool InExtent(const TShape& r, const TRect& extent)
			{
				return !(r.m_minX < extent.m_minX) && !(extent.m_maxX < r.m_maxX) && !(r.m_minY < extent.m_minY) && !(extent.m_maxY < r.m_maxY);
			}

			// intersects (closed) <=> minX <= q.maxX, minY <= q.maxY, maxX >= q.minX, maxY >= q.minY.
			// All rects are inside the extent, so the 4D box is from (ext.minX, ext.minY, q.minX, q.minY)
			// to (q.maxX, q.maxY, ext.maxX, ext.maxY). Bounding it by the extent instead of the coordinate
			// range keeps the Z range (and the BIGMIN jumps) small.
			static void Range(const TRect& q, const TRect& extent, ZOrder& zMin, ZOrder& zMax)
			{
				zMin = ZOrder(extent.m_minX, extent.m_minY, q.m_minX, q.m_minY);
				zMax = ZOrder(q.m_maxX, q.m_maxY, extent.m_maxX, extent.m_maxY);
			}

			static bool InQuery(const ZOrder& z, const TRect& q)
			{
				TCoord xMin, yMin, xMax, yMax;
				z.getXY(xMin, yMin, xMax, yMax);
				return !(q.m_maxX < xMin) && !(q.m_maxY < yMin) && !(xMax < q.m_minX) && !(yMax < q.m_minY);
			}

			static bool NextInRange(const ZOrder& z, const ZOrder& zMin, const ZOrder& zMax, ZOrder& zNext)
			{
				return FindRectMinZVal(z, zMin, zMax, zNext);
			}
		};
	}
	}
}
