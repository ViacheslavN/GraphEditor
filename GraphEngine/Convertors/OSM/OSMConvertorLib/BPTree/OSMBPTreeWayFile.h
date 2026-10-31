#pragma once

// The way shape file of the OSM -> B+ tree conversion (a bptreedb storage, CFileStorage + CFilePager):
//   page 0     the header (SOSMBPTreeWayHeader)
//   the rest   the shape tree TWayShapeTree: int64 way id -> the compressed shape (OSMWayTreeTypes.h), the stream pages
//              of the big shapes
// COSMBPTreeWayFile reads it.

#include "OSMWayTreeTypes.h"
#include "OSMBPTreeSettings.h"

#include <memory>
#include <string>
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

            class COSMBPTreeWayFile
            {
            public:
                COSMBPTreeWayFile();
                ~COSMBPTreeWayFile();

                // nPageSize - the way page size of the conversion (SOSMBPTreeSettings::nWayPageSize)
                void Open(const std::string& sWayFile, uint32_t nPageSize = 8192, uint32_t nTreeCacheNodes = 64, uint32_t nPagerCachePages = 256);
                void Close();

                const SOSMBPTreeWayHeader& Header() const { return m_header; }
                // the compressed shape of a way (ShapeFormat.h), false - no such way (or no shape)
                bool GetBlob(int64_t nId, std::vector<byte_t>& blob);
                // the points of a way (1e-7 degrees)
                bool GetPoints(int64_t nId, std::vector<SOSMPointE7>& points);
                TWayShapeTree& Tree() { return *m_ptrTree; }

                // the points of a compressed shape
                static void DecodePoints(const byte_t* pBlob, size_t nSize, std::vector<SOSMPointE7>& points);

            private:
                COSMBPTreeWayFile(const COSMBPTreeWayFile&);
                COSMBPTreeWayFile& operator=(const COSMBPTreeWayFile&);

            private:
                CommonLib::IAllocPtr m_ptrAlloc;
                std::shared_ptr<bptreedb::storage::CFileStorage> m_ptrStorage;
                std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;
                std::unique_ptr<TWayShapeTree> m_ptrTree;
                SOSMBPTreeWayHeader m_header;
            };

            // the header page of the way file
            void WriteWayHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeWayHeader& header);
            SOSMBPTreeWayHeader ReadWayHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage);
        }
    }
}
