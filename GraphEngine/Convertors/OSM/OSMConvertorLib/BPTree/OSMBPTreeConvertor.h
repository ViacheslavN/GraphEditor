#pragma once

// OSM -> bptreedb B+ trees (the new conversion, step by step: the nodes first).
//
// The node file (OSMBPTreeNodeFile.h): the header page and the node tree TNodeTree: int64 id -> SOSMPointE7.
//
// The file is read by its own thread (CReaderThread: COSMReader into CNodeQueue), the nodes go to the loading thread
// by batches. The tree is built bottom-up from the nodes in the id order:
//   - several threads (SOSMBPTreeSettings::nBuildThreads > 1): the leaves are encoded by the threads of
//     CParallelLeafBuilder, written in the order, the inner levels are built by CNodeTreeBuilder;
//   - one thread: TBPMap::BulkLoad (CBulkSource).
// Planet / extracts are sorted by id; the nodes which come after a bigger id (an unsorted .osm) are kept aside and
// inserted into the built tree at the end.
//
// The parts (BPTree/):
//   OSMPointE7.h, OSMPointCompressor.h, OSMNodeLeafEncoder.h, OSMNodeLeafCompressor.h, OSMBPTreeTypes.h - the tree types
//   OSMBPTreeSettings.h      SOSMBPTreeSettings, SOSMBPTreeNodeHeader
//   OSMBPTreeNodeFile.h      COSMBPTreeNodeFile, the header page
//   OSMNodeQueue.h           CNodeQueue (the reading thread -> batches)
//   OSMReaderThread.h        CReaderThread
//   OSMBulkSource.h          CBulkSource (BulkLoad)
//   OSMNodeTreeBuilder.h     CNodeTreeBuilder (the inner levels of the written leaves)
//   OSMParallelLeafBuilder.h CParallelLeafBuilder (the leaves encoded by several threads)
//   OSMTagTreeTypes.h        TTagTree: node id -> tags (string, zlib leaves)
//   OSMTagWriter.h           CTagWriter (the tag tree written by its own thread from a queue)
//   OSMBPTreeTagFile.h       COSMBPTreeTagFile, the header page of the tag file
//
// The tags of the nodes (SOSMBPTreeSettings::bTags) go to the tag file (TagFilePath: <name>.osmtags) - the reading thread
// hands them to the tag thread (CTagWriter), it fills its tree by BulkLoad while the nodes are converted.
//
// The ways (ConvertWays, the second pass: only the blocks with ways are read):
//   - the reading thread (CWayReader) collects the ways into batches (SWayBatch, nWayBatchRefs node refs), their tags go to
//     the tag thread of the ways (<name>.osmwaytags);
//   - the resolving threads (CWayResolver) sort the ways of a batch by their smallest node ref and the refs of a way by the
//     id, find the nodes in the node tree (every thread its own pager / cache over the shared node file) and compress the
//     shapes; the shapes of a batch are sorted by the way id;
//   - the loading thread takes the batches in the order and fills the shape tree (<name>.osmways) by BulkLoad.
//   OSMWayTreeTypes.h  TWayShapeTree: way id -> the compressed shape (GeometryCompression, 1e-7 degrees, blob)
//   OSMWayResolver.h   SWayBatch, SWayShapes, CWayResolver
//   OSMWayReader.h     CWayReader, CWayReaderThread
//   OSMBPTreeWayFile.h COSMBPTreeWayFile, the header page of the way file
//   OSMSharedLeafCache.h, OSMSharedNodeReader.h - the node file read by all the resolving threads (one leaf cache)
//   OSMJobPipeline.h   TJobPipeline (the jobs of the resolving threads, the results in the order)
//   OSMBlobSource.h    TBlobSource (BulkLoad of the shape / relation tree from the results)
//
// The relations (ConvertRelations, the third pass: only the blocks with relations are read):
//   - the reading thread (CRelationReader) collects the relations into batches (SRelationBatch, nRelBatchMembers members),
//     their tags go to the tag thread of the relations (<name>.osmreltags);
//   - the resolving threads (CRelationResolver) find the node members of a batch in the node file and then the way members
//     in the way file, both in the ascending id order; the node file and the way file are shared by all the threads
//     (CSharedNodeReader, CSharedWayReader: one leaf index and one cache of the decoded leaves each); the records
//     (OSMRelationRecord.h: the shape - a polygon of the rings for multipolygon / boundary (CRingBuilder), otherwise the
//     member ways as a polyline or the member nodes as a multipoint; the members with the roles) are sorted by the id;
//   - the loading thread fills the relation tree (<name>.osmrels) by BulkLoad.
//   OSMSharedWayReader.h  CSharedWayReader
//   OSMRingBuilder.h      CRingBuilder (the rings of the member ways)
//   OSMRelationRecord.h   the record format, DecodeRelationRecord
//   OSMRelationResolver.h SRelationBatch, SRelationRecords, CRelationResolver
//   OSMRelationReader.h   CRelationReader
//   OSMBPTreeRelationFile.h COSMBPTreeRelationFile, the header page of the relation file

