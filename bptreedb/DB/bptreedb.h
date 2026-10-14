#pragma once

// bptreedb - embedded database on the B+ trees of BPtreeLib.
// The public SDK: the only header a user of the database includes.
//
//   IDatabasePtr db = CreateDatabase();
//   db->Create("map.btdb");
//   ITransactionPtr tran = db->StartTransaction(eTT_DDL);
//   ITablePtr table = db->GetSchema()->CreateTable("roads", tran);
//   table->CreateField(SFieldProp("name", dtString, 256), tran);
//   tran->Commit();
//
// Errors are reported by exceptions (CommonLib::CExcBase, std::exception); the message of the exception
// is the chain from the failed operation down to the cause.
//
// Storage: every table is a row tree (row id -> nothing) plus one tree per field (row id -> value, a NULL value
// is a missing entry) and one tree per index: (value, row id) -> nothing, or an R-tree / Z-order B+ tree of the
// bounding boxes of a geometry field. Row ids grow, so the row id trees split with the min split strategy
// (their leaves stay full).
//
// Transactions: a write transaction works on its own copies of the pages it changes (in memory, the rest in
// "<database>-tran"); the database file isn't touched until the commit. Commit writes the pages to the redo log
// "<database>-redo" (the commit point), then into the file. Rollback drops the copies. Open after a crash
// replays a complete redo log and drops an incomplete one.
// Locks: one write transaction at a time. Read transactions run in parallel with each other and with a modify
// transaction; they wait only while a commit is applied, and for a DDL transaction (it is exclusive).
// A thread mustn't commit a write transaction while it has an open read transaction (the commit waits for it).
// Objects of the SDK (tables, fields, cursors) are used by one thread at a time.

#include "../../CommonLib/CommonLib.h"
#include "../../CommonLib/variant/Variant.h"
#include "../../CommonLib/SpatialData/BoundaryBox.h"
#include "../../CommonLib/SpatialData/IGeoShape.h"

namespace bptreedb
{
	enum eTransactionDataType
	{
		eTT_UNDEFINED = 1,
		eTT_MODIFY = 2,             // insert / update / delete of rows
		eTT_SELECT = 4,             // read only
		eTT_DDL = 8                 // schema changes (tables, fields, indexes) and modify
	};

	enum eTransactionType
	{
		eTT2PL,                     // two-phase locking: pages through the transaction (write - private copies), own trees
		eTTDirectRead,              // read only (eTT_SELECT): the committed pages directly, the trees the tables share
		                            // between the direct transactions (their node caches stay warm between them)
		eWAL                        // write-ahead log (not implemented yet)
	};

	enum eDataTypes
	{
		dtUnknown = 0,
		dtNull,
		dtInteger8,
		dtInteger16,
		dtInteger32,
		dtInteger64,
		dtUInteger8,
		dtUInteger16,
		dtUInteger32,
		dtUInteger64,
		dtOid,                      // int64 reference to a row id
		dtFloat,
		dtDouble,
		dtString,                   // UTF-8
		dtBlob,
		dtGeometry                  // CommonLib::IGeoShapePtr (CGeoShape), stored as the shape blob
	};

	enum eIndexTypes
	{
		itUnknown = 0x00,
		itUnique,                   // value -> row id, the values are unique (NULLs aren't indexed)
		itMultiRegular,             // value -> row ids
		itSpatialRTree,             // dtGeometry: R*-tree of the bounding boxes (double coordinates)
		itSpatialZOrder,            // dtGeometry: Z-order B+ tree of the bounding boxes on a grid over the extent
		itText,                     // not implemented yet
		itRouting                   // not implemented yet
	};

	// Spatial queries compare the bounding box of the geometry with the query extent
	// (the exact geometry is for the caller to check).
	enum eSpatialQueryMode
	{
		sqmIntersect = 0,           // the bounding box intersects the extent
		sqmOver,                    // the bounding box contains the extent
		sqmInside,                  // the bounding box is inside the extent
		sqmByFeature                // not implemented yet
	};

	enum OpType
	{
		OpEqual,
		OpNoEqual,
		OpLess,
		OpLessOrEqual,
		OpGreater,
		OpGreaterOrEqual
	};

