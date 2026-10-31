#include "OSMRelationResolver.h"
#include "OSMRelationRecord.h"
#include "OSMRingBuilder.h"
#include "OSMBPTreeUtils.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"

#include <algorithm>
#include <numeric>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void SRelationBatch::Add(const readosm_relation& relation, bool bArea)
            {
                ids.push_back(relation.id);
                areas.push_back(bArea ? 1 : 0);
                memberStarts.push_back((uint32_t)memberRefs.size());
                for (int i = 0; i < relation.member_count; ++i)
                {
                    const readosm_member& member = relation.members[i];
                    uint8_t type = member.member_type == READOSM_MEMBER_NODE ? RelationMemberNode
                                 : member.member_type == READOSM_MEMBER_WAY ? RelationMemberWay : RelationMemberRelation;
                    memberTypes.push_back(type);
                    memberRefs.push_back(member.id);
                    roleStarts.push_back((uint32_t)roles.size());
                    if (member.role)
                        roles += member.role;
                }
            }

            // a resolving thread: its lookups of the shared readers, the compressor, the buffers
            struct CRelationResolver::SWorker
            {
                std::unique_ptr<CSharedNodeReader::CLookup> ptrNodes;
                std::unique_ptr<CSharedWayReader::CLookup>  ptrWays;
                std::unique_ptr<GeometryCompression::CShapeCompressor> ptrCompressor;
                CRingBuilder rings;

                std::vector<uint32_t> order;                         // the relations by the id
                std::vector<std::pair<int64_t, uint32_t> > sorted;  // the members of a type: id, index of the member
                std::vector<SOSMPointE7> nodePoints;                // by the index of the member
                std::vector<uint8_t>  found;                        // by the index of the member
                std::vector<uint32_t> wayStarts, waySizes;          // the points of a way member in wayPoints
                std::vector<SOSMPointE7> wayPoints;
                std::vector<SOSMPointE7> points;
                std::vector<int64_t>  xs, ys;
                std::vector<uint32_t> parts;
                std::vector<byte_t>   shape;
                std::vector<byte_t>   record;
            };

            CRelationResolver::CRelationResolver(const SOSMBPTreeSettings& settings,
                                                 std::shared_ptr<bptreedb::storage::CFileStorage> ptrNodeStorage, const SOSMBPTreeNodeHeader& nodeHeader,
                                                 std::shared_ptr<bptreedb::storage::CFileStorage> ptrWayStorage, const SOSMBPTreeWayHeader& wayHeader,
                                                 int nThreads) :
                    m_settings(settings)
            {
                m_ptrNodes.reset(new CSharedNodeReader(ptrNodeStorage, nodeHeader, 0, m_settings.nRelNodeCacheMB, PhysicalMemory() / 4));
                m_ptrWays.reset(new CSharedWayReader(ptrWayStorage, wayHeader, m_settings.nRelWayCacheMB));
                m_ptrPipeline.reset(new TJobPipeline<SRelationBatch, SRelationRecords>(nThreads, [this]()
                {
                    std::shared_ptr<SWorker> ptrWorker = std::make_shared<SWorker>();
                    ptrWorker->ptrNodes.reset(new CSharedNodeReader::CLookup(*m_ptrNodes));
                    ptrWorker->ptrWays.reset(new CSharedWayReader::CLookup(*m_ptrWays));
                    GeometryCompression::SShapeCompressParams params;
                    params.nScaleExponent = ShapeScaleExponent;
                    ptrWorker->ptrCompressor.reset(new GeometryCompression::CShapeCompressor(params));
                    return [this, ptrWorker](SRelationBatch& batch, SRelationRecords& records) { Resolve(batch, records, *ptrWorker); };
                }));
            }

            CRelationResolver::~CRelationResolver()
            {
                Stop();
            }

            void CRelationResolver::Resolve(SRelationBatch& batch, SRelationRecords& records, SWorker& w)
            {
                const uint32_t n = (uint32_t)batch.Count();
                const uint32_t nMembers = (uint32_t)batch.memberRefs.size();
                SOSMBPTreeRelationHeader& counts = records.counts;
                counts.nRelationCount = n;
                counts.nMemberCount = nMembers;

                w.found.assign(nMembers, 0);
                w.nodePoints.resize(nMembers);
                w.wayStarts.assign(nMembers, 0);
                w.waySizes.assign(nMembers, 0);
                w.wayPoints.clear();

                // the members of a type by the id: the file is read forward, the same member is looked up once
                auto sortMembers = [&](uint8_t type)
                {
                    w.sorted.clear();
                    for (uint32_t m = 0; m < nMembers; ++m)
                        if (batch.memberTypes[m] == type)
                            w.sorted.emplace_back(batch.memberRefs[m], m);
                    std::sort(w.sorted.begin(), w.sorted.end());
                };

                // the nodes
                sortMembers(RelationMemberNode);
                counts.nNodeMembers = w.sorted.size();
                uint64_t nLookups = 0;
                for (size_t k = 0; k < w.sorted.size(); ++k)
                {
                    uint32_t m = w.sorted[k].second;
                    if (k > 0 && w.sorted[k].first == w.sorted[k - 1].first)
                    {
                        uint32_t nPrev = w.sorted[k - 1].second;
                        w.found[m] = w.found[nPrev];
                        w.nodePoints[m] = w.nodePoints[nPrev];
                        continue;
                    }
                    ++nLookups;
                    if (w.ptrNodes->Get(w.sorted[k].first, w.nodePoints[m]))
                        w.found[m] = 1;
                }
                m_nNodeLookups += nLookups;

                // the ways: their points
                sortMembers(RelationMemberWay);
                counts.nWayMembers = w.sorted.size();
                nLookups = 0;
                for (size_t k = 0; k < w.sorted.size(); ++k)
                {
                    uint32_t m = w.sorted[k].second;
                    if (k > 0 && w.sorted[k].first == w.sorted[k - 1].first)
                    {
                        uint32_t nPrev = w.sorted[k - 1].second;
                        w.found[m] = w.found[nPrev];
                        w.wayStarts[m] = w.wayStarts[nPrev];
                        w.waySizes[m] = w.waySizes[nPrev];
                        continue;
                    }
                    ++nLookups;
                    if (w.ptrWays->GetPoints(w.sorted[k].first, w.points))
                    {
                        w.found[m] = 1;
                        w.wayStarts[m] = (uint32_t)w.wayPoints.size();
                        w.waySizes[m] = (uint32_t)w.points.size();
                        w.wayPoints.insert(w.wayPoints.end(), w.points.begin(), w.points.end());
                    }
                }
                m_nWayLookups += nLookups;

                // the records in the relation id order
                w.order.resize(n);
                std::iota(w.order.begin(), w.order.end(), 0u);
                std::sort(w.order.begin(), w.order.end(), [&](uint32_t a, uint32_t b) { return batch.ids[a] < batch.ids[b]; });
                for (uint32_t r : w.order)
                {
                    uint32_t nStart = batch.memberStarts[r];
                    uint32_t nCount = batch.MemberCount(r);
                    uint32_t nWays = 0, nNodes = 0;
                    for (uint32_t m = nStart; m < nStart + nCount; ++m)
                    {
                        uint8_t type = batch.memberTypes[m];
                        if (type == RelationMemberRelation)
                            counts.nRelationMembers += 1;
                        else if (!w.found[m])
                            (type == RelationMemberNode ? counts.nMissingNodes : counts.nMissingWays) += 1;
                        else if (type == RelationMemberWay)
                            ++nWays;
                        else
                            ++nNodes;
                    }

                    // the shape
                    w.xs.clear();
                    w.ys.clear();
                    w.parts.clear();
                    w.shape.clear();
                    CommonLib::eShapeType shapeType = CommonLib::shape_type_null;
                    if (batch.areas[r])
                    {
                        w.rings.Clear();
                        for (uint32_t m = nStart; m < nStart + nCount; ++m)
                            if (batch.memberTypes[m] == RelationMemberWay && w.found[m])
                                w.rings.AddWay(w.wayPoints.data() + w.wayStarts[m], w.waySizes[m]);
                        if (w.rings.Build(w.xs, w.ys, w.parts))
                        {
                            shapeType = CommonLib::shape_type_polygon;
                            counts.nPolygons += 1;
                        }
                        else
                            counts.nAreasWithoutRings += 1;
                        counts.nUnclosedWays += w.rings.Unclosed();
                    }
                    else if (nWays != 0)
                    {
                        // a part per way
                        for (uint32_t m = nStart; m < nStart + nCount; ++m)
                        {
                            if (batch.memberTypes[m] != RelationMemberWay || !w.found[m] || w.waySizes[m] < 2)
                                continue;
                            const SOSMPointE7* p = w.wayPoints.data() + w.wayStarts[m];
                            for (uint32_t i = 0; i < w.waySizes[m]; ++i)
                            {
                                w.xs.push_back(p[i].lon);
                                w.ys.push_back(p[i].lat);
                            }
                            w.parts.push_back(w.waySizes[m]);
                        }
                        if (!w.parts.empty())
                            shapeType = CommonLib::shape_type_polyline;
                    }
                    else if (nNodes != 0)
                    {
                        for (uint32_t m = nStart; m < nStart + nCount; ++m)
                        {
                            if (batch.memberTypes[m] != RelationMemberNode || !w.found[m])
                                continue;
                            w.xs.push_back(w.nodePoints[m].lon);
                            w.ys.push_back(w.nodePoints[m].lat);
                        }
                        shapeType = CommonLib::shape_type_multipoint;
                    }
                    if (shapeType != CommonLib::shape_type_null)
                    {
                        if (!w.ptrCompressor->CompressQuantized(shapeType, w.xs.data(), w.ys.data(), (uint32_t)w.xs.size(),
                                                                w.parts.data(), (uint32_t)w.parts.size(), w.shape))
                            throw CommonLib::CExcBase("the shape of relation {0} isn't compressed", batch.ids[r]);
                        counts.nWithShape += 1;
                        counts.nShapeBytes += w.shape.size();
                    }

                    // the record
                    w.record.clear();
                    WriteVarUInt(w.record, w.shape.size());
                    w.record.insert(w.record.end(), w.shape.begin(), w.shape.end());
                    WriteVarUInt(w.record, nCount);
                    int64_t nPrev = 0;
                    for (uint32_t m = nStart; m < nStart + nCount; ++m)
                    {
                        w.record.push_back(batch.memberTypes[m]);
                        WriteVarUInt(w.record, ZigZag64(batch.memberRefs[m] - nPrev));
                        nPrev = batch.memberRefs[m];
                        uint32_t nRole = batch.RoleSize(m);
                        WriteVarUInt(w.record, nRole);
                        const byte_t* pRole = (const byte_t*)batch.roles.data() + batch.roleStarts[m];
                        w.record.insert(w.record.end(), pRole, pRole + nRole);
                    }
                    records.ids.push_back(batch.ids[r]);
                    records.offsets.push_back((uint32_t)records.data.size());
                    records.sizes.push_back((uint32_t)w.record.size());
                    records.data.insert(records.data.end(), w.record.begin(), w.record.end());
                    counts.nRecordBytes += w.record.size();
                }
            }
        }
    }
}
