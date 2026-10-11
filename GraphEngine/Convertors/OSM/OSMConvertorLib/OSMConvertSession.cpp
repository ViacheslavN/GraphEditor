#include "OSMConvertSession.h"
#include "OSMReader.h"
#include "OSMTableWriter.h"
#include "OSMFeatureWriter.h"
#include "OSMWayStore.h"
#include "OSMTempFile.h"
#include "OSMProgressThrottle.h"
#include "OSMMapStyle.h"
#include "OSMConvertor.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <cstdlib>
#include <cstring>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const uint64_t ProgressStep = 100000;

            typedef COSMStoredTags CStoredTags;
            typedef TOSMTagList TTagList;
            typedef COSMFeatureWriter CFeatureWriter;
            typedef COSMFeatureWriterPtr CFeatureWriterPtr;

            // a layer of the schema in the current run
            struct SLayerRun
            {
                IOSMLayerPtr      ptrLayer;     // null - removed from the OSM map
                CFeatureWriterPtr ptrWriter;    // converted (not excluded): the tags of its objects go to the tags table
            };

            // a table of the current run
            struct STableRun
            {
                IOSMTablePtr      ptrTable;
                COSMTableWriterPtr ptrWriter;   // converted (not excluded)
                COSMTableWriterPtr ptrMembers;  // routes
            };

            // a multipolygon of a converted layer, built after the ways are read
            struct SMultipolygon
            {
                int64_t              nId = 0;
                SOSMMatch            match;
                TTagList             tags;
                std::vector<int64_t> vecWays;
            };

            // the multipolygons wait for their ways in a temporary file (the planet has millions of them):
            // id, layer, key, value, tags, member ways
            void WriteMultipolygon(COSMTempFile& file, const readosm_relation& relation, const SOSMMatch& match, const std::vector<int64_t>& vecWays)
            {
                file.WriteValue((int64_t)relation.id);
                file.WriteValue((int32_t)match.nLayer);
                file.WriteString(match.sKey);
                file.WriteString(match.sValue);
                file.WriteValue((uint32_t)relation.tag_count);
                for(int i = 0; i < relation.tag_count; ++i)
                {
                    file.WriteString(relation.tags[i].key ? relation.tags[i].key : "");
                    file.WriteString(relation.tags[i].value ? relation.tags[i].value : "");
                }
                file.WriteValue((uint32_t)vecWays.size());
                file.Write(vecWays.data(), vecWays.size() * sizeof(int64_t));
            }

            bool ReadMultipolygon(COSMTempFile& file, SMultipolygon& mp)
            {
                if(!file.ReadValue(mp.nId))
                    return false;
                int32_t nLayer = -1;
                uint32_t nTags = 0, nWays = 0;
                bool bOk = file.ReadValue(nLayer) && file.ReadString(mp.match.sKey) && file.ReadString(mp.match.sValue) && file.ReadValue(nTags);
                mp.match.nLayer = nLayer;
                mp.tags.resize(bOk ? nTags : 0);
                for(uint32_t i = 0; bOk && i < nTags; ++i)
                    bOk = file.ReadString(mp.tags[i].first) && file.ReadString(mp.tags[i].second);
                bOk = bOk && file.ReadValue(nWays);
                if(bOk)
                {
                    mp.vecWays.resize(nWays);
                    bOk = nWays == 0 || file.Read(mp.vecWays.data(), nWays * sizeof(int64_t));
                }
                if(!bOk)
                    throw CommonLib::CExcBase("The multipolygon file {0} is truncated", file.Path());
                return true;
            }

            // the state of the conversion shared by the passes
            class CRun
            {
            public:
                CRun(const COSMSchema& schema, const SOSMConvertSettings& settings, const COSMProjection& projection,
                     IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) :
                        schema(schema), settings(settings), projection(projection), ptrProgress(ptrProgress), ptrCancel(ptrCancel),
                        vecLayers(schema.Layers().size())
                {}

                // every ProgressStep objects: the cancel is checked, the progress is sent once per second
                bool Step()
                {
                    if(++nObjects % ProgressStep)
                        return true;
                    if(throttle.IsTime())
                        Send();
                    return !ptrCancel.get() || ptrCancel->Continue();
                }

                // a block of the file is processed (.pbf); sent once per second and at the end of the pass
                bool ReadProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
                {
                    progress.nBytesRead = nBytesRead;
                    progress.nBytesTotal = nBytesTotal;
                    if(nBytesRead >= nBytesTotal)
                    {
                        throttle.Reset();
                        Send();
                    }
                    else if(throttle.IsTime())
                        Send();
                    return !ptrCancel.get() || ptrCancel->Continue();
                }

                void Send()
                {
                    progress.nFeatures = nFeatures;
                    if(bStageStarted)
                    {
                        progress.dStageSeconds = StageSeconds();
                        progress.vecItems = BuildItems();
                    }
                    if(ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }

                // ---- the progress details: SOSMProgress::vecItems of the current stage, vecStages of the finished ones

                double StageSeconds() const
                {
                    return std::chrono::duration<double>(std::chrono::steady_clock::now() - stageStart).count();
                }

                template<class F>
                void ForEachWriter(F func) const
                {
                    for(const SLayerRun& layer : vecLayers)
                    {
                        if(layer.ptrWriter.get())
                            func(layer.ptrWriter->Writer().get());
                    }
                    for(const STableRun* pTable : {&routes, &restrictions, &tagsTable})
                    {
                        if(pTable->ptrWriter.get())
                            func(pTable->ptrWriter.get());
                        if(pTable->ptrMembers.get())
                            func(pTable->ptrMembers.get());
                    }
                }

                // rows / insert seconds of the writer in the current stage
                std::pair<uint64_t, double> WriterDelta(const COSMTableWriter* pWriter) const
                {
                    if(!pWriter)
                        return std::make_pair((uint64_t)0, 0.);
                    std::pair<uint64_t, double> start(0, 0.);
                    auto it = writerStart.find(pWriter);
                    if(it != writerStart.end())
                        start = it->second;
                    return std::make_pair(pWriter->GetRowCount() - start.first, pWriter->GetInsertSeconds() - start.second);
                }

                void BeginStage(eOSMStage stage, const std::string& sMessage)
                {
                    if(bStageStarted && stage == curStage)
                        return;
                    EndStage();
                    bStageStarted = true;
                    curStage = stage;
                    sStageMessage = sMessage;
                    stageStart = std::chrono::steady_clock::now();
                    reader = SOSMReaderStats();
                    writerStart.clear();
                    ForEachWriter([this](const COSMTableWriter* pWriter)
                                  { writerStart[pWriter] = std::make_pair(pWriter->GetRowCount(), pWriter->GetInsertSeconds()); });
                    storeStart = pNodes ? pNodes->GetStats() : SOSMStoreStats();
                }

                void EndStage()
                {
                    if(!bStageStarted)
                        return;
                    SOSMStageSummary summary;
                    summary.stage = curStage;
                    summary.sMessage = sStageMessage;
                    summary.dSeconds = StageSeconds();
                    summary.vecItems = BuildItems();
                    progress.vecStages.push_back(summary);
                    bStageStarted = false;
                }

                // the end of the conversion: the last stage is summarized
                void Done()
                {
                    progress.nStepDone = progress.nStepTotal;
                    EndStage();
                    progress.vecItems.clear();
                    progress.dStageSeconds = 0.;
                    progress.stage = OSMStageFinish;
                    progress.sMessage = "Done";
                    throttle.Reset();
                    Send();
                }

                static void AddItem(std::vector<SOSMProgressItem>& items, const std::string& sName, const std::string& sInfo,
                                    uint64_t nCount, uint64_t nBytes, double dSeconds, bool bThreads = false)
                {
                    SOSMProgressItem item;
                    item.sName = sName;
                    item.sInfo = sInfo;
                    item.nCount = nCount;
                    item.nBytes = nBytes;
                    item.dSeconds = dSeconds;
                    item.bThreads = bThreads;
                    items.push_back(item);
                }

                // the feature tables of the layers of a geometry type: rows and insert time in the stage
                void AddLayerWriters(std::vector<SOSMProgressItem>& items, const char* pszName, eOSMGeometryType type) const
                {
                    std::string sTables;
                    uint64_t nRows = 0;
                    double dSeconds = 0.;
                    for(size_t i = 0; i < vecLayers.size(); ++i)
                    {
                        const SLayerRun& layer = vecLayers[i];
                        if(!layer.ptrWriter.get() || schema.Layers()[i].geometryType != type)
                            continue;
                        std::pair<uint64_t, double> delta = WriterDelta(layer.ptrWriter->Writer().get());
                        nRows += delta.first;
                        dSeconds += delta.second;
                        sTables += (sTables.empty() ? "" : ", ") + layer.ptrLayer->GetTableName();
                    }
                    if(!sTables.empty())
                        AddItem(items, pszName, "rows -> " + sTables + " (insert: compression, SQLite)", nRows, 0, dSeconds);
                }

                void AddTableWriter(std::vector<SOSMProgressItem>& items, const char* pszName, const STableRun& table) const
                {
                    if(!table.ptrWriter.get())
                        return;
                    std::pair<uint64_t, double> delta = WriterDelta(table.ptrWriter.get());
                    std::string sTables = table.ptrWriter->GetTableName();
                    if(table.ptrMembers.get())
                    {
                        std::pair<uint64_t, double> members = WriterDelta(table.ptrMembers.get());
                        delta.first += members.first;
                        delta.second += members.second;
                        sTables += ", " + table.ptrMembers->GetTableName();
                    }
                    AddItem(items, pszName, "rows -> " + sTables + " (SQLite)", delta.first, 0, delta.second);
                }

                std::vector<SOSMProgressItem> BuildItems() const
                {
                    std::vector<SOSMProgressItem> items;
                    switch(curStage)
                    {
                        case OSMStageNodes:
                        {
                            AddReaderItems(items, reader, sPath);
                            SOSMStoreStats store = pNodes ? pNodes->GetStats() : SOSMStoreStats();
                            std::string sInfo = bStoreNodes ? "coordinates -> " + store.sType + " node store " + (store.sPath.empty() ? std::string("(in memory)") : store.sPath)
                                                            : std::string("the coordinates aren't stored (no line / polygon layers)");
                            AddItem(items, "Nodes", sInfo + " (written by a background thread)", progress.nNodes,
                                    store.nBytesWritten - storeStart.nBytesWritten, store.dIOSeconds - storeStart.dIOSeconds);
                            if(store.dWaitSeconds > 0)
                                AddItem(items, "Waiting for the node writer", "the reading thread waits: the disk writes slower than the file is read",
                                        0, 0, store.dWaitSeconds - storeStart.dWaitSeconds);
                            AddLayerWriters(items, "Point features", OSMGeometryPoint);
                            std::string sRelations = std::to_string(nMultipolygons) + " multipolygons of the polygon layers";
                            if(ptrMultipolygons.get())
                                sRelations += " -> " + (ptrMultipolygons->Path().empty() ? std::string("(buffer)") : ptrMultipolygons->Path());
                            AddItem(items, "Relations", sRelations, progress.nRelations, ptrMultipolygons.get() ? ptrMultipolygons->Size() : 0,
                                    ptrMultipolygons.get() ? ptrMultipolygons->WriteSeconds() : 0.);
                            AddTableWriter(items, "Routes", routes);
                            AddTableWriter(items, "Turn restrictions", restrictions);
                            AddTableWriter(items, "Tags table", tagsTable);
                            break;
                        }
                        case OSMStageSortNodes:
                        {
                            SOSMStoreStats store = pNodes ? pNodes->GetStats() : SOSMStoreStats();
                            AddItem(items, "Node store", store.sType + " " + (store.sPath.empty() ? std::string("(in memory)") : store.sPath) +
                                    " (sorting / merging the runs, mapping the file)", progress.nNodes,
                                    store.nBytesWritten - storeStart.nBytesWritten, store.dIOSeconds - storeStart.dIOSeconds);
                            break;
                        }
                        case OSMStageWays:
                        {
                            AddReaderItems(items, reader, sPath);
                            AddItem(items, "Ways", "copied into batches for the geometry threads", progress.nWays, 0, 0.);
                            AddItem(items, "Geometry", std::to_string(nWorkerThreads) + " threads: node lookups (count), compression; CPU time of all of them; missing nodes " +
                                    std::to_string(nMissingNodes), nNodeLookups.load(), 0, nWorkerNs.load() / 1e9, true);
                            {
                                std::lock_guard<std::mutex> lock(lookupMutex);
                                AddItem(items, "Node lookups", "unique ids of the batches (count), sorted; reads of the node store: " +
                                        std::to_string(lookupStats.nReads) + ", cache hits " + std::to_string(lookupStats.nCacheHits) +
                                        "; time of all the threads", nUniqueIds, lookupStats.nBytesRead, dLookupSeconds, true);
                                AddItem(items, "Node store reads", "the time of the reads in the lookups (all the threads)", lookupStats.nReads,
                                        lookupStats.nBytesRead, lookupStats.dReadSeconds, true);
                            }
                            AddItem(items, "Waiting for the geometry threads", "the reading thread waits for a batch", 0, 0, dWaitWorkers);
                            AddLayerWriters(items, "Line features", OSMGeometryLine);
                            AddLayerWriters(items, "Polygon features", OSMGeometryPolygon);
                            if(ptrWays.get())
                                AddItem(items, "Multipolygon member ways", "points -> " + (ptrWays->PointsPath().empty() ? std::string("(buffer)") : ptrWays->PointsPath()),
                                        ptrWays->Count(), ptrWays->BytesWritten(), ptrWays->WriteSeconds());
                            AddTableWriter(items, "Tags table", tagsTable);
                            break;
                        }
                        case OSMStageMultipolygons:
                        {
                            AddItem(items, "Multipolygons", "read from " + (ptrMultipolygons.get() ? ptrMultipolygons->Path() : std::string()), progress.nStepDone, 0, 0.);
                            AddItem(items, "Member way geometry", "read from " + (ptrWays.get() ? ptrWays->PointsPath() : std::string()), nWayGets, 0, dWayGetSeconds);
                            AddItem(items, "Building polygons", "rings assembled, failed: " + std::to_string(nPolygonsFailed), nPolygonsBuilt, 0, dBuildSeconds);
                            AddLayerWriters(items, "Polygon features", OSMGeometryPolygon);
                            break;
                        }
                        default:
                            items = finishItems;
                            break;
                    }
                    return items;
                }

                // the start of a pass over the file
                void StartPass(int nPass, eOSMStage stage, const std::string& sMessage)
                {
                    progress.nPass = nPass;
                    progress.nBytesRead = 0;
                    progress.nBytesTotal = 0;
                    Report(stage, sMessage);
                }

                // a stage after the reading with nTotal steps
                void StartSteps(eOSMStage stage, const std::string& sMessage, uint64_t nTotal)
                {
                    progress.nPass = 0;
                    progress.nStepDone = 0;
                    progress.nStepTotal = nTotal;
                    Report(stage, sMessage);
                }

                void Report(eOSMStage stage, const std::string& sMessage)
                {
                    BeginStage(stage, sMessage);
                    progress.stage = stage;
                    progress.sMessage = sMessage;
                    throttle.Reset();
                    Send();
                }

                void CheckCancel() const
                {
                    if(ptrCancel.get() && !ptrCancel->Continue())
                        throw CommonLib::CExcBase("OSM conversion is canceled");
                }

                // the object belongs to a converted dataset: its tags are written to the tags table
                bool IsSelectedLayer(const SOSMMatch& match) const
                {
                    return LayerWriter(match) != nullptr;
                }

                static bool IsSelectedTable(const STableRun& table, const char* pszKey, const char* pszValue)
                {
                    return pszValue && table.ptrWriter.get() && table.ptrTable->IsValueEnabled(pszKey, pszValue);
                }

                // the feature goes to a table of this run
                CFeatureWriter* LayerWriter(const SOSMMatch& match) const
                {
                    if(!match.IsValid())
                        return nullptr;
                    const SLayerRun& layer = vecLayers[match.nLayer];
                    if(!layer.ptrWriter.get() || !layer.ptrLayer->IsValueEnabled(match.sKey, match.sValue))
                        return nullptr;
                    return layer.ptrWriter.get();
                }

                void WriteTags(char cType, int64_t nId, const COSMTags& tags, const readosm_tag* pTags)
                {
                    if(!tagsTable.ptrWriter.get())
                        return;
                    for(int i = 0; i < tags.Count(); ++i)
                    {
                        const char* pszKey = pTags[i].key ? pTags[i].key : "";
                        if(!tagsTable.ptrTable->IsValueEnabled("key", pszKey))
                            continue;
                        COSMTableWriter& writer = *tagsTable.ptrWriter;
                        writer.SetText(0, std::string(1, cType));
                        writer.SetInt64(1, nId);
                        writer.SetText(2, pszKey);
                        writer.SetText(3, pTags[i].value ? pTags[i].value : "");
                        writer.Insert();
                        ++nFeatures;
                    }
                }

                bool Project(int64_t nRef, CommonLib::GisXYPoint& pt)
                {
                    double dLon, dLat;
                    if(!pNodes->Get(nRef, dLon, dLat))
                    {
                        ++nMissingNodes;
                        return false;
                    }
                    projection.Project(dLon, dLat, pt.x, pt.y);
                    return true;
                }

            public:
                const COSMSchema&           schema;
                const SOSMConvertSettings&  settings;
                const COSMProjection&       projection;
                IProgressUpdaterPtr         ptrProgress;
                Display::ITrackCancelPtr    ptrCancel;
                SOSMProgress                progress;
                uint64_t                    nObjects = 0;
                uint64_t                    nFeatures = 0;
                uint64_t                    nMissingNodes = 0;

                std::vector<SLayerRun>      vecLayers;     // by the schema index
                STableRun                   routes;
                STableRun                   restrictions;
                STableRun                   tagsTable;
                bool                        bLines = false;      // line / polygon layers are converted: the nodes are needed
                bool                        bPolygons = false;
                bool                        bPoints = false;

                IOSMNodeStore*              pNodes = nullptr;
                bool                        bStoreNodes = false;
                std::unique_ptr<COSMTempFile> ptrMultipolygons;   // see WriteMultipolygon
                uint64_t                    nMultipolygons = 0;
                COSMIdSet                   memberWays;
                std::unique_ptr<COSMWayStore> ptrWays;
                COSMProgressThrottle        throttle;

                // the progress details
                std::string                 sPath;              // the OSM file
                SOSMReaderStats             reader;             // the reading in the current pass
                int                         nWorkerThreads = 0; // the ways pass
                std::atomic<uint64_t>       nWorkerNs{0};
                std::atomic<uint64_t>       nNodeLookups{0};
                double                      dWaitWorkers = 0.;
                mutable std::mutex          lookupMutex;        // the node lookups of the workers
                SOSMLookupStats             lookupStats;
                uint64_t                    nUniqueIds = 0;
                double                      dLookupSeconds = 0.;

                void AddLookupStats(const SOSMLookupStats& stats, uint64_t nUnique, double dSeconds)
                {
                    std::lock_guard<std::mutex> lock(lookupMutex);
                    lookupStats.Add(stats);
                    nUniqueIds += nUnique;
                    dLookupSeconds += dSeconds;
                }
                uint64_t                    nWayGets = 0;       // the multipolygons
                double                      dWayGetSeconds = 0.;
                uint64_t                    nPolygonsBuilt = 0;
                uint64_t                    nPolygonsFailed = 0;
                double                      dBuildSeconds = 0.;
                std::vector<SOSMProgressItem> finishItems;      // the saving of the tables

                bool                        bStageStarted = false;
                eOSMStage                   curStage = OSMStageNodes;
                std::string                 sStageMessage;
                std::chrono::steady_clock::time_point stageStart;
                std::map<const COSMTableWriter*, std::pair<uint64_t, double> > writerStart;   // rows, insert seconds at the start of the stage
                SOSMStoreStats              storeStart;
            };

            // pass 1: nodes (coordinates, points) and relations (routes, restrictions, multipolygons)
            class CNodesRelationsHandler : public IOSMReadHandler
            {
            public:
                explicit CNodesRelationsHandler(CRun& run) : m_run(run) {}

                virtual bool OnNode(const readosm_node& node)
                {
                    ++m_run.progress.nNodes;
                    if(node.longitude == READOSM_UNDEFINED || node.latitude == READOSM_UNDEFINED)
                        return m_run.Step();

                    if(m_run.bStoreNodes)
                        m_run.pNodes->Add(node.id, node.longitude, node.latitude);

                    if(node.tag_count > 0)
                    {
                        COSMTags tags(node.tags, node.tag_count);
                        SOSMMatch match = m_run.schema.MatchNode(tags);
                        if(CFeatureWriter* pWriter = m_run.LayerWriter(match))
                        {
                            double x, y;
                            m_run.projection.Project(node.longitude, node.latitude, x, y);
                            pWriter->Write(node.id, 'n', match, tags, COSMGeometry::CreatePoint(x, y));
                            ++m_run.nFeatures;
                        }
                        if(m_run.IsSelectedLayer(match))
                            m_run.WriteTags('n', node.id, tags, node.tags);
                    }
                    return m_run.Step();
                }

                virtual bool OnWay(const readosm_way&)
                {
                    return true;
                }

                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
                {
                    return m_run.ReadProgress(nBytesRead, nBytesTotal);
                }

                virtual void OnReaderStats(const SOSMReaderStats& stats)
                {
                    m_run.reader = stats;
                }

                virtual bool OnRelation(const readosm_relation& relation)
                {
                    ++m_run.progress.nRelations;
                    if(relation.tag_count == 0)
                        return m_run.Step();

                    COSMTags tags(relation.tags, relation.tag_count);
                    bool bSelected = false;

                    SOSMMatch match = m_run.schema.MatchRelation(tags);
                    if(m_run.LayerWriter(match))
                        AddMultipolygon(relation, match);
                    bSelected = m_run.IsSelectedLayer(match);

                    const char* pszRoute = COSMSchema::MatchRoute(tags);
                    if(pszRoute)
                    {
                        if(m_run.routes.ptrWriter.get() && m_run.routes.ptrTable->IsValueEnabled("route", pszRoute))
                            WriteRoute(relation, tags, pszRoute);
                        bSelected = bSelected || CRun::IsSelectedTable(m_run.routes, "route", pszRoute);
                    }

                    const char* pszRestriction = COSMSchema::MatchRestriction(tags);
                    if(pszRestriction)
                    {
                        if(m_run.restrictions.ptrWriter.get() && m_run.restrictions.ptrTable->IsValueEnabled("restriction", pszRestriction))
                            WriteRestriction(relation, tags, pszRestriction);
                        bSelected = bSelected || CRun::IsSelectedTable(m_run.restrictions, "restriction", pszRestriction);
                    }

                    if(bSelected)
                        m_run.WriteTags('r', relation.id, tags, relation.tags);
                    return m_run.Step();
                }

            private:
                void AddMultipolygon(const readosm_relation& relation, const SOSMMatch& match)
                {
                    m_vecWays.clear();
                    for(int i = 0; i < relation.member_count; ++i)
                    {
                        if(relation.members[i].member_type == READOSM_MEMBER_WAY)
                        {
                            m_vecWays.push_back(relation.members[i].id);
                            m_run.memberWays.Add(relation.members[i].id);
                        }
                    }
                    if(m_vecWays.empty())
                        return;
                    if(!m_run.ptrMultipolygons.get())
                        m_run.ptrMultipolygons.reset(new COSMTempFile(m_run.settings.sTempDir, "_multipolygons.bin"));
                    WriteMultipolygon(*m_run.ptrMultipolygons, relation, match, m_vecWays);
                    ++m_run.nMultipolygons;
                }

                const std::vector<SOSMMember>& Members(const readosm_relation& relation)
                {
                    m_vecMembers.resize(relation.member_count > 0 ? (size_t)relation.member_count : 0);
                    for(size_t i = 0; i < m_vecMembers.size(); ++i)
                    {
                        const readosm_member& member = relation.members[i];
                        m_vecMembers[i] = SOSMMember{OSMMemberTypeChar(member.member_type), member.id, member.role};
                    }
                    return m_vecMembers;
                }

                void WriteRoute(const readosm_relation& relation, const COSMTags& tags, const char* pszRoute)
                {
                    const std::vector<SOSMMember>& members = Members(relation);
                    OSMWriteRoute(*m_run.routes.ptrWriter, *m_run.routes.ptrMembers, relation.id, tags, pszRoute, m_run.settings.sNameLanguage,
                                  members.data(), (int)members.size());
                    ++m_run.nFeatures;
                }

                void WriteRestriction(const readosm_relation& relation, const COSMTags& tags, const char* pszRestriction)
                {
                    const std::vector<SOSMMember>& members = Members(relation);
                    OSMWriteRestriction(*m_run.restrictions.ptrWriter, relation.id, tags, pszRestriction, members.data(), (int)members.size());
                    ++m_run.nFeatures;
                }

            private:
                CRun& m_run;
                std::vector<int64_t> m_vecWays;
                std::vector<SOSMMember> m_vecMembers;
            };

            // pass 2: ways (lines, polygons, members of the multipolygons) in parallel:
            //  - the reader thread copies the ways of the blocks into batches;
            //  - the workers find the layers, the node coordinates (each worker has its own node lookup) and make the
            //    geometry: compressed blobs straight from the points (no CGeoShape), for longitude / latitude with 7 decimals
            //    straight from the OSM integers of the node store;
            //  - the reader thread writes the results of the batches in the order of the file: the table rows,
            //    the multipolygon ways, the tags table. The output is the same as the one of a sequential pass.
            class CWaysHandler : public IOSMReadHandler
            {
            public:
                CWaysHandler(CRun& run, std::shared_ptr<GeometryCompression::CShapeCompressor> ptrCompressor, int nThreads, uint64_t nNodeCacheBytes) :
                        m_run(run), m_ptrCompressor(ptrCompressor), m_pCurrent(nullptr), m_bStop(false)
                {
                    if(nThreads <= 0)
                    {
                        unsigned int nCores = std::thread::hardware_concurrency();
                        nThreads = nCores > 1 ? (int)nCores - 1 : 1;
                    }
                    nThreads = (std::min)(nThreads, 32);
                    m_nMaxInFlight = (size_t)nThreads + 2;    // the batches are big (BatchRefs)
                    m_run.nWorkerThreads = nThreads;

                    // the OSM integers go to the blob as they are: 1e-7 degrees = 7 decimals of longitude / latitude
                    m_bIntegers = m_ptrCompressor.get() && !m_run.projection.IsWebMercator() && m_ptrCompressor->GetParams().nScaleExponent == 7;

                    m_run.memberWays.Seal();
                    m_vecWorkers.resize((size_t)nThreads);
                    for(size_t i = 0; i < m_vecWorkers.size(); ++i)
                    {
                        SWorker& worker = m_vecWorkers[i];
                        if(m_run.pNodes)
                            worker.ptrLookup = m_run.pNodes->CreateLookup((std::max)((uint64_t)16 * 1024 * 1024, nNodeCacheBytes / m_vecWorkers.size()));
                        if(m_ptrCompressor.get())
                            worker.ptrCompressor.reset(new GeometryCompression::CShapeCompressor(m_ptrCompressor->GetParams()));
                    }
                    try
                    {
                        for(size_t i = 0; i < m_vecWorkers.size(); ++i)
                            m_vecWorkers[i].thread = std::thread(&CWaysHandler::WorkerLoop, this, i);
                    }
                    catch (...)
                    {
                        Stop();
                        throw;
                    }
                    m_pCurrent = NewBatch();
                }

                ~CWaysHandler()
                {
                    Stop();
                }

                virtual bool OnNode(const readosm_node&) { return true; }
                virtual bool OnRelation(const readosm_relation&) { return true; }

                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
                {
                    return m_run.ReadProgress(nBytesRead, nBytesTotal);
                }

                virtual void OnReaderStats(const SOSMReaderStats& stats)
                {
                    m_run.reader = stats;
                }

                virtual bool OnWay(const readosm_way& way)
                {
                    ++m_run.progress.nWays;
                    SBatch& batch = *m_pCurrent;
                    SWay item;
                    item.nId = way.id;
                    item.nRefStart = (uint32_t)batch.refs.size();
                    item.nRefCount = (uint32_t)(way.node_ref_count > 0 ? way.node_ref_count : 0);
                    item.nTagStart = (uint32_t)(batch.tagOffsets.size() / 2);
                    item.nTagCount = (uint32_t)(way.tag_count > 0 ? way.tag_count : 0);
                    batch.refs.insert(batch.refs.end(), way.node_refs, way.node_refs + item.nRefCount);
                    for(uint32_t i = 0; i < item.nTagCount; ++i)
                    {
                        batch.tagOffsets.push_back((uint32_t)batch.text.size());
                        const char* pszKey = way.tags[i].key ? way.tags[i].key : "";
                        batch.text.append(pszKey, strlen(pszKey) + 1);
                        batch.tagOffsets.push_back((uint32_t)batch.text.size());
                        const char* pszValue = way.tags[i].value ? way.tags[i].value : "";
                        batch.text.append(pszValue, strlen(pszValue) + 1);
                    }
                    batch.ways.push_back(item);

                    if(batch.ways.size() >= BatchWays || batch.refs.size() >= BatchRefs || batch.text.size() >= BatchText)
                        Submit();
                    return m_run.Step();
                }

                // after the reading: the last batches are written
                void Finish()
                {
                    Submit();
                    while(!m_inFlight.empty())
                        WriteFront();
                }

            private:
                // big batches: the more node ids are sorted together, the more of them are read by one read of the node store
                static const size_t BatchWays = 65536;
                static const size_t BatchRefs = 2 * 1024 * 1024;
                static const size_t BatchText = 16 * 1024 * 1024;

                struct SWay
                {
                    int64_t  nId;
                    uint32_t nRefStart;
                    uint32_t nRefCount;
                    uint32_t nTagStart;   // pairs in tagOffsets
                    uint32_t nTagCount;
                };

                struct SResult
                {
                    CFeatureWriter* pLine = nullptr;
                    CFeatureWriter* pPolygon = nullptr;
                    SOSMMatch line;
                    SOSMMatch polygon;
                    CommonLib::IGeoShapePtr ptrLine;      // not compressed (no compressor / out of the integer range)
                    CommonLib::IGeoShapePtr ptrPolygon;
                    uint32_t nLineBlob = 0, nLineBlobSize = 0;
                    uint32_t nPolygonBlob = 0, nPolygonBlobSize = 0;
                    uint32_t nMemberStart = 0, nMemberCount = 0;   // the points of a multipolygon member way
                    bool     bTags = false;                        // the tags go to the tags table
                };

                struct SBatch
                {
                    std::vector<SWay>     ways;
                    std::vector<int64_t>  refs;
                    std::vector<uint32_t> tagOffsets;   // key, value: offsets of the strings in text
                    std::string           text;

                    std::vector<SResult>  results;
                    std::vector<GeometryCompression::TByte> blobs;
                    TOSMPoints            memberPoints;
                    uint64_t              nMissingNodes = 0;
                    std::exception_ptr    ptrError;
                    bool                  bDone = false;

                    void Clear()
                    {
                        ways.clear();
                        refs.clear();
                        tagOffsets.clear();
                        text.clear();
                        results.clear();
                        blobs.clear();
                        memberPoints.clear();
                        nMissingNodes = 0;
                        ptrError = nullptr;
                        bDone = false;
                    }

                    void Tags(const SWay& way, std::vector<readosm_tag>& tags) const
                    {
                        tags.clear();
                        for(uint32_t i = 0; i < way.nTagCount; ++i)
                        {
                            readosm_tag tag = {text.data() + tagOffsets[2 * (way.nTagStart + i)], text.data() + tagOffsets[2 * (way.nTagStart + i) + 1]};
                            tags.push_back(tag);
                        }
                    }
                };

                struct SWorker
                {
                    std::unique_ptr<IOSMNodeLookup> ptrLookup;
                    COSMRefResolver resolver;              // the node refs of the batch, resolved at once
                    std::vector<uint32_t> needed;          // the ways of the batch which need the geometry
                    std::unique_ptr<GeometryCompression::CShapeCompressor> ptrCompressor;
                    std::vector<readosm_tag> tags;
                    TOSMPoints points;
                    std::vector<int64_t> xs, ys;
                    std::vector<GeometryCompression::TByte> blob;
                    std::thread thread;
                };

                SBatch* NewBatch()
                {
                    if(!m_vecFree.empty())
                    {
                        SBatch* pBatch = m_vecFree.back();
                        m_vecFree.pop_back();
                        pBatch->Clear();
                        return pBatch;
                    }
                    m_vecBatches.push_back(std::unique_ptr<SBatch>(new SBatch()));
                    return m_vecBatches.back().get();
                }

                void Submit()
                {
                    if(!m_pCurrent || m_pCurrent->ways.empty())
                        return;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_queue.push_back(m_pCurrent);
                    }
                    m_cvWork.notify_one();
                    m_inFlight.push_back(m_pCurrent);
                    m_pCurrent = NewBatch();

                    // the finished batches are written, the reader waits when too many are in work
                    while(!m_inFlight.empty() && (m_inFlight.size() > m_nMaxInFlight || IsDone(m_inFlight.front())))
                        WriteFront();
                }

                bool IsDone(SBatch* pBatch)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    return pBatch->bDone;
                }

                void WriteFront()
                {
                    SBatch* pBatch = m_inFlight.front();
                    {
                        auto start = std::chrono::steady_clock::now();
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cvDone.wait(lock, [pBatch]() { return pBatch->bDone; });
                        m_run.dWaitWorkers += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                    }
                    m_inFlight.pop_front();
                    m_vecFree.push_back(pBatch);
                    if(pBatch->ptrError)
                        std::rethrow_exception(pBatch->ptrError);
                    Write(*pBatch);
                }

                void Stop()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bStop = true;
                        m_queue.clear();    // not processed: the conversion stops
                    }
                    m_cvWork.notify_all();
                    for(size_t i = 0; i < m_vecWorkers.size(); ++i)
                    {
                        if(m_vecWorkers[i].thread.joinable())
                            m_vecWorkers[i].thread.join();
                    }
                }

                void WorkerLoop(size_t nWorker)
                {
                    SWorker& worker = m_vecWorkers[nWorker];
                    for(;;)
                    {
                        SBatch* pBatch = nullptr;
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            m_cvWork.wait(lock, [this]() { return m_bStop || !m_queue.empty(); });
                            if(m_bStop)
                                return;
                            pBatch = m_queue.front();
                            m_queue.pop_front();
                        }
                        auto start = std::chrono::steady_clock::now();
                        try
                        {
                            Process(worker, *pBatch);
                        }
                        catch (...)
                        {
                            pBatch->ptrError = std::current_exception();
                        }
                        m_run.nWorkerNs.fetch_add((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count(),
                                                  std::memory_order_relaxed);
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            pBatch->bDone = true;
                        }
                        m_cvDone.notify_all();
                    }
                }

                // positive - counter clockwise; relative to the first point (the integers of the planet stay exact)
                template<class TX>
                static double SignedArea(const TX* pX, const TX* pY, size_t nCount)
                {
                    double dArea = 0.;
                    for(size_t i = 1; i + 1 < nCount; ++i)
                        dArea += double(pX[i] - pX[0]) * double(pY[i + 1] - pY[0]) - double(pX[i + 1] - pX[0]) * double(pY[i] - pY[0]);
                    return dArea / 2.;
                }

                void AppendBlob(SBatch& batch, const std::vector<GeometryCompression::TByte>& blob, uint32_t& nStart, uint32_t& nSize)
                {
                    nStart = (uint32_t)batch.blobs.size();
                    nSize = (uint32_t)blob.size();
                    batch.blobs.insert(batch.blobs.end(), blob.begin(), blob.end());
                }

                // the workers: everything but the writing
                void Process(SWorker& worker, SBatch& batch)
                {
                    uint64_t nLookups = 0;
                    struct SCount
                    {
                        std::atomic<uint64_t>& total;
                        uint64_t& n;
                        ~SCount() { total.fetch_add(n, std::memory_order_relaxed); }
                    } count{m_run.nNodeLookups, nLookups};
                    batch.results.assign(batch.ways.size(), SResult());
                    worker.needed.clear();
                    for(size_t w = 0; w < batch.ways.size(); ++w)
                    {
                        const SWay& way = batch.ways[w];
                        SResult& result = batch.results[w];
                        if(way.nRefCount < 2)
                            continue;

                        batch.Tags(way, worker.tags);
                        COSMTags tags(worker.tags.empty() ? nullptr : worker.tags.data(), (int)worker.tags.size());
                        const int64_t* pRefs = batch.refs.data() + way.nRefStart;
                        const uint32_t nRefs = way.nRefCount;

                        bool bMember = !m_run.memberWays.IsEmpty() && m_run.memberWays.ContainsSealed(way.nId);
                        bool bClosed = nRefs >= 4 && pRefs[0] == pRefs[nRefs - 1];
                        SOSMMatch line, polygon;
                        CFeatureWriter* pLine = nullptr;
                        CFeatureWriter* pPolygon = nullptr;
                        if(way.nTagCount > 0)
                        {
                            m_run.schema.MatchWay(tags, bClosed, line, polygon);
                            pLine = m_run.LayerWriter(line);
                            pPolygon = m_run.LayerWriter(polygon);
                            result.bTags = m_run.IsSelectedLayer(line) || m_run.IsSelectedLayer(polygon);
                        }
                        if(!(pLine || pPolygon || bMember) || !worker.ptrLookup)
                            continue;
                        nLookups += nRefs;
                        // the geometry after all the node refs are resolved
                        result.pLine = pLine;
                        result.line = line;
                        result.pPolygon = pPolygon;
                        result.polygon = polygon;
                        result.nMemberCount = bMember ? 1 : 0;
                        worker.needed.push_back((uint32_t)w);
                    }

                    // the node refs of the batch: sorted, every id once, the store is read in the id order
                    worker.resolver.Clear();
                    for(uint32_t w : worker.needed)
                        worker.resolver.Add(batch.refs.data(), batch.ways[w].nRefStart, batch.ways[w].nRefCount);
                    if(!worker.needed.empty())
                    {
                        auto lookupStart = std::chrono::steady_clock::now();
                        worker.resolver.Resolve(*worker.ptrLookup, batch.refs.size(),
                                                [this]() { return !m_run.ptrCancel.get() || m_run.ptrCancel->Continue(); });
                        m_run.AddLookupStats(worker.ptrLookup->TakeStats(), worker.resolver.UniqueCount(),
                                             std::chrono::duration<double>(std::chrono::steady_clock::now() - lookupStart).count());
                    }

                    for(uint32_t w : worker.needed)
                    {
                        const SWay& way = batch.ways[w];
                        SResult& result = batch.results[w];
                        CFeatureWriter* pLine = result.pLine;
                        CFeatureWriter* pPolygon = result.pPolygon;
                        SOSMMatch line = result.line, polygon = result.polygon;
                        bool bMember = result.nMemberCount != 0;
                        result.pLine = nullptr;
                        result.pPolygon = nullptr;
                        result.nMemberCount = 0;
                        const int64_t* pRefs = batch.refs.data() + way.nRefStart;
                        if(m_bIntegers)
                            ProcessIntegers(worker, batch, result, pRefs, way.nRefCount, pLine, line, pPolygon, polygon, bMember);
                        else
                            ProcessDoubles(worker, batch, result, pRefs, way.nRefCount, pLine, line, pPolygon, polygon, bMember);
                    }
                }

                void ProcessDoubles(SWorker& worker, SBatch& batch, SResult& result, const int64_t* pRefs, uint32_t nRefs,
                                    CFeatureWriter* pLine, const SOSMMatch& line, CFeatureWriter* pPolygon, const SOSMMatch& polygon, bool bMember)
                {
                    TOSMPoints& points = worker.points;
                    points.clear();
                    const size_t nBase = (size_t)(pRefs - batch.refs.data());
                    for(uint32_t i = 0; i < nRefs; ++i)
                    {
                        int32_t nLon, nLat;
                        if(!worker.resolver.Get(nBase + i, nLon, nLat))
                        {
                            ++batch.nMissingNodes;
                            continue;
                        }
                        CommonLib::GisXYPoint pt;
                        m_run.projection.Project(nLon / 1e7, nLat / 1e7, pt.x, pt.y);
                        points.push_back(pt);
                    }
                    const uint32_t nPoints = (uint32_t)points.size();

                    if(pLine && nPoints >= 2)
                    {
                        result.pLine = pLine;
                        result.line = line;
                        if(!worker.ptrCompressor || !worker.ptrCompressor->Compress(CommonLib::shape_type_polyline, points.data(), nPoints, &nPoints, 1, worker.blob))
                            result.ptrLine = COSMGeometry::CreatePolyline(std::vector<TOSMPoints>(1, points));
                        else
                            AppendBlob(batch, worker.blob, result.nLineBlob, result.nLineBlobSize);
                    }

                    // the ring is closed if the nodes are (a missing node of an extract can make it open)
                    bool bRing = nPoints >= 4 && points.front().x == points.back().x && points.front().y == points.back().y;
                    if(pPolygon && bRing)
                    {
                        result.pPolygon = pPolygon;
                        result.polygon = polygon;
                        bool bCompressed = false;
                        if(worker.ptrCompressor)
                        {
                            // outer ring: clockwise (y goes up), as COSMGeometry::CreatePolygon
                            TOSMPoints ring(points);
                            double dArea = 0.;
                            for(uint32_t i = 1; i + 1 < nPoints; ++i)
                                dArea += (ring[i].x - ring[0].x) * (ring[i + 1].y - ring[0].y) - (ring[i + 1].x - ring[0].x) * (ring[i].y - ring[0].y);
                            if(dArea > 0.)
                                std::reverse(ring.begin(), ring.end());
                            bCompressed = worker.ptrCompressor->Compress(CommonLib::shape_type_polygon, ring.data(), nPoints, &nPoints, 1, worker.blob);
                            if(bCompressed)
                                AppendBlob(batch, worker.blob, result.nPolygonBlob, result.nPolygonBlobSize);
                        }
                        if(!bCompressed)
                            result.ptrPolygon = COSMGeometry::CreatePolygon(std::vector<TOSMPoints>(1, points), std::vector<bool>(1, true));
                    }

                    if(bMember && nPoints >= 2)
                    {
                        result.nMemberStart = (uint32_t)batch.memberPoints.size();
                        result.nMemberCount = nPoints;
                        batch.memberPoints.insert(batch.memberPoints.end(), points.begin(), points.end());
                    }
                }

                // longitude / latitude, 7 decimals: the integers of the node store are the quantized coordinates
                void ProcessIntegers(SWorker& worker, SBatch& batch, SResult& result, const int64_t* pRefs, uint32_t nRefs,
                                     CFeatureWriter* pLine, const SOSMMatch& line, CFeatureWriter* pPolygon, const SOSMMatch& polygon, bool bMember)
                {
                    std::vector<int64_t>& xs = worker.xs;
                    std::vector<int64_t>& ys = worker.ys;
                    xs.clear();
                    ys.clear();
                    const size_t nBase = (size_t)(pRefs - batch.refs.data());
                    for(uint32_t i = 0; i < nRefs; ++i)
                    {
                        int32_t nLon, nLat;
                        if(!worker.resolver.Get(nBase + i, nLon, nLat))
                        {
                            ++batch.nMissingNodes;
                            continue;
                        }
                        xs.push_back(nLon);
                        ys.push_back(nLat);
                    }
                    const uint32_t nPoints = (uint32_t)xs.size();

                    if(pLine && nPoints >= 2)
                    {
                        result.pLine = pLine;
                        result.line = line;
                        worker.ptrCompressor->CompressQuantized(CommonLib::shape_type_polyline, xs.data(), ys.data(), nPoints, &nPoints, 1, worker.blob);
                        AppendBlob(batch, worker.blob, result.nLineBlob, result.nLineBlobSize);
                    }

                    bool bRing = nPoints >= 4 && xs.front() == xs.back() && ys.front() == ys.back();
                    if(pPolygon && bRing)
                    {
                        result.pPolygon = pPolygon;
                        result.polygon = polygon;
                        if(SignedArea(xs.data(), ys.data(), nPoints) > 0.)
                        {
                            std::vector<int64_t> rx(xs.rbegin(), xs.rend()), ry(ys.rbegin(), ys.rend());
                            worker.ptrCompressor->CompressQuantized(CommonLib::shape_type_polygon, rx.data(), ry.data(), nPoints, &nPoints, 1, worker.blob);
                        }
                        else
                            worker.ptrCompressor->CompressQuantized(CommonLib::shape_type_polygon, xs.data(), ys.data(), nPoints, &nPoints, 1, worker.blob);
                        AppendBlob(batch, worker.blob, result.nPolygonBlob, result.nPolygonBlobSize);
                    }

                    if(bMember && nPoints >= 2)
                    {
                        result.nMemberStart = (uint32_t)batch.memberPoints.size();
                        result.nMemberCount = nPoints;
                        for(uint32_t i = 0; i < nPoints; ++i)
                        {
                            CommonLib::GisXYPoint pt;
                            pt.x = xs[i] / 1e7;
                            pt.y = ys[i] / 1e7;
                            batch.memberPoints.push_back(pt);
                        }
                    }
                }

                // the reader thread, in the order of the file
                void Write(SBatch& batch)
                {
                    m_run.nMissingNodes += batch.nMissingNodes;
                    for(size_t w = 0; w < batch.ways.size(); ++w)
                    {
                        const SWay& way = batch.ways[w];
                        SResult& result = batch.results[w];
                        if(!result.pLine && !result.pPolygon && !result.nMemberCount && !result.bTags)
                            continue;

                        batch.Tags(way, m_tags);
                        COSMTags tags(m_tags.empty() ? nullptr : m_tags.data(), (int)m_tags.size());
                        if(result.pLine)
                        {
                            if(result.ptrLine.get())
                                result.pLine->Write(way.nId, 'w', result.line, tags, result.ptrLine);
                            else
                                result.pLine->WriteBlob(way.nId, 'w', result.line, tags, batch.blobs.data() + result.nLineBlob, result.nLineBlobSize);
                            ++m_run.nFeatures;
                        }
                        if(result.pPolygon)
                        {
                            if(result.ptrPolygon.get())
                                result.pPolygon->Write(way.nId, 'w', result.polygon, tags, result.ptrPolygon);
                            else
                                result.pPolygon->WriteBlob(way.nId, 'w', result.polygon, tags, batch.blobs.data() + result.nPolygonBlob, result.nPolygonBlobSize);
                            ++m_run.nFeatures;
                        }
                        if(result.nMemberCount)
                            m_run.ptrWays->Add(way.nId, batch.memberPoints.data() + result.nMemberStart, result.nMemberCount);
                        if(result.bTags)
                            m_run.WriteTags('w', way.nId, tags, m_tags.data());
                    }
                }

            private:
                CRun& m_run;
                std::shared_ptr<GeometryCompression::CShapeCompressor> m_ptrCompressor;
                bool m_bIntegers = false;
                size_t m_nMaxInFlight = 4;

                std::vector<std::unique_ptr<SBatch> > m_vecBatches;   // all the batches, they live longer than the workers
                std::vector<SBatch*> m_vecFree;
                std::deque<SBatch*>  m_inFlight;     // submitted, in the order of the file
                SBatch*              m_pCurrent;
                std::vector<readosm_tag> m_tags;

                std::mutex m_mutex;
                std::condition_variable m_cvWork;
                std::condition_variable m_cvDone;
                std::deque<SBatch*> m_queue;          // waiting for a worker
                bool m_bStop;
                std::vector<SWorker> m_vecWorkers;    // the last member: the threads are stopped first
            };

            void Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects, int nThreads)
            {
                if(!COSMReader::Read(sPath, handler, nObjects, nThreads))
                    throw CommonLib::CExcBase("OSM conversion is canceled");
            }
        }

        COSMConvertSession::COSMConvertSession(const COSMSchema& schema, const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap,
                                               const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                                               GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace) :
                m_schema(schema), m_settings(settings), m_ptrOSMMap(ptrOSMMap), m_excludeList(excludeList), m_ptrMap(ptrMap), m_ptrWorkspace(ptrWorkspace),
                m_projection(settings.bWebMercator)
        {
            if(!m_ptrOSMMap.get())
                throw CommonLib::CExcBase("OSM conversion: the OSM map is null");
            if(!m_ptrWorkspace.get())
                throw CommonLib::CExcBase("OSM conversion: the workspace is null");
        }

        COSMConvertSession::~COSMConvertSession()
        {

        }

        bool COSMConvertSession::IsConverted(const IOSMDataset& dataset) const
        {
            // a map made by Analyze knows the empty datasets, a map without counts (CreateMap) converts everything
            if(m_excludeList.IsLayerExcluded(dataset.GetName()))
                return false;
            return !m_ptrOSMMap->IsScanned() || dataset.GetFeatureCount() > 0;
        }

        void COSMConvertSession::Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            // the layers without features aren't added to the map when the counts aren't known (their tables are created)
            const bool bSkipEmptyLayers = !m_ptrOSMMap->IsScanned();
            GeoDatabase::ITransactionPtr ptrTransaction;
            bool bBulkMode = false;
            std::unique_ptr<IOSMNodeStore> ptrNodes;   // the node coordinates (temporary files), deleted at the end
            try
            {
                CRun run(m_schema, m_settings, m_projection, ptrProgress, ptrCancel);
                Geometry::ISpatialReferencePtr ptrSpatRef = m_projection.CreateSpatialReference();
                std::string sNameKey = m_settings.sNameLanguage.empty() ? std::string() : "name:" + m_settings.sNameLanguage;

                // one compressor for all the layers (it keeps its buffers)
                std::shared_ptr<GeometryCompression::CShapeCompressor> ptrCompressor;
                if(m_settings.compression.bEnabled)
                {
                    // the precision by the settings: auto (the units), manual, maximum (see SOSMCompressSettings)
                    ptrCompressor = std::make_shared<GeometryCompression::CShapeCompressor>(COSMConvertor::CompressParams(m_settings, m_ptrOSMMap));
                }

                // the layers of the schema which are in the OSM map; the tables
                for(size_t i = 0; i < m_schema.Layers().size(); ++i)
                    run.vecLayers[i].ptrLayer = m_ptrOSMMap->FindLayer(m_schema.Layers()[i].sName);
                for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
                {
                    IOSMTablePtr ptrTable = m_ptrOSMMap->GetTable(i);
                    STableRun& table = ptrTable->GetTableType() == OSMTableRoutes ? run.routes :
                                       ptrTable->GetTableType() == OSMTableRestrictions ? run.restrictions : run.tagsTable;
                    table.ptrTable = ptrTable;
                }

                bool bAny = false;
                for(size_t i = 0; i < run.vecLayers.size() && !bAny; ++i)
                    bAny = run.vecLayers[i].ptrLayer.get() && IsConverted(*run.vecLayers[i].ptrLayer);
                for(int i = 0; i < m_ptrOSMMap->GetTableCount() && !bAny; ++i)
                    bAny = IsConverted(*m_ptrOSMMap->GetTable(i));
                if(!bAny)
                    return;

                // the database settings for the speed while the transaction lasts
                m_ptrWorkspace->SetBulkLoadMode(true);
                bBulkMode = true;
                ptrTransaction = m_ptrWorkspace->StartTransaction(GeoDatabase::ttModify);

                // the writers of the converted datasets
                std::vector<IOSMLayerPtr> vecRunLayers;
                for(size_t i = 0; i < run.vecLayers.size(); ++i)
                {
                    SLayerRun& layer = run.vecLayers[i];
                    if(!layer.ptrLayer.get() || !IsConverted(*layer.ptrLayer))
                        continue;
                    const SOSMLayerDef& def = m_schema.Layers()[i];
                    layer.ptrWriter = std::make_shared<CFeatureWriter>(def, *layer.ptrLayer, m_excludeList, sNameKey, m_ptrWorkspace, ptrTransaction,
                                                                       ptrSpatRef, ptrCompressor, m_settings.sTempDir);
                    run.bLines = run.bLines || def.geometryType != OSMGeometryPoint;
                    run.bPolygons = run.bPolygons || def.geometryType == OSMGeometryPolygon;
                    run.bPoints = run.bPoints || def.geometryType == OSMGeometryPoint;
                    vecRunLayers.push_back(layer.ptrLayer);
                }
                for(STableRun* pTable : {&run.routes, &run.restrictions, &run.tagsTable})
                {
                    if(!pTable->ptrTable.get() || !IsConverted(*pTable->ptrTable))
                        continue;
                    pTable->ptrWriter = OSMCreateTableWriter(m_ptrWorkspace, ptrTransaction, *pTable->ptrTable, m_excludeList);
                    if(pTable == &run.routes)
                    {
                        typedef COSMTableWriter::SField F;
                        pTable->ptrMembers = std::make_shared<COSMTableWriter>(m_ptrWorkspace, ptrTransaction, pTable->ptrTable->GetTableName() + "_members", std::vector<F>{
                                {"route_id", GeoDatabase::dtInteger64}, {"seq", GeoDatabase::dtInteger64}, {"member_type", GeoDatabase::dtString},
                                {"member_id", GeoDatabase::dtInteger64}, {"role", GeoDatabase::dtString}});
                    }
                }

                // the tags table also needs the ways when it is converted
                bool bWays = run.bLines || run.tagsTable.ptrWriter.get();

                std::error_code ec;
                // the folder of the temporary files (node coordinates, multipolygons, spatial index runs) is created
                if(!m_settings.sTempDir.empty())
                {
                    std::filesystem::create_directories(std::filesystem::u8path(m_settings.sTempDir), ec);
                    if(!std::filesystem::is_directory(std::filesystem::u8path(m_settings.sTempDir), ec))
                        throw CommonLib::CExcBase("The folder for the temporary files can't be created: {0}", m_settings.sTempDir);
                }
                uint64_t nFileSize = std::filesystem::file_size(std::filesystem::u8path(m_ptrOSMMap->GetPath()), ec);
                if(ec)
                    nFileSize = 0;
                run.progress.nFileSize = nFileSize;
                run.progress.nPassCount = bWays ? 2 : 1;

                // pass 1: nodes and relations
                if(run.bLines)
                {
                    // the flat store for a continent / the planet, the sorted one for an extract (see eOSMNodeStore)
                    ptrNodes = IOSMNodeStore::Create(m_settings.nodeStore, m_ptrOSMMap->GetPath(), nFileSize, m_settings.sTempDir,
                                                     (uint64_t)m_settings.nNodeCacheMB * 1024 * 1024, (uint64_t)m_settings.nNodeWriteBufferMB * 1024 * 1024);
                    ptrNodes->SetContinue([&run]() { return !run.ptrCancel.get() || run.ptrCancel->Continue(); });
                    run.bStoreNodes = true;
                }
                run.pNodes = ptrNodes.get();
                run.sPath = m_ptrOSMMap->GetPath();

                run.StartPass(1, OSMStageNodes, "Reading nodes and relations");
                {
                    CNodesRelationsHandler handler(run);
                    // the nodes are skipped when only the relation tables are converted
                    bool bNodes = run.bStoreNodes || run.bPoints || run.tagsTable.ptrWriter.get();
                    Read(m_ptrOSMMap->GetPath(), handler, (bNodes ? COSMReader::ReadNodes : 0) | COSMReader::ReadRelations, m_settings.nThreads);
                }

                if(run.bStoreNodes)
                {
                    run.progress.nPass = 0;
                    run.Report(OSMStageSortNodes, "Indexing node coordinates");
                    ptrNodes->Finish([&run](uint64_t nNodes)
                                       {
                                           run.progress.nNodes = nNodes;
                                           if(run.throttle.IsTime())
                                               run.Send();
                                           return !run.ptrCancel.get() || run.ptrCancel->Continue();
                                       });
                    run.CheckCancel();
                }

                // pass 2: ways
                if(bWays)
                {
                    if(!run.memberWays.IsEmpty())
                    {
                        // the memory for the cached index blocks of the member ways
                        uint64_t nWayMemory = (std::max)((uint64_t)16, (uint64_t)m_settings.nNodeCacheMB / 8) * 1024 * 1024;
                        run.ptrWays.reset(new COSMWayStore(m_settings.sTempDir, nWayMemory));
                    }

                    run.StartPass(2, OSMStageWays, "Reading ways");
                    CWaysHandler handler(run, ptrCompressor, m_settings.nThreads, (uint64_t)m_settings.nNodeCacheMB * 1024 * 1024);
                    Read(m_ptrOSMMap->GetPath(), handler, COSMReader::ReadWays, m_settings.nThreads);
                    handler.Finish();
                    if(run.ptrWays.get())
                        run.ptrWays->Finish();
                }

                // multipolygons
                if(run.nMultipolygons > 0 && run.ptrWays.get())
                {
                    run.StartSteps(OSMStageMultipolygons, "Building multipolygons", run.nMultipolygons);
                    std::vector<TOSMPoints> ways;
                    std::vector<TOSMPoints> rings;
                    std::vector<bool> outer;
                    SMultipolygon mp;
                    run.ptrMultipolygons->Rewind();
                    while(ReadMultipolygon(*run.ptrMultipolygons, mp))
                    {
                        ++run.progress.nStepDone;
                        if(mp.match.nLayer < 0 || mp.match.nLayer >= (int)run.vecLayers.size() || !run.vecLayers[mp.match.nLayer].ptrWriter.get())
                            throw CommonLib::CExcBase("The multipolygon file is damaged: relation {0}", mp.nId);
                        ways.resize(mp.vecWays.size());
                        size_t nWays = 0;
                        auto getStart = std::chrono::steady_clock::now();
                        for(size_t w = 0; w < mp.vecWays.size(); ++w)
                        {
                            if(run.ptrWays->Get(mp.vecWays[w], ways[nWays]))
                                ++nWays;
                        }
                        auto buildStart = std::chrono::steady_clock::now();
                        run.dWayGetSeconds += std::chrono::duration<double>(buildStart - getStart).count();
                        run.nWayGets += mp.vecWays.size();
                        ways.resize(nWays);

                        CommonLib::IGeoShapePtr ptrShape;
                        if(COSMGeometry::BuildMultipolygon(ways, rings, outer))
                            ptrShape = COSMGeometry::CreatePolygon(rings, outer);
                        run.dBuildSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - buildStart).count();
                        if(ptrShape.get())
                        {
                            ++run.nPolygonsBuilt;
                            CStoredTags tags(mp.tags);
                            run.vecLayers[mp.match.nLayer].ptrWriter->Write(mp.nId, 'r', mp.match, tags.Tags(), ptrShape);
                            ++run.nFeatures;
                        }
                        else
                            ++run.nPolygonsFailed;
                        if(!run.Step())
                            throw CommonLib::CExcBase("OSM conversion is canceled");
                    }
                }

                run.EndStage();   // the details of the multipolygons / ways use the temporary files
                run.ptrMultipolygons.reset();
                run.ptrWays.reset();
                run.pNodes = nullptr;
                ptrNodes.reset();   // the node coordinates aren't needed any more: the temporary files are deleted
                run.StartSteps(OSMStageFinish, "Saving tables", vecRunLayers.size());
                std::vector<GeoDatabase::ITablePtr> vecTables(vecRunLayers.size());
                std::vector<uint64_t> vecRowCounts(vecRunLayers.size());
                for(size_t i = 0; i < vecRunLayers.size(); ++i)
                {
                    SLayerRun& layer = run.vecLayers[m_schema.FindLayerIndex(vecRunLayers[i]->GetName())];
                    run.progress.nStepDone = i;
                    run.Report(OSMStageFinish, "Building the spatial index: " + vecRunLayers[i]->GetDisplayName());
                    run.CheckCancel();
                    auto start = std::chrono::steady_clock::now();
                    layer.ptrWriter->Writer()->Finish();
                    vecTables[i] = layer.ptrWriter->Writer()->GetTable();
                    vecRowCounts[i] = layer.ptrWriter->Writer()->GetRowCount();
                    CRun::AddItem(run.finishItems, "Spatial index: " + vecRunLayers[i]->GetDisplayName(), "packed from the boxes -> " + vecRunLayers[i]->GetTableName(),
                                  vecRowCounts[i], 0, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
                }
                for(STableRun* pTable : {&run.routes, &run.restrictions, &run.tagsTable})
                {
                    if(pTable->ptrWriter.get())
                        pTable->ptrWriter->Finish();
                    if(pTable->ptrMembers.get())
                        pTable->ptrMembers->Finish();
                }

                // the small scale layers read only their rows: the indexes of their query columns
                for(size_t i = 0; i < vecRunLayers.size(); ++i)
                {
                    if(COSMMapStyle::OverviewLayers(vecRunLayers[i]->GetName()).empty() || vecRowCounts[i] == 0)
                        continue;
                    run.Report(OSMStageFinish, "Indexing the small scale layers: " + vecRunLayers[i]->GetDisplayName());
                    run.CheckCancel();
                    double dSeconds = OSMCreateOverviewIndexes(ptrTransaction, *vecRunLayers[i], vecTables[i]);
                    CRun::AddItem(run.finishItems, "Index of the small scale layers: " + vecRunLayers[i]->GetDisplayName(),
                                  "the query columns of the overview layers -> " + vecRunLayers[i]->GetTableName(), 0, 0, dSeconds);
                }

                run.Report(OSMStageFinish, "Committing the transaction");
                auto commitStart = std::chrono::steady_clock::now();
                ptrTransaction->Commit();
                CRun::AddItem(run.finishItems, "Commit", "the database file", 0, 0, std::chrono::duration<double>(std::chrono::steady_clock::now() - commitStart).count());
                ptrTransaction.reset();
                bBulkMode = false;
                m_ptrWorkspace->SetBulkLoadMode(false);

                if(m_ptrMap.get() && m_settings.bAddLayersToMap)
                {
                    COSMMapLayers mapLayers(m_ptrMap, m_projection);
                    for(size_t i = 0; i < vecRunLayers.size(); ++i)
                    {
                        // Convert of a map without counts creates the tables of all the layers: the empty ones aren't shown
                        if(bSkipEmptyLayers && vecRowCounts[i] == 0)
                            continue;
                        mapLayers.AddLayer(*vecRunLayers[i], vecTables[i]);
                    }
                }

                run.Done();
            }
            catch (std::exception& exc)
            {
                if(ptrTransaction.get())
                {
                    try
                    {
                        ptrTransaction->Rollback();
                    }
                    catch (...)
                    {
                    }
                }
                if(bBulkMode)
                {
                    try
                    {
                        ptrTransaction.reset();
                        m_ptrWorkspace->SetBulkLoadMode(false);
                    }
                    catch (...)
                    {
                    }
                }
                CommonLib::CExcBase::RegenExc("Failed to convert OSM map {0}", m_ptrOSMMap->GetPath(), exc);
                throw;
            }
        }
    }
}