	// The value of a field in CommonLib::CVariant:
	//   dtInteger8..dtUInteger64 - int8_t..uint64_t, dtOid - int64_t, dtFloat - float, dtDouble - double,
	//   dtString - std::string (UTF-8; std::wstring is accepted and converted), dtBlob - CommonLib::CBlobPtr_t,
	//   dtGeometry - CommonLib::IGeoShapePtr (a CGeoShape; the shape blob as CBlobPtr_t is accepted too),
	//   NULL - the empty variant.
	// A value of another number type is converted to the type of the field (with a range check).

	// Properties of a new field.
	struct SFieldProp
	{
		SFieldProp() {}
		SFieldProp(const std::string& sName, eDataTypes type, uint32_t nLength = 0) : m_sName(sName), m_type(type), m_nLength(nLength) {}

		std::string m_sName;
		std::string m_sAlias;
		eDataTypes m_type{ dtUnknown };
		// dtString: the maximum length in bytes (UTF-8), 0 - unlimited. Strings up to the page size / 4 are stored
		// in the leaf nodes, longer ones (and unlimited) like blobs. Indexed strings: up to 256 bytes.
		uint32_t m_nLength{ 0 };
		bool m_bNotNull{ false };
		bool m_bUnique{ false };                  // creates the unique index of the field
		CommonLib::CVariant m_defaultValue;       // used when an insert doesn't set the field
	};

	// Properties of a new index.
	struct SIndexProp
	{
		SIndexProp(eIndexTypes type = itMultiRegular) : m_type(type) {}
		eIndexTypes m_type{ itMultiRegular };

		// itSpatialZOrder: the grid over the extent of the data. Every geometry has to be inside the extent.
		// The cell is the precision the queries need (not the precision of the geometry): the index finds
		// candidates by the cells, the bounding boxes are checked afterwards. m_dCellSize = 0 - the longer side of
		// the extent / m_nGridCells.
		CommonLib::bbox m_extent;
		double m_dCellSize{ 0 };
		uint32_t m_nGridCells{ 1u << 20 };

		// itSpatialRTree / itSpatialZOrder on a table with rows: the bounding boxes of the geometries (box, row id),
		// when the caller has them already (f.e. collected while the rows were inserted). The index is built from
		// them, the geometry values aren't read (a big table: the reading of all the geometry blobs is the most of
		// the time). They have to be the boxes of all the non NULL geometries of the field; the vector is used up.
		std::vector<std::pair<CommonLib::bbox, int64_t> >* m_pSpatialEntries{ nullptr };
	};

	// Parameters of the database (used by Create; Open uses the cache sizes only).
	struct SDatabaseParams
	{
		uint32_t m_nPageSize{ 8192 };             // page of the file and of the tree nodes: a power of 2, 8K..1M
		uint32_t m_nPageCacheSize{ 1000 };        // pages in the cache of the file pager
		uint32_t m_nTreeCacheSize{ 64 };          // nodes kept in memory per tree
		// nodes kept in memory per spatial index (R-tree, Z-order): an R-tree insert visits more nodes than a
		// B+ tree insert (forced reinsert, splits), a cache much smaller than the index makes inserts slow
		uint32_t m_nSpatialCacheSize{ 1000 };
		uint32_t m_nCompressLevel{ 6 };           // zlib level of the node compressors
		uint32_t m_nTranMemoryPages{ 4096 };      // changed pages a write transaction keeps in memory (the rest in a file)
	};

	// Space statistics of a tree of a table (ITransaction::GetSpaceStatistics).
	struct SSpaceInfo
	{
		std::string m_sTable;
		std::string m_sObject;              // the field, "" - the rows of the table (the row tree)
		std::string m_sKind;                // "rows", "values", "index", "unique index", "spatial index R-tree", "spatial index Z-order"
		eDataTypes m_type{ dtUnknown };     // the type of the field
		uint32_t m_nLevels{ 0 };            // the levels of the tree with the leaves
		uint64_t m_nInnerPages{ 0 };
		uint64_t m_nLeafPages{ 0 };
		uint64_t m_nBlobPages{ 0 };         // the stream pages of the big blobs (values over 1 KB), with bBlobs
		uint64_t m_nInfoPages{ 0 };         // the info page of the tree
		bool m_bBlobsCounted{ false };      // m_nBlobPages is counted (a blob tree with bBlobs)

		uint64_t Pages() const { return m_nInnerPages + m_nLeafPages + m_nBlobPages + m_nInfoPages; }
	};

	struct SDatabaseSpace
	{
		uint32_t m_nPageSize{ 0 };
		uint64_t m_nFilePages{ 0 };         // the pages of the database file (the committed ones)
		std::vector<SSpaceInfo> m_trees;
	};

