#pragma once

// The B+ tree types of the conversion of OSM into bptreedb (BPtreeLib):
//
//   nodes:  int64 node id -> SOSMPointE7 (latitude, longitude in 1e-7 degrees, as OSM stores them: lossless)
//     keys   - TValueDiffEncoder + CUnsignedNumLenEncoder: the ids are sorted, the first id raw, then the
//              differences NumLen coded (an extract / the planet: mostly small steps)
//     values - TPointCompressor: the first point raw, then the differences of the latitudes and of the longitudes,
//              each coordinate in its own CSignedNumLenEncoder stream (a difference can be negative)
//     inner  - the keys as the leaves, the child links TBaseValueEncoder + CUnsignedNumLenEncoder
//
// The parts: OSMPointE7.h (SOSMPointE7), OSMPointCompressor.h (TPointCompressor), OSMNodeLeafEncoder.h (CNodeLeafEncoder),
// OSMNodeLeafCompressor.h (TNodeIdEncoder, TNodeLeafCompressor).

#include "../../../../../CommonLib/CommonLib.h"
#include "../../../../../bptreedb/BPtreeLib/BPMap.h"
#include "../../../../../bptreedb/BPtreeLib/BPTreeInnerNode.h"
#include "../../../../../bptreedb/BPtreeLib/BPTreeMapLeafNode.h"
#include "../../../../../bptreedb/BPtreeLib/BaseNodeCompressor.h"
#include "../../../../../bptreedb/BPtreeLib/BaseEncoder.h"
#include "../../../../../bptreedb/BPtreeLib/OrderedKey.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/CompressorParams.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../../../../bptreedb/StorageLib/Storage.h"
#include "OSMPointE7.h"
#include "OSMPointCompressor.h"
#include "OSMNodeLeafEncoder.h"
#include "OSMNodeLeafCompressor.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            typedef bptreedb::TBaseValueEncoder<int64_t, bptreedb::CUnsignedNumLenEncoder> TLinkEncoder;              // child addresses
            typedef bptreedb::TOrderedComp<int64_t> TNodeIdComp;

            typedef bptreedb::BPTreeLeafNodeMap<int64_t, SOSMPointE7, TNodeLeafCompressor> TNodeLeaf;
            typedef bptreedb::BPTreeInnerNode<int64_t,
                    bptreedb::TBaseNodeCompressor<int64_t, int64_t, TNodeIdEncoder, TLinkEncoder> > TNodeInner;
            typedef bptreedb::TBPMap<int64_t, SOSMPointE7, TNodeIdComp, bptreedb::storage::IFilePager, TNodeInner, TNodeLeaf> TNodeTree;

            // the compressor parameters of the node tree; nCompressRatio - "CompressRatio" of the leaves (TNodeLeafCompressor)
            inline bptreedb::TCompressorParamsBasePtr CreateCompressParams(int32_t nCompressRatio = TNodeLeafCompressor::DefaultCompressRatio)
            {
                bptreedb::TCompressorParamsBasePtr ptrParams(new bptreedb::CompressorParamsBase());
                for (bptreedb::ECompressParams id : { bptreedb::eInnerKey, bptreedb::eInnerValue, bptreedb::eLeafKey, bptreedb::eLeafValue })
                {
                    bptreedb::TCompressorParamsPtr ptrComp(new bptreedb::CompressorParams());
                    if (id == bptreedb::eLeafKey)
                        ptrComp->SetIntParam("CompressRatio", nCompressRatio);
                    ptrParams->AddCompressParams(ptrComp, id);
                }
                return ptrParams;
            }
        }
    }
}
