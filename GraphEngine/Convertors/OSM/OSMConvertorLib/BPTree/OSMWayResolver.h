#pragma once

// CWayResolver: the node refs of the ways resolved by several threads through the node tree, the shapes of the ways.

#include "OSMBPTreeTypes.h"
#include "OSMBPTreeSettings.h"
#include "OSMWayTreeTypes.h"
#include "OSMSharedNodeReader.h"
#include "OSMJobPipeline.h"
#include "../readosm/readosm.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace bptreedb {
    namespace storage {
        class CFileStorage;
    }
}

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the ways of a job: the refs of way i are refs[refStarts[i], refStarts[i + 1]) (the last one - to the end)
            struct SWayBatch
            {
                std::vector<int64_t>  ids;
                std::vector<uint32_t> refStarts;
                std::vector<int64_t>  refs;

                void Add(const readosm_way& way);
                size_t   Count() const { return ids.size(); }
                uint32_t RefCount(size_t i) const { return (uint32_t)((i + 1 < refStarts.size() ? refStarts[i + 1] : refs.size()) - refStarts[i]); }
                const int64_t* Refs(size_t i) const { return refs.data() + refStarts[i]; }
                void Clear() { ids.clear(); refStarts.clear(); refs.clear(); }
            };

            // the shapes of a job in the way id order (the ways without a shape aren't here)
            struct SWayShapes
            {
                std::vector<int64_t>  ids;
                std::vector<uint32_t> offsets;          // the shape of way i: data[offsets[i], offsets[i] + sizes[i])
                std::vector<uint32_t> sizes;
                std::vector<byte_t>   data;
                uint64_t nWays = 0;                     // the ways of the job
                uint64_t nRefs = 0;
                uint64_t nMissingRefs = 0;
                uint64_t nWaysWithMissing = 0;
                uint64_t nWaysWithoutShape = 0;
            };

            // The resolving threads:
            //   - a job is a batch of ways (SWayBatch, Submit by the reading thread, numbered in the order);
            //   - a thread sorts all the node refs of the batch by the id and finds them in the node tree in that order: the
            //     tree is read forward, a lookup starts in the leaf of the previous one, a leaf is decoded once per batch
            //     (the refs of a batch spread over the node ids: the bigger the batch, the less often a leaf is decoded
            //     again - nWayBatchRefs); the same node of several ways is looked up once;
            //   - the points of every way in its order (the missing nodes skipped) are compressed (CShapeCompressor,
            //     1e-7 degrees), the shapes of a job are sorted by the way id, the writer takes the jobs in the order (Next).
            // The node file is read by all the threads through one CSharedNodeReader: the leaf index and the cache of the
            // decoded leaves (nNodeCacheLeaves) are shared, a leaf decoded by one thread is used by the others.
            class CWayResolver
            {
            public:
                CWayResolver(const SOSMBPTreeSettings& settings, std::shared_ptr<bptreedb::storage::CFileStorage> ptrNodeStorage,
                             const SOSMBPTreeNodeHeader& nodeHeader, int nThreads);
                ~CWayResolver();

                // the reading thread: a job (taken, the batch is empty after it); waits while too many jobs are in flight;
                // false - the resolver stopped (an error / Stop)
                bool Submit(SWayBatch& batch) { return m_ptrPipeline->Submit(batch); }
                // the reading thread: no more jobs
                void InputDone() { m_ptrPipeline->InputDone(); }
                // the writer: the shapes of the next job in the order, false - all the jobs are taken; the error of a thread
                // is thrown
                bool Next(SWayShapes& shapes) { return m_ptrPipeline->Next(shapes); }
                // the threads stop (canceled / an error elsewhere)
                void Stop() { m_ptrPipeline->Stop(); }

                int      Threads() const { return m_ptrPipeline->Threads(); }
                double   ResolveSeconds() const { return m_ptrPipeline->WorkSeconds(); }   // all the threads
                const CSharedNodeReader& Nodes() const { return *m_ptrNodes; }
                double   WaitSeconds() const { return m_ptrPipeline->WaitSeconds(); }      // the writer waited for the jobs
                uint64_t Lookups() const { return m_nLookups.load(); }                     // the node tree finds (unique refs of a job)

            private:
                struct SWorker;

                CWayResolver(const CWayResolver&);
                CWayResolver& operator=(const CWayResolver&);

                void Resolve(SWayBatch& batch, SWayShapes& shapes, SWorker& worker);

            private:
                SOSMBPTreeSettings m_settings;
                std::unique_ptr<CSharedNodeReader> m_ptrNodes;
                std::atomic<uint64_t> m_nLookups{0};
                std::unique_ptr<TJobPipeline<SWayBatch, SWayShapes> > m_ptrPipeline;    // the last: stopped first
            };
        }
    }
}
