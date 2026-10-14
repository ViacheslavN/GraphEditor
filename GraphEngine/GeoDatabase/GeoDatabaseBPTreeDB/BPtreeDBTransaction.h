#pragma once

#include "../GeoDatabase.h"
#include "BPtreeDBContext.h"

namespace GraphEngine {
    namespace GeoDatabase {

        // A bptreedb transaction:
        //   ttSelect - eTT_SELECT, direct read (the committed pages);
        //   ttModify - eTT_MODIFY (eTT_DDL in the bulk load mode: the tables of the load are created in it);
        //   ttDDL    - eTT_DDL (exclusive).
        // A write transaction is the write transaction of the workspace: the tables / queries of the same thread
        // use it. A transaction that isn't committed is rolled back by the destructor.
        class CBPtreeDBTransaction : public ITransaction
        {
        public:

            CBPtreeDBTransaction(CBPtreeDBContextPtr ptrContext, eTransactionType type);
            virtual ~CBPtreeDBTransaction();

            virtual void Commit() ;
            virtual void Rollback();

            // INSERT text with "?" parameters (the columns of the cursor)
            virtual IInsertCursorPtr CreateInsertCusor(const std::string&  sqlInsertQuery);
            virtual IInsertCursorPtr CreateInsertCusor(ITablePtr pTable);
            virtual IUpdateCursorPtr CreateUpdateCusor(const std::string&  sqlUpdateQuery);
            virtual IUpdateCursorPtr CreateUpdateCusor(ITablePtr pTable, const std::string& whereClause = "");
            virtual IDeleteCursorPtr CreateDeleteCusor(ITablePtr pTable, IFieldSetPtr ptrFileds = nullptr);
            virtual void Executequery(const std::string& sqlQuery);

            bptreedb::ITransactionPtr GetDBTransaction() const { return m_ptrTran; }

        private:
            void End();

        private:
            CBPtreeDBContextPtr m_ptrContext;             // first: released last
            bptreedb::ITransactionPtr m_ptrTran;
            bool m_bWrite{ false };
        };

        // Deletes rows by the value of the OID field of the table (the bptreedb row id if the table has none)
        class CBPtreeDBDeleteCursor : public IDeleteCursor
        {
        public:
            CBPtreeDBDeleteCursor(const std::string& sTable, const std::string& sOIDField, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContextPtr ptrContext);
            virtual ~CBPtreeDBDeleteCursor();

            virtual void DeleteRow(int64_t oid);

        private:
            CBPtreeDBContextPtr m_ptrContext;
            bptreedb::ITransactionPtr m_ptrTran;
            bptreedb::IDeleteCursorPtr m_ptrCursor;
            std::string m_sTable;
            std::string m_sOIDField;
        };

    }
}