	typedef std::shared_ptr<class IField> IFieldPtr;
	typedef std::shared_ptr<class IFields> IFieldsPtr;
	typedef std::shared_ptr<class IFieldSet> IFieldSetPtr;
	typedef std::shared_ptr<class IRow> IRowPtr;
	typedef std::shared_ptr<class IIndex> IIndexPtr;
	typedef std::shared_ptr<class ITable> ITablePtr;
	typedef std::shared_ptr<class ISchema> ISchemaPtr;
	typedef std::shared_ptr<class ITransaction> ITransactionPtr;
	typedef std::shared_ptr<class IDatabase> IDatabasePtr;
	typedef std::shared_ptr<class ICursor> ICursorPtr;
	typedef std::shared_ptr<class ISelectCursor> ISelectCursorPtr;
	typedef std::shared_ptr<class IInsertCursor> IInsertCursorPtr;
	typedef std::shared_ptr<class IUpdateCursor> IUpdateCursorPtr;
	typedef std::shared_ptr<class IDeleteCursor> IDeleteCursorPtr;
	typedef std::shared_ptr<class IStatement> IStatementPtr;

	class IField
	{
	public:
		IField() {}
		virtual ~IField() {}
		virtual eDataTypes GetType() const = 0;
		virtual const std::string& GetName() const = 0;
		virtual const std::string& GetAlias() const = 0;
		virtual uint32_t GetLength() const = 0;
		virtual bool GetIsNotNull() const = 0;
		virtual const CommonLib::CVariant& GetDefaultValue() const = 0;
		virtual bool GetIsUnique() const = 0;
		virtual IIndexPtr GetIndex() const = 0;   // the index of the field, null if none
	};

	// Fields of a table (a snapshot: changes of the table aren't visible in it).
	class IFields
	{
	public:
		IFields() {}
		virtual ~IFields() {}
		virtual int32_t GetFieldCount() const = 0;
		virtual IFieldPtr GetField(int32_t nIndex) const = 0;
		virtual IFieldPtr GetField(const std::string& sName) const = 0;   // null if not found
		virtual int32_t FindField(const std::string& sName) const = 0;    // -1 if not found
	};

	// Names of the fields a cursor works with (the columns of the cursor, in this order).
	class IFieldSet
	{
	public:
		IFieldSet() {}
		virtual ~IFieldSet() {}
		virtual int32_t GetCount() const = 0;
		virtual const std::string& Get(int32_t nIndex) const = 0;
		virtual int32_t Find(const std::string& sField) const = 0;        // -1 if not found
		virtual void Add(const std::string& sField) = 0;
		virtual void Remove(const std::string& sField) = 0;
		virtual void Clear() = 0;

		static IFieldSetPtr Create();
		static IFieldSetPtr Create(const std::vector<std::string>& fields);
	};

	// Values of one row; the columns follow the field set of the cursor that created it.
	class IRow
	{
	public:
		IRow() {}
		virtual ~IRow() {}
		virtual int32_t GetCount() const = 0;
		virtual const CommonLib::CVariant& GetValue(int32_t nCol) const = 0;
		virtual void SetValue(int32_t nCol, const CommonLib::CVariant& value) = 0;
		virtual bool IsNull(int32_t nCol) const = 0;
		virtual IFieldSetPtr GetFieldSet() const = 0;
		virtual int64_t GetRowID() const = 0;
		virtual void SetRowID(int64_t nRowID) = 0;
	};

	class IIndex
	{
	public:
		IIndex() {}
		virtual ~IIndex() {}
		virtual eIndexTypes GetType() const = 0;
		virtual IFieldPtr GetField() const = 0;
	};

	class ITable
	{
	public:
		ITable() {}
		virtual ~ITable() {}
		virtual const std::string& GetName() const = 0;
		virtual int64_t GetRowCount() const = 0;     // the committed rows (a write transaction's changes count after its commit)

		virtual IFieldsPtr GetFields() const = 0;
		virtual IFieldPtr GetField(const std::string& sName) const = 0;  // null if not found
		// a field added to a table with rows: NULL in the rows, or the default value if it has one
		virtual IFieldPtr CreateField(const SFieldProp& prop, ITransactionPtr ptrTran) = 0;
		virtual void DropField(const std::string& sName, ITransactionPtr ptrTran) = 0;

