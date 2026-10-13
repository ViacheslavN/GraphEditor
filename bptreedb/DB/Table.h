#pragma once

// A table: the metadata (fields, indexes, counters) stored in the schema; the trees of its data are opened
// in a tree set (STableTrees) on the pager of a transaction:
//   the row tree (which rows exist), a value tree per field (row id -> value), the index trees.
// A transaction has its own tree sets (CTransaction::Trees); direct read transactions share the tree set of
// the table on the committed pages (opened again after every commit). The mutex of a tree set serializes the
// work with its trees (cursors of parallel transactions on a shared set).

#include "DBCommon.h"
#include "DBTrees.h"
#include "Fields/Field.h"

namespace bptreedb
{
	namespace db
	{
		class CTransaction;
		typedef std::shared_ptr<class CTable> CTablePtr;

		// the trees of a field
		struct SFieldTrees
		{
			IValueFieldPtr m_ptrValues;
			IIndexTreePtr m_ptrIndex;
			ISpatialIndexTreePtr m_ptrSpatial;
		};

		// the trees of a table opened on one pager, and what a write transaction changes in the table
		// (applied to the table by the commit)
		struct STableTrees
		{
			explicit STableTrees(storage::IFilePagerPtr ptrPager) : m_ptrPager(ptrPager) {}

			storage::IFilePagerPtr m_ptrPager;
			IRowTreePtr m_ptrRowTree;
			std::map<uint32_t, SFieldTrees> m_fields;     // by field id
			std::recursive_mutex m_mutex;

			bool m_bWrite{ false };                       // the counters below are set
			int64_t m_nNextRowId{ 0 };
			int64_t m_nRowCountDelta{ 0 };

			void Flush();                                 // writes the changed nodes to the pager
		};
		typedef std::shared_ptr<STableTrees> STableTreesPtr;

		class CTable : public ITable, public std::enable_shared_from_this<CTable>
		{
		public:
			CTable(SDBContextPtr ptrContext);
			virtual ~CTable();

			// ITable
			virtual const std::string& GetName() const;
			virtual int64_t GetRowCount() const;          // committed rows
			virtual IFieldsPtr GetFields() const;
			virtual IFieldPtr GetField(const std::string& sName) const;
			virtual IFieldPtr CreateField(const SFieldProp& prop, ITransactionPtr ptrTran);
			virtual void DropField(const std::string& sName, ITransactionPtr ptrTran);
			virtual int32_t GetIndexCount() const;
			virtual IIndexPtr GetIndex(int32_t nIndex) const;
			virtual IIndexPtr GetIndex(const std::string& sField) const;
			virtual IIndexPtr CreateIndex(const std::string& sField, const SIndexProp& prop, ITransactionPtr ptrTran);
			virtual void DropIndex(const std::string& sField, ITransactionPtr ptrTran);

			// a new table: its row tree in the tree set of the creating transaction
			void Create(uint32_t nId, const std::string& sName, STableTrees& trees);
			uint32_t GetId() const { return m_nId; }

			// pTrees: the counters of a write transaction instead of the committed ones (the commit writes them)
			void Write(CommonLib::IWriteStream* pStream, const STableTrees* pTrees = nullptr) const;
			void Read(CommonLib::IReadStream* pStream);
			// the metadata of a table read from the schema (after a rollback of DDL)
			void Assign(const CTable& table);
			// the counters of a committed write transaction
			void ApplyWrite(const STableTrees& trees);

			bool IsDirty() const { return m_bDirty; }
			void ClearDirty() { m_bDirty = false; }
			void SetDropped();
			void CheckNotDropped() const;

			// the tree set on the committed pages shared by the direct read transactions
			STableTreesPtr DirectTrees();
			void CloseDirectTrees();

			std::vector<CFieldPtr> GetFieldList() const;
			CFieldPtr FindField(const std::string& sName) const;           // null if not found
			CFieldPtr GetFieldThrow(const std::string& sName) const;

			// data: the caller has a transaction of the right type
			// values - the fields to set; the other fields get their default values
			// bMinSplit: the rows come in the order of the indexes (the index trees split with the min split strategy)
			int64_t InsertRow(CTransaction& tran, const std::vector<std::pair<CFieldPtr, CommonLib::CVariant> >& values, bool bMinSplit = false);
			bool DeleteRow(CTransaction& tran, int64_t nRowId);
			void UpdateRow(CTransaction& tran, int64_t nRowId, const std::vector<std::pair<CFieldPtr, CommonLib::CVariant> >& values);
			bool RowExists(CTransaction& tran, int64_t nRowId);

			IRowSourcePtr ScanRows(CTransaction& tran);
			IRowSourcePtr Select(CTransaction& tran, CFieldPtr ptrField, const CommonLib::CVariant& key, OpType op);
			IRowSourcePtr SpatialSelect(CTransaction& tran, CFieldPtr ptrField, const CommonLib::bbox& extent, eSpatialQueryMode mode);
			IValueReaderPtr CreateReader(CTransaction& tran, CFieldPtr ptrField);

		private:
			IRowTreePtr RowTree(STableTrees& trees);
			SFieldTrees& Trees(STableTrees& trees, const CFieldPtr& ptrField);    // opens the trees of the field
			void CheckField(const CFieldPtr& ptrField) const;      // belongs to the table
			CTransaction& CheckTran(ITransactionPtr ptrTran, bool bDDL) const;

			// unique / spatial checks of a new value of a row (nRowId = -1 for an insert)
			void CheckValue(const CFieldPtr& ptrField, SFieldTrees& trees, const CommonLib::CVariant& value, int64_t nRowId);
			void InsertValue(const CFieldPtr& ptrField, SFieldTrees& trees, int64_t nRowId, const CommonLib::CVariant& value);
			void RemoveValue(const CFieldPtr& ptrField, SFieldTrees& trees, int64_t nRowId, const CommonLib::CVariant& value);
			// fills the new (empty) index of the field from the values: sorted / STR bulk load. bChanged is set
			// when the index tree starts to change (a duplicate or a geometry outside the Z-order extent throws before)
			void BuildIndex(const CFieldPtr& ptrField, const CIndexPtr& ptrIndex, SFieldTrees& trees, bool& bChanged);

		private:
			SDBContextPtr m_ptrContext;
			uint32_t m_nId{ 0 };
			std::string m_sName;
			int64_t m_nRowTreePage{ -1 };
			int64_t m_nNextRowId{ 1 };
			int64_t m_nRowCount{ 0 };
			uint32_t m_nNextFieldId{ 1 };
			std::vector<CFieldPtr> m_fields;
			bool m_bDirty{ false };
			bool m_bDropped{ false };

			STableTreesPtr m_ptrDirect;                   // the shared tree set of the direct reads
			uint64_t m_nDirectCommits{ 0 };               // the commit it was opened after
			mutable std::recursive_mutex m_mutex;         // the metadata
		};
	}
}
