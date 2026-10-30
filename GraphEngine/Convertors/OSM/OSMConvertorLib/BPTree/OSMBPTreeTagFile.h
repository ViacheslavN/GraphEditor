#pragma once

// The tag file of the OSM -> B+ tree conversion (a bptreedb storage, CFileStorage + CFilePager):
//   page 0     the header (SOSMBPTreeTagHeader)
//   the rest   the tag tree TTagTree: int64 node id -> the tags (OSMTagTreeTypes.h), only the nodes with tags
// COSMBPTreeTagFile reads it.

#include "OSMTagTreeTypes.h"
#include "OSMBPTreeSettings.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bptreedb {
    namespace storage {
        class CFileStorage;
        class CFilePager;
    }
}

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class COSMBPTreeTagFile
            {
            public:
                COSMBPTreeTagFile();
                ~COSMBPTreeTagFile();

                // nPageSize - the tag page size of the conversion (SOSMBPTreeSettings::nTagPageSize)
                void Open(const std::string& sTagFile, uint32_t nPageSize = 16384, uint32_t nTreeCacheNodes = 64, uint32_t nPagerCachePages = 256);
                void Close();

                const SOSMBPTreeTagHeader& Header() const { return m_header; }
                // the encoded tags of a node (key US value RS ...), false - the node has no tags
                bool Get(int64_t nId, std::string& sTags);
                bool Get(int64_t nId, std::vector<std::pair<std::string, std::string> >& tags);
                TTagTree& Tree() { return *m_ptrTree; }

            private:
                COSMBPTreeTagFile(const COSMBPTreeTagFile&);
                COSMBPTreeTagFile& operator=(const COSMBPTreeTagFile&);

            private:
                CommonLib::IAllocPtr m_ptrAlloc;
                std::shared_ptr<bptreedb::storage::CFileStorage> m_ptrStorage;
                std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;
                std::unique_ptr<TTagTree> m_ptrTree;
                SOSMBPTreeTagHeader m_header;
            };

            // the header page of the tag file
            void WriteTagHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeTagHeader& header);
            SOSMBPTreeTagHeader ReadTagHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage);
        }
    }
}
