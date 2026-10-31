#include "OSMWayResolver.h"
#include "OSMBPTreeUtils.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void SWayBatch::Add(const readosm_way& way)
            {
                ids.push_back(way.id);
                refStarts.push_back((uint32_t)refs.size());
                if (way.node_ref_count > 0)
                    refs.insert(refs.end(), way.node_refs, way.node_refs + way.node_ref_count);
            }

            // a resolving thread: its lookups of the shared node reader, the compressor, the buffers
            struct CWayResolver::SWorker
            {
                std::unique_ptr<CSharedNodeReader::CLookup> ptrLookup;
                std::unique_ptr<GeometryCompression::CShapeCompressor> ptrCompressor;

                std::vector<uint32_t> order;                         // the ways by the id
                std::vector<std::pair<int64_t, uint32_t> > sorted;  // the refs of the batch: id, index in SWayBatch::refs
                std::vector<SOSMPointE7> points;                    // by the index of the ref
                std::vector<uint8_t>  found;
                std::vector<int64_t>  xs, ys;                       // the points of a shape
                std::vector<byte_t>   blob;
            };

            CWayResolver::CWayResolver(const SOSMBPTreeSettings& settings, std::shared_ptr<bptreedb::storage::CFileStorage> ptrNodeStorage,
                                       const SOSMBPTreeNodeHeader& nodeHeader, int nThreads) :
                    m_settings(settings)
            {
                m_ptrNodes.reset(new CSharedNodeReader(ptrNodeStorage, nodeHeader, m_settings.nNodeCacheLeaves, m_settings.nNodeCacheMB));
                m_ptrPipeline.reset(new TJobPipeline<SWayBatch, SWayShapes>(nThreads, [this]()
                {
                    // the state of a thread: its lookups of the shared node reader, the compressor, the buffers
                    std::shared_ptr<SWorker> ptrWorker = std::make_shared<SWorker>();
                    ptrWorker->ptrLookup.reset(new CSharedNodeReader::CLookup(*m_ptrNodes));
                    GeometryCompression::SShapeCompressParams params;
                    params.nScaleExponent = ShapeScaleExponent;
                    ptrWorker->ptrCompressor.reset(new GeometryCompression::CShapeCompressor(params));
                    return [this, ptrWorker](SWayBatch& batch, SWayShapes& shapes) { Resolve(batch, shapes, *ptrWorker); };
                }));
            }

            CWayResolver::~CWayResolver()
            {
                Stop();
            }

            void CWayResolver::Resolve(SWayBatch& batch, SWayShapes& shapes, SWorker& w)
            {
                const uint32_t n = (uint32_t)batch.Count();
                const uint32_t nAllRefs = (uint32_t)batch.refs.size();
                shapes.nWays = n;
                shapes.nRefs = nAllRefs;
                CSharedNodeReader::CLookup& lookup = *w.ptrLookup;

                // all the refs of the batch by the id: the node tree is read once in the key order (a leaf is decoded once,
                // the next lookup starts in the leaf of the previous one)
                w.sorted.resize(nAllRefs);
                for (uint32_t i = 0; i < nAllRefs; ++i)
                    w.sorted[i] = std::make_pair(batch.refs[i], i);
                std::sort(w.sorted.begin(), w.sorted.end());

                w.points.resize(nAllRefs);
                w.found.assign(nAllRefs, 0);
                uint64_t nLookups = 0;
                for (uint32_t k = 0; k < nAllRefs; ++k)
                {
                    uint32_t nPos = w.sorted[k].second;
                    if (k > 0 && w.sorted[k].first == w.sorted[k - 1].first)
                    {
                        // the same node again (a node of several ways, a closed way)
                        uint32_t nPrev = w.sorted[k - 1].second;
                        w.found[nPos] = w.found[nPrev];
                        w.points[nPos] = w.points[nPrev];
                        continue;
                    }
                    ++nLookups;
                    if (lookup.Get(w.sorted[k].first, w.points[nPos]))
                        w.found[nPos] = 1;
                }
                m_nLookups += nLookups;

                // the shapes in the way id order (a batch of a sorted file is in the order already)
                w.order.resize(n);
                std::iota(w.order.begin(), w.order.end(), 0u);
                std::sort(w.order.begin(), w.order.end(), [&](uint32_t a, uint32_t b) { return batch.ids[a] < batch.ids[b]; });
                for (uint32_t nWay : w.order)
                {
                    // the points in the order of the way, the missing nodes skipped
                    uint32_t nStart = batch.refStarts[nWay];
                    uint32_t nRefs = batch.RefCount(nWay);
                    w.xs.clear();
                    w.ys.clear();
                    uint32_t nMissing = 0;
                    for (uint32_t i = nStart; i < nStart + nRefs; ++i)
                    {
                        if (!w.found[i])
                        {
                            ++nMissing;
                            continue;
                        }
                        w.xs.push_back(w.points[i].lon);
                        w.ys.push_back(w.points[i].lat);
                    }
                    shapes.nMissingRefs += nMissing;
                    if (nMissing)
                        shapes.nWaysWithMissing += 1;
                    if (w.xs.size() < 2)
                    {
                        shapes.nWaysWithoutShape += 1;
                        continue;
                    }

                    uint32_t nPoints = (uint32_t)w.xs.size();
                    w.blob.clear();
                    if (!w.ptrCompressor->CompressQuantized(CommonLib::shape_type_polyline, w.xs.data(), w.ys.data(), nPoints, &nPoints, 1, w.blob))
                        throw CommonLib::CExcBase("the shape of way {0} isn't compressed", batch.ids[nWay]);
                    shapes.ids.push_back(batch.ids[nWay]);
                    shapes.offsets.push_back((uint32_t)shapes.data.size());
                    shapes.sizes.push_back((uint32_t)w.blob.size());
                    shapes.data.insert(shapes.data.end(), w.blob.begin(), w.blob.end());
                }
            }
        }
    }
}