		virtual int32_t GetIndexCount() const = 0;
		virtual IIndexPtr GetIndex(int32_t nIndex) const = 0;
		virtual IIndexPtr GetIndex(const std::string& sField) const = 0; // null if the field has no index
		// One index per field. On a table with rows the index is built from them at once (sorted keys / STR
		// packing: full pages, every node written once), so for a big load create the indexes after the data.
		// A duplicate in a unique index (or a geometry outside a Z-order extent) throws before anything changes:
		// the transaction goes on without the index.
		virtual IIndexPtr CreateIndex(const std::string& sField, const SIndexProp& prop, ITransactionPtr ptrTran) = 0;
		virtual void DropIndex(const std::string& sField, ITransactionPtr ptrTran) = 0;
	};

	class ISchema
	{
	public:
		ISchema() {}
		virtual ~ISchema() {}
		virtual int32_t GetTableCount() const = 0;
		virtual ITablePtr GetTable(int32_t nIndex) const = 0;
		virtual ITablePtr GetTable(const std::string& sName) const = 0;  // null if not found
		virtual ITablePtr CreateTable(const std::string& sName, ITransactionPtr ptrTran) = 0;
		virtual void DropTable(const std::string& sName, ITransactionPtr ptrTran) = 0;
	};

	class ICursor
	{
	public:
		ICursor() {}
		virtual ~ICursor() {}

		virtual int32_t ColumnCount() const = 0;
		virtual std::string ColumnName(int32_t col) const = 0;
		virtual bool ColumnIsNull(int32_t col) const = 0;
		virtual eDataTypes GetColumnType(int32_t col) const = 0;
		virtual int32_t GetColumnBytes(int32_t col) const = 0;  // size of the value, 0 for NULL
		virtual int32_t FindFieldByName(const std::string& name) const = 0;
		virtual IFieldSetPtr GetFieldSet() const = 0;

		virtual bool Next() = 0;                       // moves to the next row (to the first one after creation)
		virtual int64_t GetRowID() const = 0;
	};

	// Reads the values of the current row. A number column can be read as another number type if the value fits.
	// Reading a NULL column throws (check ColumnIsNull, or use ReadValue: the empty variant).
	class ISelectCursor : public ICursor
	{
	public:
		ISelectCursor() {}
		virtual ~ISelectCursor() {}

		virtual int8_t ReadInt8(int32_t col) const = 0;
		virtual uint8_t ReadUInt8(int32_t col) const = 0;
		virtual int16_t ReadInt16(int32_t col) const = 0;
		virtual uint16_t ReadUInt16(int32_t col) const = 0;
		virtual int32_t ReadInt32(int32_t col) const = 0;
		virtual uint32_t ReadUInt32(int32_t col) const = 0;
		virtual int64_t ReadInt64(int32_t col) const = 0;
		virtual uint64_t ReadUInt64(int32_t col) const = 0;
		virtual float ReadFloat(int32_t col) const = 0;
		virtual double ReadDouble(int32_t col) const = 0;
		virtual void ReadText(int32_t col, std::string& text) const = 0;
		virtual std::string ReadText(int32_t col) const = 0;
		virtual void ReadTextW(int32_t col, std::wstring& text) const = 0;
		virtual std::wstring ReadTextW(int32_t col) const = 0;
		// the buffer belongs to the cursor and is valid until the next Next()
		virtual void ReadBlob(int32_t col, const byte_t** pBuf, int32_t& size) const = 0;
		virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const = 0;
		virtual const CommonLib::CVariant& ReadValue(int32_t col) const = 0;
		virtual IRowPtr CreateRow() const = 0;         // a row with the columns of the cursor
		virtual void FillRow(IRowPtr ptrRow) const = 0;
	};

	// Inserts rows: bind the values of the columns, Next() inserts the row. A column that isn't bound is
	// the default value of the field (NULL if it has none).
	class IInsertCursor
	{
	public:
		IInsertCursor() {}
		virtual ~IInsertCursor() {}

		virtual int32_t ColumnCount() const = 0;
		virtual std::string ColumnName(int32_t col) const = 0;
		virtual bool ColumnIsNull(int32_t col) const = 0;
		virtual eDataTypes GetColumnType(int32_t col) const = 0;
		virtual int32_t GetColumnBytes(int32_t col) const = 0;
		virtual IFieldSetPtr GetFieldSet() const = 0;

