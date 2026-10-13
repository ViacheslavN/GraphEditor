#pragma once

// The schema: the tables, stored in a page stream (the address is in the file header).
//   uint32 magic, uint32 version, uint32 next table id, uint32 table count, tables

#include "DBCommon.h"
#include "Table.h"

namespace bptreedb
{
	namespace db
	{
		typedef std::shared_ptr<class CSchema> CSchemaPtr;

		class CSchema : public ISchema, public std::enable_shared_from_this<CSchema>
		{
		public:
			CSchema(SDBContextPtr ptrContext, int64_t nSchemaPage);
			virtual ~CSchema();

			// ISchema
			virtual int32_t GetTableCount() const;
			virtual ITablePtr GetTable(int32_t nIndex) const;
			virtual ITablePtr GetTable(const std::string& sName) const;
			virtual ITablePtr CreateTable(const std::string& sName, ITransactionPtr ptrTran);
			virtual void DropTable(const std::string& sName, ITransactionPtr ptrTran);

			CTablePtr FindTable(const std::string& sName) const;      // null if not found
			std::vector<CTablePtr> GetTables() const;
			CTablePtr GetTableThrow(const std::string& sName) const;

			// writes the schema through a pager (bNew - the first time, the stream pages are new);
			// getTrees gives the tree set of a write transaction for a table: its counters are written
			typedef std::function<const STableTrees*(const CTable&)> TGetTrees;
			void Save(storage::IFilePagerPtr ptrPager, bool bNew, const TGetTrees& getTrees = TGetTrees());
			void Load();                 // from the committed pages
			// after a rollback of DDL: the schema from the committed pages; the table objects stay (the dropped
			// ones are marked)
			void Reload();

			bool IsDirty() const;
			void SetClosed();            // the database closed: the shared trees are closed
			void Committed();            // the dropped tables are gone, the tables aren't dirty

		private:
			void ReadTables(std::vector<CTablePtr>& tables, uint32_t& nNextTableId);

		private:
			static constexpr uint32_t SchemaMagic = 0x48435342;   // "BSCH"
			static constexpr uint32_t SchemaVersion = 1;

			SDBContextPtr m_ptrContext;
			int64_t m_nSchemaPage{ -1 };
			uint32_t m_nNextTableId{ 1 };
			std::vector<CTablePtr> m_tables;
			std::vector<CTablePtr> m_dropped;     // dropped by the active transaction (a rollback brings them back)
			bool m_bDirty{ false };
			mutable std::recursive_mutex m_mutex;
		};
	}
}
