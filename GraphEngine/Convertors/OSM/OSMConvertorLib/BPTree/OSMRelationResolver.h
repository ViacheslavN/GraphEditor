#pragma once

// CRelationResolver: the members of the relations resolved by several threads through the node file and the way file,
// the records of the relations (OSMRelationRecord.h).

#include "OSMBPTreeTypes.h"
#include "OSMBPTreeSettings.h"
#include "OSMWayTreeTypes.h"
#include "OSMSharedNodeReader.h"
#include "OSMSharedWayReader.h"
#include "OSMJobPipeline.h"
#include "../readosm/readosm.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace bptreedb {
    namespace storage {
        class CFileStorage;
    }
}

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the relations of a job: the members of relation i are [memberStarts[i], memberStarts[i + 1]) (the last one -
            // to the end), the role of member m - roles[roleStarts[m], roleStarts[m + 1])
            struct SRelationBatch
            {
                std::vector<int64_t>  ids;
                std::vector<uint8_t>  areas;          // type=multipolygon / boundary
                std::vector<uint32_t> memberStarts;
                std::vector<uint8_t>  memberTypes;    // eRelationMemberType
                std::vector<int64_t>  memberRefs;
                std::vector<uint32_t> roleStarts;
                std::string           roles;

                void Add(const readosm_relation& relation, bool bArea);
                size_t   Count() const { return ids.size(); }
                uint32_t MemberCount(size_t i) const { return (uint32_t)((i + 1 < memberStarts.size() ? memberStarts[i + 1] : memberRefs.size()) - memberStarts[i]); }
                uint32_t RoleSize(size_t m) const { return (uint32_t)((m + 1 < roleStarts.size() ? roleStarts[m + 1] : roles.size()) - roleStarts[m]); }
            };

            // the records of a job in the relation id order
            struct SRelationRecords
            {
                std::vector<int64_t>  ids;
                std::vector<uint32_t> offsets;
                std::vector<uint32_t> sizes;
                std::vector<byte_t>   data;
                SOSMBPTreeRelationHeader counts;     // the counts of the job (nRelationCount ... nShapeBytes)
            };

            // The resolving threads (TJobPipeline):
            //   - a job is a batch of relations (SRelationBatch, by the reading thread);
            //   - a thread sorts the node members of the batch by the id and finds them in the node file in that order, then
            //     the way members - in the way file (the files are read forward, a leaf is decoded once per job, the same
            //     member is looked up once);
            //   - the shapes (CRingBuilder for the areas) and the records of the relations in the id order.
            // The node file and the way file are read by all the threads through one CSharedNodeReader / CSharedWayReader
            // (the leaf index and the cache of the decoded leaves are shared).
            class CRelationResolver
            {
            public:
                CRelationResolver(const SOSMBPTreeSettings& settings,
                                  std::shared_ptr<bptreedb::storage::CFileStorage> ptrNodeStorage, const SOSMBPTreeNodeHeader& nodeHeader,
                                  std::shared_ptr<bptreedb::storage::CFileStorage> ptrWayStorage, const SOSMBPTreeWayHeader& wayHeader,
                                  int nThreads);
                ~CRelationResolver();

                bool Submit(SRelationBatch& batch) { return m_ptrPipeline->Submit(batch); }
                void InputDone() { m_ptrPipeline->InputDone(); }
                bool Next(SRelationRecords& records) { return m_ptrPipeline->Next(records); }
                void Stop() { m_ptrPipeline->Stop(); }

                int      Threads() const { return m_ptrPipeline->Threads(); }
                double   ResolveSeconds() const { return m_ptrPipeline->WorkSeconds(); }
                double   WaitSeconds() const { return m_ptrPipeline->WaitSeconds(); }
                const CSharedNodeReader& Nodes() const { return *m_ptrNodes; }
                const CSharedWayReader&  Ways() const { return *m_ptrWays; }
                uint64_t NodeLookups() const { return m_nNodeLookups.load(); }
                uint64_t WayLookups() const { return m_nWayLookups.load(); }

            private:
                struct SWorker;

                CRelationResolver(const CRelationResolver&);
                CRelationResolver& operator=(const CRelationResolver&);

                void Resolve(SRelationBatch& batch, SRelationRecords& records, SWorker& worker);

            private:
                SOSMBPTreeSettings m_settings;
                std::unique_ptr<CSharedNodeReader> m_ptrNodes;
                std::unique_ptr<CSharedWayReader>  m_ptrWays;
                std::atomic<uint64_t> m_nNodeLookups{0};
                std::atomic<uint64_t> m_nWayLookups{0};
                std::unique_ptr<TJobPipeline<SRelationBatch, SRelationRecords> > m_ptrPipeline;    // the last: stopped first
            };
        }
    }
}
