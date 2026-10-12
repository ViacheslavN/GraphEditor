#include "OSMSplitter.h"
#include "OSMAnalyzer.h"
#include "OSMReader.h"
#include "OSMFlatNodeStore.h"
#include "OSMNodeStore.h"
#include "OSMMapStyle.h"
#include "OSMProgressThrottle.h"
#include "../../../../CommonLib/Serialize/SerializeXML.h"
#include "../../../CartographyLib/layers/FeatureLayer.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace GraphEngine {
    namespace Convertors {

        // ---------------- metadata

        void COSMSplitMeta::Save(const std::string& sMetaPath, const COSMMap& map, const std::string& sSourcePath, uint64_t nSourceSize,
                                 const std::string& sNodeFile)
        {
            try
            {
                CommonLib::xml::CXMLDoc xmlDoc;
                CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(xmlDoc.GetNodes());
                CommonLib::ISerializeObjPtr ptrSplit = ptrRoot->CreateChildNode("OSMSplit");
                ptrSplit->AddPropertyInt32("Version", OSMSplit::Version);
                ptrSplit->AddPropertyString("Source", sSourcePath);
                ptrSplit->AddPropertyIntU64("SourceSize", nSourceSize);
                ptrSplit->AddPropertyString("NodeFile", sNodeFile);

                map.Save(ptrSplit);

                // the symbols of the layers: feature layers without a table, they can be edited here before the export
                CommonLib::ISerializeObjPtr ptrStyles = ptrSplit->CreateChildNode("Styles");
                for(int i = 0; i < map.GetLayerCount(); ++i)
                {
                    IOSMLayerPtr ptrLayer = map.GetLayer(i);
                    Cartography::IFeatureLayerPtr ptrStyle = COSMMapStyle::CreateLayer(*ptrLayer, GeoDatabase::ITablePtr());
                    CommonLib::ISerializeObjPtr ptrNode = ptrStyles->CreateChildNode("Style");
                    ptrNode->AddPropertyString("Layer", ptrLayer->GetName());
                    ptrStyle->Save(ptrNode->CreateChildNode("FeatureLayer"));
                }
                xmlDoc.Save(sMetaPath);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save the OSM split metadata {0}", sMetaPath, exc);
                throw;
            }
        }

        void COSMSplitMeta::Load(const std::string& sMetaPath)
        {
            try
            {
                m_doc.Clear();
                m_doc.Open(sMetaPath);
                CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(m_doc.GetNodes());
                if(!ptrRoot->IsChildExists("OSMSplit"))
                    throw CommonLib::CExcBase("Not OSM split data");
                CommonLib::ISerializeObjPtr ptrSplit = ptrRoot->GetChild("OSMSplit");
                int nVersion = ptrSplit->GetPropertyInt32("Version", 0);
                if(nVersion != OSMSplit::Version)
                    throw CommonLib::CExcBase("Unsupported version {0} of the OSM split data", nVersion);
                m_sSourcePath = ptrSplit->GetPropertyString("Source", std::string());
                m_sNodeFile = ptrSplit->GetPropertyString("NodeFile", std::string());

                m_ptrMap = std::make_shared<COSMMap>();
                m_ptrMap->Load(ptrSplit);
                m_ptrMap->SetPath(sMetaPath);    // the format: split
                m_ptrStyles = ptrSplit->IsChildExists("Styles") ? ptrSplit->GetChild("Styles") : CommonLib::ISerializeObjPtr();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load the OSM split metadata {0}", sMetaPath, exc);
                throw;
            }
        }

        CommonLib::ISerializeObjPtr COSMSplitMeta::GetStyle(const std::string& sLayer) const
        {
            if(!m_ptrStyles.get())
                return CommonLib::ISerializeObjPtr();
            for(uint32_t i = 0, sz = m_ptrStyles->GetChildCnt(); i < sz; ++i)
            {
                CommonLib::ISerializeObjPtr ptrStyle = m_ptrStyles->GetChild(i);
                if(ptrStyle->GetPropertyString("Layer", std::string()) == sLayer && ptrStyle->IsChildExists("FeatureLayer"))
                    return ptrStyle->GetChild("FeatureLayer");
            }
            return CommonLib::ISerializeObjPtr();
        }

        // ---------------- splitting

        namespace
        {
            const uint64_t ProgressStep = 100000;
            typedef std::chrono::steady_clock TClock;

            double Since(TClock::time_point start)
            {
                return std::chrono::duration<double>(TClock::now() - start).count();
            }


            int32_t E7(double dValue)
            {
                return (int32_t)std::lround(dValue * 1e7);
            }

            uint64_t PhysicalMemory()
            {
#ifdef _WIN32
                MEMORYSTATUSEX status;
                status.dwLength = sizeof(status);
                if(::GlobalMemoryStatusEx(&status))
                    return status.ullTotalPhys;
                return 8ull * 1024 * 1024 * 1024;
#else
                long nPages = sysconf(_SC_PHYS_PAGES);
                long nPageSize = sysconf(_SC_PAGE_SIZE);
                return nPages > 0 && nPageSize > 0 ? (uint64_t)nPages * (uint64_t)nPageSize : 8ull * 1024 * 1024 * 1024;
#endif
            }

            std::string GB(uint64_t nBytes)
            {
                char sz[32];
                snprintf(sz, sizeof(sz), "%.1f GB", nBytes / (1024. * 1024 * 1024));
                return sz;
            }

            // the coordinates of a node ref in a batch
            struct SCoord
            {
                int32_t lon;
                int32_t lat;
            };

            // the ways of the flat node store: how the coordinate of a ref is found
            enum eWayMode
            {
                WayLookup  = 0,   // the node store lookups (the sorted store of an extract)
                WaySegment = 1,   // a pass for a part of the ids in memory: the coordinates of its refs -> the stream of the part
                WayFinal   = 2    // the last part in memory + the streams of the earlier parts -> ways.dat
            };

            // the files being written and the progress
            class CSplitRun
            {
            public:
                CSplitRun(const COSMSchema& schema, COSMMapPtr ptrMap, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) :
                        analyzer(schema, ptrMap), ptrProgress(ptrProgress), ptrCancel(ptrCancel)
                {}

                bool Step()
                {
                    if(++nObjects % ProgressStep)
                        return true;
                    if(throttle.IsTime())
                        Send();
                    return !ptrCancel.get() || ptrCancel->Continue();
                }

                void CheckCancel() const
                {
                    if(ptrCancel.get() && !ptrCancel->Continue())
                        throw CommonLib::CExcBase("OSM splitting is canceled");
                }

                void BeginStage(eOSMStage stage, int nPass, const std::string& sMessage)
                {
                    EndStage();
                    bStage = true;
                    progress.stage = stage;
                    progress.sMessage = sMessage;
                    progress.nPass = nPass;
                    progress.nBytesRead = 0;
                    progress.nBytesTotal = 0;
                    stageStart = TClock::now();
                    reader = SOSMReaderStats();
                    throttle.Reset();
                    Send();
                }

                void EndStage()
                {
                    if(!bStage)
                        return;
                    SOSMStageSummary summary;
                    summary.stage = progress.stage;
                    summary.sMessage = progress.sMessage;
                    summary.dSeconds = Since(stageStart);
                    summary.vecItems = BuildItems();
                    progress.vecStages.push_back(summary);
                    bStage = false;
                }

                void Done()
                {
                    EndStage();
                    progress.vecItems.clear();
                    progress.dStageSeconds = 0.;
                    progress.sMessage = "Done";
                    Send();
                }

                void Send()
                {
                    progress.nNodes = analyzer.NodeCount();
                    progress.nWays = (std::max)(analyzer.WayCount(), nPassWays);
                    progress.nRelations = analyzer.RelationCount();
                    if(bStage)
                    {
                        progress.dStageSeconds = Since(stageStart);
                        progress.vecItems = BuildItems();
                    }
                    if(ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }

                std::vector<SOSMProgressItem> BuildItems() const
                {
                    std::vector<SOSMProgressItem> items;
                    auto add = [&items](const std::string& sName, const std::string& sInfo, uint64_t nCount, uint64_t nBytes, double dSeconds, bool bThreads)
                    {
                        SOSMProgressItem item;
                        item.sName = sName;
                        item.sInfo = sInfo;
                        item.nCount = nCount;
                        item.nBytes = nBytes;
                        item.dSeconds = dSeconds;
                        item.bThreads = bThreads;
                        items.push_back(item);
                    };
                    if(progress.stage == OSMStageFinish)
                    {
                        add("Indexes", "sorted (id, offset) -> nodes.idx, ways.idx, relations.idx",
                            nodesIndex.Count() + waysIndex.Count() + relationsIndex.Count(), 0, dIndexSeconds, false);
                        add("Strings, metadata", "keys / roles -> strings.dat; layers, columns, symbols -> osm_split.xml", nStrings, 0, dMetaSeconds, false);
                        return items;
                    }
                    if(progress.stage == OSMStageSortNodes && ptrSegment.get())
                    {
                        add("Node coordinates in memory", "part " + std::to_string(nSegment + 1) + " of " + std::to_string(nSegments) + ": ids " +
                            std::to_string(nSegment * nSegmentRecords) + " .. " + std::to_string((nSegment + 1) * nSegmentRecords) + " (" +
                            GB(nSegmentRecords * 8) + ") read sequentially from nodes.flat", 0, ptrSegment->BytesRead() - nSegmentBytesStart,
                            ptrSegment->ReadSeconds() - dSegmentSecondsStart, false);
                        return items;
                    }
                    if(progress.stage == OSMStageWays && ptrSegment.get())
                    {
                        // a pass of an earlier part: the coordinates of its refs -> the stream
                        AddReaderItems(items, reader, sPath);
                        add("Ways", "the refs of the ids in memory (part " + std::to_string(nSegment + 1) + " of " + std::to_string(nSegments) + ")",
                            nPassWays, 0, 0., false);
                        add("Geometry", std::to_string(nWorkerThreads) + " threads: node refs (count), the coordinates from memory; time of all of them",
                            nNodeLookups.load(), 0, nWorkerNs.load() / 1e9, true);
                        add("Waiting for the geometry threads", "the reading thread waits for a batch", 0, 0, dWaitWorkers, false);
                        if(!streams.empty())
                            add("Coordinates of the part", "-> " + streams.back()->Path() + " (8 bytes per ref, in the ref order)",
                                streams.back()->Size() / 8, streams.back()->Size(), streams.back()->IOSeconds(), false);
                        return items;
                    }
                    if(progress.stage == OSMStageSortNodes)
                    {
                        SOSMStoreStats store = ptrNodes.get() ? ptrNodes->GetStats() : SOSMStoreStats();
                        add("Node coordinates", store.sType + " node store: sorting / merging, mapping for the lookups", progress.nNodes,
                            0, Since(stageStart), false);
                        return items;
                    }

                    AddReaderItems(items, reader, sPath);
                    SOSMStoreStats store = ptrNodes.get() ? ptrNodes->GetStats() : SOSMStoreStats();
                    std::string sCoords = store.sType + " node store " + (store.sPath.empty() ? std::string("(in memory)") : store.sPath);
                    if(bSortedNodes)
                        sCoords += " + " + sortedNodes.Path();
                    add("Nodes", "coordinates -> " + sCoords + ", tagged -> " + nodesFile.Path(), analyzer.NodeCount(),
                        store.nBytesWritten + nodesFile.Size() + (bSortedNodes ? sortedNodes.Size() : 0),
                        store.dIOSeconds + nodesFile.IOSeconds() + (bSortedNodes ? sortedNodes.IOSeconds() : 0.), false);
                    if(store.dWaitSeconds > 0)
                        add("Waiting for the node writer", "the reading thread waits: the disk writes slower than the file is read",
                            0, 0, store.dWaitSeconds, false);
                    if(progress.stage == OSMStageSplit)
                    {
                        add("Ways", "with the coordinates -> " + waysFile.Path(), analyzer.WayCount(), waysFile.Size(), waysFile.IOSeconds(), false);
                        if(ptrSegment.get())
                        {
                            uint64_t nStreamBytes = 0;
                            double dStreamSeconds = 0.;
                            for(const std::unique_ptr<COSMDataFile>& ptrStream : streams)
                            {
                                nStreamBytes += ptrStream->BytesRead();
                                dStreamSeconds += ptrStream->IOSeconds();
                            }
                            add("Coordinates", "part " + std::to_string(nSegments) + " in memory, the earlier parts from their streams (read)",
                                nStreamBytes / 8, nStreamBytes, dStreamSeconds, false);
                        }
                        add("Geometry", std::to_string(nWorkerThreads) + " threads: node refs (count), lookups and encoding; time of all of them; missing nodes " +
                            std::to_string(nMissingNodes.load()), nNodeLookups.load(), 0, nWorkerNs.load() / 1e9, true);
                        SOSMLookupStats lookups;
                        uint64_t nUnique = 0;
                        {
                            std::lock_guard<std::mutex> lock(lookupMutex);
                            lookups = lookupStats;
                            nUnique = nUniqueIds;
                        }
                        add("Node lookups", "unique ids of the batches (count), sorted; reads of the node store: " + std::to_string(lookups.nReads) +
                            ", cache hits " + std::to_string(lookups.nCacheHits) + "; time of all the threads", nUnique, lookups.nBytesRead,
                            nLookupNs.load() / 1e9, true);
                        add("Node store reads", "the time of the reads in the lookups (all the threads)", lookups.nReads, lookups.nBytesRead,
                            lookups.dReadSeconds, true);
                        add("Waiting for the geometry threads", "the reading thread waits for a batch", 0, 0, dWaitWorkers, false);
                        add("Relations", "-> " + relationsFile.Path(), analyzer.RelationCount(), relationsFile.Size(), relationsFile.IOSeconds(), false);
                        add("Index entries", "(id, offset) -> .idx", nodesIndex.Count() + waysIndex.Count() + relationsIndex.Count(), 0,
                            nodesIndex.IOSeconds() + waysIndex.IOSeconds() + relationsIndex.IOSeconds(), false);
                    }
                    return items;
                }

            public:
                COSMAnalyzer              analyzer;
                IProgressUpdaterPtr       ptrProgress;
                Display::ITrackCancelPtr  ptrCancel;
                SOSMProgress              progress;
                COSMProgressThrottle      throttle;
                uint64_t                  nObjects = 0;
                bool                      bStage = false;
                TClock::time_point        stageStart;
                SOSMReaderStats           reader;
                std::string               sPath;

                COSMStringTable           strings;
                std::unique_ptr<IOSMNodeStore> ptrNodes;   // the coordinates for the ways: flat (planet) / sorted (extracts)
                COSMDataFile              sortedNodes;    // nodes.sorted is kept: {id, lon, lat} in the file order
                bool                      bSortedNodes = false;
                uint64_t                  nNodeCacheBytes = 0;
                COSMDataFile              nodesFile, waysFile, relationsFile;
                COSMIdIndexWriter         nodesIndex, waysIndex, relationsIndex;
                COSMRecordBuffer          record;

                int                       nWorkerThreads = 0;
                std::atomic<uint64_t>     nWorkerNs{0};
                std::atomic<uint64_t>     nNodeLookups{0};
                std::atomic<uint64_t>     nMissingNodes{0};
                std::atomic<uint64_t>     nLookupNs{0};       // the workers resolving the node refs (sorting, reading the store)
                mutable std::mutex        lookupMutex;
                SOSMLookupStats           lookupStats;        // the reads of the node store by the workers
                uint64_t                  nUniqueIds = 0;

                void AddLookupStats(const SOSMLookupStats& stats, uint64_t nUnique)
                {
                    std::lock_guard<std::mutex> lock(lookupMutex);
                    lookupStats.Add(stats);
                    nUniqueIds += nUnique;
                }
                // the flat store: the coordinates of the ways from the parts of nodes.flat in memory (no random reads)
                const COSMFlatNodeStore*  pFlat = nullptr;
                std::unique_ptr<COSMNodeSegment> ptrSegment;
                uint64_t                  nSegmentRecords = 0;
                uint64_t                  nSegments = 0;
                uint64_t                  nSegment = 0;           // the part in memory
                std::vector<std::unique_ptr<COSMDataFile> > streams;   // the coordinates of the refs of the earlier parts
                uint64_t                  nPassWays = 0;
                uint64_t                  nSegmentBytesStart = 0;
                double                    dSegmentSecondsStart = 0.;

                double                    dWaitWorkers = 0.;
                double                    dIndexSeconds = 0.;
                double                    dMetaSeconds = 0.;
                uint64_t                  nStrings = 0;
            };

            // The ways: the reading thread copies them into batches, the workers find the coordinates of the nodes and encode
            // the points, the reading thread writes the records in the file order (the tags need the string table).
            class CWayWriter
            {
            public:
                CWayWriter(CSplitRun& run, int nThreads, eWayMode mode = WayLookup) : m_run(run), m_mode(mode)
                {
                    if(nThreads <= 0)
                    {
                        unsigned int nCores = std::thread::hardware_concurrency();
                        nThreads = nCores > 1 ? (int)nCores - 1 : 1;
                    }
                    nThreads = (std::min)(nThreads, 32);
                    m_nMaxInFlight = (size_t)nThreads + 2;    // the batches are big (BatchRefs)
                    m_run.nWorkerThreads = nThreads;
                    m_vecWorkers.resize((size_t)nThreads);
                    // every worker has its own lookup: the cache of the blocks of the sorted store (the flat one maps the file)
                    if(m_mode == WayLookup)
                    {
                        for(size_t i = 0; i < m_vecWorkers.size(); ++i)
                            m_vecWorkers[i].ptrLookup = m_run.ptrNodes->CreateLookup((std::max)((uint64_t)16 * 1024 * 1024, m_run.nNodeCacheBytes / m_vecWorkers.size()));
                    }
                    try
                    {
                        for(size_t i = 0; i < m_vecWorkers.size(); ++i)
                            m_vecWorkers[i].thread = std::thread(&CWayWriter::WorkerLoop, this, i);
                    }
                    catch (...)
                    {
                        Stop();
                        throw;
                    }
                    m_pCurrent = NewBatch();
                }

                ~CWayWriter()
                {
                    Stop();
                }

                void Add(const readosm_way& way)
                {
                    SBatch& batch = *m_pCurrent;
                    SWay item;
                    item.nId = way.id;
                    item.nRefStart = (uint32_t)batch.refs.size();
                    item.nRefCount = (uint32_t)(way.node_ref_count > 0 ? way.node_ref_count : 0);
                    item.nTagStart = (uint32_t)(batch.tagOffsets.size() / 2);
                    item.nTagCount = (uint32_t)(way.tag_count > 0 ? way.tag_count : 0);
                    batch.refs.insert(batch.refs.end(), way.node_refs, way.node_refs + item.nRefCount);
                    if(m_mode == WayFinal)
                        ResolveStreams(batch, item.nRefStart, item.nRefCount);
                    // a pass of a part needs only the refs
                    for(uint32_t i = 0; m_mode != WaySegment && i < item.nTagCount; ++i)
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
                }

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
                    int64_t  nId = 0;
                    uint32_t nRefStart = 0, nRefCount = 0;
                    uint32_t nTagStart = 0, nTagCount = 0;   // pairs in tagOffsets
                    uint32_t nFlags = 0;
                    size_t   nPointsStart = 0, nPointsSize = 0;
                };

                struct SBatch
                {
                    std::vector<SWay>     ways;
                    std::vector<int64_t>  refs;
                    std::vector<uint32_t> tagOffsets;
                    std::string           text;
                    std::vector<uint8_t>  points;      // the encoded points / the raw coordinates of the refs of the part (WaySegment)
                    std::vector<SCoord>   coords;      // WayFinal: the coordinates of the refs from the streams
                    std::vector<uint8_t>  state;       // WayFinal: 0 - no node, 1 - coords, 2 - the part in memory
                    std::exception_ptr    ptrError;
                    bool                  bDone = false;

                    void Clear()
                    {
                        ways.clear();
                        refs.clear();
                        coords.clear();
                        state.clear();
                        tagOffsets.clear();
                        text.clear();
                        points.clear();
                        ptrError = nullptr;
                        bDone = false;
                    }
                };

                struct SWorker
                {
                    std::unique_ptr<IOSMNodeLookup> ptrLookup;
                    COSMRefResolver      resolver;
                    std::vector<int32_t> lons, lats;
                    COSMRecordBuffer     buffer;
                    std::thread          thread;
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
                        TClock::time_point start = TClock::now();
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cvDone.wait(lock, [pBatch]() { return pBatch->bDone; });
                        m_run.dWaitWorkers += Since(start);
                    }
                    m_inFlight.pop_front();
                    m_vecFree.push_back(pBatch);
                    if(pBatch->ptrError)
                        std::rethrow_exception(pBatch->ptrError);
                    Write(*pBatch);
                }

                // the reading thread: the records in the file order
                void Write(SBatch& batch)
                {
                    if(m_mode == WaySegment)
                    {
                        if(!batch.points.empty())
                            m_run.streams.back()->Append(batch.points.data(), batch.points.size());
                        return;
                    }
                    for(const SWay& way : batch.ways)
                    {
                        m_tags.resize(way.nTagCount);
                        for(uint32_t i = 0; i < way.nTagCount; ++i)
                        {
                            const uint32_t* pOffsets = batch.tagOffsets.data() + 2 * (way.nTagStart + i);
                            m_tags[i] = readosm_tag{batch.text.data() + pOffsets[0], batch.text.data() + pOffsets[1]};
                        }
                        COSMRecordBuffer& record = m_run.record;
                        COSMSplitCodec::EncodeWayHead(record, m_run.strings, way.nId, way.nFlags, m_tags.data(), (int)m_tags.size());
                        record.PutBytes(batch.points.data() + way.nPointsStart, way.nPointsSize);
                        uint64_t nOffset = m_run.waysFile.AppendRecord(record);
                        m_run.waysIndex.Add(way.nId, nOffset);
                    }
                }

                void Stop()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bStop = true;
                        m_queue.clear();
                    }
                    m_cvWork.notify_all();
                    for(SWorker& worker : m_vecWorkers)
                    {
                        if(worker.thread.joinable())
                            worker.thread.join();
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
                        TClock::time_point start = TClock::now();
                        try
                        {
                            Process(worker, *pBatch);
                        }
                        catch (...)
                        {
                            pBatch->ptrError = std::current_exception();
                        }
                        m_run.nWorkerNs.fetch_add((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(TClock::now() - start).count(),
                                                  std::memory_order_relaxed);
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            pBatch->bDone = true;
                        }
                        m_cvDone.notify_all();
                    }
                }

                // WayFinal, the reading thread (in the ref order): the refs of the earlier parts get the next coordinates of
                // their streams, the refs of the part in memory are resolved by the workers
                void ResolveStreams(SBatch& batch, uint32_t nStart, uint32_t nCount)
                {
                    const uint64_t nLast = m_run.nSegments - 1;
                    batch.coords.resize(nStart + nCount);
                    batch.state.resize(nStart + nCount);
                    for(uint32_t i = nStart; i < nStart + nCount; ++i)
                    {
                        int64_t nId = batch.refs[i];
                        SCoord& coord = batch.coords[i];
                        uint8_t& state = batch.state[i];
                        if(nId < 0)
                        {
                            state = m_run.pFlat->GetE7(nId, coord.lon, coord.lat) ? 1 : 0;   // in memory
                            continue;
                        }
                        uint64_t nPart = (uint64_t)nId / m_run.nSegmentRecords;
                        if(nPart == nLast)
                            state = 2;
                        else if(nPart < nLast)
                        {
                            COSMFlatNodeStore::SRecord record;
                            if(!m_run.streams[(size_t)nPart]->Read(&record, sizeof(record)))
                                throw CommonLib::CExcBase("The coordinates of the part {0} end before the ways", nPart + 1);
                            state = COSMFlatNodeStore::DecodeE7(record, coord.lon, coord.lat) ? 1 : 0;
                        }
                        else
                            state = 0;   // after the end of the node file
                    }
                }

                // WaySegment: the coordinates of the refs in the part in memory, in the ref order (an empty record - no node)
                void ProcessSegment(SBatch& batch)
                {
                    const COSMNodeSegment& segment = *m_run.ptrSegment;
                    uint64_t nRefs = 0;
                    for(int64_t nId : batch.refs)
                    {
                        if(!segment.Contains(nId))
                            continue;
                        const COSMFlatNodeStore::SRecord& record = segment.Record(nId);
                        const uint8_t* p = (const uint8_t*)&record;
                        batch.points.insert(batch.points.end(), p, p + sizeof(record));
                        ++nRefs;
                    }
                    m_run.nNodeLookups.fetch_add(nRefs, std::memory_order_relaxed);
                }

                // WayFinal: the coordinates from the streams (the reading thread) and the part in memory
                void ProcessFinal(SWorker& worker, SBatch& batch)
                {
                    const COSMNodeSegment& segment = *m_run.ptrSegment;
                    uint64_t nMissing = 0;
                    for(SWay& way : batch.ways)
                    {
                        const int64_t* pRefs = batch.refs.data() + way.nRefStart;
                        const uint32_t nRefs = way.nRefCount;
                        worker.lons.clear();
                        worker.lats.clear();
                        for(uint32_t i = 0; i < nRefs; ++i)
                        {
                            size_t nRef = way.nRefStart + i;
                            int32_t nLon = 0, nLat = 0;
                            bool bFound = batch.state[nRef] == 1 ? (nLon = batch.coords[nRef].lon, nLat = batch.coords[nRef].lat, true) :
                                          batch.state[nRef] == 2 ? segment.GetE7(pRefs[i], nLon, nLat) : false;
                            if(bFound)
                            {
                                worker.lons.push_back(nLon);
                                worker.lats.push_back(nLat);
                            }
                            else
                                ++nMissing;
                        }
                        way.nFlags = (nRefs >= 4 && pRefs[0] == pRefs[nRefs - 1] ? OSMSplit::WayClosed : 0) |
                                     (worker.lons.size() < nRefs ? OSMSplit::WayMissingNodes : 0);
                        worker.buffer.Clear();
                        COSMSplitCodec::EncodePoints(worker.buffer, worker.lons.data(), worker.lats.data(), worker.lons.size());
                        way.nPointsStart = batch.points.size();
                        way.nPointsSize = worker.buffer.Size();
                        batch.points.insert(batch.points.end(), worker.buffer.Data().begin(), worker.buffer.Data().end());
                    }
                    m_run.nNodeLookups.fetch_add(batch.refs.size(), std::memory_order_relaxed);
                    m_run.nMissingNodes.fetch_add(nMissing, std::memory_order_relaxed);
                }

                // the coordinates of the nodes, the points encoded (the missing nodes of an extract are skipped):
                // all the node refs of the batch are sorted and resolved at once (the store is read in the id order)
                void Process(SWorker& worker, SBatch& batch)
                {
                    if(m_mode == WaySegment)
                    {
                        ProcessSegment(batch);
                        return;
                    }
                    if(m_mode == WayFinal)
                    {
                        ProcessFinal(worker, batch);
                        return;
                    }
                    uint64_t nMissing = 0;
                    worker.resolver.Clear();
                    for(const SWay& way : batch.ways)
                        worker.resolver.Add(batch.refs.data(), way.nRefStart, way.nRefCount);
                    TClock::time_point lookupStart = TClock::now();
                    worker.resolver.Resolve(*worker.ptrLookup, batch.refs.size(),
                                            [this]() { return !m_run.ptrCancel.get() || m_run.ptrCancel->Continue(); });
                    m_run.nLookupNs.fetch_add((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(TClock::now() - lookupStart).count(),
                                              std::memory_order_relaxed);
                    m_run.AddLookupStats(worker.ptrLookup->TakeStats(), worker.resolver.UniqueCount());

                    for(SWay& way : batch.ways)
                    {
                        const int64_t* pRefs = batch.refs.data() + way.nRefStart;
                        const uint32_t nRefs = way.nRefCount;
                        worker.lons.clear();
                        worker.lats.clear();
                        for(uint32_t i = 0; i < nRefs; ++i)
                        {
                            int32_t nLon, nLat;
                            if(worker.resolver.Get(way.nRefStart + i, nLon, nLat))
                            {
                                worker.lons.push_back(nLon);
                                worker.lats.push_back(nLat);
                            }
                            else
                                ++nMissing;
                        }
                        // as the conversion classifies the ways (COSMSchema::MatchWay)
                        way.nFlags = (nRefs >= 4 && pRefs[0] == pRefs[nRefs - 1] ? OSMSplit::WayClosed : 0) |
                                     (worker.lons.size() < nRefs ? OSMSplit::WayMissingNodes : 0);
                        worker.buffer.Clear();
                        COSMSplitCodec::EncodePoints(worker.buffer, worker.lons.data(), worker.lats.data(), worker.lons.size());
                        way.nPointsStart = batch.points.size();
                        way.nPointsSize = worker.buffer.Size();
                        batch.points.insert(batch.points.end(), worker.buffer.Data().begin(), worker.buffer.Data().end());
                    }
                    m_run.nNodeLookups.fetch_add(batch.refs.size(), std::memory_order_relaxed);
                    m_run.nMissingNodes.fetch_add(nMissing, std::memory_order_relaxed);
                }

            private:
                CSplitRun& m_run;
                eWayMode m_mode;
                size_t m_nMaxInFlight = 4;
                std::vector<std::unique_ptr<SBatch> > m_vecBatches;
                std::vector<SBatch*> m_vecFree;
                std::deque<SBatch*>  m_inFlight;
                SBatch*              m_pCurrent = nullptr;
                std::vector<readosm_tag> m_tags;

                std::mutex m_mutex;
                std::condition_variable m_cvWork;
                std::condition_variable m_cvDone;
                std::deque<SBatch*> m_queue;
                bool m_bStop = false;
                std::vector<SWorker> m_vecWorkers;    // the last member: the threads are stopped first
            };

            // a pass over the ways for a part of the node ids in memory (WaySegment)
            class CSegmentHandler : public IOSMReadHandler
            {
            public:
                CSegmentHandler(CSplitRun& run, CWayWriter& ways) : m_run(run), m_ways(ways) {}

                virtual bool OnNode(const readosm_node&) { return true; }
                virtual bool OnRelation(const readosm_relation&) { return true; }
                virtual bool OnWay(const readosm_way& way)
                {
                    ++m_run.nPassWays;
                    m_ways.Add(way);
                    return m_run.Step();
                }

                virtual void OnReaderStats(const SOSMReaderStats& stats)
                {
                    m_run.reader = stats;
                }

                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
                {
                    m_run.progress.nBytesRead = nBytesRead;
                    m_run.progress.nBytesTotal = nBytesTotal;
                    if(nBytesRead >= nBytesTotal || m_run.throttle.IsTime())
                        m_run.Send();
                    return !m_run.ptrCancel.get() || m_run.ptrCancel->Continue();
                }

            private:
                CSplitRun&  m_run;
                CWayWriter& m_ways;
            };

            class CSplitHandler : public IOSMReadHandler
            {
            public:
                // bOnePass - the ways start when the nodes are done (a sorted file); a node after a way stops the reading
                // (IsUnsorted): the splitting starts again with two passes
                CSplitHandler(CSplitRun& run, bool bOnePass, int nThreads) : m_run(run), m_bOnePass(bOnePass), m_nThreads(nThreads) {}

                virtual bool OnNode(const readosm_node& node)
                {
                    if(m_ptrWays.get())
                    {
                        m_bUnsorted = true;
                        return false;
                    }
                    m_run.analyzer.AddNode(node);
                    if(node.longitude != READOSM_UNDEFINED && node.latitude != READOSM_UNDEFINED)
                    {
                        m_run.ptrNodes->Add(node.id, node.longitude, node.latitude);
                        if(m_run.bSortedNodes)
                        {
#pragma pack(push, 4)
                            struct { int64_t id; int32_t lon; int32_t lat; } record = {node.id, E7(node.longitude), E7(node.latitude)};
#pragma pack(pop)
                            m_run.sortedNodes.Append(&record, sizeof(record));
                        }
                        if(node.tag_count > 0)
                        {
                            COSMSplitCodec::EncodeNode(m_run.record, m_run.strings, node.id, E7(node.longitude), E7(node.latitude), node.tags, node.tag_count);
                            m_run.nodesIndex.Add(node.id, m_run.nodesFile.AppendRecord(m_run.record));
                        }
                    }
                    return m_run.Step();
                }

                virtual bool OnWay(const readosm_way& way)
                {
                    if(!m_ptrWays.get())
                        StartWays();
                    m_run.analyzer.AddWay(way);
                    m_ptrWays->Add(way);
                    return m_run.Step();
                }

                virtual bool OnRelation(const readosm_relation& relation)
                {
                    m_run.analyzer.AddRelation(relation);
                    COSMSplitCodec::EncodeRelation(m_run.record, m_run.strings, relation);
                    m_run.relationsIndex.Add(relation.id, m_run.relationsFile.AppendRecord(m_run.record));
                    return m_run.Step();
                }

                virtual void OnReaderStats(const SOSMReaderStats& stats)
                {
                    m_run.reader = stats;
                }

                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
                {
                    m_run.progress.nBytesRead = nBytesRead;
                    m_run.progress.nBytesTotal = nBytesTotal;
                    if(nBytesRead >= nBytesTotal || m_run.throttle.IsTime())
                        m_run.Send();
                    return !m_run.ptrCancel.get() || m_run.ptrCancel->Continue();
                }

                bool IsUnsorted() const { return m_bUnsorted; }

                // the ways of the final pass of the flat store (the coordinates from the memory and the streams)
                void SetWays(std::unique_ptr<CWayWriter> ptrWays) { m_ptrWays = std::move(ptrWays); }

                // the last batches of the ways are written
                void Finish()
                {
                    if(m_ptrWays.get())
                        m_ptrWays->Finish();
                    m_ptrWays.reset();
                }

                // the coordinates of all the nodes are stored: the store is finished (sorted / mapped), the ways can find them
                void StartWays()
                {
                    if(!m_run.ptrNodes->IsFinished())
                        m_run.ptrNodes->Finish([this](uint64_t) { return !m_run.ptrCancel.get() || m_run.ptrCancel->Continue(); });
                    m_run.CheckCancel();
                    m_ptrWays.reset(new CWayWriter(m_run, m_nThreads));
                }

            private:
                CSplitRun& m_run;
                bool m_bOnePass;
                int  m_nThreads;
                bool m_bUnsorted = false;
                std::unique_ptr<CWayWriter> m_ptrWays;
            };
            // The flat store (the planet): the ways get the coordinates of their nodes without random reads of the disk.
            //  1. nodes (+ nodes.dat) -> nodes.flat;
            //  2. the node ids are cut into parts of nWaysNodeMemoryMB; for every part but the last one: the part is read into memory
            //     (sequentially), a pass over the ways writes the coordinates of their refs in the part to a stream (in the ref order);
            //  3. the last part is read into memory, the final pass over the ways (and the relations) takes the coordinates of every ref
            //     from the memory or from the next record of the stream of its part, the ways are written to ways.dat.
            // Everything is read / written sequentially: the passes over the way blocks of the OSM file, nodes.flat once, the streams.
            void SplitFlat(CSplitRun& run, const SOSMConvertSettings& settings, const std::string& sOSMPath, const std::string& sFolder)
            {
                COSMFlatNodeStore* pFlat = dynamic_cast<COSMFlatNodeStore*>(run.ptrNodes.get());
                if(!pFlat)
                    throw CommonLib::CExcBase("OSM splitting: the flat node store is expected");

                // 1. the nodes
                run.progress.nPassCount = 3;    // an estimate until the parts are known
                {
                    CSplitHandler handler(run, false, settings.nThreads);
                    run.BeginStage(OSMStageNodes, 1, "Splitting: nodes -> nodes.flat, nodes.dat");
                    if(!COSMReader::Read(sOSMPath, handler, COSMReader::ReadNodes, settings.nThreads))
                        throw CommonLib::CExcBase("OSM splitting is canceled");
                }
                pFlat->Finish([&run](uint64_t) { return !run.ptrCancel.get() || run.ptrCancel->Continue(); });
                run.CheckCancel();
                run.pFlat = pFlat;

                // 2. the parts of the ids which fit into the memory
                uint64_t nMemory = settings.nWaysNodeMemoryMB ? (uint64_t)settings.nWaysNodeMemoryMB * 1024 * 1024 : PhysicalMemory() / 10 * 6;
                run.nSegmentRecords = (std::max)((uint64_t)1 << 17, nMemory / sizeof(COSMFlatNodeStore::SRecord));
                uint64_t nRecords = (std::max)((uint64_t)1, pFlat->FileRecords());
                run.nSegmentRecords = (std::min)(run.nSegmentRecords, nRecords);
                run.nSegments = (nRecords + run.nSegmentRecords - 1) / run.nSegmentRecords;
                // the memory decides the number of the parts, the ids are divided equally: the smallest peak of the memory
                // (the planet with 128 GB: 2 parts of ~53 GB, not 77 + 29 GB)
                run.nSegmentRecords = (nRecords + run.nSegments - 1) / run.nSegments;
                run.progress.nPassCount = (int)run.nSegments + 1;
                run.ptrSegment.reset(new COSMNodeSegment());
                const std::string sFlatPath = pFlat->FilePath();
                std::string sTempDir = settings.sTempDir.empty() ? sFolder : settings.sTempDir;

                auto loadPart = [&](uint64_t nPart)
                {
                    run.nSegment = nPart;
                    run.nSegmentBytesStart = run.ptrSegment->BytesRead();
                    run.dSegmentSecondsStart = run.ptrSegment->ReadSeconds();
                    run.BeginStage(OSMStageSortNodes, 0, "Loading node coordinates into memory: part " + std::to_string(nPart + 1) + " of " +
                                   std::to_string(run.nSegments) + " (" + GB(run.nSegmentRecords * sizeof(COSMFlatNodeStore::SRecord)) + ")");
                    run.progress.nNodes = 0;
                    // the last part: only the rest of the ids (no memory is cleared for the ids after the end of the file)
                    uint64_t nPartStart = nPart * run.nSegmentRecords;
                    uint64_t nPartCount = (std::min)(run.nSegmentRecords, nRecords - (std::min)(nRecords, nPartStart));
                    run.ptrSegment->Load(sFlatPath, nPartStart, nPartCount, [&run](uint64_t)
                    {
                        if(run.throttle.IsTime())
                            run.Send();
                        return !run.ptrCancel.get() || run.ptrCancel->Continue();
                    });
                };

                for(uint64_t nPart = 0; nPart + 1 < run.nSegments; ++nPart)
                {
                    loadPart(nPart);
                    std::unique_ptr<COSMDataFile> ptrStream(new COSMDataFile());
                    ptrStream->Create(OSMSplit::FilePath(sTempDir, ("_coords_part" + std::to_string(nPart + 1) + ".tmp").c_str()));
                    run.streams.push_back(std::move(ptrStream));

                    run.nPassWays = 0;
                    run.BeginStage(OSMStageWays, (int)nPart + 2, "Coordinates of the ways: part " + std::to_string(nPart + 1) + " of " +
                                   std::to_string(run.nSegments));
                    {
                        CWayWriter ways(run, settings.nThreads, WaySegment);
                        CSegmentHandler handler(run, ways);
                        if(!COSMReader::Read(sOSMPath, handler, COSMReader::ReadWays, settings.nThreads))
                            throw CommonLib::CExcBase("OSM splitting is canceled");
                        ways.Finish();
                    }
                    run.streams.back()->Close();
                    run.streams.back()->Open(run.streams.back()->Path());   // read by the final pass
                    run.streams.back()->SetReadBufferSize(64 * 1024 * 1024);   // fewer seeks: the OSM file and the streams are read at once
                }

                // 3. the last part in memory, the final pass: ways (+ the streams), relations
                loadPart(run.nSegments - 1);
                run.nPassWays = 0;
                run.BeginStage(OSMStageSplit, (int)run.nSegments + 1, "Splitting: ways and relations");
                {
                    CSplitHandler handler(run, false, settings.nThreads);
                    handler.SetWays(std::unique_ptr<CWayWriter>(new CWayWriter(run, settings.nThreads, WayFinal)));
                    if(!COSMReader::Read(sOSMPath, handler, COSMReader::ReadWays | COSMReader::ReadRelations, settings.nThreads))
                        throw CommonLib::CExcBase("OSM splitting is canceled");
                    handler.Finish();
                }

                // the streams are deleted, the memory is free
                for(std::unique_ptr<COSMDataFile>& ptrStream : run.streams)
                {
                    std::string sStream = ptrStream->Path();
                    ptrStream->Close();
                    std::error_code ec;
                    std::filesystem::remove(std::filesystem::u8path(sStream), ec);
                }
                run.EndStage();
                run.ptrSegment->Free();
            }
        }

        COSMSplitter::COSMSplitter(const COSMSchema& schema, const SOSMConvertSettings& settings) : m_schema(schema), m_settings(settings)
        {

        }

        COSMMapPtr COSMSplitter::Split(const std::string& sOSMPath, const std::string& sFolder, bool bKeepNodeFile,
                                       IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            try
            {
                eOSMFileFormat format = COSMMap::FormatByPath(sOSMPath);
                if(format != OSMFormatXML && format != OSMFormatPBF)
                    throw CommonLib::CExcBase("Unknown OSM file format, expected .osm or .pbf");
                std::error_code ec;
                std::filesystem::create_directories(std::filesystem::u8path(sFolder), ec);
                if(!std::filesystem::is_directory(std::filesystem::u8path(sFolder), ec))
                    throw CommonLib::CExcBase("The folder can't be created: {0}", sFolder);

                // planet / extracts are sorted: one pass (the ways come after all the nodes); null - not sorted
                COSMMapPtr ptrMap = Run(sOSMPath, sFolder, bKeepNodeFile, false, ptrProgress, ptrCancel);
                if(!ptrMap.get())
                    ptrMap = Run(sOSMPath, sFolder, bKeepNodeFile, true, ptrProgress, ptrCancel);
                return ptrMap;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to split OSM file {0}", sOSMPath, exc);
                throw;
            }
        }

        COSMMapPtr COSMSplitter::Run(const std::string& sOSMPath, const std::string& sFolder, bool bKeepNodeFile, bool bTwoPasses,
                                     IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            COSMMapPtr ptrMap = m_schema.CreateMap();
            ptrMap->SetPath(sOSMPath);

            CSplitRun run(m_schema, ptrMap, ptrProgress, ptrCancel);
            run.sPath = sOSMPath;
            std::error_code ec;
            uint64_t nFileSize = std::filesystem::file_size(std::filesystem::u8path(sOSMPath), ec);
            run.progress.nFileSize = ec ? 0 : nFileSize;
            run.progress.nPassCount = bTwoPasses ? 2 : 1;

            // the node store as the conversion chooses it (Auto: flat for the planet / continents, sorted for extracts):
            // the flat file of an extract is millions of small writes and random page reads of a sparse 100 GB file
            eOSMNodeStore storeType = IOSMNodeStore::ResolveType(m_settings.nodeStore, sOSMPath, run.progress.nFileSize);
            std::string sNodeFile;
            run.nNodeCacheBytes = (uint64_t)m_settings.nNodeCacheMB * 1024 * 1024;
            if(storeType == OSMNodeStoreFlat)
            {
                run.ptrNodes.reset(new COSMFlatNodeStore(OSMSplit::FilePath(sFolder, OSMSplit::NodesFlatFile), bKeepNodeFile,
                                                         (uint64_t)m_settings.nNodeWriteBufferMB * 1024 * 1024));
                if(bKeepNodeFile)
                    sNodeFile = OSMSplit::NodesFlatFile;
            }
            else
            {
                // the sorted store in the temporary folder (the split folder by default), nodes.sorted is written next to it:
                // an unsorted file (two passes) has no sorted copy
                std::string sTempDir = m_settings.sTempDir.empty() ? sFolder : m_settings.sTempDir;
                run.ptrNodes = IOSMNodeStore::Create(OSMNodeStoreSorted, sOSMPath, run.progress.nFileSize, sTempDir, run.nNodeCacheBytes);
                if(bKeepNodeFile && !bTwoPasses)
                {
                    run.sortedNodes.Create(OSMSplit::FilePath(sFolder, OSMSplit::NodesSortedFile));
                    run.bSortedNodes = true;
                    sNodeFile = OSMSplit::NodesSortedFile;
                }
            }
            // Add waits for the disk (the flat store): the cancel is checked meanwhile
            run.ptrNodes->SetContinue([&run]() { return !run.ptrCancel.get() || run.ptrCancel->Continue(); });
            run.nodesFile.Create(OSMSplit::FilePath(sFolder, OSMSplit::NodesData));
            run.waysFile.Create(OSMSplit::FilePath(sFolder, OSMSplit::WaysData));
            run.relationsFile.Create(OSMSplit::FilePath(sFolder, OSMSplit::RelationsData));
            run.nodesIndex.Create(OSMSplit::FilePath(sFolder, OSMSplit::NodesIndex));
            run.waysIndex.Create(OSMSplit::FilePath(sFolder, OSMSplit::WaysIndex));
            run.relationsIndex.Create(OSMSplit::FilePath(sFolder, OSMSplit::RelationsIndex));

            if(storeType == OSMNodeStoreFlat)
                SplitFlat(run, m_settings, sOSMPath, sFolder);
            else
            {
                CSplitHandler handler(run, !bTwoPasses, m_settings.nThreads);
                if(bTwoPasses)
                {
                    run.BeginStage(OSMStageNodes, 1, "Splitting: nodes (the file isn't sorted)");
                    if(!COSMReader::Read(sOSMPath, handler, COSMReader::ReadNodes, m_settings.nThreads))
                        throw CommonLib::CExcBase("OSM splitting is canceled");
                    run.BeginStage(OSMStageSortNodes, 0, "Mapping node coordinates");
                    handler.StartWays();
                    run.BeginStage(OSMStageSplit, 2, "Splitting: ways and relations");
                    if(!COSMReader::Read(sOSMPath, handler, COSMReader::ReadWays | COSMReader::ReadRelations, m_settings.nThreads))
                        throw CommonLib::CExcBase("OSM splitting is canceled");
                }
                else
                {
                    run.BeginStage(OSMStageSplit, 1, "Splitting the file: nodes, ways, relations");
                    if(!COSMReader::Read(sOSMPath, handler, COSMReader::ReadAll, m_settings.nThreads))
                    {
                        if(handler.IsUnsorted())
                            return COSMMapPtr();    // a node after the ways: two passes
                        throw CommonLib::CExcBase("OSM splitting is canceled");
                    }
                }
                handler.Finish();
            }

            // the indexes, the strings, the metadata
            run.BeginStage(OSMStageFinish, 0, "Saving the indexes and the metadata");
            TClock::time_point start = TClock::now();
            run.nodesFile.Close();
            run.waysFile.Close();
            run.relationsFile.Close();
            run.nodesIndex.Finish();
            run.waysIndex.Finish();
            run.relationsIndex.Finish();
            run.dIndexSeconds = Since(start);
            run.CheckCancel();

            start = TClock::now();
            run.strings.Save(OSMSplit::FilePath(sFolder, OSMSplit::StringsFile));
            run.nStrings = run.strings.Count();
            run.analyzer.Finish();
            if(bTwoPasses)
                ptrMap->SetSorted(false);
            if(!run.ptrNodes->IsFinished())
                run.ptrNodes->Finish();  // the buffer is written (a file without ways)
            run.ptrNodes.reset();    // closed (the temporary / not kept files are deleted)
            if(run.bSortedNodes)
                run.sortedNodes.Close();

            std::string sMetaPath = OSMSplit::FilePath(sFolder, OSMSplit::MetaFile);
            COSMSplitMeta::Save(sMetaPath, *ptrMap, sOSMPath, run.progress.nFileSize, sNodeFile);
            run.dMetaSeconds = Since(start);
            ptrMap->SetPath(sMetaPath);
            run.Done();
            return ptrMap;
        }
    }
}
