#pragma once

// A transaction: the lifecycle and the locks; the pages through its pager, the trees in its tree sets.
//
//   kind                       locks of the database                         pages / trees
//   eTT_SELECT, eTT2PL         shared                                        committed pages, own tree sets
//   eTT_SELECT, eTTDirectRead  shared                                        committed pages, the tree sets the
//                                                                            tables share (warm node caches)
//   eTT_MODIFY                 writer mutex; exclusive only to apply the commit   CTransactionPager (private pages)
//   eTT_DDL                    writer mutex + exclusive for the whole transaction CTransactionPager
//
// Readers don't wait for a modify transaction until its commit applies the pages (the schema objects are shared
// and DDL changes them in place: DDL is exclusive).
// Commit of a write transaction: the trees write their nodes to the private pages, the schema is written if it
// changed (or the counters of the tables), the private pages go to the redo log (the commit point), then under
// the exclusive lock into the committed pages and the file, then the log is deleted.
// Rollback: the private pages are dropped (DDL: the schema is read again from the committed pages).
// A write transaction that failed in the middle of a change can only be rolled back.
// The destructor of an active transaction rolls it back.

#include "../DBCommon.h"
#include "../Schema.h"
#include "TransactionPager.h"

namespace bptreedb
{
	namespace db
	{
		class CDatabase;
		typedef std::shared_ptr<CDatabase> CDatabasePtr;
		typedef std::shared_ptr<class CTransaction> CTransactionPtr;

		class CTransaction : public ITransaction, public std::enable_shared_from_this<CTransaction>
		{
		public:
			CTransaction(CDatabasePtr ptrDB, eTransactionDataType type, eTransactionType tranType);
			virtual ~CTransaction();

			void Begin();      // waits for the locks

			// ITransaction
			virtual void Commit();
			virtual void Rollback();
			virtual bool IsActive() const;
			virtual eTransactionType GetTransactionType() const { return m_tranType; }
			virtual eTransactionDataType GetTransactionDataType() const { return m_type; }

			virtual ISelectCursorPtr executeQuery(const std::string& sQuery);
			virtual int64_t execute(const std::string& sSQL);
			virtual IStatementPtr prepare(const std::string& sSQL);
			virtual ISelectCursorPtr executeSpatialQuery(const CommonLib::bbox& extent, const std::string& sTable, const std::string& sSpatialField,
				eSpatialQueryMode mode, IFieldSetPtr ptrFieldSet);
			virtual ISelectCursorPtr executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet);
			virtual ISelectCursorPtr executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::string& sField, const CommonLib::CVariant& var, OpType opType);
			virtual ISelectCursorPtr executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::vector<int64_t>& rowIds);
			virtual IInsertCursorPtr createInsertCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, bool bMinSplit);
			virtual IUpdateCursorPtr createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet);
			virtual IUpdateCursorPtr createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::string& sField, const CommonLib::CVariant& var, OpType opType);
			virtual IUpdateCursorPtr createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::vector<int64_t>& rowIds);
			virtual IDeleteCursorPtr createDeleteCursor(const std::string& sTable);

			bool IsWrite() const { return m_type == eTT_MODIFY || m_type == eTT_DDL; }
			void CheckActive() const;
			void CheckWrite() const;
			void CheckDDL() const;
			void SetFailed() { m_bFailed = true; }
			const SDBContext* Context() const;
			CSchemaPtr Schema() const;

			// the pager of the transaction: private pages of a write transaction, the committed pages of a read one
			storage::IFilePagerPtr Pager() const;
			// the tree set of a table in this transaction (direct reads: the one the table shares)
			STableTreesPtr Trees(CTable& table);

		private:
			void Finish();
			void FlushTrees();
			void ReleaseLocks();

		private:
			CDatabasePtr m_ptrDB;
			eTransactionDataType m_type;
			eTransactionType m_tranType;
			std::atomic<bool> m_bActive{ false };
			bool m_bFailed{ false };

			std::unique_lock<std::mutex> m_writerLock;
			std::shared_lock<std::shared_mutex> m_readLock;
			std::unique_lock<std::shared_mutex> m_exclusiveLock;

			CTranStorageIOPtr m_ptrIO;                        // write transactions
			CTransactionPagerPtr m_ptrPager;
			std::map<const CTable*, STableTreesPtr> m_trees;   // own tree sets
			mutable std::recursive_mutex m_mutex;     // Commit / Rollback; the schema is locked under it
			std::mutex m_treesMutex;                  // m_trees only, a leaf lock: Trees is called under the schema lock
		};
	}
}
