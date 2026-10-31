// COSMBPTreeConvertor::ConvertRelations: the third pass (the relations), see OSMBPTreeConvertor.h.

#include "OSMBPTreeConvertor.h"
#include "OSMBPTreeUtils.h"
#include "OSMBPTreeProgress.h"
#include "OSMTagWriter.h"
#include "OSMWayReader.h"
#include "OSMRelationResolver.h"
#include "OSMRelationReader.h"
#include "OSMBPTreeRelationFile.h"
#include "OSMBlobSource.h"
#include "../OSMReader.h"
#include "../OSMProgressThrottle.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <thread>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            namespace
            {
                void AddCounts(SOSMBPTreeRelationHeader& h, const SOSMBPTreeRelationHeader& job)
                {
                    h.nMemberCount += job.nMemberCount;
                    h.nNodeMembers += job.nNodeMembers;
                    h.nWayMembers += job.nWayMembers;
                    h.nRelationMembers += job.nRelationMembers;
                    h.nMissingNodes += job.nMissingNodes;
                    h.nMissingWays += job.nMissingWays;
                    h.nWithShape += job.nWithShape;
                    h.nPolygons += job.nPolygons;
                    h.nAreasWithoutRings += job.nAreasWithoutRings;
                    h.nUnclosedWays += job.nUnclosedWays;
                    h.nShapeBytes += job.nShapeBytes;
                }

                std::string CacheInfo(size_t nLeaves, uint64_t nCapacity, uint64_t nHits, uint64_t nWaits)
                {
                    return std::to_string(nLeaves) + " leaves, the shared cache " + std::to_string(nCapacity) + " leaves: " +
                           std::to_string(nHits) + " found, " + std::to_string(nWaits) + " waited for another thread";
                }
            }

            SOSMBPTreeRelationResult COSMBPTreeConvertor::ConvertRelations(const std::string& sOSMPath, const std::string& sNodeFile,
                                                                           IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
            {
                SOSMBPTreeRelationResult result;
                result.sRelationFile = RelationFilePath(sNodeFile);
                auto start = TClock::now();
                try
                {
                    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));

                    // the node file and the way file: one storage each, the resolving threads read them through their own pagers
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrNodeStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    ptrNodeStorage->Open(sNodeFile.c_str(), false, 0, m_settings.nPageSize);
                    SOSMBPTreeNodeHeader nodeHeader;
                    {
                        std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrNodeStorage, 4));
                        nodeHeader = ReadNodeHeader(ptrPager, 0);
                    }
                    if (nodeHeader.nPageSize != m_settings.nPageSize)
                        throw CommonLib::CExcBase("the page size of the node file is {0}, not {1}", nodeHeader.nPageSize, m_settings.nPageSize);

                    std::string sWayFile = WayFilePath(sNodeFile);
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrWayStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 2, bptreedb::storage::IStorageCipherPtr()));
                    ptrWayStorage->Open(sWayFile.c_str(), false, 0, m_settings.nWayPageSize);
                    SOSMBPTreeWayHeader wayHeader;
                    {
                        std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrWayStorage, 4));
                        wayHeader = ReadWayHeader(ptrPager, 0);
                    }
                    if (wayHeader.nPageSize != m_settings.nWayPageSize)
                        throw CommonLib::CExcBase("the page size of the way file is {0}, not {1}", wayHeader.nPageSize, m_settings.nWayPageSize);

                    // the relation file
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 3, bptreedb::storage::IStorageCipherPtr()));
                    ptrStorage->Open(result.sRelationFile.c_str(), true, 0, m_settings.nRelPageSize);
                    std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrStorage, m_settings.nRelPagerCachePages));
                    SOSMBPTreeRelationHeader& header = result.header;
                    header.nPageSize = m_settings.nRelPageSize;
                    int64_t nHeaderPage = ptrPager->GetNewFilePageAddr();     // 0: the first page of the new file

                    TRelationTree tree(-1, ptrPager, ptrAlloc, m_settings.nRelTreeCacheNodes, m_settings.nRelPageSize, ShapeInlineSize(m_settings.nRelPageSize), true);
                    tree.InnitTree(CreateShapeCompressParams(), true, 0, bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
                    header.nTreeInfoPage = tree.GetPageBTreeInfo();

                    std::unique_ptr<CTagWriter> ptrTags;
                    if (m_settings.bRelationTags)
                    {
                        result.sTagFile = RelationTagFilePath(sNodeFile);
                        ptrTags.reset(new CTagWriter(m_settings, result.sTagFile));
                    }

                    int nResolveThreads = m_settings.nResolveThreads > 0 ? m_settings.nResolveThreads
                                        : (int)(std::min)(16u, (std::max)(2u, std::thread::hardware_concurrency() / 2));
                    CRelationResolver resolver(m_settings, ptrNodeStorage, nodeHeader, ptrWayStorage, wayHeader, nResolveThreads);
                    CRelationReader reader(m_settings, resolver, ptrTags.get(), ptrCancel);

                    // the progress: the stages of the nodes and the ways stay (Convert)
                    SOSMProgress& progress = m_progress;
                    progress.stage = OSMStageMultipolygons;
                    progress.sMessage = "Relations into the B+ tree";
                    progress.nPass = m_nPassCount > 2 ? 3 : 1;
                    progress.nPassCount = m_nPassCount;
                    progress.nFileSize = FileSize(sOSMPath);
                    progress.nBytesRead = 0;
                    progress.nBytesTotal = 0;
                    COSMProgressThrottle throttle;
                    auto stageStart = TClock::now();
                    typedef TBlobSource<CRelationResolver, SRelationRecords> TSource;
                    TSource* pSource = nullptr;

                    auto buildItems = [&]()
                    {
                        std::vector<SOSMProgressItem> items;
                        AddReaderItems(items, reader.ReaderStats(), sOSMPath);
                        items.push_back(Item("Resolving members", std::to_string(resolver.Threads()) + " threads, CPU time of all of them; " +
                                             std::to_string(resolver.NodeLookups()) + " nodes, " + std::to_string(resolver.WayLookups()) + " ways",
                                             resolver.NodeLookups() + resolver.WayLookups(), 0, resolver.ResolveSeconds(), true));
                        const auto& nodes = resolver.Nodes().Cache();
                        items.push_back(Item("Decoding node leaves", CacheInfo(resolver.Nodes().LeafCount(), nodes.Capacity(), nodes.Hits(), nodes.Waits()),
                                             nodes.Decodes(), 0, nodes.DecodeNs() / 1e9, true));
                        const auto& ways = resolver.Ways().Cache();
                        items.push_back(Item("Decoding way leaves", CacheInfo(resolver.Ways().LeafCount(), ways.Capacity(), ways.Hits(), ways.Waits()),
                                             ways.Decodes(), 0, ways.DecodeNs() / 1e9, true));
                        double dWait = resolver.WaitSeconds();
                        items.push_back(Item("Relations tree", result.sRelationFile + " (BulkLoad, the loading thread)",
                                             pSource ? pSource->Written() : header.nRelationCount, pSource ? pSource->Bytes() : header.nRecordBytes,
                                             (std::max)(0., Since(stageStart) - dWait)));
                        items.push_back(Item("Waiting for relations", "the loading thread waits for the resolving threads", 0, 0, dWait));
                        if (ptrTags)
                        {
                            items.push_back(Item("Relation tags tree", result.sTagFile + " (the tag thread, BulkLoad)", ptrTags->Nodes(), ptrTags->RawBytes(),
                                                 ptrTags->WorkSeconds()));
                            items.push_back(Item("Waiting for tags", "the tag thread waits for the read tags", 0, 0, ptrTags->WaitSeconds()));
                        }
                        return items;
                    };
                    auto send = [&](bool bForce)
                    {
                        if (!ptrProgress.get() || (!bForce && !throttle.IsTime()))
                            return;
                        progress.nRelations = reader.Read();
                        progress.nFeatures = pSource ? pSource->Written() : header.nRelationCount;
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

                    // the reading thread -> the resolving threads -> the records in the order -> BulkLoad (this thread)
                    {
                        CWayReaderThread readerThread(reader, sOSMPath, [&]()
                        {
                            reader.Stop();
                            resolver.Stop();
                            if (ptrTags)
                                ptrTags->Stop();
                        });
                        uint64_t nTaken = 0;
                        TSource source(resolver, tree, ptrCancel, [&](const SRelationRecords& records)
                        {
                            AddCounts(header, records.counts);
                            nTaken += records.counts.nRelationCount;
                        }, [&]() { send(false); });
                        pSource = &source;
                        tree.BulkLoad(source);

                        readerThread.Join(source.IsCanceled());
                        reader.RethrowError();
                        if (source.IsCanceled() || (ptrCancel.get() && !ptrCancel->Continue()))
                            throw CommonLib::CExcBase("the conversion of the relations is canceled");

                        result.nUnsorted = source.AsideCount();
                        result.nDuplicates = source.InsertAside();
                        header.nRelationCount = source.Written();
                        header.nRecordBytes = source.Bytes();
                        header.nMinId = source.MinId();
                        header.nMaxId = source.MaxId();
                        result.nRead = nTaken;
                        pSource = nullptr;
                    }
                    resolver.Stop();
                    result.dResolveSeconds = resolver.ResolveSeconds();
                    result.dWaitSeconds = resolver.WaitSeconds();
                    result.nNodeLookups = resolver.NodeLookups();
                    result.nWayLookups = resolver.WayLookups();
                    result.nNodeLeafDecodes = resolver.Nodes().Cache().Decodes();
                    result.nWayLeafDecodes = resolver.Ways().Cache().Decodes();
                    send(true);
                    endStage();

                    // the files
                    progress.stage = OSMStageFinish;
                    progress.sMessage = "Writing the relation files";
                    stageStart = TClock::now();
                    send(true);

                    tree.Flush();
                    WriteRelationHeader(ptrPager, nHeaderPage, header);
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

                    result.nFileSize = FileSize(result.sRelationFile);
                    result.dSeconds = Since(start);

                    progress.sMessage = "Done";
                    progress.vecItems.clear();
                    progress.dStageSeconds = 0.;
                    progress.nFeatures = header.nRelationCount;
                    if (ptrProgress.get())
                        ptrProgress->UpdateStatusInfo(progress);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to convert the relations of {0} into {1}", sOSMPath, result.sRelationFile, exc);
                    throw;
                }
                return result;
            }

            std::string COSMBPTreeConvertor::FormatResult(const SOSMBPTreeRelationResult& result)
            {
                const SOSMBPTreeRelationHeader& h = result.header;
                char szText[2048];
                snprintf(szText, sizeof(szText),
                         "Relations: %llu (read %llu, unsorted %llu, duplicates %llu), with a shape %llu, polygons %llu\n"
                         "Areas without a closed ring: %llu, their ways out of the rings %llu\n"
                         "Members: %llu: nodes %llu (missing %llu), ways %llu (missing %llu), relations %llu\n"
                         "Lookups: %llu nodes (%llu leaves decoded), %llu ways (%llu leaves decoded)\n"
                         "Relation file: %.1f MB, the records %.1f MB, the shapes %.1f MB\n"
                         "Time: %.1f s (resolving %.1f s CPU of all the threads, the writer waited %.1f s)\n",
                         (unsigned long long)h.nRelationCount, (unsigned long long)result.nRead, (unsigned long long)result.nUnsorted,
                         (unsigned long long)result.nDuplicates, (unsigned long long)h.nWithShape, (unsigned long long)h.nPolygons,
                         (unsigned long long)h.nAreasWithoutRings, (unsigned long long)h.nUnclosedWays,
                         (unsigned long long)h.nMemberCount, (unsigned long long)h.nNodeMembers, (unsigned long long)h.nMissingNodes,
                         (unsigned long long)h.nWayMembers, (unsigned long long)h.nMissingWays, (unsigned long long)h.nRelationMembers,
                         (unsigned long long)result.nNodeLookups, (unsigned long long)result.nNodeLeafDecodes,
                         (unsigned long long)result.nWayLookups, (unsigned long long)result.nWayLeafDecodes,
                         result.nFileSize / (1024. * 1024.), h.nRecordBytes / (1024. * 1024.), h.nShapeBytes / (1024. * 1024.),
                         result.dSeconds, result.dResolveSeconds, result.dWaitSeconds);
                std::string sText = szText;
                if (!result.sTagFile.empty())
                {
                    const SOSMBPTreeTagHeader& t = result.tagHeader;
                    snprintf(szText, sizeof(szText),
                             "Relation tags: %llu relations with %llu tags (unsorted %llu, duplicates %llu, cut to %u bytes %llu)\n"
                             "Relation tag file: %.1f MB, the tags %.1f MB, compression %.2f\n",
                             (unsigned long long)t.nNodeCount, (unsigned long long)t.nTagCount, (unsigned long long)result.nTagUnsorted,
                             (unsigned long long)result.nTagDuplicates, t.nMaxLen, (unsigned long long)t.nTruncated,
                             result.nTagFileSize / (1024. * 1024.), t.nRawBytes / (1024. * 1024.),
                             result.nTagFileSize ? (double)t.nRawBytes / result.nTagFileSize : 0.);
                    sText += szText;
                }
                return sText;
            }
        }
    }
}
