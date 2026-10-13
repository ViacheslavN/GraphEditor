#pragma once
#include "SpatialKey.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace bptreedb
{
	namespace spatiallib
	{
		// Grid over the extent of the data: converts double coordinates to the integer coordinates of the
		// spatial indexes (as UniGIS rc2key / key2rc).
		//
		// Choose the cell from the precision the queries need, not the precision of the geometry: the index
		// finds candidates, the exact geometry is checked afterwards. A coarser grid gives smaller keys
		// (shorter Z deltas) at the same query speed, and a few more candidates.
		//
		// Grid coordinate of x: (x - extent.min) / cell, in [0, cells]. Rounding is outward, so nothing is lost:
		//   rect:  min -> floor, max -> ceil   (the grid rect covers the rect)
		//   point: floor                       (the grid line at or below the point)
		//   query: as a rect
		// If a rect (point) intersects the query, their grid rects intersect too (closed intervals).
		template <class _TCoord>
		class TSpatialGrid
		{
		public:
			typedef _TCoord TCoord;
			typedef TRect2D<double> TRectD;
			typedef TPoint2D<double> TPointD;
			typedef TRect2D<TCoord> TGridRect;
			typedef TPoint2D<TCoord> TGridPoint;

			TSpatialGrid() {}

			// cells on the longer side of the extent; the cell is square
			TSpatialGrid(const TRectD& extent, uint64_t nCells)
			{
				double dSize = (std::max)(extent.m_maxX - extent.m_minX, extent.m_maxY - extent.m_minY);
				Init(extent, dSize > 0 ? dSize / (double)nCells : 1.0);
			}

			TSpatialGrid(const TRectD& extent, double dCellSize)
			{
				Init(extent, dCellSize);
			}

			const TRectD& Extent() const { return m_extent; }
			double CellSize() const { return m_dCell; }

			// the extent in grid coordinates: give it to the index (TBPZOrderIndex extent)
			TGridRect GridExtent() const { return TGridRect(TCoord(0), TCoord(0), m_nMaxX, m_nMaxY); }

			bool InExtent(const TRectD& r) const
			{
				return !(r.m_minX < m_extent.m_minX) && !(m_extent.m_maxX < r.m_maxX) && !(r.m_minY < m_extent.m_minY) && !(m_extent.m_maxY < r.m_maxY);
			}

			bool InExtent(const TPointD& p) const
			{
				return InExtent(TRectD(p.m_x, p.m_y, p.m_x, p.m_y));
			}

			TGridRect ToGrid(const TRectD& r) const
			{
				return TGridRect(Floor(r.m_minX, m_extent.m_minX, m_nMaxX), Floor(r.m_minY, m_extent.m_minY, m_nMaxY),
					Ceil(r.m_maxX, m_extent.m_minX, m_nMaxX), Ceil(r.m_maxY, m_extent.m_minY, m_nMaxY));
			}

			TGridPoint ToGrid(const TPointD& p) const
			{
				TGridPoint g;
				g.m_x = Floor(p.m_x, m_extent.m_minX, m_nMaxX);
				g.m_y = Floor(p.m_y, m_extent.m_minY, m_nMaxY);
				return g;
			}

			// the area of a grid rect / point (covers the original shape)
			TRectD FromGrid(const TGridRect& g) const
			{
				return TRectD(m_extent.m_minX + g.m_minX * m_dCell, m_extent.m_minY + g.m_minY * m_dCell,
					m_extent.m_minX + g.m_maxX * m_dCell, m_extent.m_minY + g.m_maxY * m_dCell);
			}

			TRectD FromGrid(const TGridPoint& g) const
			{
				return TRectD(m_extent.m_minX + g.m_x * m_dCell, m_extent.m_minY + g.m_y * m_dCell,
					m_extent.m_minX + (g.m_x + 1.0) * m_dCell, m_extent.m_minY + (g.m_y + 1.0) * m_dCell);
			}

		private:
			void Init(const TRectD& extent, double dCellSize)
			{
				if (extent.m_maxX < extent.m_minX || extent.m_maxY < extent.m_minY)
					throw CommonLib::CExcBase("[TSpatialGrid] wrong extent");
				if (!(dCellSize > 0))
					throw CommonLib::CExcBase("[TSpatialGrid] wrong cell size");

				m_extent = extent;
				m_dCell = dCellSize;
				double dMaxX = std::ceil((extent.m_maxX - extent.m_minX) / dCellSize);
				double dMaxY = std::ceil((extent.m_maxY - extent.m_minY) / dCellSize);
				if (dMaxX >= (double)(std::numeric_limits<TCoord>::max)() || dMaxY >= (double)(std::numeric_limits<TCoord>::max)())
					throw CommonLib::CExcBase("[TSpatialGrid] too many cells for the coordinate type");
				m_nMaxX = (TCoord)dMaxX;
				m_nMaxY = (TCoord)dMaxY;
			}

			// clamped to [0, max]: a value outside the extent goes to the border (the index rejects objects
			// outside the extent, check them with InExtent first)
			TCoord Floor(double v, double origin, TCoord nMax) const
			{
				double g = std::floor((v - origin) / m_dCell);
				return g <= 0 ? TCoord(0) : (g >= (double)nMax ? nMax : (TCoord)g);
			}

			TCoord Ceil(double v, double origin, TCoord nMax) const
			{
				double g = std::ceil((v - origin) / m_dCell);
				return g <= 0 ? TCoord(0) : (g >= (double)nMax ? nMax : (TCoord)g);
			}

		private:
			TRectD m_extent;
			double m_dCell{ 1.0 };
			TCoord m_nMaxX{ 0 };
			TCoord m_nMaxY{ 0 };
		};
	}
}
