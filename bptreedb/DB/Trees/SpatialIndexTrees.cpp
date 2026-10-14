// The spatial indexes of the geometry fields: R-tree and Z-order B+ tree of the bounding boxes.

#include "TreeTypes.h"

namespace bptreedb
{
	namespace db
	{
		using namespace trees;

		namespace
		{
			//--------------------------------------------------------------------------------------------
			// spatial indexes

			bool MatchMode(const CommonLib::bbox& bb, const CommonLib::bbox& extent, eSpatialQueryMode mode)
			{
				bool bIntersects = !(bb.xMax < extent.xMin || extent.xMax < bb.xMin || bb.yMax < extent.yMin || extent.yMax < bb.yMin);
				switch (mode)
				{
				case sqmIntersect: return bIntersects;
				case sqmOver: return bb.xMin <= extent.xMin && bb.yMin <= extent.yMin && extent.xMax <= bb.xMax && extent.yMax <= bb.yMax;
				case sqmInside: return extent.xMin <= bb.xMin && extent.yMin <= bb.yMin && bb.xMax <= extent.xMax && bb.yMax <= extent.yMax;
				default:
					throw CommonLib::CExcBase("spatial query mode {0} isn't supported", (int)mode);
				}
			}

			void SortUnique(std::vector<int64_t>& rows)
			{
				std::sort(rows.begin(), rows.end());
				rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
			}

			class CRTreeIndex : public ISpatialIndexTree
			{
			public:
				typedef spatiallib::rtree::TRTree<double> TTree;
				typedef TTree::TRect TRect;

				CRTreeIndex(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage)
				{
					spatiallib::rtree::SRTreeParams params;
					params.m_nCompressLevel = (int32_t)ptrContext->m_nCompressLevel;
					m_ptrTree.reset(new TTree(nInfoPage, ptrPager, ptrContext->m_nSpatialCacheSize, params));
					m_ptrTree->Open();   // a new tree is written by Open
				}

				virtual int64_t GetInfoPage() const { return m_ptrTree->GetPageInfo(); }
				virtual uint64_t GetChangeCount() const { return m_nChanges; }
				virtual void GetSpace(STreeSpace& space, bool /*bBlobs*/)
				{
					uint64_t nInner = 0, nLeaves = 0;
					uint32_t nLevels = 0;
					m_ptrTree->GetPages(nInner, nLeaves, nLevels);
					space.m_nInnerPages += nInner;
					space.m_nLeafPages += nLeaves;
					space.m_nInfoPages += 1;
					space.m_nLevels = nLevels;
				}
				virtual void Flush()
				{
					if (m_bChanged)
						m_ptrTree->Flush();
					m_bChanged = false;
				}

				virtual void Insert(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					m_ptrTree->insert(ToRect(bb), nRowId);
				}

				virtual bool Remove(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					return m_ptrTree->remove(ToRect(bb), nRowId);
				}

				virtual void Query(const CommonLib::bbox& extent, eSpatialQueryMode mode, std::vector<int64_t>& rows, bool& bExact)
				{
					m_ptrTree->search(ToRect(extent), [&](const TRect& r, int64_t nRowId)
					{
						CommonLib::bbox bb;
						bb.xMin = r.m_minX; bb.yMin = r.m_minY; bb.xMax = r.m_maxX; bb.yMax = r.m_maxY;
						if (MatchMode(bb, extent, mode))
							rows.push_back(nRowId);
						return true;
					});
					SortUnique(rows);
					bExact = true;
				}

				virtual uint64_t Build(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries)
				{
					std::vector<TTree::TEntry> treeEntries;
					treeEntries.reserve(entries.size());
					for (const auto& e : entries)
						treeEntries.emplace_back(ToRect(e.first), e.second);
					std::vector<std::pair<CommonLib::bbox, int64_t> >().swap(entries);

					uint64_t nCount = treeEntries.size();
					++m_nChanges;
					m_bChanged = true;
					m_ptrTree->BulkLoad(treeEntries);     // STR packing, writes the nodes
					return nCount;
				}

			private:
				static TRect ToRect(const CommonLib::bbox& bb) { return TRect(bb.xMin, bb.yMin, bb.xMax, bb.yMax); }

				std::unique_ptr<TTree> m_ptrTree;
				uint64_t m_nChanges{ 0 };
				bool m_bChanged{ false };
			};

