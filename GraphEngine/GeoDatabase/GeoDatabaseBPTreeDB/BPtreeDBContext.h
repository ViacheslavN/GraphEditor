#pragma once

#include "../GeoDatabase.h"
#include "../../../bptreedb/DB/bptreedb.h"
#include <map>
#include <mutex>
#include <thread>
#include <functional>
#include <fstream>
#include <memory>

namespace GraphEngine {
    namespace GeoDatabase {

        // a box with coordinates (bbox_type_invalid is 0xFFFFFFFF: "type & bbox_type_normal" is true for it)
        inline bool IsNormalBox(const CommonLib::bbox& bb)
        {
            return (bb.type & CommonLib::bbox_basic_type_mask) == CommonLib::bbox_type_normal;
        }

        // The spatial index of a spatial table: bptreedb has two kinds
        enum eBPtreeDBSpatialIndexType
        {
            bsitRTree = 0,     // R*-tree of the bounding boxes: no extent is needed
            bsitZOrder = 1     // Z-order B+ tree on a grid over the extent: every geometry has to be inside the extent
        };

        struct SBPtreeDBSpatialIndexParams
        {
            eBPtreeDBSpatialIndexType type{ bsitRTree };
            // Z-order: the cell of the grid, 0 - the longer side of the extent / nGridCells
            double dCellSize{ 0 };
            uint32_t nGridCells{ 1u << 20 };
        };

        // Indexes of a table that are built at once at the end of a bulk load (sorted keys / packed nodes)
        // instead of row by row: the unique index of the primary key and the spatial index.
        //
        // The boxes of the inserted shapes are collected for the spatial index (the index is built from them: reading
        // the geometry of a big table back from the transaction pages takes much longer than the load). Over
        // MaxMemoryBoxes they go to a temporary file next to the database.
        struct SBPtreeDBPendingIndexes
        {
            SBPtreeDBPendingIndexes(const std::string& sTempPathUtf8) : sTempPath(sTempPathUtf8) {}
            ~SBPtreeDBPendingIndexes();
            SBPtreeDBPendingIndexes(const SBPtreeDBPendingIndexes&) = delete;
            SBPtreeDBPendingIndexes& operator=(const SBPtreeDBPendingIndexes&) = delete;

            std::vector<std::string> vecUniqueFields;
            std::string sSpatialField;                  // empty - no spatial index
            SBPtreeDBSpatialIndexParams spatialParams;
            CommonLib::bbox extent;                     // Z-order grid: the declared extent + the inserted shapes
            // the boxes of all the rows are collected: the table was empty, the rows came only through the insert
            // cursors of the workspace; false - the index reads the geometry of the table
            bool bCompleteBoxes{ true };

            void AddExtent(const CommonLib::bbox& bb);
            void AddBox(const CommonLib::bbox& bb, int64_t nRowId);
            // the collected boxes (the file is read back and removed); false - not complete
            bool TakeBoxes(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries);
            void DropBoxes();

            static const size_t MaxMemoryBoxes = 4u << 20;   // 4M boxes (160 MB) in memory per table

        private:
            struct SBox
            {
                double xMin, yMin, xMax, yMax;
                int64_t nRowId;
            };
            void Spill();

            std::string sTempPath;
            std::vector<SBox> m_boxes;
            std::unique_ptr<std::ofstream> m_ptrFile;
            uint64_t m_nSpilled{ 0 };
        };
        typedef std::shared_ptr<SBPtreeDBPendingIndexes> SBPtreeDBPendingIndexesPtr;

        // The state a workspace shares with its tables, transactions and cursors: the database, the write
        // transaction of the workspace, the bulk load mode and the indexes waiting for the end of a bulk load.
        //
        // bptreedb allows one write transaction at a time; a DDL transaction is exclusive (also for the readers),
        // so a thread that has the write transaction must do everything through it: the tables created while
        // it is open, the queries of the same thread (they see its changes, as on one SQLite connection).
        class CBPtreeDBContext
        {
        public:
            explicit CBPtreeDBContext(bptreedb::IDatabasePtr ptrDatabase);
            ~CBPtreeDBContext();                         // closes the database

