#pragma once

// The tag tree of the OSM -> B+ tree conversion: int64 node id -> the tags of the node as one string.
//
//   keys   - TNodeIdEncoder (as the node tree: sorted ids, NumLen of the differences)
//   values - StringValue, the leaves compressed by zlib (CZlibCutStringComp, TCutNodeCompressor: a leaf is encoded once
//            and cut where the page is full); only the nodes with tags are in the tree
//   inner  - TNodeInner (the same as the node tree)
//
// The tags of a node: key US value RS key US value ... (US = 0x1F, RS = 0x1E; the control characters aren't allowed
// in OSM tags - XML 1.0 can't have them). A value is at most nMaxLen bytes (the string tree keeps one value in half
// a page): longer tags are cut at a tag boundary (counted).

#include "OSMBPTreeTypes.h"

#include "../../../../../bptreedb/BPtreeLib/BPStringValueMap.h"
#include "../../../../../bptreedb/BPtreeLib/CutNodeCompressor.h"
#include "../../../../../bptreedb/BPtreeLib/StringVal.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/StringCompressor/ZlibCutStringEncoder.h"

#include <string>
#include <utility>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            typedef bptreedb::BPTreeLeafNodeMap<int64_t, bptreedb::StringValue,
                    bptreedb::TCutNodeCompressor<int64_t, bptreedb::StringValue, TNodeIdEncoder, bptreedb::CZlibCutStringComp> > TTagLeaf;
            typedef TNodeInner TTagInner;
            typedef bptreedb::TBPStringValueMap<int64_t, TNodeIdComp, bptreedb::storage::IFilePager, TTagInner, TTagLeaf> TTagTree;

            const char TagKeySeparator = '\x1F';     // between the key and the value of a tag
            const char TagSeparator = '\x1E';        // between the tags

            // the compressor parameters of the tag tree: nMaxLen - the longest value, zlib level
            inline bptreedb::TCompressorParamsBasePtr CreateTagCompressParams(uint32_t nMaxLen, int32_t nCompressLevel = 6)
            {
                bptreedb::TCompressorParamsBasePtr ptrParams(new bptreedb::CompressorParamsBase());
                for (bptreedb::ECompressParams id : { bptreedb::eInnerKey, bptreedb::eInnerValue, bptreedb::eLeafKey, bptreedb::eLeafValue })
                {
                    bptreedb::TCompressorParamsPtr ptrComp(new bptreedb::CompressorParams());
                    ptrComp->SetIntParam("MaxLen", (int32_t)nMaxLen);
                    ptrComp->SetIntParam("compressLevel", nCompressLevel);
                    ptrComp->SetIntParam("compressRate", 3);
                    ptrParams->AddCompressParams(ptrComp, id);
                }
                return ptrParams;
            }

            // the tags of a value (pszTags, nLen - without the terminating zero)
            inline void DecodeTags(const char* pszTags, size_t nLen, std::vector<std::pair<std::string, std::string> >& tags)
            {
                tags.clear();
                size_t nPos = 0;
                while (nPos < nLen)
                {
                    size_t nEnd = nPos;
                    while (nEnd < nLen && pszTags[nEnd] != TagSeparator)
                        ++nEnd;
                    size_t nKeyEnd = nPos;
                    while (nKeyEnd < nEnd && pszTags[nKeyEnd] != TagKeySeparator)
                        ++nKeyEnd;
                    tags.emplace_back(std::string(pszTags + nPos, nKeyEnd - nPos),
                                      nKeyEnd < nEnd ? std::string(pszTags + nKeyEnd + 1, nEnd - nKeyEnd - 1) : std::string());
                    nPos = nEnd + 1;
                }
            }
        }
    }
}
