#pragma once
// Bulk load of a SQLite R-tree (the spatial index of a table): instead of inserting every row into the R-tree
// (R* insert: choose leaf, split, reinsert - the main cost of loading millions of rows), the boxes are collected,
// sorted along the Hilbert curve and packed bottom up into full nodes, which are written straight into the
// shadow tables of the R-tree (<index>_node, <index>_rowid, <index>_parent) - the way GDAL bulk loads
// GeoPackage indexes. The packed tree is also smaller (the nodes are full) and faster to query.
//
// Any number of boxes: they are kept in memory up to the limit, then written to a temporary file in sorted runs
// which are merged by Build. The entries already in the index are kept (they are read back and packed with the new ones).

#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/sqlitelib/Database.h"
#include "../../CommonLib/SpatialData/BoundaryBox.h"
#include "../../CommonLib/filesystem/File.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteRTreeBuilder
        {
        public:
            // sTempDir - the temporary files (UTF-8), empty - the system temporary folder
            CSQLiteRTreeBuilder(const std::string& sTempDir = std::string(), uint64_t nMemoryBytes = 256ull * 1024 * 1024);
            ~CSQLiteRTreeBuilder();

            void     Add(int64_t nRowId, const CommonLib::bbox& box);
            uint64_t Count() const { return m_nCount; }

            // replaces the content of the R-tree sIndexName (rtree(id, minX, maxX, minY, maxY)) by the packed tree
            // of its old entries and the added ones; in the transaction of the caller
            void Build(CommonLib::database::IDatabasePtr ptrDatabase, const std::string& sIndexName);

            // the node fan-out of an index created with this page size (for the tests)
            static uint32_t MaxCellsForNodeSize(uint32_t nNodeSize);

        public:
#pragma pack(push, 4)
            struct SEntry
            {
                uint64_t nKey;      // Hilbert index of the box center
                int64_t  nId;
                float    box[4];    // minX, maxX, minY, maxY rounded outwards to floats (as the R-tree stores them)
            };
#pragma pack(pop)

        private:
            CSQLiteRTreeBuilder(const CSQLiteRTreeBuilder&);
            CSQLiteRTreeBuilder& operator=(const CSQLiteRTreeBuilder&);

        private:
            std::string m_sTempDir;
            uint64_t    m_nMemoryBytes;
            std::shared_ptr<void> m_ptrSorter;   // the external sorter of the entries
            uint64_t    m_nCount;
        };
    }
}