		virtual int64_t Next() = 0;                    // inserts the bound row, returns its row id, clears the bindings
		virtual int64_t Insert(IRowPtr ptrRow) = 0;    // inserts a row of CreateRow()
		virtual IRowPtr CreateRow() const = 0;

		virtual void BindNull(int32_t col) = 0;
		virtual void BindInt8(int32_t col, int8_t val) = 0;
		virtual void BindUInt8(int32_t col, uint8_t val) = 0;
		virtual void BindInt16(int32_t col, int16_t val) = 0;
		virtual void BindUInt16(int32_t col, uint16_t val) = 0;
		virtual void BindInt32(int32_t col, int32_t val) = 0;
		virtual void BindUInt32(int32_t col, uint32_t val) = 0;
		virtual void BindInt64(int32_t col, int64_t val) = 0;
		virtual void BindUInt64(int32_t col, uint64_t val) = 0;
		virtual void BindFloat(int32_t col, float val) = 0;
		virtual void BindDouble(int32_t col, double val) = 0;
		virtual void BindText(int32_t col, const std::string& text, bool copy) = 0;
		virtual void BindTextW(int32_t col, const std::wstring& text, bool copy) = 0;
		virtual void BindBlob(int32_t col, const byte_t* pBuf, int32_t size, bool copy) = 0;
		virtual void BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape) = 0;
		virtual void BindValue(int32_t col, const CommonLib::CVariant& value) = 0;
		virtual void Finish() = 0;                     // the cursor can't be used after it
	};

	// Selects rows like ISelectCursor and changes the current one. The rows are selected when the cursor is
	// created, so an update doesn't change which rows the cursor visits.
	class IUpdateCursor : public ISelectCursor
	{
	public:
		IUpdateCursor() {}
		virtual ~IUpdateCursor() {}
		virtual void UpdateRow(IRowPtr ptrRow) = 0;   // the values of the row's columns become the current row's
		virtual void UpdateValue(int32_t col, const CommonLib::CVariant& value) = 0;
	};

	class IDeleteCursor
	{
	public:
		IDeleteCursor() {}
		virtual ~IDeleteCursor() {}
		virtual bool DeleteRow(int64_t nRowID) = 0;   // false if there is no such row
	};

	// A prepared SQL statement (ITransaction::prepare): parsed and planned once, executed many times with other
	// values of the parameters - the "?" of the text, numbered from 0 in the order of the text. Every parameter
	// has to be bound (NULL is the empty variant). A statement belongs to its transaction; one cursor of
	// executeQuery is valid at a time (the next execution makes the previous cursor invalid).
	class IStatement
	{
	public:
		IStatement() {}
		virtual ~IStatement() {}

		virtual uint32_t GetParameterCount() const = 0;
		virtual void BindParameter(uint32_t nIndex, const CommonLib::CVariant& value) = 0;
		virtual void ClearParameters() = 0;
		virtual bool IsQuery() const = 0;                 // SELECT: executeQuery, other statements: execute
		virtual ISelectCursorPtr executeQuery() = 0;
		virtual int64_t execute() = 0;                    // the rows inserted / updated / deleted (0 for DDL, SELECT)
		virtual std::string GetSQL() const = 0;
		// the plan as text: the operators (one per line, the inputs indented), how every table is read
		// ("table roads: index cls =", "spatial index geom", "scan") and the join algorithms
		virtual std::string GetPlan() const = 0;
	};

	class ITransaction
	{
	public:
		ITransaction() {}
		virtual ~ITransaction() {}

		virtual void Commit() = 0;
		virtual void Rollback() = 0;
		virtual bool IsActive() const = 0;
		virtual eTransactionType GetTransactionType() const = 0;
		virtual eTransactionDataType GetTransactionDataType() const = 0;

		// Insert on the right: the rows the transaction inserts / updates come in the order of the indexed values
		// (a sorted load, an export). The index trees split a full leaf by one element (min split) when the new key
		// is the last one of the leaf, so the leaves stay full; a key in the middle splits it in halves anyway, the
		// setting is only about the fill. The row and value trees always do this (the row ids only grow).
		// Off by default (random inserts: leaves split in halves). createInsertCursor(..., bMinSplit) - for one cursor.
		virtual void SetInsertOnRight(bool bOnRight) = 0;
		virtual bool GetInsertOnRight() const = 0;

		// SQL (DB/SQLEngine): SELECT with joins, subqueries, GROUP BY, ORDER BY, LIMIT, UNION; INSERT, UPDATE, DELETE;
		// CREATE / DROP TABLE, ALTER TABLE ADD / DROP COLUMN, CREATE / DROP INDEX. Write statements need a write
		// transaction (DDL - eTT_DDL). A table is read by its indexes when the conditions allow it.
		// executeQuery: one SELECT, the cursor reads the result (GetRowID - the row of the table for a query of one
		// table without aggregation, else -1)
		virtual ISelectCursorPtr executeQuery(const std::string& sQuery) = 0;
		// statements separated by ";" (a SELECT runs, its rows are dropped): the rows inserted / updated / deleted
		virtual int64_t execute(const std::string& sSQL) = 0;
		// one statement with "?" parameters
		virtual IStatementPtr prepare(const std::string& sSQL) = 0;
		// rows by the spatial index of the geometry field (it has to have one)
		virtual ISelectCursorPtr executeSpatialQuery(const CommonLib::bbox& extent, const std::string& sTable, const std::string& sSpatialField,
			eSpatialQueryMode mode = sqmIntersect, IFieldSetPtr ptrFieldSet = IFieldSetPtr()) = 0;
		// all rows (by row id); a null field set - all fields
		virtual ISelectCursorPtr executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet = IFieldSetPtr()) = 0;
		// rows where "sField opType var" (by the index of the field if it has one; NULL values never match)
		virtual ISelectCursorPtr executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::string& sField, const CommonLib::CVariant& var, OpType opType) = 0;
		// the rows of the list (in its order) that exist
		virtual ISelectCursorPtr executeSelectQuery(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::vector<int64_t>& rowIds) = 0;

		// bMinSplit: the rows come in ascending order of the indexed values (a bulk load sorted by the index key):
		// the index trees then split a full leaf by moving only its last key to the new leaf, so the leaves stay full
		// instead of half full. Don't set it for unsorted data: it would leave almost empty leaves.
		// (The row tree and the "row id -> value" trees of the fields always split this way: row ids only grow.)
		virtual IInsertCursorPtr createInsertCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet = IFieldSetPtr(), bool bMinSplit = false) = 0;
		virtual IUpdateCursorPtr createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet = IFieldSetPtr()) = 0;
		virtual IUpdateCursorPtr createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::string& sField, const CommonLib::CVariant& var, OpType opType) = 0;
		virtual IUpdateCursorPtr createUpdateCursor(const std::string& sTable, IFieldSetPtr ptrFieldSet, const std::vector<int64_t>& rowIds) = 0;
		virtual IDeleteCursorPtr createDeleteCursor(const std::string& sTable) = 0;

		// Space statistics: the pages of the trees of the tables (sTable empty - all of them): the rows, the values
		// of every field, the indexes. Fast: the inner nodes are read, the leaves are counted by the links of the
		// lowest inner level. bBlobs: also the stream pages of the big blobs (values over 1 KB: long strings, blobs,
		// geometry) - the leaves of those trees are read and the page chains followed. The pages not in the trees
		// (the schema, the pages a tree doesn't use any more) are m_nFilePages minus the sum.
		// SQL: SELECT * FROM bptreedb_space (bptreedb_space_full - with the blobs).
		virtual void GetSpaceStatistics(SDatabaseSpace& space, const std::string& sTable = std::string(), bool bBlobs = false) = 0;
	};

	class IDatabase
	{
	public:
		IDatabase() {}
		virtual ~IDatabase() {}

		virtual void Create(const std::string& sPathUtf8, const SDatabaseParams& params = SDatabaseParams()) = 0;
		virtual void Create(const std::wstring& sPath, const SDatabaseParams& params = SDatabaseParams()) = 0;
		virtual void Open(const std::string& sPathUtf8, const SDatabaseParams& params = SDatabaseParams()) = 0;
		virtual void Open(const std::wstring& sPath, const SDatabaseParams& params = SDatabaseParams()) = 0;
		virtual void Close() = 0;              // every transaction has to be finished
		virtual bool IsOpen() const = 0;

		virtual ISchemaPtr GetSchema() const = 0;
		// blocks until the lock is free: a read transaction waits for the write transaction, a write transaction
		// for all transactions (a thread mustn't start a write transaction while it has another one)
		virtual ITransactionPtr StartTransaction(eTransactionDataType type, eTransactionType tranType = eTT2PL) = 0;
	};

	IDatabasePtr CreateDatabase();
}
