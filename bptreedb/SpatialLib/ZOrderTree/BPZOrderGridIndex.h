#pragma once
#include "BPZOrderIndex.h"
#include "../SpatialGrid.h"

namespace bptreedb
{
	namespace spatiallib
	{
	namespace zorder
	{
		// TBPZOrderIndex with double coordinates: shapes and queries go through TSpatialGrid (outward rounding).
		// search() gives candidates: every object that intersects the query, plus objects whose grid cells
		// touch the query cells (check the exact geometry). The callback gets the id and the grid area of the
		// object (a rect that covers it).
		// A shape is removed by the same coordinates it was inserted with (they give the same grid key).
		template <class _TTraits>
		class TBPZOrderGridIndex
		{
		public:
			typedef _TTraits TTraits;
			typedef TBPZOrderIndex<TTraits> TIndex;
			typedef typename TIndex::TCoord TCoord;
			typedef typename TIndex::TShape TGridShape;
			typedef TSpatialGrid<TCoord> TGrid;
			typedef typename TGrid::TRectD TRectD;
			typedef typename std::conditional<std::is_same<TGridShape, TPoint2D<TCoord> >::value, TPoint2D<double>, TRectD>::type TShape;
			typedef std::function<bool(const TRectD& gridArea, int64_t nId)> TSearchCallback;

			TBPZOrderGridIndex(int64_t nPageInfo, storage::IFilePagerPtr pStorage, CommonLib::IAllocPtr pAlloc, uint32_t nCacheSize, uint32_t nPageSize, const TGrid& grid) :
				m_grid(grid), m_index(nPageInfo, pStorage, pAlloc, nCacheSize, nPageSize, grid.GridExtent())
			{
			}

			void Create() { m_index.Create(); }
			int64_t GetPageInfo() const { return m_index.GetPageInfo(); }
			const TGrid& Grid() const { return m_grid; }
			TIndex& Index() { return m_index; }

			void insert(const TShape& shape, int64_t nId)
			{
				m_index.insert(ToGrid(shape, nId), nId);
			}

			bool remove(const TShape& shape, int64_t nId)
			{
				return m_index.remove(ToGrid(shape, nId), nId);
			}

			// objects: pairs (TShape, id)
			template<class TObjects>
			uint64_t BulkLoad(const TObjects& objects)
			{
				std::vector<std::pair<TGridShape, int64_t> > grid;
				grid.reserve(objects.size());
				for (const auto& obj : objects)
					grid.emplace_back(ToGrid(obj.first, obj.second), obj.second);
				return m_index.BulkLoad(grid);
			}

			SZOrderQueryStats search(const TRectD& rect, const TSearchCallback& func, EZOrderQueryMode mode = eZQueryBigMin)
			{
				if (rect.m_maxX < rect.m_minX || rect.m_maxY < rect.m_minY)
					return SZOrderQueryStats();
				const TGrid& grid = m_grid;
				return m_index.search(grid.ToGrid(rect), [&grid, &func](const TGridShape& s, int64_t nId) { return func(grid.FromGrid(s), nId); }, mode);
			}

			void Flush() { m_index.Flush(); }

		private:
			TGridShape ToGrid(const TShape& shape, int64_t nId) const
			{
				if (!m_grid.InExtent(shape))
					throw CommonLib::CExcBase("[TBPZOrderGridIndex] object {0} is outside the extent of the grid", nId);
				return m_grid.ToGrid(shape);
			}

		private:
			TGrid m_grid;
			TIndex m_index;
		};
	}
	}
}
