#include "Transaction.h"
#include "../Database.h"
#include "RedoLog.h"
#include "../Cursors/SelectCursor.h"
#include "../Cursors/UpdateCursor.h"
#include "../Cursors/InsertCursor.h"
#include "../Cursors/DeleteCursor.h"
#include "../SQLEngine/SQLDriver.h"
#include "../SQLEngine/Statement.h"
#include <filesystem>

namespace bptreedb
{
	namespace db
	{
		CTransaction::CTransaction(CDatabasePtr ptrDB, eTransactionDataType type, eTransactionType tranType) :
			m_ptrDB(ptrDB), m_type(type), m_tranType(tranType)
		{
		}

		CTransaction::~CTransaction()
		{
			try
			{
				if (m_bActive)
					Rollback();
			}
			catch (...)
			{
			}
		}

		// called once by CDatabase::StartTransaction before the transaction is shared: no m_mutex (it would be
		// held while waiting for the database locks)
		void CTransaction::Begin()
		{
			try
			{
				m_ptrDB->TransactionStarted();
				try
				{
					// the writer mutex before the state lock (CDatabase::ApplyCommit takes them in this order too)
					if (IsWrite())
					{
						m_writerLock = std::unique_lock<std::mutex>(m_ptrDB->WriterMutex());
						if (m_type == eTT_DDL)
							m_exclusiveLock = std::unique_lock<std::shared_mutex>(m_ptrDB->StateLock());
						m_ptrDB->CheckUsable();

						const SDBContextPtr& ptrContext = m_ptrDB->Context();
						m_ptrIO = std::make_shared<CTranStorageIO>(ptrContext->m_ptrPager->GetStorageIO(), ptrContext->m_nTranMemoryPages, ptrContext->m_sPath + L"-tran");
						m_ptrPager = std::make_shared<CTransactionPager>(ptrContext->m_ptrAlloc, m_ptrIO);
					}
					else
					{
						m_readLock = std::shared_lock<std::shared_mutex>(m_ptrDB->StateLock());
						m_ptrDB->CheckUsable();
					}
				}
				catch (...)
				{
					ReleaseLocks();
					m_ptrDB->TransactionFinished();
					throw;
				}
				m_bActive = true;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to begin the transaction", excSrc);
				throw;
			}
		}

		void CTransaction::ReleaseLocks()
		{
			if (m_exclusiveLock.owns_lock())
				m_exclusiveLock.unlock();
			if (m_readLock.owns_lock())
				m_readLock.unlock();
			if (m_writerLock.owns_lock())
				m_writerLock.unlock();
		}

		void CTransaction::Finish()
		{
			m_bActive = false;
			{
				std::lock_guard<std::mutex> treesLocker(m_treesMutex);
				m_trees.clear();
			}
			if (m_ptrIO.get())
				m_ptrIO->Clear();
			m_ptrPager.reset();
			m_ptrIO.reset();
			ReleaseLocks();
			m_ptrDB->TransactionFinished();
		}

		void CTransaction::FlushTrees()
		{
			for (auto& item : m_trees)
				item.second->Flush();
		}

		void CTransaction::Commit()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				if (!m_bActive)
					throw CommonLib::CExcBase("the transaction isn't active");
				if (m_bFailed)
					throw CommonLib::CExcBase("a change of the transaction failed: it can only be rolled back");

