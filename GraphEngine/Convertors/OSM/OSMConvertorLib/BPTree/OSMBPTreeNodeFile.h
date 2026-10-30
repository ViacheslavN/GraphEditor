#pragma once

// The node file of the OSM -> B+ tree conversion (a bptreedb storage, CFileStorage + CFilePager):
//   page 0     the header (SOSMBPTreeNodeHeader)
//   the rest   the node tree TNodeTree: int64 id -> SOSMPointE7 (OSMBPTreeTypes.h)
// COSMBPTreeNodeFile reads it.

#include "OSMBPTreeTypes.h"
#include "OSMBPTreeSettings.h"

#include <memory>
#include <string>

namespace bptreedb {
    namespace storage {
        class CFileStorage;
        class CFilePager;
    }
}

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class COSMBPTreeNodeFile
            {
            public:
                COSMBPTreeNodeFile();
                ~COSMBPTreeNodeFile();

                // nPageSize - the page size of the conversion (SOSMBPTreeSettings::nPageSize)
                void Open(const std::string& sNodeFile, uint32_t nPageSize = 8192, uint32_t nTreeCacheNodes = 256, uint32_t nPagerCachePages = 1024);
                void Close();

                const SOSMBPTreeNodeHeader& Header() const { return m_header; }
                bool Get(int64_t nId, SOSMPointE7& point);
                TNodeTree& Tree() { return *m_ptrTree; }

            private:
                COSMBPTreeNodeFile(const COSMBPTreeNodeFile&);
                COSMBPTreeNodeFile& operator=(const COSMBPTreeNodeFile&);

            private:
                CommonLib::IAllocPtr m_ptrAlloc;
                std::shared_ptr<bptreedb::storage::CFileStorage> m_ptrStorage;
                std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;
                std::unique_ptr<TNodeTree> m_ptrTree;
                SOSMBPTreeNodeHeader m_header;
            };

            // the header page of the node file
            void WriteNodeHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeNodeHeader& header);
            SOSMBPTreeNodeHeader ReadNodeHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage);
        }
    }
}
