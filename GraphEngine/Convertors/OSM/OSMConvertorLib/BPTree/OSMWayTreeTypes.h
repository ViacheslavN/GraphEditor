#pragma once

// The shape tree of the ways: int64 way id -> the shape of the way, a compressed shape blob (GeometryCompression,
// ShapeFormat.h: a polyline of one part, the coordinates are the OSM integers - 1e-7 degrees, scale exponent 7: lossless).
//
//   keys   - TNodeIdEncoder (sorted ids, NumLen of the differences)
//   values - BlobValue (CBlobValueEncoder): a shape up to the inline size is kept in the leaf, a bigger one in stream pages
//            (TBPBlobValueMap); the blobs are compressed already
//   inner  - TNodeInner
//
// The points of a way are its node refs in the order of the way, the refs to the nodes which aren't in the node file are
// skipped; a way with less than 2 points has no shape. Closed ways stay polylines (the polygons are made by the export).

#include "OSMBPTreeTypes.h"

#include "../../../../../bptreedb/BPtreeLib/BPBlobValueMap.h"
#include "../../../../../bptreedb/BPtreeLib/BlobVal.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/BlobCompressor/BlobValueEncoder.h"
#include "../../../../../GeometryCompression/ShapeCompressor.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            typedef bptreedb::BPTreeLeafNodeMap<int64_t, bptreedb::BlobValue,
                    bptreedb::TBaseNodeCompressor<int64_t, bptreedb::BlobValue, TNodeIdEncoder, bptreedb::CBlobValueEncoder> > TShapeLeaf;
            typedef TNodeInner TShapeInner;
            typedef bptreedb::TBPBlobValueMap<int64_t, TNodeIdComp, bptreedb::storage::IFilePager, TShapeInner, TShapeLeaf> TWayShapeTree;

            const int ShapeScaleExponent = 7;    // the OSM 1e-7 degrees

            // the compressor parameters of the shape tree
            inline bptreedb::TCompressorParamsBasePtr CreateShapeCompressParams()
            {
                bptreedb::TCompressorParamsBasePtr ptrParams(new bptreedb::CompressorParamsBase());
                for (bptreedb::ECompressParams id : { bptreedb::eInnerKey, bptreedb::eInnerValue, bptreedb::eLeafKey, bptreedb::eLeafValue })
                    ptrParams->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), id);
                return ptrParams;
            }

            // the inline size of the shape tree for a page size (an entry has to fit in a leaf twice, TBPBlobValueMap)
            inline uint32_t ShapeInlineSize(uint32_t nPageSize)
            {
                return (std::min)((uint32_t)TWayShapeTree::DefaultMaxInlineSize, nPageSize / 4);
            }
        }
    }
}
