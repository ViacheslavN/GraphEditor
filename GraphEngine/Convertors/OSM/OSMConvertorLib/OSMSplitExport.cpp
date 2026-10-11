#include "OSMSplitExport.h"
#include "OSMSplitter.h"
#include "OSMSplitFormat.h"
#include "OSMFeatureWriter.h"
#include "OSMConvertor.h"
#include "OSMProgressThrottle.h"
#include "OSMMapStyle.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            typedef std::chrono::steady_clock TClock;
            typedef GeometryCompression::TByte TByte;

            double Since(TClock::time_point start)
            {
                return std::chrono::duration<double>(TClock::now() - start).count();
            }

            // the batches of the parallel stages: the records of the reader thread
            const size_t BatchRecords = 65536;
            const size_t BatchBytes = 16 * 1024 * 1024;
            const size_t ReadBufferBytes = 32 * 1024 * 1024;

            // multipolygons: the member ways of a group of relations are read at once (one forward pass over ways.idx, ways.dat)
            const size_t GroupMemberRefs = 64 * 1024 * 1024;
            const size_t GroupRelationBytes = 1024 * 1024 * 1024;
            const size_t MemberReadBytes = 64 * 1024 * 1024;

            struct SLayerOut
            {
                IOSMLayerPtr         ptrLayer;
                eOSMGeometryType     type = OSMGeometryPoint;
                COSMFeatureWriterPtr ptrWriter;    // converted
                uint64_t             nRowsStart = 0;
                double               dInsertStart = 0.;
            };

            struct STableOut
            {
                IOSMTablePtr       ptrTable;
                COSMTableWriterPtr ptrWriter;
                COSMTableWriterPtr ptrMembers;
            };

            // the state of the export shared by the stages
            class CExport
            {
            public:
                CExport(const COSMSchema& schema, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) :
                        schema(schema), ptrProgress(ptrProgress), ptrCancel(ptrCancel), vecLayers(schema.Layers().size())
                {}

                // thread safe (the workers): the layers / the maps of the enabled values are only read
                COSMFeatureWriter* LayerWriter(const SOSMMatch& match) const
                {
                    if(!match.IsValid())
                        return nullptr;
                    const SLayerOut& layer = vecLayers[match.nLayer];
                    if(!layer.ptrWriter.get() || !layer.ptrLayer->IsValueEnabled(match.sKey, match.sValue))
                        return nullptr;
                    return layer.ptrWriter.get();
                }

                bool IsConvertedTable(const STableOut& table, const char* pszKey, const char* pszValue) const
                {
                    return pszValue && table.ptrWriter.get() && table.ptrTable->IsValueEnabled(pszKey, pszValue);
                }

                void WriteTags(char cType, int64_t nId, const readosm_tag* pTags, size_t nTags)
                {
                    if(!tags.ptrWriter.get())
                        return;
                    COSMTableWriter& writer = *tags.ptrWriter;
                    const std::string sType(1, cType);
                    for(size_t i = 0; i < nTags; ++i)
                    {
                        const readosm_tag& tag = pTags[i];
                        if(!tags.ptrTable->IsValueEnabled("key", tag.key))
                            continue;
                        writer.SetText(0, sType);
                        writer.SetInt64(1, nId);
                        writer.SetText(2, tag.key);
                        writer.SetText(3, tag.value);
                        writer.Insert();
                        ++nFeatures;
                    }
                }

                // the progress from time to time, false - canceled
                bool Tick()
                {
                    if(throttle.IsTime())
                        Send();
                    return !ptrCancel.get() || ptrCancel->Continue();
                }

                void CheckCancel() const
                {
                    if(ptrCancel.get() && !ptrCancel->Continue())
                        throw CommonLib::CExcBase("OSM export is canceled");
                }

                // the reading of the stage: the reader thread updates the counters
                void SetRead(uint64_t nBytes, double dSeconds)
                {
                    nReadBytes.store(nBytes, std::memory_order_relaxed);
                    nReadNs.store((uint64_t)(dSeconds * 1e9), std::memory_order_relaxed);
                }

                void BeginStage(eOSMStage stage, int nPass, const std::string& sMessage, const std::string& sFile, uint64_t nFileSize)
                {
                    EndStage();
                    bStage = true;
                    sStageFile = sFile;
                    progress.stage = stage;
                    progress.sMessage = sMessage;
                    progress.nPass = nPass;
                    progress.nBytesRead = 0;
                    progress.nBytesTotal = nFileSize;
                    progress.nStepDone = progress.nStepTotal = 0;
                    SetRead(0, 0.);
                    nStageObjects = 0;
                    nWorkerNs = 0;
                    dWaitReader = dWaitWorkers = 0.;
                    nWorkerThreads = 0;
                    extraItems.clear();
                    stageStart = TClock::now();
                    for(SLayerOut& layer : vecLayers)
                    {
                        if(layer.ptrWriter.get())
                        {
                            layer.nRowsStart = layer.ptrWriter->Writer()->GetRowCount();
                            layer.dInsertStart = layer.ptrWriter->Writer()->GetInsertSeconds();
                        }
                    }
                    for(STableOut* pTable : {&routes, &restrictions, &tags})
                        tableStart[pTable] = pTable->ptrWriter.get() ? std::make_pair(pTable->ptrWriter->GetRowCount(), pTable->ptrWriter->GetInsertSeconds())
                                                                   : std::make_pair((uint64_t)0, 0.);
                    throttle.Reset();
                    Send();
                }

                void EndStage()
                {
                    if(!bStage)
                        return;
                    if(!sStageFile.empty())
                        progress.nBytesRead = progress.nBytesTotal;
                    SOSMStageSummary summary;
                    summary.stage = progress.stage;
                    summary.sMessage = progress.sMessage;
                    summary.dSeconds = Since(stageStart);
                    summary.vecItems = BuildItems();
                    progress.vecStages.push_back(summary);
                    bStage = false;
                }

                void Send()
                {
                    progress.nFeatures = nFeatures;
                    if(!sStageFile.empty())
                        progress.nBytesRead = nReadBytes.load(std::memory_order_relaxed);
                    if(bStage)
                    {
                        progress.dStageSeconds = Since(stageStart);
                        progress.vecItems = BuildItems();
                    }
                    if(ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }

                void Done()
                {
                    EndStage();
                    progress.vecItems.clear();
                    progress.dStageSeconds = 0.;
                    progress.stage = OSMStageFinish;
                    progress.sMessage = "Done";
                    Send();
                }

                static void Add(std::vector<SOSMProgressItem>& items, const std::string& sName, const std::string& sInfo,
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

                std::vector<SOSMProgressItem> BuildItems() const
                {
                    std::vector<SOSMProgressItem> items;
                    if(progress.stage == OSMStageFinish)
                        return finishItems;

                    const double dStage = Since(stageStart);
                    double dMain = 0.;     // the time of the main thread explained by the items
                    if(!sStageFile.empty())
                    {
                        double dRead = nReadNs.load(std::memory_order_relaxed) / 1e9;
                        Add(items, "Reading the file", sStageFile + (nWorkerThreads ? " (sequential, the reader thread)" : " (sequential)"),
                            nStageObjects, nReadBytes.load(std::memory_order_relaxed), dRead);
                        if(!nWorkerThreads)
                            dMain += dRead;
                    }
                    if(nWorkerThreads)
                    {
                        Add(items, "Decoding, classification, compression", std::to_string(nWorkerThreads) + " threads; CPU time of all of them",
                            nStageObjects, 0, nWorkerNs.load(std::memory_order_relaxed) / 1e9, true);
                        Add(items, "Waiting for the reader", "the main thread waits: the disk reads slower than the rows are written", 0, 0, dWaitReader);
                        Add(items, "Waiting for the workers", "the main thread waits: the threads compute slower than the rows are written", 0, 0, dWaitWorkers);
                        dMain += dWaitReader + dWaitWorkers;
                    }
                    for(const SOSMProgressItem& item : extraItems)
                    {
                        items.push_back(item);
                        if(!item.bThreads)
                            dMain += item.dSeconds;
                    }

                    // the rows written in the stage by the geometry type
                    for(eOSMGeometryType type : {OSMGeometryPoint, OSMGeometryLine, OSMGeometryPolygon})
                    {
                        std::string sTables;
                        uint64_t nRows = 0;
                        double dSeconds = 0.;
                        for(const SLayerOut& layer : vecLayers)
                        {
                            if(!layer.ptrWriter.get() || layer.type != type)
                                continue;
                            uint64_t nLayerRows = layer.ptrWriter->Writer()->GetRowCount() - layer.nRowsStart;
                            nRows += nLayerRows;
                            dSeconds += layer.ptrWriter->Writer()->GetInsertSeconds() - layer.dInsertStart;
                            if(nLayerRows)
                                sTables += (sTables.empty() ? "" : ", ") + layer.ptrLayer->GetTableName();
                        }
                        if(nRows == 0)
                            continue;
                        const char* pszName = type == OSMGeometryPoint ? "Point features" : (type == OSMGeometryLine ? "Line features" : "Polygon features");
                        Add(items, pszName, "rows -> " + sTables + " (insert: SQLite, the main thread)", nRows, 0, dSeconds);
                        dMain += dSeconds;
                    }
                    for(const STableOut* pTable : {&routes, &restrictions, &tags})
                    {
                        if(!pTable->ptrWriter.get())
                            continue;
                        auto it = tableStart.find(pTable);
                        uint64_t nRows = pTable->ptrWriter->GetRowCount() - (it != tableStart.end() ? it->second.first : 0);
                        double dSeconds = pTable->ptrWriter->GetInsertSeconds() - (it != tableStart.end() ? it->second.second : 0.);
                        if(nRows == 0)
                            continue;
                        Add(items, "Rows of " + pTable->ptrTable->GetDisplayName(), "-> " + pTable->ptrWriter->GetTableName() + " (SQLite)", nRows, 0, dSeconds);
                        dMain += dSeconds;
                    }
                    Add(items, "The rest of the main thread", "setting the fields, the other work", 0, 0, (std::max)(0., dStage - dMain));
                    return items;
                }

            public:
                const COSMSchema&         schema;
                COSMProjection            projection;
                IProgressUpdaterPtr       ptrProgress;
                Display::ITrackCancelPtr  ptrCancel;
                SOSMProgress              progress;
                COSMProgressThrottle      throttle;
                uint64_t                  nStageObjects = 0;
                uint64_t                  nFeatures = 0;
                bool                      bStage = false;
                std::string               sStageFile;
                TClock::time_point        stageStart;
                std::map<const STableOut*, std::pair<uint64_t, double> > tableStart;
                std::vector<SOSMProgressItem> finishItems;
                std::vector<SOSMProgressItem> extraItems;    // the items of the relations stage

                // the parallel stages
                std::atomic<uint64_t>     nReadBytes{0};
                std::atomic<uint64_t>     nReadNs{0};
                std::atomic<uint64_t>     nWorkerNs{0};
                double                    dWaitReader = 0., dWaitWorkers = 0.;
                int                       nWorkerThreads = 0;

                std::vector<SLayerOut>    vecLayers;     // by the schema index
                STableOut                 routes, restrictions, tags;
                COSMStringTable           strings;

                std::shared_ptr<GeometryCompression::CShapeCompressor> ptrCompressor;
                bool                      bIntegers = false;   // the OSM integers are the quantized coordinates (lon / lat, 7 decimals)
            };

            // a feature made by a worker, written by the main thread
            struct SFeature
            {
                int64_t            nId = 0;
                COSMFeatureWriter* pWriter = nullptr;
                SOSMMatch          match;
                CommonLib::IGeoShapePtr ptrShape;      // not compressed (no compressor / out of the integer range)
                uint32_t           nBlob = 0, nBlobSize = 0;
                uint32_t           nTagStart = 0, nTagCount = 0;
                bool               bTags = false;      // the last feature of the object: its tags go to the tags table
            };

            // the records of a .dat file: read by the reader thread, processed by a worker, written by the main thread
            struct SBatch
            {
                std::vector<uint8_t>  data;
                std::vector<uint32_t> starts;         // record i: [starts[i], starts[i + 1])

                std::vector<SFeature> features;
                std::vector<readosm_tag> tags;        // point into data and the string table
                std::vector<TByte>    blobs;
                std::exception_ptr    ptrError;
                bool                  bDone = false;

                size_t Count() const { return starts.empty() ? 0 : starts.size() - 1; }
                const uint8_t* Record(size_t i) const { return data.data() + starts[i]; }
                size_t Size(size_t i) const { return starts[i + 1] - starts[i]; }

                void Clear()
                {
                    data.clear();
                    starts.clear();
                    features.clear();
                    tags.clear();
                    blobs.clear();
                    ptrError = nullptr;
                    bDone = false;
                }

                void AddBlob(SFeature& feature, const std::vector<TByte>& blob)
                {
                    feature.nBlob = (uint32_t)blobs.size();
                    feature.nBlobSize = (uint32_t)blob.size();
                    blobs.insert(blobs.end(), blob.begin(), blob.end());
                }

                void AddTags(SFeature& feature, const SOSMSplitObject& object)
                {
                    feature.nTagStart = (uint32_t)tags.size();
                    feature.nTagCount = (uint32_t)object.tags.size();
                    tags.insert(tags.end(), object.tags.begin(), object.tags.end());
                }
            };

            struct SWorker
            {
                std::unique_ptr<GeometryCompression::CShapeCompressor> ptrCompressor;
                SOSMSplitObject object;
                TOSMPoints points, ring;
                std::vector<int64_t> xs, ys;
                std::vector<TByte> blob;
                std::thread thread;
            };

            // A .dat file in parallel:
            //  - the reader thread reads the records sequentially (big reads) into batches;
            //  - the workers decode, classify and compress them (the features of the batch);
            //  - the main thread (Run) writes the batches in the order of the file: SQLite is written by one thread.
            class CPipeline
            {
            public:
                typedef std::function<void(SWorker&, SBatch&)> TProcess;
                typedef std::function<void(SBatch&)> TWrite;

                CPipeline(CExport& run, const std::string& sPath, int nThreads, TProcess process) :
                        m_run(run), m_process(process)
                {
                    m_file.Open(sPath);
                    m_file.SetReadBufferSize(ReadBufferBytes);
                    m_vecWorkers.resize((size_t)nThreads);
                    for(SWorker& worker : m_vecWorkers)
                    {
                        if(m_run.ptrCompressor.get())
                            worker.ptrCompressor.reset(new GeometryCompression::CShapeCompressor(m_run.ptrCompressor->GetParams()));
                    }
                    // the batches: every worker has one, some are read ahead, some are waiting to be written
                    m_vecBatches.resize((size_t)nThreads * 2 + 4);
                    for(auto& ptrBatch : m_vecBatches)
                    {
                        ptrBatch.reset(new SBatch());
                        m_free.push_back(ptrBatch.get());
                    }
                }

                ~CPipeline()
                {
                    Stop();
                }

                void Run(TWrite write)
                {
                    // after BeginStage of the stage (it resets the counters)
                    m_run.nWorkerThreads = (int)m_vecWorkers.size();
                    try
                    {
                        m_reader = std::thread(&CPipeline::ReaderLoop, this);
                        for(size_t i = 0; i < m_vecWorkers.size(); ++i)
                            m_vecWorkers[i].thread = std::thread(&CPipeline::WorkerLoop, this, i);
                    }
                    catch (...)
                    {
                        Stop();
                        throw;
                    }

                    for(;;)
                    {
                        SBatch* pBatch = nullptr;
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            for(;;)
                            {
                                if(!m_order.empty() && m_order.front()->bDone)
                                {
                                    pBatch = m_order.front();
                                    m_order.pop_front();
                                    break;
                                }
                                if(m_order.empty() && m_bReadDone)
                                    break;
                                const bool bReader = m_order.empty();
                                TClock::time_point start = TClock::now();
                                m_cvDone.wait_for(lock, std::chrono::milliseconds(200));
                                (bReader ? m_run.dWaitReader : m_run.dWaitWorkers) += Since(start);
                                lock.unlock();
                                bool bContinue = m_run.Tick();
                                lock.lock();
                                if(!bContinue)
                                    throw CommonLib::CExcBase("OSM export is canceled");
                            }
                        }
                        if(!pBatch)
                        {
                            if(m_ptrReadError)
                                std::rethrow_exception(m_ptrReadError);
                            break;
                        }
                        if(pBatch->ptrError)
                            std::rethrow_exception(pBatch->ptrError);

                        write(*pBatch);
                        m_run.nStageObjects += pBatch->Count();
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            m_free.push_back(pBatch);
                        }
                        m_cvFree.notify_one();
                        if(!m_run.Tick())
                            throw CommonLib::CExcBase("OSM export is canceled");
                    }
                    Stop();
                }

            private:
                void Stop()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bStop = true;
                        m_queue.clear();
                    }
                    m_cvWork.notify_all();
                    m_cvFree.notify_all();
                    if(m_reader.joinable())
                        m_reader.join();
                    for(SWorker& worker : m_vecWorkers)
                    {
                        if(worker.thread.joinable())
                            worker.thread.join();
                    }
                }

                void ReaderLoop()
                {
                    try
                    {
                        bool bEnd = false;
                        while(!bEnd)
                        {
                            SBatch* pBatch = nullptr;
                            {
                                std::unique_lock<std::mutex> lock(m_mutex);
                                m_cvFree.wait(lock, [this]() { return m_bStop || !m_free.empty(); });
                                if(m_bStop)
                                    return;
                                pBatch = m_free.back();
                                m_free.pop_back();
                            }
                            pBatch->Clear();
                            pBatch->starts.push_back(0);
                            const uint8_t* pRecord = nullptr;
                            size_t nSize = 0;
                            while(pBatch->Count() < BatchRecords && pBatch->data.size() < BatchBytes)
                            {
                                if(!m_file.NextRecord(pRecord, nSize))
                                {
                                    bEnd = true;
                                    break;
                                }
                                pBatch->data.insert(pBatch->data.end(), pRecord, pRecord + nSize);
                                pBatch->starts.push_back((uint32_t)pBatch->data.size());
                            }
                            m_run.SetRead(m_file.BytesRead(), m_file.IOSeconds());

                            std::lock_guard<std::mutex> lock(m_mutex);
                            if(pBatch->Count())
                            {
                                m_queue.push_back(pBatch);
                                m_order.push_back(pBatch);
                                m_cvWork.notify_one();
                            }
                            else
                                m_free.push_back(pBatch);
                        }
                    }
                    catch (...)
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_ptrReadError = std::current_exception();
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bReadDone = true;
                    }
                    m_cvDone.notify_all();
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
                            m_process(worker, *pBatch);
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

            private:
                CExport&     m_run;
                TProcess     m_process;
                COSMDataFile m_file;         // read by the reader thread only

                std::vector<std::unique_ptr<SBatch> > m_vecBatches;
                std::vector<SBatch*> m_free;
                std::deque<SBatch*>  m_queue;      // waiting for a worker
                std::deque<SBatch*>  m_order;      // read, in the order of the file
                bool                 m_bStop = false;
                bool                 m_bReadDone = false;
                std::exception_ptr   m_ptrReadError;
                std::mutex           m_mutex;
                std::condition_variable m_cvWork, m_cvDone, m_cvFree;
                std::thread          m_reader;
                std::vector<SWorker> m_vecWorkers;
            };

            // positive - counter clockwise; relative to the first point (the integers of the planet stay exact)
            double SignedArea(const int64_t* pX, const int64_t* pY, size_t nCount)
            {
                double dArea = 0.;
                for(size_t i = 1; i + 1 < nCount; ++i)
                    dArea += double(pX[i] - pX[0]) * double(pY[i + 1] - pY[0]) - double(pX[i + 1] - pX[0]) * double(pY[i] - pY[0]);
                return dArea / 2.;
            }

            // the workers: nodes.dat -> point features
            void ProcessNodes(CExport& run, SWorker& worker, SBatch& batch)
            {
                SOSMSplitObject& node = worker.object;
                for(size_t i = 0; i < batch.Count(); ++i)
                {
                    COSMSplitCodec::DecodeNode(batch.Record(i), batch.Size(i), run.strings, node);
                    SOSMMatch match = run.schema.MatchNode(node.Tags());
                    COSMFeatureWriter* pWriter = run.LayerWriter(match);
                    if(!pWriter)
                        continue;

                    batch.features.push_back(SFeature());
                    SFeature& feature = batch.features.back();
                    feature.nId = node.nId;
                    feature.pWriter = pWriter;
                    feature.match = std::move(match);
                    feature.bTags = true;
                    batch.AddTags(feature, node);

                    if(run.bIntegers)
                    {
                        int64_t x = node.nLon, y = node.nLat;
                        if(worker.ptrCompressor->CompressQuantized(CommonLib::shape_type_point, &x, &y, 1, nullptr, 0, worker.blob))
                        {
                            batch.AddBlob(feature, worker.blob);
                            continue;
                        }
                    }
                    CommonLib::GisXYPoint pt;
                    run.projection.Project(node.nLon / 1e7, node.nLat / 1e7, pt.x, pt.y);
                    if(worker.ptrCompressor && worker.ptrCompressor->Compress(CommonLib::shape_type_point, &pt, 1, nullptr, 0, worker.blob))
                        batch.AddBlob(feature, worker.blob);
                    else
                        feature.ptrShape = COSMGeometry::CreatePoint(pt.x, pt.y);
                }
            }

            // the workers: ways.dat -> line / polygon features
            void ProcessWays(CExport& run, SWorker& worker, SBatch& batch)
            {
                SOSMSplitObject& way = worker.object;
                for(size_t i = 0; i < batch.Count(); ++i)
                {
                    // the head first: most of the ways (without tags, not converted) need no points
                    COSMSplitCodec::DecodeWay(batch.Record(i), batch.Size(i), run.strings, way, false);
                    if(way.tags.empty() || way.nPoints < 2)
                        continue;
                    SOSMMatch line, polygon;
                    run.schema.MatchWay(way.Tags(), (way.nFlags & OSMSplit::WayClosed) != 0, line, polygon);
                    COSMFeatureWriter* pLine = run.LayerWriter(line);
                    COSMFeatureWriter* pPolygon = run.LayerWriter(polygon);
                    if(!pLine && !pPolygon)
                        continue;
                    COSMSplitCodec::DecodeWay(batch.Record(i), batch.Size(i), run.strings, way, true);
                    const uint32_t nPoints = (uint32_t)way.lons.size();
                    const size_t nFirst = batch.features.size();

                    bool bDone = false;
                    if(run.bIntegers)
                    {
                        std::vector<int64_t>& xs = worker.xs;
                        std::vector<int64_t>& ys = worker.ys;
                        xs.assign(way.lons.begin(), way.lons.end());
                        ys.assign(way.lats.begin(), way.lats.end());
                        bool bRing = nPoints >= 4 && xs.front() == xs.back() && ys.front() == ys.back();
                        bool bOk = true;
                        std::vector<TByte> lineBlob;
                        if(pLine)
                        {
                            bOk = worker.ptrCompressor->CompressQuantized(CommonLib::shape_type_polyline, xs.data(), ys.data(), nPoints, &nPoints, 1, worker.blob);
                            lineBlob.swap(worker.blob);
                        }
                        std::vector<TByte> polygonBlob;
                        if(bOk && pPolygon && bRing)
                        {
                            // outer ring: clockwise (y goes up), as COSMGeometry::CreatePolygon
                            if(SignedArea(xs.data(), ys.data(), nPoints) > 0.)
                            {
                                std::reverse(xs.begin(), xs.end());
                                std::reverse(ys.begin(), ys.end());
                            }
                            bOk = worker.ptrCompressor->CompressQuantized(CommonLib::shape_type_polygon, xs.data(), ys.data(), nPoints, &nPoints, 1, worker.blob);
                            polygonBlob.swap(worker.blob);
                        }
                        if(bOk)
                        {
                            if(pLine)
                            {
                                batch.features.push_back(SFeature());
                                batch.features.back().pWriter = pLine;
                                batch.features.back().match = line;
                                batch.AddBlob(batch.features.back(), lineBlob);
                            }
                            if(pPolygon && bRing)
                            {
                                batch.features.push_back(SFeature());
                                batch.features.back().pWriter = pPolygon;
                                batch.features.back().match = polygon;
                                batch.AddBlob(batch.features.back(), polygonBlob);
                            }
                            bDone = true;
                        }
                    }
                    if(!bDone)
                    {
                        TOSMPoints& points = worker.points;
                        points.resize(nPoints);
                        for(uint32_t p = 0; p < nPoints; ++p)
                            run.projection.Project(way.lons[p] / 1e7, way.lats[p] / 1e7, points[p].x, points[p].y);
                        if(pLine)
                        {
                            batch.features.push_back(SFeature());
                            SFeature& feature = batch.features.back();
                            feature.pWriter = pLine;
                            feature.match = line;
                            if(worker.ptrCompressor && worker.ptrCompressor->Compress(CommonLib::shape_type_polyline, points.data(), nPoints, &nPoints, 1, worker.blob))
                                batch.AddBlob(feature, worker.blob);
                            else
                                feature.ptrShape = COSMGeometry::CreatePolyline(std::vector<TOSMPoints>(1, points));
                        }
                        // the ring is closed if the nodes are (a missing node of an extract can make it open)
                        bool bRing = nPoints >= 4 && points.front().x == points.back().x && points.front().y == points.back().y;
                        if(pPolygon && bRing)
                        {
                            batch.features.push_back(SFeature());
                            SFeature& feature = batch.features.back();
                            feature.pWriter = pPolygon;
                            feature.match = polygon;
                            bool bCompressed = false;
                            if(worker.ptrCompressor)
                            {
                                TOSMPoints& ring = worker.ring;
                                ring = points;
                                if(COSMGeometry::SignedArea(ring) > 0.)
                                    std::reverse(ring.begin(), ring.end());
                                bCompressed = worker.ptrCompressor->Compress(CommonLib::shape_type_polygon, ring.data(), nPoints, &nPoints, 1, worker.blob);
                                if(bCompressed)
                                    batch.AddBlob(feature, worker.blob);
                            }
                            if(!bCompressed)
                                feature.ptrShape = COSMGeometry::CreatePolygon(std::vector<TOSMPoints>(1, points), std::vector<bool>(1, true));
                        }
                    }

                    if(batch.features.size() == nFirst)
                        continue;
                    for(size_t f = nFirst; f < batch.features.size(); ++f)
                        batch.features[f].nId = way.nId;
                    batch.AddTags(batch.features.back(), way);
                    batch.features.back().bTags = true;
                    // the tags of the object: all its features share them
                    for(size_t f = nFirst; f + 1 < batch.features.size(); ++f)
                    {
                        batch.features[f].nTagStart = batch.features.back().nTagStart;
                        batch.features[f].nTagCount = batch.features.back().nTagCount;
                    }
                }
            }

            // the main thread: the features of a batch, in the order of the file
            void WriteFeatures(CExport& run, SBatch& batch, char cType)
            {
                for(const SFeature& feature : batch.features)
                {
                    const readosm_tag* pTags = batch.tags.empty() ? nullptr : batch.tags.data() + feature.nTagStart;
                    COSMTags tags(feature.nTagCount ? pTags : nullptr, (int)feature.nTagCount);
                    if(feature.ptrShape.get())
                        feature.pWriter->Write(feature.nId, cType, feature.match, tags, feature.ptrShape);
                    else
                        feature.pWriter->WriteBlob(feature.nId, cType, feature.match, tags, batch.blobs.data() + feature.nBlob, feature.nBlobSize);
                    ++run.nFeatures;
                    if(feature.bTags)
                        run.WriteTags(cType, feature.nId, pTags, feature.nTagCount);
                }
            }

            // a forward reader of records at ascending offsets: big reads, the small gaps are read through (HDD: no seeks)
            class CForwardReader
            {
            public:
                explicit CForwardReader(COSMDataFile& file) : m_file(file) {}

                // the payload of the record at nOffset (valid until the next call)
                const uint8_t* Get(uint64_t nOffset, size_t& nSize)
                {
                    Ensure(nOffset, 10);
                    COSMRecordReader reader(m_buffer.data() + (nOffset - m_nStart), (size_t)(m_nStart + m_buffer.size() - nOffset));
                    uint64_t nPayload = reader.Varint();
                    size_t nHead = 1;
                    for(uint64_t n = nPayload; n >= 0x80; n >>= 7)
                        ++nHead;
                    Ensure(nOffset, nHead + (size_t)nPayload);
                    nSize = (size_t)nPayload;
                    return m_buffer.data() + (nOffset - m_nStart) + nHead;
                }

            private:
                void Ensure(uint64_t nOffset, size_t nNeed)
                {
                    const uint64_t nFileSize = m_file.Size();
                    uint64_t nEnd = (std::min)(nOffset + nNeed, nFileSize);
                    if(nOffset >= m_nStart && nEnd <= m_nStart + m_buffer.size())
                        return;
                    size_t nRead = (size_t)(std::min)((uint64_t)(std::max)(MemberReadBytes, nNeed), nFileSize - nOffset);
                    m_buffer.resize(nRead);
                    m_file.ReadAt(nOffset, m_buffer.data(), nRead);
                    m_nStart = nOffset;
                }

            private:
                COSMDataFile& m_file;
                std::vector<uint8_t> m_buffer;
                uint64_t m_nStart = 0;
            };

            // Multipolygons by groups: the relations of a group are kept, the member way ids are sorted, their offsets are
            // found by one forward pass over ways.idx, the ways are read by one forward pass over ways.dat.
            class CMultipolygons
            {
            public:
                CMultipolygons(CExport& run, const std::string& sFolder) : m_run(run), m_sFolder(sFolder) {}

                // the progress with the items of the multipolygons, false - canceled
                bool Tick(const char* pszStep)
                {
                    m_run.progress.sMessage = std::string("Exporting relations: multipolygons, ") + pszStep;
                    m_run.extraItems.clear();
                    AddItems(m_run.extraItems);
                    return m_run.Tick();
                }

                // a relation of a polygon layer (the record is copied)
                void Add(const uint8_t* pRecord, size_t nSize, const SOSMSplitObject& relation)
                {
                    uint32_t nRelation = (uint32_t)m_starts.size();
                    m_starts.push_back((uint64_t)m_data.size());
                    m_data.insert(m_data.end(), pRecord, pRecord + nSize);
                    m_sizes.push_back((uint32_t)nSize);
                    for(const SOSMMember& member : relation.members)
                    {
                        if(member.cType == 'w')
                            m_refs.push_back(member.nId);
                    }
                    (void)nRelation;
                }

                bool IsFull() const { return m_refs.size() >= GroupMemberRefs || m_data.size() >= GroupRelationBytes; }
                uint64_t Relations() const { return m_nRelations + m_starts.size(); }

                // the member ways of the group, the polygons; the group is cleared
                void Flush()
                {
                    if(m_starts.empty())
                        return;
                    ++m_nGroups;
                    ResolveOffsets();
                    ReadWays();
                    Build();
                    m_nRelations += m_starts.size();
                    m_data.clear();
                    m_data.shrink_to_fit();
                    m_starts.clear();
                    m_sizes.clear();
                    m_refs.clear();
                    m_ids.clear();
                    m_offsets.clear();
                    m_points.clear();
                    m_points.shrink_to_fit();
                    m_wayStart.clear();
                }

                void AddItems(std::vector<SOSMProgressItem>& items) const
                {
                    CExport::Add(items, "Member way ids", "sorted, every id once; groups of the relations: " + std::to_string(m_nGroups),
                                 m_nUniqueWays, 0, m_dSortSeconds);
                    CExport::Add(items, "Member way offsets", "one forward pass over ways.idx per group; not found: " + std::to_string(m_nNotFound),
                                 m_nUniqueWays - m_nNotFound, m_nIndexBytes, m_dIndexSeconds);
                    CExport::Add(items, "Member ways", "one forward pass over ways.dat per group (the offsets ascending, big reads)",
                                 m_nWaysRead, m_nWaysBytes, m_dWaysSeconds);
                    CExport::Add(items, "Building polygons", "rings assembled (the main thread); built " + std::to_string(m_nBuilt) + ", failed " +
                                 std::to_string(m_nFailed), m_nBuilt + m_nFailed, 0, m_dBuildSeconds);
                }

            private:
                void ResolveOffsets()
                {
                    TClock::time_point start = TClock::now();
                    m_ids = m_refs;
                    std::sort(m_ids.begin(), m_ids.end());
                    m_ids.erase(std::unique(m_ids.begin(), m_ids.end()), m_ids.end());
                    m_nUniqueWays += m_ids.size();
                    m_dSortSeconds += Since(start);
                    Tick("the offsets of the member ways (ways.idx)");

                    // ways.idx: {id, offset} sorted by id, merged with the sorted ids
                    start = TClock::now();
                    m_offsets.assign(m_ids.size(), UINT64_MAX);
                    const uint64_t nIndexBytes0 = m_nIndexBytes;
                    const double dIndexSeconds0 = m_dIndexSeconds;
                    COSMDataFile index;
                    index.Open(OSMSplit::FilePath(m_sFolder, OSMSplit::WaysIndex));
                    index.SetReadBufferSize(ReadBufferBytes);
                    struct { int64_t nId; uint64_t nOffset; } entry;
                    size_t i = 0;
                    uint64_t nEntries = 0;
                    while(i < m_ids.size() && index.Read(&entry, sizeof(entry)))
                    {
                        while(i < m_ids.size() && m_ids[i] < entry.nId)
                            ++i;
                        if(i < m_ids.size() && m_ids[i] == entry.nId)
                            m_offsets[i++] = entry.nOffset;
                        if((++nEntries & 0xFFFFF) == 0)
                        {
                            m_nIndexBytes = nIndexBytes0 + index.BytesRead();
                            m_dIndexSeconds = dIndexSeconds0 + Since(start);
                            if(!Tick("the offsets of the member ways (ways.idx)"))
                                throw CommonLib::CExcBase("OSM export is canceled");
                        }
                    }
                    for(uint64_t nOffset : m_offsets)
                        m_nNotFound += nOffset == UINT64_MAX ? 1 : 0;
                    m_nIndexBytes = nIndexBytes0 + index.BytesRead();
                    m_dIndexSeconds = dIndexSeconds0 + Since(start);
                }

                void ReadWays()
                {
                    TClock::time_point start = TClock::now();
                    // the ids by the offset (ways.dat is in the id order for a sorted file, but not always)
                    std::vector<uint32_t> order;
                    order.reserve(m_ids.size());
                    for(uint32_t i = 0; i < (uint32_t)m_ids.size(); ++i)
                    {
                        if(m_offsets[i] != UINT64_MAX)
                            order.push_back(i);
                    }
                    std::sort(order.begin(), order.end(), [this](uint32_t a, uint32_t b) { return m_offsets[a] < m_offsets[b]; });

                    COSMDataFile ways;
                    ways.Open(OSMSplit::FilePath(m_sFolder, OSMSplit::WaysData));
                    CForwardReader reader(ways);
                    m_wayStart.assign(m_ids.size() + 1, 0);
                    std::vector<uint32_t> counts(m_ids.size(), 0);
                    std::vector<uint64_t> starts(m_ids.size(), 0);
                    SOSMSplitObject way;
                    uint64_t nDone = 0;
                    const uint64_t nWaysBytes0 = m_nWaysBytes;
                    const double dWaysSeconds0 = m_dWaysSeconds;
                    for(uint32_t i : order)
                    {
                        size_t nSize = 0;
                        const uint8_t* pRecord = reader.Get(m_offsets[i], nSize);
                        COSMSplitCodec::DecodeWay(pRecord, nSize, m_run.strings, way, true);
                        starts[i] = m_points.size() / 2;
                        counts[i] = (uint32_t)way.lons.size();
                        for(size_t p = 0; p < way.lons.size(); ++p)
                        {
                            m_points.push_back(way.lons[p]);
                            m_points.push_back(way.lats[p]);
                        }
                        ++m_nWaysRead;
                        if((++nDone & 0xFFFF) == 0)
                        {
                            m_nWaysBytes = nWaysBytes0 + ways.BytesRead();
                            m_dWaysSeconds = dWaysSeconds0 + Since(start);
                            if(!Tick("reading the member ways (ways.dat)"))
                                throw CommonLib::CExcBase("OSM export is canceled");
                        }
                    }
                    m_wayPointStart.swap(starts);
                    m_wayPointCount.swap(counts);
                    m_nWaysBytes = nWaysBytes0 + ways.BytesRead();
                    m_dWaysSeconds = dWaysSeconds0 + Since(start);
                }

                void Build()
                {
                    TClock::time_point start = TClock::now();
                    const double dBuildSeconds0 = m_dBuildSeconds;
                    SOSMSplitObject relation;
                    std::vector<TOSMPoints> ways;
                    std::vector<TOSMPoints> rings;
                    std::vector<bool> outer;
                    for(size_t r = 0; r < m_starts.size(); ++r)
                    {
                        const uint8_t* pRecord = m_data.data() + m_starts[r];
                        COSMSplitCodec::DecodeRelation(pRecord, m_sizes[r], m_run.strings, relation);
                        COSMTags tags = relation.Tags();
                        SOSMMatch match = m_run.schema.MatchRelation(tags);
                        COSMFeatureWriter* pWriter = m_run.LayerWriter(match);
                        if(!pWriter)
                            continue;
                        ways.clear();
                        for(const SOSMMember& member : relation.members)
                        {
                            if(member.cType != 'w')
                                continue;
                            auto it = std::lower_bound(m_ids.begin(), m_ids.end(), member.nId);
                            if(it == m_ids.end() || *it != member.nId)
                                continue;
                            size_t i = (size_t)(it - m_ids.begin());
                            if(m_offsets[i] == UINT64_MAX || m_wayPointCount[i] == 0)
                                continue;
                            ways.push_back(TOSMPoints(m_wayPointCount[i]));
                            TOSMPoints& points = ways.back();
                            const int32_t* p = m_points.data() + m_wayPointStart[i] * 2;
                            for(uint32_t k = 0; k < m_wayPointCount[i]; ++k)
                                m_run.projection.Project(p[2 * k] / 1e7, p[2 * k + 1] / 1e7, points[k].x, points[k].y);
                        }
                        CommonLib::IGeoShapePtr ptrShape;
                        if(COSMGeometry::BuildMultipolygon(ways, rings, outer))
                            ptrShape = COSMGeometry::CreatePolygon(rings, outer);
                        if(ptrShape.get())
                        {
                            pWriter->Write(relation.nId, 'r', match, tags, ptrShape);
                            ++m_run.nFeatures;
                            ++m_nBuilt;
                            m_run.WriteTags('r', relation.nId, relation.tags.data(), relation.tags.size());
                        }
                        else
                            ++m_nFailed;
                        if((r & 0x3FFF) == 0)
                        {
                            m_dBuildSeconds = dBuildSeconds0 + Since(start);
                            if(!Tick("building the polygons"))
                                throw CommonLib::CExcBase("OSM export is canceled");
                        }
                    }
                    m_dBuildSeconds = dBuildSeconds0 + Since(start);
                }

            private:
                CExport&              m_run;
                std::string           m_sFolder;
                std::vector<uint8_t>  m_data;          // the records of the relations of the group
                std::vector<uint64_t> m_starts;
                std::vector<uint32_t> m_sizes;
                std::vector<int64_t>  m_refs;          // the member way ids (with duplicates)
                std::vector<int64_t>  m_ids;           // sorted, unique
                std::vector<uint64_t> m_offsets;       // by m_ids, UINT64_MAX - not in ways.idx
                std::vector<int32_t>  m_points;        // lon, lat of the member ways
                std::vector<uint64_t> m_wayStart;
                std::vector<uint64_t> m_wayPointStart; // by m_ids: the first point in m_points (pairs)
                std::vector<uint32_t> m_wayPointCount;

                uint64_t m_nRelations = 0, m_nGroups = 0, m_nUniqueWays = 0, m_nNotFound = 0, m_nWaysRead = 0;
                uint64_t m_nIndexBytes = 0, m_nWaysBytes = 0, m_nBuilt = 0, m_nFailed = 0;
                double   m_dSortSeconds = 0., m_dIndexSeconds = 0., m_dWaysSeconds = 0., m_dBuildSeconds = 0.;
            };
        }

        COSMSplitExport::COSMSplitExport(const COSMSchema& schema, const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap,
                                         const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                                         GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace) :
                m_schema(schema), m_settings(settings), m_ptrOSMMap(ptrOSMMap), m_excludeList(excludeList), m_ptrMap(ptrMap),
                m_ptrWorkspace(ptrWorkspace), m_projection(settings.bWebMercator)
        {
            if(!m_ptrOSMMap.get() || m_ptrOSMMap->GetFormat() != OSMFormatSplit)
                throw CommonLib::CExcBase("OSM export: the map isn't of split data");
            if(!m_ptrWorkspace.get())
                throw CommonLib::CExcBase("OSM export: the workspace is null");
        }

        void COSMSplitExport::Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
        {
            const std::string sFolder = OSMSplit::FolderOf(m_ptrOSMMap->GetPath());
            GeoDatabase::ITransactionPtr ptrTransaction;
            bool bBulkMode = false;
            try
            {
                COSMSplitMeta meta;
                meta.Load(m_ptrOSMMap->GetPath());

                CExport run(m_schema, ptrProgress, ptrCancel);
                run.projection = m_projection;
                run.progress.nPassCount = 3;
                run.strings.Load(OSMSplit::FilePath(sFolder, OSMSplit::StringsFile));

                int nThreads = m_settings.nThreads;
                if(nThreads <= 0)
                {
                    unsigned int nCores = std::thread::hardware_concurrency();
                    nThreads = nCores > 2 ? (int)nCores - 2 : 1;    // the reader and the main thread
                }
                nThreads = (std::min)(nThreads, 32);

                Geometry::ISpatialReferencePtr ptrSpatRef = m_projection.CreateSpatialReference();
                std::string sNameKey = m_settings.sNameLanguage.empty() ? std::string() : "name:" + m_settings.sNameLanguage;
                if(m_settings.compression.bEnabled)
                    run.ptrCompressor = std::make_shared<GeometryCompression::CShapeCompressor>(COSMConvertor::CompressParams(m_settings, m_ptrOSMMap));
                // the OSM integers go to the blob as they are: 1e-7 degrees = 7 decimals of longitude / latitude
                run.bIntegers = run.ptrCompressor.get() && !m_projection.IsWebMercator() && run.ptrCompressor->GetParams().nScaleExponent == 7;

                // converted: not excluded, with features (a split map is analyzed)
                auto isConverted = [this](const IOSMDataset& dataset)
                {
                    return !m_excludeList.IsLayerExcluded(dataset.GetName()) && (!m_ptrOSMMap->IsScanned() || dataset.GetFeatureCount() > 0);
                };

                m_ptrWorkspace->SetBulkLoadMode(true);
                bBulkMode = true;
                ptrTransaction = m_ptrWorkspace->StartTransaction(GeoDatabase::ttModify);

                bool bPoints = false, bWays = false, bPolygons = false;
                std::vector<IOSMLayerPtr> vecRunLayers;
                for(size_t i = 0; i < m_schema.Layers().size(); ++i)
                {
                    const SOSMLayerDef& def = m_schema.Layers()[i];
                    SLayerOut& layer = run.vecLayers[i];
                    layer.ptrLayer = m_ptrOSMMap->FindLayer(def.sName);
                    layer.type = def.geometryType;
                    if(!layer.ptrLayer.get() || !isConverted(*layer.ptrLayer))
                        continue;
                    layer.ptrWriter = std::make_shared<COSMFeatureWriter>(def, *layer.ptrLayer, m_excludeList, sNameKey, m_ptrWorkspace, ptrTransaction,
                                                                          ptrSpatRef, run.ptrCompressor, m_settings.sTempDir);
                    bPoints = bPoints || def.geometryType == OSMGeometryPoint;
                    bWays = bWays || def.geometryType != OSMGeometryPoint;
                    bPolygons = bPolygons || def.geometryType == OSMGeometryPolygon;
                    vecRunLayers.push_back(layer.ptrLayer);
                }
                for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
                {
                    IOSMTablePtr ptrTable = m_ptrOSMMap->GetTable(i);
                    STableOut& table = ptrTable->GetTableType() == OSMTableRoutes ? run.routes :
                                       ptrTable->GetTableType() == OSMTableRestrictions ? run.restrictions : run.tags;
                    table.ptrTable = ptrTable;
                    if(!isConverted(*ptrTable))
                        continue;
                    table.ptrWriter = OSMCreateTableWriter(m_ptrWorkspace, ptrTransaction, *ptrTable, m_excludeList);
                    if(ptrTable->GetTableType() == OSMTableRoutes)
                    {
                        typedef COSMTableWriter::SField F;
                        table.ptrMembers = std::make_shared<COSMTableWriter>(m_ptrWorkspace, ptrTransaction, ptrTable->GetTableName() + "_members", std::vector<F>{
                                {"route_id", GeoDatabase::dtInteger64}, {"seq", GeoDatabase::dtInteger64}, {"member_type", GeoDatabase::dtString},
                                {"member_id", GeoDatabase::dtInteger64}, {"role", GeoDatabase::dtString}});
                    }
                }
                const bool bTags = run.tags.ptrWriter.get() != nullptr;
                const bool bRelations = bPolygons || run.routes.ptrWriter.get() || run.restrictions.ptrWriter.get() || bTags;

                // 1. the nodes with tags: points
                if(bPoints || bTags)
                {
                    const std::string sPath = OSMSplit::FilePath(sFolder, OSMSplit::NodesData);
                    CPipeline pipeline(run, sPath, nThreads, [&run](SWorker& worker, SBatch& batch) { ProcessNodes(run, worker, batch); });
                    COSMDataFile size;
                    size.Open(sPath);
                    run.BeginStage(OSMStageNodes, 1, "Exporting nodes", sPath, size.Size());
                    pipeline.Run([&run](SBatch& batch)
                    {
                        run.progress.nNodes += batch.Count();
                        WriteFeatures(run, batch, 'n');
                    });
                }

                // 2. the ways: lines, polygons
                if(bWays || bTags)
                {
                    const std::string sPath = OSMSplit::FilePath(sFolder, OSMSplit::WaysData);
                    CPipeline pipeline(run, sPath, nThreads, [&run](SWorker& worker, SBatch& batch) { ProcessWays(run, worker, batch); });
                    COSMDataFile size;
                    size.Open(sPath);
                    run.BeginStage(OSMStageWays, 2, "Exporting ways", sPath, size.Size());
                    pipeline.Run([&run](SBatch& batch)
                    {
                        run.progress.nWays += batch.Count();
                        WriteFeatures(run, batch, 'w');
                    });
                }

                // 3. the relations: routes, restrictions; multipolygons by groups (their member ways: forward passes)
                if(bRelations)
                {
                    const std::string sPath = OSMSplit::FilePath(sFolder, OSMSplit::RelationsData);
                    COSMDataFile file;
                    file.Open(sPath);
                    file.SetReadBufferSize(ReadBufferBytes);
                    run.BeginStage(OSMStageMultipolygons, 3, "Exporting relations: routes, restrictions, multipolygons", sPath, file.Size());
                    run.nWorkerThreads = 0;
                    CMultipolygons multipolygons(run, sFolder);
                    SOSMSplitObject object;
                    const uint8_t* pRecord = nullptr;
                    size_t nSize = 0;
                    auto updateItems = [&]()
                    {
                        run.extraItems.clear();
                        multipolygons.AddItems(run.extraItems);
                    };
                    while(file.NextRecord(pRecord, nSize))
                    {
                        ++run.progress.nRelations;
                        ++run.nStageObjects;
                        if((run.nStageObjects & 0xFFF) == 0)
                        {
                            run.SetRead(file.BytesRead(), file.IOSeconds());
                            updateItems();
                            if(!run.Tick())
                                throw CommonLib::CExcBase("OSM export is canceled");
                        }
                        COSMSplitCodec::DecodeRelation(pRecord, nSize, run.strings, object);
                        if(object.tags.empty())
                            continue;
                        COSMTags tags = object.Tags();
                        bool bSelected = false;

                        if(bPolygons && run.LayerWriter(m_schema.MatchRelation(tags)))
                        {
                            // the tags are written with the polygon
                            multipolygons.Add(pRecord, nSize, object);
                            if(multipolygons.IsFull())
                            {
                                run.progress.sMessage = "Exporting relations: multipolygons of a group";
                                multipolygons.Flush();
                                updateItems();
                                run.progress.sMessage = "Exporting relations: routes, restrictions, multipolygons";
                            }
                        }
                        else
                        {
                            const char* pszRoute = COSMSchema::MatchRoute(tags);
                            if(pszRoute && run.IsConvertedTable(run.routes, "route", pszRoute))
                                bSelected = true;
                            const char* pszRestriction = COSMSchema::MatchRestriction(tags);
                            if(pszRestriction && run.IsConvertedTable(run.restrictions, "restriction", pszRestriction))
                                bSelected = true;
                            if(bSelected)
                                run.WriteTags('r', object.nId, object.tags.data(), object.tags.size());
                        }

                        const char* pszRoute = COSMSchema::MatchRoute(tags);
                        if(pszRoute && run.IsConvertedTable(run.routes, "route", pszRoute))
                        {
                            OSMWriteRoute(*run.routes.ptrWriter, *run.routes.ptrMembers, object.nId, tags, pszRoute, m_settings.sNameLanguage,
                                          object.members.data(), (int)object.members.size());
                            ++run.nFeatures;
                        }
                        const char* pszRestriction = COSMSchema::MatchRestriction(tags);
                        if(pszRestriction && run.IsConvertedTable(run.restrictions, "restriction", pszRestriction))
                        {
                            OSMWriteRestriction(*run.restrictions.ptrWriter, object.nId, tags, pszRestriction, object.members.data(), (int)object.members.size());
                            ++run.nFeatures;
                        }
                    }
                    run.SetRead(file.BytesRead(), file.IOSeconds());
                    run.progress.sMessage = "Exporting relations: multipolygons";
                    multipolygons.Flush();
                    updateItems();
                }

                // the spatial indexes, the commit
                run.BeginStage(OSMStageFinish, 0, "Saving tables", std::string(), 0);
                run.nWorkerThreads = 0;
                run.progress.nStepTotal = vecRunLayers.size();
                std::vector<GeoDatabase::ITablePtr> vecTables(vecRunLayers.size());
                for(size_t i = 0; i < vecRunLayers.size(); ++i)
                {
                    SLayerOut& layer = run.vecLayers[m_schema.FindLayerIndex(vecRunLayers[i]->GetName())];
                    run.progress.nStepDone = i;
                    run.progress.sMessage = "Building the spatial index: " + vecRunLayers[i]->GetDisplayName();
                    run.throttle.Reset();
                    run.Send();
                    run.CheckCancel();
                    TClock::time_point start = TClock::now();
                    layer.ptrWriter->Writer()->Finish();
                    vecTables[i] = layer.ptrWriter->Writer()->GetTable();
                    CExport::Add(run.finishItems, "Spatial index: " + vecRunLayers[i]->GetDisplayName(), "packed from the boxes -> " + vecRunLayers[i]->GetTableName(),
                                 layer.ptrWriter->Writer()->GetRowCount(), 0, Since(start));
                }
                for(STableOut* pTable : {&run.routes, &run.restrictions, &run.tags})
                {
                    if(pTable->ptrWriter.get())
                        pTable->ptrWriter->Finish();
                    if(pTable->ptrMembers.get())
                        pTable->ptrMembers->Finish();
                }
                // the small scale layers read only their rows: the indexes of their query columns
                for(size_t i = 0; i < vecRunLayers.size(); ++i)
                {
                    if(COSMMapStyle::OverviewLayers(vecRunLayers[i]->GetName()).empty())
                        continue;
                    run.progress.sMessage = "Indexing the small scale layers: " + vecRunLayers[i]->GetDisplayName();
                    run.throttle.Reset();
                    run.Send();
                    run.CheckCancel();
                    double dSeconds = OSMCreateOverviewIndexes(ptrTransaction, *vecRunLayers[i], vecTables[i]);
                    CExport::Add(run.finishItems, "Index of the small scale layers: " + vecRunLayers[i]->GetDisplayName(),
                                 "the query columns of the overview layers -> " + vecRunLayers[i]->GetTableName(), 0, 0, dSeconds);
                }
                run.progress.sMessage = "Committing the transaction";
                run.Send();
                TClock::time_point commitStart = TClock::now();
                ptrTransaction->Commit();
                CExport::Add(run.finishItems, "Commit", "the database file", 0, 0, Since(commitStart));
                ptrTransaction.reset();
                bBulkMode = false;
                m_ptrWorkspace->SetBulkLoadMode(false);

                // the layers with the symbols of the metadata
                if(m_ptrMap.get() && m_settings.bAddLayersToMap)
                {
                    COSMMapLayers mapLayers(m_ptrMap, m_projection);
                    for(size_t i = 0; i < vecRunLayers.size(); ++i)
                        mapLayers.AddLayer(*vecRunLayers[i], vecTables[i], meta.GetStyle(vecRunLayers[i]->GetName()));
                }

                run.progress.nStepDone = run.progress.nStepTotal;
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
                CommonLib::CExcBase::RegenExc("Failed to export the OSM split data {0}", sFolder, exc);
                throw;
            }
        }
    }
}