#include "../OSMConvertorLib.h"
#include "OSMBPTreeTypes.h"
#include "OSMBPTreeSettings.h"
#include "OSMBPTreeNodeFile.h"
#include "OSMBPTreeTagFile.h"
#include "OSMBPTreeWayFile.h"
#include "OSMBPTreeRelationFile.h"

#include <string>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            struct SOSMBPTreeNodeResult
            {
                SOSMBPTreeNodeHeader header;
                uint64_t nRead = 0;                 // the nodes of the file
                uint64_t nUnsorted = 0;             // came after a bigger id: inserted at the end
                uint64_t nDuplicates = 0;           // an id met again: skipped
                uint64_t nFileSize = 0;             // the node file
                double   dSeconds = 0.;
                double   dReadSeconds = 0.;         // the loading thread waited for the file (the reading is the bottleneck)
                double   dLoadSeconds = 0.;         // building the tree (the loading thread)
                double   dUnsortedSeconds = 0.;     // inserting the unsorted nodes

                // the tag file (empty - no tags, SOSMBPTreeSettings::bTags)
                std::string         sTagFile;
                SOSMBPTreeTagHeader tagHeader;
                uint64_t nTagUnsorted = 0;          // tagged nodes out of the id order: inserted at the end
                uint64_t nTagDuplicates = 0;
                uint64_t nTagFileSize = 0;
                double   dTagSeconds = 0.;          // the tag thread (without waiting for the tags)
                double   dTagFinishSeconds = 0.;    // the loading thread waited for the tag thread after the reading
            };

            struct SOSMBPTreeWayResult
            {
                std::string         sWayFile;
                SOSMBPTreeWayHeader header;
                uint64_t nRead = 0;                 // the ways of the file
                uint64_t nUnsorted = 0;             // ways with a shape out of the id order: inserted at the end
                uint64_t nDuplicates = 0;
                uint64_t nFileSize = 0;
                double   dSeconds = 0.;
                double   dResolveSeconds = 0.;      // the resolving threads, CPU of all of them
                double   dWaitSeconds = 0.;         // the loading thread waited for the resolved jobs

                // the tags of the ways (empty - no tags, SOSMBPTreeSettings::bWayTags)
                std::string         sTagFile;
                SOSMBPTreeTagHeader tagHeader;      // nNodeCount - the ways with tags
                uint64_t nTagUnsorted = 0;
                uint64_t nTagDuplicates = 0;
                uint64_t nTagFileSize = 0;
                double   dTagSeconds = 0.;
                double   dTagFinishSeconds = 0.;
            };

            struct SOSMBPTreeRelationResult
            {
                std::string              sRelationFile;
                SOSMBPTreeRelationHeader header;
                uint64_t nRead = 0;                 // the relations of the file
                uint64_t nUnsorted = 0;
                uint64_t nDuplicates = 0;
                uint64_t nFileSize = 0;
                uint64_t nNodeLookups = 0;          // the node file finds (the unique node members of a job)
                uint64_t nWayLookups = 0;
                uint64_t nNodeLeafDecodes = 0;      // the leaves decoded by the shared readers
                uint64_t nWayLeafDecodes = 0;
                double   dSeconds = 0.;
                double   dResolveSeconds = 0.;      // the resolving threads, CPU of all of them
                double   dWaitSeconds = 0.;         // the loading thread waited for the resolved jobs

                // the tags of the relations (empty - no tags, SOSMBPTreeSettings::bRelationTags)
                std::string         sTagFile;
                SOSMBPTreeTagHeader tagHeader;      // nNodeCount - the relations with tags
                uint64_t nTagUnsorted = 0;
                uint64_t nTagDuplicates = 0;
                uint64_t nTagFileSize = 0;
                double   dTagSeconds = 0.;
                double   dTagFinishSeconds = 0.;
            };

            struct SOSMBPTreeResult
            {
                SOSMBPTreeNodeResult     nodes;
                SOSMBPTreeWayResult      ways;
                SOSMBPTreeRelationResult relations;    // empty - SOSMBPTreeSettings::bRelations is off
            };

            class COSMBPTreeConvertor
            {
            public:
                explicit COSMBPTreeConvertor(const SOSMBPTreeSettings& settings = SOSMBPTreeSettings());

                // the nodes of an .osm / .pbf file (sOSMPath, UTF-8) into the node file sNodeFile (UTF-8, replaced) and the tags
                // of the nodes (TagFilePath); canceled - throws "canceled", the files are incomplete
                SOSMBPTreeNodeResult ConvertNodes(const std::string& sOSMPath, const std::string& sNodeFile,
                                                  IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

                // the ways of the file: their shapes (the node refs resolved through the node file sNodeFile, made by ConvertNodes)
                // into WayFilePath(sNodeFile), their tags into WayTagFilePath(sNodeFile)
                SOSMBPTreeWayResult ConvertWays(const std::string& sOSMPath, const std::string& sNodeFile,
                                                IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

                // the relations of the file: their records (the members, the shapes made of the nodes of sNodeFile and the ways
                // of WayFilePath(sNodeFile), made by ConvertNodes / ConvertWays) into RelationFilePath(sNodeFile), their tags
                // into RelationTagFilePath(sNodeFile)
                SOSMBPTreeRelationResult ConvertRelations(const std::string& sOSMPath, const std::string& sNodeFile,
                                                          IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

                // the nodes, the ways, the relations (SOSMBPTreeSettings::bRelations): three passes over the file, the second
                // and the third ones read only the blocks with ways / relations
                SOSMBPTreeResult Convert(const std::string& sOSMPath, const std::string& sNodeFile,
                                         IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

                // the stages so far (SOSMProgress::vecStages: Convert keeps the stages of the nodes)
                const SOSMProgress& GetProgress() const { return m_progress; }

                static std::string FormatResult(const SOSMBPTreeNodeResult& result);
                static std::string FormatResult(const SOSMBPTreeWayResult& result);
                static std::string FormatResult(const SOSMBPTreeRelationResult& result);
                static std::string FormatResult(const SOSMBPTreeResult& result);

            private:
                SOSMBPTreeSettings m_settings;
                SOSMProgress m_progress;
                int m_nPassCount = 1;
            };

            // all the files of a conversion into the node file sNodeFile (to delete an incomplete one)
            inline std::vector<std::string> ConversionFiles(const std::string& sNodeFile)
            {
                return { sNodeFile, TagFilePath(sNodeFile), WayFilePath(sNodeFile), WayTagFilePath(sNodeFile),
                         RelationFilePath(sNodeFile), RelationTagFilePath(sNodeFile) };
            }
        }
    }
}
