#include "Schema.h"
#include "Transactions/Transaction.h"
#include "../StorageLib/ReadStreamPage.h"
#include "../StorageLib/WriteStreamPage.h"

namespace bptreedb
{
	namespace db
	{
		CSchema::CSchema(SDBContextPtr ptrContext, int64_t nSchemaPage) : m_ptrContext(ptrContext), m_nSchemaPage(nSchemaPage)
		{
		}

		CSchema::~CSchema()
		{
		}

		int32_t CSchema::GetTableCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return (int32_t)m_tables.size();
		}

		ITablePtr CSchema::GetTable(int32_t nIndex) const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (nIndex < 0 || nIndex >= (int32_t)m_tables.size())
				throw CommonLib::CExcBase("table index {0} is out of range, tables {1}", nIndex, (uint32_t)m_tables.size());
			return m_tables[nIndex];
		}

		ITablePtr CSchema::GetTable(const std::string& sName) const
		{
			return FindTable(sName);
		}

		CTablePtr CSchema::FindTable(const std::string& sName) const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			for (const CTablePtr& ptrTable : m_tables)
				if (ptrTable->GetName() == sName)
					return ptrTable;
			return CTablePtr();
		}

		CTablePtr CSchema::GetTableThrow(const std::string& sName) const
		{
			CTablePtr ptrTable = FindTable(sName);
			if (ptrTable.get() == nullptr)
				throw CommonLib::CExcBase("no table '{0}'", sName);
			return ptrTable;
		}

		ITablePtr CSchema::CreateTable(const std::string& sName, ITransactionPtr ptrTran)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				CTransaction* pTran = dynamic_cast<CTransaction*>(ptrTran.get());
				if (pTran == nullptr || pTran->Context() != m_ptrContext.get())
					throw CommonLib::CExcBase("no transaction of the database");
				pTran->CheckDDL();
				if (FindTable(sName).get())
					throw CommonLib::CExcBase("the table exists already");

				CTablePtr ptrTable = std::make_shared<CTable>(m_ptrContext);
				try
				{
					ptrTable->Create(m_nNextTableId, sName, *pTran->Trees(*ptrTable));
				}
				catch (...)
				{
					pTran->SetFailed();
					throw;
				}
				++m_nNextTableId;
				m_tables.push_back(ptrTable);
				m_bDirty = true;
				return ptrTable;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create table '{0}'", sName, excSrc);
				throw;
			}
		}

		void CSchema::DropTable(const std::string& sName, ITransactionPtr ptrTran)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				CTransaction* pTran = dynamic_cast<CTransaction*>(ptrTran.get());
				if (pTran == nullptr || pTran->Context() != m_ptrContext.get())
					throw CommonLib::CExcBase("no transaction of the database");
				pTran->CheckDDL();

				CTablePtr ptrTable = GetTableThrow(sName);
				m_tables.erase(std::find(m_tables.begin(), m_tables.end(), ptrTable));
				ptrTable->SetDropped();
				m_dropped.push_back(ptrTable);
				m_bDirty = true;
				// the pages of the trees aren't reused yet (no free page management)
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to drop table '{0}'", sName, excSrc);
				throw;
			}
		}

		bool CSchema::IsDirty() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (m_bDirty)
				return true;
			for (const CTablePtr& ptrTable : m_tables)
				if (ptrTable->IsDirty())
					return true;
			return false;
		}

		void CSchema::Save(storage::IFilePagerPtr ptrPager, bool bNew, const TGetTrees& getTrees)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				// a rewrite reuses the page chain; a shorter schema leaves old bytes after its end, but the reader
				// stops at the last table
				storage::TWriteStreamPage stream(ptrPager);
				stream.Open(m_nSchemaPage, bNew);
				stream.Write(SchemaMagic);
				stream.Write(SchemaVersion);
				stream.Write(m_nNextTableId);
				stream.Write((uint32_t)m_tables.size());
				for (const CTablePtr& ptrTable : m_tables)
					ptrTable->Write(&stream, getTrees ? getTrees(*ptrTable) : nullptr);
				stream.Close();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to save the schema, page {0}", m_nSchemaPage, excSrc);
				throw;
			}
		}

		void CSchema::ReadTables(std::vector<CTablePtr>& tables, uint32_t& nNextTableId)
		{
			storage::TReadStreamPage stream(m_ptrContext->m_ptrPager);
			stream.Open(m_nSchemaPage);
			uint32_t nMagic = stream.ReadIntu32();
			if (nMagic != SchemaMagic)
				throw CommonLib::CExcBase("wrong schema magic {0}", nMagic);
			uint32_t nVersion = stream.ReadIntu32();
			if (nVersion != SchemaVersion)
				throw CommonLib::CExcBase("unsupported schema version {0}", nVersion);

			nNextTableId = stream.ReadIntu32();
			uint32_t nTables = stream.ReadIntu32();
			tables.clear();
			for (uint32_t i = 0; i < nTables; ++i)
			{
				CTablePtr ptrTable = std::make_shared<CTable>(m_ptrContext);
				ptrTable->Read(&stream);
				tables.push_back(ptrTable);
			}
		}

		void CSchema::Load()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				ReadTables(m_tables, m_nNextTableId);
				m_bDirty = false;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to load the schema, page {0}", m_nSchemaPage, excSrc);
				throw;
			}
		}

		void CSchema::Reload()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				std::vector<CTablePtr> tables;
				uint32_t nNextTableId = 0;
				ReadTables(tables, nNextTableId);

				// keep the objects the user may hold: tables by id
				std::vector<CTablePtr> result;
				for (const CTablePtr& ptrLoaded : tables)
				{
					CTablePtr ptrTable;
					for (const CTablePtr& ptrOld : m_tables)
						if (ptrOld->GetId() == ptrLoaded->GetId())
							ptrTable = ptrOld;
					for (const CTablePtr& ptrOld : m_dropped)
						if (ptrOld->GetId() == ptrLoaded->GetId())
							ptrTable = ptrOld;

					if (ptrTable.get())
						ptrTable->Assign(*ptrLoaded);
					else
						ptrTable = ptrLoaded;     // dropped in the transaction: a new object
					result.push_back(ptrTable);
				}
				for (const CTablePtr& ptrOld : m_tables)
				{
					if (std::find(result.begin(), result.end(), ptrOld) == result.end())
						ptrOld->SetDropped();     // created in the transaction
				}

				m_tables.swap(result);
				m_dropped.clear();
				m_nNextTableId = nNextTableId;
				m_bDirty = false;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to reload the schema, page {0}", m_nSchemaPage, excSrc);
				throw;
			}
		}

		std::vector<CTablePtr> CSchema::GetTables() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_tables;
		}

		void CSchema::Committed()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_dropped.clear();
			m_bDirty = false;
			for (const CTablePtr& ptrTable : m_tables)
				ptrTable->ClearDirty();
		}

		void CSchema::SetClosed()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			for (const CTablePtr& ptrTable : m_tables)
				ptrTable->CloseDirectTrees();
		}
	}
}
