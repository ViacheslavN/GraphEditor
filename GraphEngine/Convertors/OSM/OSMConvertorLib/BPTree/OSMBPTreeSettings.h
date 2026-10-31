#pragma once

// The settings of the OSM -> B+ tree conversion and the header of the node file.

#include "OSMPointE7.h"
#include "OSMNodeLeafCompressor.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            struct SOSMBPTreeSettings
            {
                uint32_t nPageSize = 8192;          // the page of the storage = the node of the tree
                uint32_t nTreeCacheNodes = 256;     // the tree nodes kept in memory
                uint32_t nPagerCachePages = 1024;   // the pages cached by the pager
                uint32_t nBatchNodes = 64 * 1024;   // the nodes handed from the reading thread to the loading one at once
                uint32_t nQueueBatches = 256;       // the batches waiting for the loading thread (the reading waits then): 16M nodes,
                                                    // 256 MB - the tree is built while the reading thread reads the file
                int      nThreads = 0;              // the decoding threads of a .pbf file, 0 - by the processors
                int      nBuildThreads = 0;         // the threads encoding the leaves of the tree: 0 - by the processors (2..16),
                                                    // 1 - one thread (TBPMap::BulkLoad)
                uint32_t nChunkNodes = 2 * 1024 * 1024;   // the nodes of a job of an encoding thread (its last leaf is partly filled)
                int32_t  nCompressRatio = TNodeLeafCompressor::DefaultCompressRatio;   // the count limit of the first leaf (BulkLoad):
                                                    // page size * ratio / raw entry size

                // the tag tree (OSMTagTreeTypes.h): its own file (TagFilePath), written by its own thread (CTagWriter)
                bool     bTags = true;
                uint32_t nTagPageSize = 16384;      // the page of the tag file
                uint32_t nTagMaxLen = 8000;         // the longest tags of a node (at most half a page), longer ones are cut
                uint32_t nTagTreeCacheNodes = 64;
                uint32_t nTagPagerCachePages = 256;
                uint32_t nTagBatchBytes = 4 * 1024 * 1024;   // the tags handed to the tag thread at once
                uint32_t nTagQueueBatches = 32;     // the batches waiting for the tag thread (the reading waits then)
                int32_t  nTagCompressLevel = 6;     // zlib

                // the ways (ConvertWays): the shapes (OSMWayTreeTypes.h) and the tags (the tag tree) in their own files
                uint32_t nWayPageSize = 8192;       // the page of the shape file
                uint32_t nWayTreeCacheNodes = 64;
                uint32_t nWayPagerCachePages = 1024;
                uint32_t nWayBatchRefs = 8 * 1024 * 1024;   // the node refs of the ways of a job of a resolving thread (~200 MB
                                                    // of a thread): a leaf of the node tree is decoded once per job
                int      nResolveThreads = 0;       // the threads resolving the node refs: 0 - by the processors (2..16)
                uint32_t nNodeCacheLeaves = 0;      // the decoded leaves of the node tree shared by all the resolving threads
                                                    // (CSharedNodeReader, ~35 KB a leaf): 0 - all the leaves of the node file
                                                    // if they fit in nNodeCacheMB, otherwise as many as fit
                uint32_t nNodeCacheMB = 0;          // the memory of the leaf cache when nNodeCacheLeaves = 0: 0 - half of the
                                                    // physical memory (Asia: ~37 GB for all its 1.1M leaves)
                bool     bWayTags = true;           // the tags of the ways: <name>.osmwaytags

                // the relations (ConvertRelations): the records (the shape and the members, OSMRelationRecord.h) and the tags
                // in their own files; the node file and the way file are read by all the resolving threads (nResolveThreads)
                bool     bRelations = true;         // Convert: the third pass
                uint32_t nRelPageSize = 8192;       // the page of the relation file
                uint32_t nRelTreeCacheNodes = 64;
                uint32_t nRelPagerCachePages = 1024;
                uint32_t nRelBatchMembers = 1024 * 1024;   // the members of the relations of a job of a resolving thread
                uint32_t nRelNodeCacheMB = 0;       // the shared cache of the node leaves: 0 - a quarter of the physical memory
                uint32_t nRelWayCacheMB = 0;        // the shared cache of the way leaves: 0 - a quarter of the physical memory
                bool     bRelationTags = true;      // the tags of the relations: <name>.osmreltags
            };

            // the header of the node file (page 0)
            struct SOSMBPTreeNodeHeader
            {
                static constexpr uint64_t Magic = 0x31444E5042534F4Dull;   // "MOSBPND1"
                static constexpr uint32_t Version = 2;     // 2: the storage addresses in 512-byte blocks

                uint32_t    nPageSize = 0;
                int64_t     nTreeInfoPage = -1;
                uint64_t    nNodeCount = 0;
                int64_t     nMinId = 0;
                int64_t     nMaxId = 0;
                SOSMPointE7 minPoint;                // the bounds (1e-7 degrees)
                SOSMPointE7 maxPoint;

                // a node of the tree: the count, the id range, the bounds
                void Add(int64_t nId, const SOSMPointE7& point)
                {
                    if (nNodeCount == 0)
                    {
                        nMinId = nMaxId = nId;
                        minPoint = maxPoint = point;
                    }
                    else
                    {
                        nMinId = (std::min)(nMinId, nId);
                        nMaxId = (std::max)(nMaxId, nId);
                        minPoint.lat = (std::min)(minPoint.lat, point.lat);
                        minPoint.lon = (std::min)(minPoint.lon, point.lon);
                        maxPoint.lat = (std::max)(maxPoint.lat, point.lat);
                        maxPoint.lon = (std::max)(maxPoint.lon, point.lon);
                    }
                    nNodeCount += 1;
                }
            };

            // the header of the tag file (page 0)
            struct SOSMBPTreeTagHeader
            {
                static constexpr uint64_t Magic = 0x3147545042534F4Dull;   // "MOSBPTG1"
                static constexpr uint32_t Version = 2;     // 2: the storage addresses in 512-byte blocks

                uint32_t nPageSize = 0;
                int64_t  nTreeInfoPage = -1;
                uint32_t nMaxLen = 0;
                uint64_t nNodeCount = 0;             // the nodes with tags
                uint64_t nTagCount = 0;              // their tags
                uint64_t nRawBytes = 0;              // the size of the values (the encoded tags)
                uint64_t nTruncated = 0;             // the nodes with the tags cut to nMaxLen
            };

            // the header of the way shape file (page 0)
            struct SOSMBPTreeWayHeader
            {
                static constexpr uint64_t Magic = 0x3157575042534F4Dull;   // "MOSBPWW1"
                static constexpr uint32_t Version = 2;     // 2: the storage addresses in 512-byte blocks

                uint32_t nPageSize = 0;
                int64_t  nTreeInfoPage = -1;
                uint64_t nWayCount = 0;              // the ways with a shape
                uint64_t nRefCount = 0;              // the node refs of all the ways read
                uint64_t nMissingRefs = 0;           // refs to the nodes which aren't in the node file (skipped)
                uint64_t nWaysWithMissing = 0;       // ways with such refs
                uint64_t nWaysWithoutShape = 0;      // ways with less than 2 points (no shape)
                uint64_t nShapeBytes = 0;            // the compressed shapes
                int64_t  nMinId = 0;
                int64_t  nMaxId = 0;
            };

            // the header of the relation file (page 0)
            struct SOSMBPTreeRelationHeader
            {
                static constexpr uint64_t Magic = 0x314C525042534F4Dull;   // "MOSBPRL1"
                static constexpr uint32_t Version = 2;     // 2: the storage addresses in 512-byte blocks

                uint32_t nPageSize = 0;
                int64_t  nTreeInfoPage = -1;
                uint64_t nRelationCount = 0;         // all the relations (every one has a record)
                uint64_t nMemberCount = 0;
                uint64_t nNodeMembers = 0;
                uint64_t nWayMembers = 0;
                uint64_t nRelationMembers = 0;
                uint64_t nMissingNodes = 0;          // node members which aren't in the node file
                uint64_t nMissingWays = 0;           // way members which aren't in the way file (or have no shape)
                uint64_t nWithShape = 0;             // the relations with a shape
                uint64_t nPolygons = 0;              // of them: polygons (multipolygon / boundary)
                uint64_t nAreasWithoutRings = 0;     // multipolygon / boundary relations without a closed ring
                uint64_t nUnclosedWays = 0;          // their member ways which don't close a ring
                uint64_t nRecordBytes = 0;           // the records
                uint64_t nShapeBytes = 0;            // of them: the shapes
                int64_t  nMinId = 0;
                int64_t  nMaxId = 0;
            };

            // the files of a node file <name>.osmnodes (the extension is replaced, otherwise added):
            inline std::string RelatedFilePath(const std::string& sNodeFile, const char* pszExt)
            {
                static const std::string sNodeExt = ".osmnodes";
                if (sNodeFile.size() > sNodeExt.size() && sNodeFile.compare(sNodeFile.size() - sNodeExt.size(), sNodeExt.size(), sNodeExt) == 0)
                    return sNodeFile.substr(0, sNodeFile.size() - sNodeExt.size()) + pszExt;
                return sNodeFile + pszExt;
            }
            // the tags of the nodes: <name>.osmtags
            inline std::string TagFilePath(const std::string& sNodeFile) { return RelatedFilePath(sNodeFile, ".osmtags"); }
            // the shapes of the ways: <name>.osmways
            inline std::string WayFilePath(const std::string& sNodeFile) { return RelatedFilePath(sNodeFile, ".osmways"); }
            // the tags of the ways: <name>.osmwaytags
            inline std::string WayTagFilePath(const std::string& sNodeFile) { return RelatedFilePath(sNodeFile, ".osmwaytags"); }
            // the records of the relations: <name>.osmrels
            inline std::string RelationFilePath(const std::string& sNodeFile) { return RelatedFilePath(sNodeFile, ".osmrels"); }
            // the tags of the relations: <name>.osmreltags
            inline std::string RelationTagFilePath(const std::string& sNodeFile) { return RelatedFilePath(sNodeFile, ".osmreltags"); }
        }
    }
}
