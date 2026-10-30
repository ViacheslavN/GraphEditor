#include "OSMBPTreeConvertor.h"
#include "OSMBPTreeNodeFile.h"
#include "OSMBPTreeUtils.h"
#include "OSMNodeQueue.h"
#include "OSMBulkSource.h"
#include "OSMReaderThread.h"
#include "OSMNodeTreeBuilder.h"
#include "OSMParallelLeafBuilder.h"
#include "OSMTagWriter.h"
#include "OSMWayResolver.h"
#include "OSMWayReader.h"
#include "OSMBPTreeWayFile.h"
#include "OSMBlobSource.h"
#include "OSMBPTreeProgress.h"
#include "../OSMReader.h"
#include "../OSMProgressThrottle.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            COSMBPTreeConvertor::COSMBPTreeConvertor(const SOSMBPTreeSettings& settings) : m_settings(settings)
            {
                m_settings.nBatchNodes = (std::max)(1024u, m_settings.nBatchNodes);
                m_settings.nQueueBatches = (std::max)(1u, m_settings.nQueueBatches);
            }

            SOSMBPTreeNodeResult COSMBPTreeConvertor::ConvertNodes(const std::string& sOSMPath, const std::string& sNodeFile,
                                                                   IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
            {
                SOSMBPTreeNodeResult result;
                auto start = TClock::now();
                try
                {
                    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    ptrStorage->Open(sNodeFile.c_str(), true, 0, m_settings.nPageSize);
                    std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrStorage, m_settings.nPagerCachePages));

                    SOSMBPTreeNodeHeader& header = result.header;
                    header.nPageSize = m_settings.nPageSize;
                    int64_t nHeaderPage = ptrPager->GetNewFilePageAddr();     // 0: the first page of the new file

                    CNodeTreeBuilder tree(-1, ptrPager, ptrAlloc, m_settings.nTreeCacheNodes, m_settings.nPageSize, true);
                    tree.InnitTree(CreateCompressParams(m_settings.nCompressRatio), true, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
                    header.nTreeInfoPage = tree.GetPageBTreeInfo();

                    // the progress
                    SOSMProgress& progress = m_progress;
                    progress = SOSMProgress();
                    progress.stage = OSMStageNodes;
                    progress.sMessage = "Nodes into the B+ tree";
                    progress.nPass = 1;
                    progress.nPassCount = m_nPassCount;
                    progress.nFileSize = FileSize(sOSMPath);
                    COSMProgressThrottle throttle;
                    auto stageStart = TClock::now();

                    CNodeQueue queue(m_settings, ptrCancel);

                    // the tags: their own file and thread (the reading thread hands them over)
                    std::unique_ptr<CTagWriter> ptrTags;
                    if (m_settings.bTags)
                    {
                        result.sTagFile = TagFilePath(sNodeFile);
                        ptrTags.reset(new CTagWriter(m_settings, result.sTagFile));
                        queue.SetTagWriter(ptrTags.get());
                    }
                    int nBuildThreads = m_settings.nBuildThreads > 0 ? m_settings.nBuildThreads
                                      : (int)(std::min)(16u, (std::max)(2u, std::thread::hardware_concurrency() / 2));
                    CBulkSource* pSource = nullptr;
                    CParallelLeafBuilder* pParallel = nullptr;
                    double dQueueWait = 0.;
                    auto buildItems = [&]()
                    {
                        std::vector<SOSMProgressItem> items;
                        AddReaderItems(items, queue.ReaderStats(), sOSMPath);
                        double dWait = pSource ? pSource->WaitSeconds() : dQueueWait;
                        if (pParallel)
                        {
                            items.push_back(Item("Encoding leaves", std::to_string(pParallel->Threads()) + " threads, CPU time of all of them",
                                                 pParallel->Leaves(), 0, pParallel->EncodeSeconds(), true));
                            items.push_back(Item("Writing leaves", sNodeFile + " (the loading thread)", header.nNodeCount,
                                                 (uint64_t)pParallel->Leaves() * m_settings.nPageSize, pParallel->WriteSeconds()));
                            items.push_back(Item("Waiting for the leaves", "the loading thread waits for the encoding threads", 0, 0, pParallel->WaitSeconds()));
                        }
                        else if (pSource)
                            items.push_back(Item("Nodes tree", sNodeFile + " (BulkLoad, the loading thread)", header.nNodeCount, 0,
                                                 (std::max)(0., Since(stageStart) - dWait)));
                        items.push_back(Item("Waiting for the file", "the loading thread waits for the read nodes", 0, 0, dWait));
                        if (ptrTags)
                        {
                            items.push_back(Item("Tags tree", result.sTagFile + " (the tag thread, BulkLoad)", ptrTags->Nodes(), ptrTags->RawBytes(),
                                                 ptrTags->WorkSeconds()));
                            items.push_back(Item("Waiting for tags", "the tag thread waits for the read tags", 0, 0, ptrTags->WaitSeconds()));
                        }
                        if (queue.UnsortedCount() != 0)
                            items.push_back(Item("Unsorted nodes", "kept in memory, inserted at the end", queue.UnsortedCount(), 0, 0.));
                        return items;
                    };
                    auto send = [&](bool bForce)
                    {
                        if (!ptrProgress.get() || (!bForce && !throttle.IsTime()))
                            return;
                        progress.nNodes = queue.Read();
                        progress.nFeatures = header.nNodeCount;
                        progress.nBytesRead = queue.BytesRead();
                        progress.nBytesTotal = queue.BytesTotal();
                        progress.dStageSeconds = Since(stageStart);
                        progress.vecItems = buildItems();
                        ptrProgress->UpdateStatusInfo(progress);
                    };
                    auto endStage = [&]()
                    {
                        SOSMStageSummary summary;
                        summary.stage = progress.stage;
                        summary.sMessage = progress.sMessage;
                        summary.dSeconds = Since(stageStart);
                        summary.vecItems = buildItems();
                        progress.vecStages.push_back(summary);
                    };
                    send(true);

                    // the reading thread -> the leaves encoded by several threads -> the inner levels
                    if (nBuildThreads > 1)
                    {
                        CReaderThread reader(queue, sOSMPath);
                        auto loadStart = TClock::now();
                        bool bCanceled = false;
                        {
                            CParallelLeafBuilder builder(tree.CompressParams(), tree.NodePagePayload(), nBuildThreads, m_settings.nChunkNodes, ptrPager);
                            pParallel = &builder;
                            TBatch batch;
                            for (;;)
                            {
                                if (!queue.Continue())
                                {
                                    bCanceled = true;
                                    break;
                                }
                                send(false);
                                batch.clear();
                                if (!queue.Pop(batch, dQueueWait))
                                    break;
                                for (const SNode& node : batch)
                                    header.Add(node.id, node.point);
                                builder.Add(batch);
                            }
                            if (!bCanceled)
                                builder.Finish();
                            builder.Stop();
                            send(true);
                            result.dReadSeconds = dQueueWait;

                            reader.Join(bCanceled);
                            queue.RethrowError();
                            if (bCanceled || (ptrCancel.get() && !ptrCancel->Continue()))
                                throw CommonLib::CExcBase("the conversion of the nodes is canceled");

                            tree.BuildInner(builder.Level());
                            endStage();
                            pParallel = nullptr;
                        }
                        result.dLoadSeconds = Since(loadStart) - dQueueWait;
                    }
                    else
                    // the reading thread -> BulkLoad
                    {
                        CReaderThread reader(queue, sOSMPath);
                        CBulkSource source(queue, header, [&]() { send(false); });
                        pSource = &source;

                        auto loadStart = TClock::now();
                        tree.BulkLoad(source);
                        result.dLoadSeconds = Since(loadStart) - source.WaitSeconds();
                        result.dReadSeconds = source.WaitSeconds();

                        reader.Join(source.IsCanceled());
                        pSource = nullptr;
                        queue.RethrowError();
                        if (source.IsCanceled() || (ptrCancel.get() && !ptrCancel->Continue()))
                            throw CommonLib::CExcBase("the conversion of the nodes is canceled");
                    }
                    result.nRead = queue.Read();

                    // the tag tree: the reading is done, the tag thread finishes its file
                    if (ptrTags)
                    {
                        auto tagStart = TClock::now();
                        ptrTags->Finish();
                        result.dTagFinishSeconds = Since(tagStart);
                        result.tagHeader = ptrTags->Header();
                        result.nTagUnsorted = ptrTags->Unsorted();
                        result.nTagDuplicates = ptrTags->Duplicates();
                        result.dTagSeconds = ptrTags->WorkSeconds();
                        result.nTagFileSize = FileSize(result.sTagFile);
                    }
                    if (nBuildThreads <= 1)
                    {
                        send(true);
                        endStage();
                    }

                    // the nodes out of the id order
                    std::vector<SNode>& unsorted = queue.Unsorted();
                    result.nUnsorted = unsorted.size();
                    if (!unsorted.empty())
                    {
                        progress.stage = OSMStageSortNodes;
                        progress.sMessage = "Unsorted nodes into the B+ tree";
                        progress.nPass = 0;
                        stageStart = TClock::now();
                        send(true);

                        std::stable_sort(unsorted.begin(), unsorted.end(), [](const SNode& a, const SNode& b) { return a.id < b.id; });
                        for (size_t i = 0; i < unsorted.size(); ++i)
                        {
                            const SNode& node = unsorted[i];
                            if (!tree.find(node.id).IsNull())
                            {
                                result.nDuplicates += 1;
                                continue;
                            }
                            tree.insert(node.id, node.point);
                            header.Add(node.id, node.point);
                            if ((i & 0xFFFF) == 0)
                            {
                                if (ptrCancel.get() && !ptrCancel->Continue())
                                    throw CommonLib::CExcBase("the conversion of the nodes is canceled");
                                send(false);
                            }
                        }
                        result.dUnsortedSeconds = Since(stageStart);
                        endStage();
                    }

                    // the tree, the header
                    progress.stage = OSMStageFinish;
                    progress.sMessage = "Writing the node file";
                    stageStart = TClock::now();
                    send(true);

                    tree.Flush();
                    WriteNodeHeader(ptrPager, nHeaderPage, header);
                    ptrPager->Flush();
                    ptrStorage->Close();
                    endStage();

                    result.nFileSize = FileSize(sNodeFile);
                    result.dSeconds = Since(start);

                    progress.sMessage = "Done";
                    progress.vecItems.clear();
                    progress.dStageSeconds = 0.;
                    progress.nFeatures = header.nNodeCount;
                    if (ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to convert the nodes of {0} into {1}", sOSMPath, sNodeFile, exc);
                    throw;
                }
                return result;
            }

            SOSMBPTreeWayResult COSMBPTreeConvertor::ConvertWays(const std::string& sOSMPath, const std::string& sNodeFile,
                                                                 IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
            {
                SOSMBPTreeWayResult result;
                result.sWayFile = WayFilePath(sNodeFile);
                auto start = TClock::now();
                try
                {
                    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));

                    // the node file: one storage, the resolving threads read it through their own pagers
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrNodeStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    ptrNodeStorage->Open(sNodeFile.c_str(), false, 0, m_settings.nPageSize);
                    SOSMBPTreeNodeHeader nodeHeader;
                    {
                        std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrNodeStorage, 4));
                        nodeHeader = ReadNodeHeader(ptrPager, 0);
                    }
                    if (nodeHeader.nPageSize != m_settings.nPageSize)
                        throw CommonLib::CExcBase("the page size of the node file is {0}, not {1}", nodeHeader.nPageSize, m_settings.nPageSize);

                    // the shape file
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 2, bptreedb::storage::IStorageCipherPtr()));
                    ptrStorage->Open(result.sWayFile.c_str(), true, 0, m_settings.nWayPageSize);
                    std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrStorage, m_settings.nWayPagerCachePages));
                    SOSMBPTreeWayHeader& header = result.header;
                    header.nPageSize = m_settings.nWayPageSize;
                    int64_t nHeaderPage = ptrPager->GetNewFilePageAddr();     // 0: the first page of the new file

                    TWayShapeTree tree(-1, ptrPager, ptrAlloc, m_settings.nWayTreeCacheNodes, m_settings.nWayPageSize, ShapeInlineSize(m_settings.nWayPageSize), true);
                    tree.InnitTree(CreateShapeCompressParams(), true, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
                    header.nTreeInfoPage = tree.GetPageBTreeInfo();

                    // the tags of the ways: their own file and thread
                    std::unique_ptr<CTagWriter> ptrTags;
                    if (m_settings.bWayTags)
                    {
                        result.sTagFile = WayTagFilePath(sNodeFile);
                        ptrTags.reset(new CTagWriter(m_settings, result.sTagFile));
                    }

                    int nResolveThreads = m_settings.nResolveThreads > 0 ? m_settings.nResolveThreads
                                        : (int)(std::min)(16u, (std::max)(2u, std::thread::hardware_concurrency() / 2));
                    CWayResolver resolver(m_settings, ptrNodeStorage, nodeHeader, nResolveThreads);
                    CWayReader reader(m_settings, resolver, ptrTags.get(), ptrCancel);

                    // the progress: the stages of ConvertNodes stay (Convert)
                    SOSMProgress& progress = m_progress;
                    progress.stage = OSMStageWays;
                    progress.sMessage = "Ways into the B+ tree";
                    progress.nPass = m_nPassCount > 1 ? 2 : 1;
                    progress.nPassCount = m_nPassCount;
                    progress.nFileSize = FileSize(sOSMPath);
                    progress.nBytesRead = 0;
                    progress.nBytesTotal = 0;
                    COSMProgressThrottle throttle;
                    auto stageStart = TClock::now();
                    typedef TBlobSource<CWayResolver, SWayShapes> TSource;
                    TSource* pSource = nullptr;
                    uint64_t nWaysTaken = 0;

                    auto buildItems = [&]()
                    {
                        std::vector<SOSMProgressItem> items;
                        AddReaderItems(items, reader.ReaderStats(), sOSMPath);
                        items.push_back(Item("Resolving node refs", std::to_string(resolver.Threads()) + " threads, CPU time of all of them",
                                             resolver.Lookups(), 0, resolver.ResolveSeconds(), true));
                        const CSharedNodeReader& nodes = resolver.Nodes();
                        items.push_back(Item("Decoding node leaves", std::to_string(nodes.LeafCount()) + " leaves, the shared cache " +
                                             std::to_string(nodes.CacheLeaves()) + " leaves: " + std::to_string(nodes.CacheHits()) + " found, " +
                                             std::to_string(nodes.CacheWaits()) + " waited for another thread",
                                             nodes.Decodes(), 0, nodes.DecodeNs() / 1e9, true));
                        double dWait = resolver.WaitSeconds();
                        items.push_back(Item("Shapes tree", result.sWayFile + " (BulkLoad, the loading thread)", pSource ? pSource->Written() : header.nWayCount,
                                             pSource ? pSource->Bytes() : header.nShapeBytes, (std::max)(0., Since(stageStart) - dWait)));
                        items.push_back(Item("Waiting for shapes", "the loading thread waits for the resolving threads", 0, 0, dWait));
                        if (ptrTags)
                        {
                            items.push_back(Item("Way tags tree", result.sTagFile + " (the tag thread, BulkLoad)", ptrTags->Nodes(), ptrTags->RawBytes(),
                                                 ptrTags->WorkSeconds()));
                            items.push_back(Item("Waiting for tags", "the tag thread waits for the read tags", 0, 0, ptrTags->WaitSeconds()));
                        }
                        return items;
                    };
                    auto send = [&](bool bForce)
                    {
                        if (!ptrProgress.get() || (!bForce && !throttle.IsTime()))
                            return;
                        progress.nWays = reader.Read();
                        progress.nFeatures = pSource ? pSource->Written() : header.nWayCount;
                        progress.nBytesRead = reader.BytesRead();
                        progress.nBytesTotal = reader.BytesTotal();
                        progress.dStageSeconds = Since(stageStart);
                        progress.vecItems = buildItems();
                        ptrProgress->UpdateStatusInfo(progress);
                    };
                    auto endStage = [&]()
                    {
                        SOSMStageSummary summary;
                        summary.stage = progress.stage;
                        summary.sMessage = progress.sMessage;
                        summary.dSeconds = Since(stageStart);
                        summary.vecItems = buildItems();
                        progress.vecStages.push_back(summary);
                    };
                    send(true);

                    // the reading thread -> the resolving threads -> the shapes in the order -> BulkLoad (this thread)
                    {
                        CWayReaderThread readerThread(reader, sOSMPath, [&]()
                        {
                            reader.Stop();
                            resolver.Stop();
                            if (ptrTags)
                                ptrTags->Stop();
                        });
                        TSource source(resolver, tree, ptrCancel, [&](const SWayShapes& shapes)
                        {
                            header.nRefCount += shapes.nRefs;
                            header.nMissingRefs += shapes.nMissingRefs;
                            header.nWaysWithMissing += shapes.nWaysWithMissing;
                            header.nWaysWithoutShape += shapes.nWaysWithoutShape;
                            nWaysTaken += shapes.nWays;
                        }, [&]() { send(false); });
                        pSource = &source;
                        tree.BulkLoad(source);

                        readerThread.Join(source.IsCanceled());
                        reader.RethrowError();
                        if (source.IsCanceled() || (ptrCancel.get() && !ptrCancel->Continue()))
                            throw CommonLib::CExcBase("the conversion of the ways is canceled");

                        // the ways out of the id order
                        result.nUnsorted = source.AsideCount();
                        result.nDuplicates = source.InsertAside();
                        header.nWayCount = source.Written();
                        header.nShapeBytes = source.Bytes();
                        header.nMinId = source.MinId();
                        header.nMaxId = source.MaxId();
                        result.nRead = nWaysTaken;
                        pSource = nullptr;
                    }
                    resolver.Stop();
                    result.dResolveSeconds = resolver.ResolveSeconds();
                    result.dWaitSeconds = resolver.WaitSeconds();
                    send(true);
                    endStage();

                    // the files
                    progress.stage = OSMStageFinish;
                    progress.sMessage = "Writing the way files";
                    stageStart = TClock::now();
                    send(true);

                    tree.Flush();
                    WriteWayHeader(ptrPager, nHeaderPage, header);
                    ptrPager->Flush();
                    ptrStorage->Close();

                    if (ptrTags)
                    {
                        auto tagStart = TClock::now();
                        ptrTags->Finish();
                        result.dTagFinishSeconds = Since(tagStart);
                        result.tagHeader = ptrTags->Header();
                        result.nTagUnsorted = ptrTags->Unsorted();
                        result.nTagDuplicates = ptrTags->Duplicates();
                        result.dTagSeconds = ptrTags->WorkSeconds();
                        result.nTagFileSize = FileSize(result.sTagFile);
                    }
                    endStage();

                    result.nFileSize = FileSize(result.sWayFile);
                    result.dSeconds = Since(start);

                    progress.sMessage = "Done";
                    progress.vecItems.clear();
                    progress.dStageSeconds = 0.;
                    progress.nFeatures = header.nWayCount;
                    if (ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to convert the ways of {0} into {1}", sOSMPath, result.sWayFile, exc);
                    throw;
                }
                return result;
            }

            SOSMBPTreeResult COSMBPTreeConvertor::Convert(const std::string& sOSMPath, const std::string& sNodeFile,
                                                          IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
            {
                SOSMBPTreeResult result;
                m_nPassCount = m_settings.bRelations ? 3 : 2;
                try
                {
                    result.nodes = ConvertNodes(sOSMPath, sNodeFile, ptrProgress, ptrCancel);
                    result.ways = ConvertWays(sOSMPath, sNodeFile, ptrProgress, ptrCancel);
                    if (m_settings.bRelations)
                        result.relations = ConvertRelations(sOSMPath, sNodeFile, ptrProgress, ptrCancel);
                }
                catch (...)
                {
                    m_nPassCount = 1;
                    throw;
                }
                m_nPassCount = 1;
                return result;
            }

            std::string COSMBPTreeConvertor::FormatResult(const SOSMBPTreeWayResult& result)
            {
                const SOSMBPTreeWayHeader& h = result.header;
                char szText[2048];
                snprintf(szText, sizeof(szText),
                         "Ways: %llu with a shape (read %llu, unsorted %llu, duplicates %llu, without a shape %llu)\n"
                         "Node refs: %llu, missing %llu (in %llu ways)\n"
                         "Way file: %.1f MB, the shapes %.1f MB, %.2f bytes per ref\n"
                         "Time: %.1f s (resolving %.1f s CPU of all the threads, the writer waited %.1f s)\n",
                         (unsigned long long)h.nWayCount, (unsigned long long)result.nRead, (unsigned long long)result.nUnsorted,
                         (unsigned long long)result.nDuplicates, (unsigned long long)h.nWaysWithoutShape,
                         (unsigned long long)h.nRefCount, (unsigned long long)h.nMissingRefs, (unsigned long long)h.nWaysWithMissing,
                         result.nFileSize / (1024. * 1024.), h.nShapeBytes / (1024. * 1024.),
                         h.nRefCount ? (double)result.nFileSize / h.nRefCount : 0.,
                         result.dSeconds, result.dResolveSeconds, result.dWaitSeconds);
                std::string sText = szText;
                if (!result.sTagFile.empty())
                {
                    const SOSMBPTreeTagHeader& t = result.tagHeader;
                    snprintf(szText, sizeof(szText),
                             "Way tags: %llu ways with %llu tags (unsorted %llu, duplicates %llu, cut to %u bytes %llu)\n"
                             "Way tag file: %.1f MB, the tags %.1f MB, compression %.2f\n",
                             (unsigned long long)t.nNodeCount, (unsigned long long)t.nTagCount, (unsigned long long)result.nTagUnsorted,
                             (unsigned long long)result.nTagDuplicates, t.nMaxLen, (unsigned long long)t.nTruncated,
                             result.nTagFileSize / (1024. * 1024.), t.nRawBytes / (1024. * 1024.),
                             result.nTagFileSize ? (double)t.nRawBytes / result.nTagFileSize : 0.);
                    sText += szText;
                }
                return sText;
            }

            std::string COSMBPTreeConvertor::FormatResult(const SOSMBPTreeResult& result)
            {
                std::string sText = FormatResult(result.nodes) + "\n" + FormatResult(result.ways);
                if (!result.relations.sRelationFile.empty())
                    sText += "\n" + FormatResult(result.relations);
                return sText;
            }

            std::string COSMBPTreeConvertor::FormatResult(const SOSMBPTreeNodeResult& result)
            {
                const SOSMBPTreeNodeHeader& h = result.header;
                char szText[2048];
                double dBytesPerNode = h.nNodeCount ? (double)result.nFileSize / h.nNodeCount : 0.;
                double dRaw = (double)h.nNodeCount * (sizeof(int64_t) + 2 * sizeof(int32_t));
                snprintf(szText, sizeof(szText),
                         "Nodes: %llu (read %llu, unsorted %llu, duplicates %llu)\n"
                         "Ids: %lld .. %lld\n"
                         "Bounds: lat %.7f .. %.7f, lon %.7f .. %.7f\n"
                         "File: %.1f MB, %.2f bytes per node (raw 16), compression %.2f\n"
                         "Time: %.1f s (building the tree %.1f s, waiting for the file %.1f s, unsorted nodes %.1f s)\n",
                         (unsigned long long)h.nNodeCount, (unsigned long long)result.nRead, (unsigned long long)result.nUnsorted,
                         (unsigned long long)result.nDuplicates, (long long)h.nMinId, (long long)h.nMaxId,
                         h.minPoint.Lat(), h.maxPoint.Lat(), h.minPoint.Lon(), h.maxPoint.Lon(),
                         result.nFileSize / (1024. * 1024.), dBytesPerNode, result.nFileSize ? dRaw / result.nFileSize : 0.,
                         result.dSeconds, result.dLoadSeconds, result.dReadSeconds, result.dUnsortedSeconds);
                std::string sText = szText;
                if (!result.sTagFile.empty())
                {
                    const SOSMBPTreeTagHeader& t = result.tagHeader;
                    snprintf(szText, sizeof(szText),
                             "Tags: %llu nodes with %llu tags (unsorted %llu, duplicates %llu, cut to %u bytes %llu)\n"
                             "Tag file: %.1f MB, the tags %.1f MB, compression %.2f, %.2f bytes per tagged node\n"
                             "Tag thread: %.1f s, the reading waited for it %.1f s at the end\n",
                             (unsigned long long)t.nNodeCount, (unsigned long long)t.nTagCount, (unsigned long long)result.nTagUnsorted,
                             (unsigned long long)result.nTagDuplicates, t.nMaxLen, (unsigned long long)t.nTruncated,
                             result.nTagFileSize / (1024. * 1024.), t.nRawBytes / (1024. * 1024.),
                             result.nTagFileSize ? (double)t.nRawBytes / result.nTagFileSize : 0.,
                             t.nNodeCount ? (double)result.nTagFileSize / t.nNodeCount : 0.,
                             result.dTagSeconds, result.dTagFinishSeconds);
                    sText += szText;
                }
                return sText;
            }
        }
    }
}
