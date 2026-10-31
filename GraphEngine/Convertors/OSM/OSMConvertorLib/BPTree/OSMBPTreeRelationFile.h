#pragma once

// The relation file of the OSM -> B+ tree conversion (a bptreedb storage, CFileStorage + CFilePager):
//   page 0     the header (SOSMBPTreeRelationHeader)
//   the rest   the relation tree TRelationTree: int64 relation id -> the record (OSMRelationRecord.h: the shape, the members),
//              the stream pages of the big records
// COSMBPTreeRelationFile reads it.

#include "OSMWayTreeTypes.h"
#include "OSMBPTreeSettings.h"
#include "OSMRelationRecord.h"

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

            typedef TWayShapeTree TRelationTree;    // int64 id -> a blob

            class COSMBPTreeRelationFile
            {
            public:
                COSMBPTreeRelationFile();
                ~COSMBPTreeRelationFile();

                // nPageSize - the relation page size of the conversion (SOSMBPTreeSettings::nRelPageSize)
                void Open(const std::string& sRelationFile, uint32_t nPageSize = 8192, uint32_t nTreeCacheNodes = 64, uint32_t nPagerCachePages = 256);
                void Close();

                const SOSMBPTreeRelationHeader& Header() const { return m_header; }
                // the record of a relation, false - no such relation
                bool GetBlob(int64_t nId, std::vector<byte_t>& blob);
                bool GetRelation(int64_t nId, SRelationRecord& record);
                TRelationTree& Tree() { return *m_ptrTree; }

            private:
                COSMBPTreeRelationFile(const COSMBPTreeRelationFile&);
                COSMBPTreeRelationFile& operator=(const COSMBPTreeRelationFile&);

            private:
                CommonLib::IAllocPtr m_ptrAlloc;
                std::shared_ptr<bptreedb::storage::CFileStorage> m_ptrStorage;
                std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;
                std::unique_ptr<TRelationTree> m_ptrTree;
                SOSMBPTreeRelationHeader m_header;
            };

            // the header page of the relation file
            void WriteRelationHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeRelationHeader& header);
            SOSMBPTreeRelationHeader ReadRelationHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage);
        }
    }
}