			class CZOrderIndex : public ISpatialIndexTree
			{
			public:
				typedef spatiallib::zorder::TBPZOrderGridIndex<spatiallib::zorder::TZOrderRectTraits<spatiallib::ZOrderRect2DU32> > TIndex;
				typedef TIndex::TGrid TGrid;
				typedef TIndex::TRectD TRectD;

				CZOrderIndex(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, const SSpatialIndexParams& params, int64_t nInfoPage)
				{
					TGrid grid(TRectD(params.m_extent.xMin, params.m_extent.yMin, params.m_extent.xMax, params.m_extent.yMax), params.m_dCellSize);
					m_ptrIndex.reset(new TIndex(nInfoPage, ptrPager, ptrContext->m_ptrAlloc, ptrContext->m_nSpatialCacheSize, ptrContext->m_nPageSize, grid));
					if (nInfoPage == -1)
					{
						m_ptrIndex->Create();
						m_bChanged = true;
					}
				}

				virtual int64_t GetInfoPage() const { return m_ptrIndex->GetPageInfo(); }
				virtual uint64_t GetChangeCount() const { return m_nChanges; }
				virtual void GetSpace(STreeSpace& space, bool /*bBlobs*/) { BPTreeSpace(m_ptrIndex->Index().Tree(), space); }
				virtual void Flush()
				{
					if (m_bChanged)
						m_ptrIndex->Flush();
					m_bChanged = false;
				}

				virtual void Insert(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					m_ptrIndex->insert(ToRect(bb), nRowId);
				}

				virtual bool Remove(const CommonLib::bbox& bb, int64_t nRowId)
				{
					++m_nChanges;
					m_bChanged = true;
					return m_ptrIndex->remove(ToRect(bb), nRowId);
				}

				virtual void Query(const CommonLib::bbox& extent, eSpatialQueryMode mode, std::vector<int64_t>& rows, bool& bExact)
				{
					if (mode != sqmIntersect && mode != sqmOver && mode != sqmInside)
						throw CommonLib::CExcBase("spatial query mode {0} isn't supported", (int)mode);

					// the extent clipped by the grid: the index keeps nothing outside it
					const TRectD& ge = m_ptrIndex->Grid().Extent();
					TRectD q((std::max)(extent.xMin, ge.m_minX), (std::max)(extent.yMin, ge.m_minY), (std::min)(extent.xMax, ge.m_maxX), (std::min)(extent.yMax, ge.m_maxY));
					if (q.m_maxX < q.m_minX || q.m_maxY < q.m_minY)
						return;

					m_ptrIndex->search(q, [&](const TRectD&, int64_t nRowId) { rows.push_back(nRowId); return true; });
					SortUnique(rows);
					bExact = false;   // candidates by the grid cells
				}

				virtual uint64_t Build(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries)
				{
					std::vector<std::pair<TRectD, int64_t> > objects;
					objects.reserve(entries.size());
					for (const auto& e : entries)
						objects.emplace_back(ToRect(e.first), e.second);
					std::vector<std::pair<CommonLib::bbox, int64_t> >().swap(entries);

					++m_nChanges;
					m_bChanged = true;
					return m_ptrIndex->BulkLoad(objects);     // sorted Z keys, bulk load of the B+ tree
				}

			private:
				static TRectD ToRect(const CommonLib::bbox& bb) { return TRectD(bb.xMin, bb.yMin, bb.xMax, bb.yMax); }

				std::unique_ptr<TIndex> m_ptrIndex;
				uint64_t m_nChanges{ 0 };
				bool m_bChanged{ false };
			};
		}

		ISpatialIndexTreePtr CreateSpatialIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, const SSpatialIndexParams& params, int64_t nInfoPage)
		{
			try
			{
				switch (params.m_type)
				{
				case itSpatialRTree: return std::make_shared<CRTreeIndex>(ptrContext, ptrPager, nInfoPage);
				case itSpatialZOrder: return std::make_shared<CZOrderIndex>(ptrContext, ptrPager, params, nInfoPage);
				default:
					throw CommonLib::CExcBase("index type {0} isn't spatial", (int)params.m_type);
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the spatial index, info page {0}", nInfoPage, excSrc);
				throw;
			}
		}
	}
}