				if (IsWrite())
				{
					CSchemaPtr ptrSchema = Schema();
					try
					{
						// the nodes of the trees into the private pages, then the schema (with the new counters)
						FlushTrees();
						bool bCounters = false;
						for (const auto& item : m_trees)
							bCounters = bCounters || item.second->m_bWrite;
						if (bCounters || ptrSchema->IsDirty())
						{
							ptrSchema->Save(m_ptrPager, false, [this](const CTable& table) -> const STableTrees*
							{
								auto it = m_trees.find(&table);
								return it != m_trees.end() ? it->second.get() : nullptr;
							});
						}

						m_ptrDB->ApplyCommit(*m_ptrIO, m_exclusiveLock.owns_lock());
					}
					catch (...)
					{
						m_bFailed = true;     // still active: Rollback drops the private pages
						throw;
					}

					// committed: the in-memory state follows
					for (const auto& item : m_trees)
						const_cast<CTable*>(item.first)->ApplyWrite(*item.second);
					ptrSchema->Committed();
				}
				Finish();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to commit the transaction", excSrc);
				throw;
			}
		}

		void CTransaction::Rollback()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				if (!m_bActive)
					throw CommonLib::CExcBase("the transaction isn't active");

				if (IsWrite())
				{
					// the file and the committed pages weren't touched: the private pages are dropped
					{
						std::lock_guard<std::mutex> treesLocker(m_treesMutex);
						m_trees.clear();
					}
					m_ptrIO->Clear();
					if (m_type == eTT_DDL)
					{
						try
						{
							Schema()->Reload();       // the tables and fields as they are in the committed schema
						}
						catch (...)
						{
							Finish();
							throw;
						}
					}
				}
				Finish();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to roll back the transaction", excSrc);
				throw;
			}
		}

		bool CTransaction::IsActive() const
		{
			return m_bActive;
		}

		void CTransaction::CheckActive() const
		{
			if (!m_bActive)
				throw CommonLib::CExcBase("the transaction isn't active");
		}

		void CTransaction::CheckWrite() const
		{
			CheckActive();
			if (!IsWrite())
				throw CommonLib::CExcBase("a read transaction can't change data");
			if (m_bFailed)
				throw CommonLib::CExcBase("a change of the transaction failed: it can only be rolled back");
		}

		void CTransaction::CheckDDL() const
		{
			CheckWrite();
			if (m_type != eTT_DDL)
				throw CommonLib::CExcBase("schema changes need a DDL transaction");
		}

		const SDBContext* CTransaction::Context() const
		{
			return m_ptrDB->Context().get();
		}

		CSchemaPtr CTransaction::Schema() const
		{
			return m_ptrDB->Schema();
		}

		storage::IFilePagerPtr CTransaction::Pager() const
		{
			if (m_ptrPager.get())
				return m_ptrPager;
			return m_ptrDB->Context()->m_ptrPager;
		}

		STableTreesPtr CTransaction::Trees(CTable& table)
		{
			CheckActive();
			if (m_tranType == eTTDirectRead)
				return table.DirectTrees();

			std::lock_guard<std::mutex> treesLocker(m_treesMutex);
			STableTreesPtr& ptrTrees = m_trees[&table];
			if (ptrTrees.get() == nullptr)
				ptrTrees = std::make_shared<STableTrees>(Pager());
			return ptrTrees;
		}

		ISelectCursorPtr CTransaction::executeQuery(const std::string& sQuery)
		{
			IStatementPtr ptrStatement = prepare(sQuery);
			if (!ptrStatement->IsQuery())
				throw CommonLib::CExcBase("SQL: executeQuery runs a SELECT, use execute: '{0}'", sQuery);
			return ptrStatement->executeQuery();
		}

		int64_t CTransaction::execute(const std::string& sSQL)
		{
			CheckActive();
			sql::CSQLDriver driver;
			sql::TStatementList statements = driver.Parse(sSQL);
			if (driver.GetParameterCount() != 0)
				throw CommonLib::CExcBase("SQL: execute has no parameters, use prepare: '{0}'", sSQL);

			// one by one: a statement is compiled after the previous ones ran (CREATE TABLE; INSERT INTO it)
			int64_t nRows = 0;
			for (const sql::SStatementPtr& ptrAst : statements)
			{
				std::string sText = statements.size() == 1 ? sSQL : ptrAst->ToSQL();
				auto ptrStatement = std::make_shared<sql::CSQLStatement>(shared_from_this(), Schema(), ptrAst, 0, sText);
				nRows += ptrStatement->execute();
			}
			return nRows;
		}

		IStatementPtr CTransaction::prepare(const std::string& sSQL)
		{
			CheckActive();
			sql::CSQLDriver driver;
			sql::SStatementPtr ptrAst = driver.ParseStatement(sSQL);
			return std::make_shared<sql::CSQLStatement>(shared_from_this(), Schema(), ptrAst, driver.GetParameterCount(), sSQL);
		}

		ISelectCursorPtr CTransaction::executeSpatialQuery(const CommonLib::bbox& extent, const std::string& sTable, const std::string& sSpatialField,
			eSpatialQueryMode mode, IFieldSetPtr ptrFieldSet)
		{
			try
			{
				CheckActive();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				IRowSourcePtr ptrSource = ptrTable->SpatialSelect(*this, ptrTable->GetFieldThrow(sSpatialField), extent, mode);
				return std::make_shared<CSelectCursor>(shared_from_this(), ptrTable, ptrFieldSet, ptrSource, false);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed spatial query of {0}.{1}", sTable, sSpatialField, excSrc);
				throw;
			}
		}

		ISelectCursorPtr CTransaction::executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet)
		{
			try
			{
				CheckActive();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				return std::make_shared<CSelectCursor>(shared_from_this(), ptrTable, ptrFieldSet, ptrTable->ScanRows(*this), false);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to select from {0}", sTable, excSrc);
				throw;
			}
		}

		ISelectCursorPtr CTransaction::executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::string& sField, const CommonLib::CVariant& var, OpType opType)
		{
			try
			{
				CheckActive();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				IRowSourcePtr ptrSource = ptrTable->Select(*this, ptrTable->GetFieldThrow(sField), var, opType);
				return std::make_shared<CSelectCursor>(shared_from_this(), ptrTable, ptrFieldSet, ptrSource, false);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to select from {0} by {1}", sTable, sField, excSrc);
				throw;
			}
		}

		IInsertCursorPtr CTransaction::createInsertCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, bool bMinSplit)
		{
			try
			{
				CheckWrite();
				return std::make_shared<CInsertCursor>(shared_from_this(), Schema()->GetTableThrow(sTable), ptrFieldSet, bMinSplit);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create an insert cursor of {0}", sTable, excSrc);
				throw;
			}
		}

		ISelectCursorPtr CTransaction::executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::vector<int64_t>& rowIds)
		{
			try
			{
				CheckActive();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				std::vector<int64_t> rows(rowIds);
				return std::make_shared<CSelectCursor>(shared_from_this(), ptrTable, ptrFieldSet, std::make_shared<CListRowSource>(std::move(rows)), true);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to select from {0} by row ids", sTable, excSrc);
				throw;
			}
		}

		IUpdateCursorPtr CTransaction::createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::vector<int64_t>& rowIds)
		{
			try
			{
				CheckWrite();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				std::vector<int64_t> rows(rowIds);
				return std::make_shared<CUpdateCursor>(shared_from_this(), ptrTable, ptrFieldSet, std::make_shared<CListRowSource>(std::move(rows)));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create an update cursor of {0} by row ids", sTable, excSrc);
				throw;
			}
		}

		IUpdateCursorPtr CTransaction::createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet)
		{
			try
			{
				CheckWrite();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				return CUpdateCursor::Create(shared_from_this(), ptrTable, ptrFieldSet, ptrTable->ScanRows(*this));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create an update cursor of {0}", sTable, excSrc);
				throw;
			}
		}

		IUpdateCursorPtr CTransaction::createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::string& sField, const CommonLib::CVariant& var, OpType opType)
		{
			try
			{
				CheckWrite();
				CTablePtr ptrTable = Schema()->GetTableThrow(sTable);
				IRowSourcePtr ptrSource = ptrTable->Select(*this, ptrTable->GetFieldThrow(sField), var, opType);
				return CUpdateCursor::Create(shared_from_this(), ptrTable, ptrFieldSet, ptrSource);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create an update cursor of {0} by {1}", sTable, sField, excSrc);
				throw;
			}
		}

		void CTransaction::GetSpaceStatistics(SDatabaseSpace& space, const std::string& sTable, bool bBlobs)
		{
			try
			{
				CheckActive();
				space = SDatabaseSpace();
				space.m_nPageSize = Context()->m_nPageSize;
				std::error_code ec;
				uint64_t nFileSize = (uint64_t)std::filesystem::file_size(std::filesystem::path(Context()->m_sPath), ec);
				if (!ec && nFileSize > CDatabase::HeaderSize)
					space.m_nFilePages = (nFileSize - CDatabase::HeaderSize + space.m_nPageSize - 1) / space.m_nPageSize;

				std::vector<CTablePtr> tables;
				if (sTable.empty())
					tables = Schema()->GetTables();
				else
					tables.push_back(Schema()->GetTableThrow(sTable));
				for (const CTablePtr& ptrTable : tables)
					ptrTable->GetSpace(*this, bBlobs, space.m_trees);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to get the space statistics{0}", sTable.empty() ? std::string() : " of " + sTable, excSrc);
				throw;
			}
		}

		IDeleteCursorPtr CTransaction::createDeleteCursor(const std::string& sTable)
		{
			try
			{
				CheckWrite();
				return std::make_shared<CDeleteCursor>(shared_from_this(), Schema()->GetTableThrow(sTable));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create a delete cursor of {0}", sTable, excSrc);
				throw;
			}
		}
	}
}
