#pragma once

// The database file:
//   raw header (HeaderSize bytes): char[8] "BPTREEDB", uint32 version, uint32 page size, int64 schema page, uint32 crc32
//   pages of the file storage (the schema stream, the trees)
// and while a write transaction runs / commits:
//   "<file>-tran"  the private pages of the transaction that don't fit in memory (deleted at its end)
//   "<file>-redo"  the redo log of a commit (CRedoLog; replayed by Open if a crash left a complete one)

#include "DBCommon.h"
#include "Schema.h"
#include "Transactions/TranStorageIO.h"
#include "../StorageLib/FileStorage.h"

namespace bptreedb
{
	namespace db
	{
		class CDatabase : public IDatabase, public std::enable_shared_from_this<CDatabase>
		{
		public:
			CDatabase();
			virtual ~CDatabase();

			// IDatabase
			virtual void Create(const std::string& sPathUtf8, const SDatabaseParams& params);
			virtual void Create(const std::wstring& sPath, const SDatabaseParams& params);
			virtual void Open(const std::string& sPathUtf8, const SDatabaseParams& params);
			virtual void Open(const std::wstring& sPath, const SDatabaseParams& params);
			virtual void Close();
			virtual bool IsOpen() const;
			virtual ISchemaPtr GetSchema() const;
			virtual ITransactionPtr StartTransaction(eTransactionDataType type, eTransactionType tranType);

			// transactions
			std::mutex& WriterMutex() { return m_writerMutex; }            // one write transaction at a time
			std::shared_mutex& StateLock() { return m_stateLock; }         // readers shared; DDL / applying a commit exclusive
			void CheckUsable() const;                                      // a failed apply of a commit: reopen
			void TransactionStarted();
			void TransactionFinished();
			CSchemaPtr Schema() const;
			const SDBContextPtr& Context() const { return m_ptrContext; }

			// the commit of a write transaction (the writer mutex is held): the redo log, then (under the exclusive
			// lock, taken here unless bExclusive) the pages into the committed pages and the file, the log deleted
			void ApplyCommit(CTranStorageIO& pages, bool bExclusive);

			// tests: called after the redo log of a commit is written, before its pages are applied (a crash there)
			void SetAfterRedoLogHook(const std::function<void()>& hook) { m_afterRedoLog = hook; }

		private:
			void OpenStorage(const std::wstring& sPath, const SDatabaseParams& params, uint32_t nPageSize);
			void WriteHeader(const std::wstring& sPath, uint32_t nPageSize, int64_t nSchemaPage);
			static void ReadHeader(const std::wstring& sPath, uint32_t& nPageSize, int64_t& nSchemaPage);
			void CloseStorage();

		public:
			static constexpr uint32_t HeaderSize = 4096;      // the raw header before the pages of the file

		private:
			static constexpr uint32_t FormatVersion = 1;

			std::wstring m_sPath;
			SDBContextPtr m_ptrContext;
			storage::CFileStoragePtr m_ptrFileStorage;
			CSchemaPtr m_ptrSchema;
			std::mutex m_writerMutex;
			std::shared_mutex m_stateLock;
			std::mutex m_stateMutex;
			uint32_t m_nActiveTransactions{ 0 };
			std::atomic<bool> m_bBroken{ false };
			std::function<void()> m_afterRedoLog;
		};
	}
}