            CBPtreeDBContext(const CBPtreeDBContext&) = delete;
            CBPtreeDBContext& operator=(const CBPtreeDBContext&) = delete;

            bptreedb::IDatabasePtr GetDatabase() const { return m_ptrDatabase; }
            bptreedb::ITablePtr GetDBTable(const std::string& sTable) const;   // null if not found
            bptreedb::ITablePtr GetDBTableThrow(const std::string& sTable) const;

            // settings
            bool IsBulkLoad() const;
            void SetBulkLoad(bool bBulk);
            // the write transactions insert in the order of the indexed values (bptreedb SetInsertOnRight);
            // the bulk load mode implies it
            bool IsInsertOnRight() const;
            void SetInsertOnRight(bool bOnRight);
            SBPtreeDBSpatialIndexParams GetDefaultSpatialIndex() const;
            void SetDefaultSpatialIndex(const SBPtreeDBSpatialIndexParams& params);
            // the maximum length in bytes (UTF-8) of a string field created without a length, 0 - unlimited
            uint32_t GetDefaultStringLength() const;
            void SetDefaultStringLength(uint32_t nLength);

            // The write transaction of the workspace (started by CBPtreeDBTransaction)
            void RegisterWriteTransaction(bptreedb::ITransactionPtr ptrTran);
            void UnregisterWriteTransaction(const bptreedb::ITransaction* pTran);
            // the write transaction if the calling thread has started it, else null
            bptreedb::ITransactionPtr GetThreadWriteTransaction() const;
            // the active write transaction of the workspace (any thread), else null
            bptreedb::ITransactionPtr GetWriteTransaction() const;
            bool HasWriteTransaction() const;

            // fn in the write transaction of the workspace, or in a new one (committed after fn); bDDL - fn changes
            // the schema (the write transaction has to be a DDL one). The open write transaction is used whatever
            // thread started it (a converter can write from its worker thread): bptreedb has one writer at a time,
            // a new transaction would wait for it forever.
            void RunInWriteTransaction(bool bDDL, const std::function<void (bptreedb::ITransactionPtr)>& fn);

            // A transaction for reading: the write transaction of the thread (bOwn = false), else a new direct read
            // one (bOwn = true, the caller commits it)
            bptreedb::ITransactionPtr GetReadTransaction(bool& bOwn);

            // pending indexes of a bulk load (they belong to the write transaction)
            SBPtreeDBPendingIndexesPtr GetPendingIndexes(const std::string& sTable) const;
            SBPtreeDBPendingIndexesPtr AddPendingIndexes(const std::string& sTable);
            // builds the pending indexes of the table / of all tables in the transaction (DDL)
            void BuildPendingIndexes(const std::string& sTable, bptreedb::ITransactionPtr ptrTran);
            void BuildAllPendingIndexes(bptreedb::ITransactionPtr ptrTran);
            void ClearPendingIndexes();                  // a rollback
            bool HasPendingIndexes() const;
            // rows were inserted / deleted not by an insert cursor of the workspace (SQL): the collected boxes
            // aren't all the rows any more
            void InvalidatePendingBoxes();

            // the temporary files (the boxes of the spatial indexes) are next to the database
            void SetDatabasePath(const std::string& sPathUtf8) { m_sDatabasePath = sPathUtf8; }

        private:
            void BuildPendingIndexesImpl(const std::string& sTable, SBPtreeDBPendingIndexesPtr ptrPending, bptreedb::ITransactionPtr ptrTran);

        private:
            bptreedb::IDatabasePtr m_ptrDatabase;
            mutable std::mutex m_mutex;

            bool m_bBulkLoad{ false };
            bool m_bInsertOnRight{ false };
            SBPtreeDBSpatialIndexParams m_defaultSpatialIndex;
            uint32_t m_nDefaultStringLength;

            std::weak_ptr<bptreedb::ITransaction> m_ptrWriteTran;
            std::thread::id m_writeThread;

            std::map<std::string, SBPtreeDBPendingIndexesPtr> m_pending;
            std::string m_sDatabasePath;
            uint32_t m_nTempFiles{ 0 };
        };
        typedef std::shared_ptr<CBPtreeDBContext> CBPtreeDBContextPtr;

    }
}
