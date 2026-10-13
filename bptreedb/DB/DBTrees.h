#pragma once

// The trees of a table behind type-erased interfaces (the templates are instantiated in DBTrees.cpp):
//   row tree      row id -> nothing                 which rows exist
//   value field   row id -> value                   one per field; NULL is a missing entry
// The row ids of new rows grow, so the row tree and the value trees always split with the min split strategy
// (a full leaf keeps its keys, the new leaf gets the last one): their leaves stay full. The index trees use it
// when the caller inserts in the order of the index (createInsertCursor(..., bMinSplit)).
//   index         (value, row id) -> nothing        unique and multi indexes (unique is checked by the table)
//   spatial index bounding box -> row id            R-tree or Z-order grid of the geometry field
//
// The trees aren't thread safe: the table serializes the calls (its mutex).
// Readers / sources survive changes of their tree: every tree counts its changes and a reader whose tree
// changed seeks again from the last row id / key it returned.

#include "DBCommon.h"

namespace bptreedb
{
	namespace db
	{
		// row ids in the order of a source (a scan of a tree, an index range, a list)
		class IRowSource
		{
		public:
			IRowSource() {}
			virtual ~IRowSource() {}
			virtual bool Next(int64_t& nRowId) = 0;
		};
		typedef std::shared_ptr<IRowSource> IRowSourcePtr;

		// row ids of a list
		class CListRowSource : public IRowSource
		{
		public:
			CListRowSource(std::vector<int64_t>&& rows) : m_rows(std::move(rows)) {}
			virtual bool Next(int64_t& nRowId)
			{
				if (m_nPos >= m_rows.size())
					return false;
				nRowId = m_rows[m_nPos++];
				return true;
			}
			size_t Size() const { return m_rows.size(); }
		private:
			std::vector<int64_t> m_rows;
			size_t m_nPos{ 0 };
		};

		class ITree
		{
		public:
			ITree() {}
			virtual ~ITree() {}
			virtual int64_t GetInfoPage() const = 0;
			virtual void Flush() = 0;                 // writes the changed nodes (nothing if not changed)
			virtual uint64_t GetChangeCount() const = 0;
		};

		class IRowTree : public ITree
		{
		public:
			virtual void Insert(int64_t nRowId) = 0;
			virtual bool Remove(int64_t nRowId) = 0;
			virtual bool Exists(int64_t nRowId) = 0;
			virtual IRowSourcePtr Scan() = 0;         // all rows by row id
		};
		typedef std::shared_ptr<IRowTree> IRowTreePtr;

		// values of one field, by row id
		class IValueReader
		{
		public:
			IValueReader() {}
			virtual ~IValueReader() {}
			// the value of a row (fast for row ids in ascending order); false and an empty value for NULL
			virtual bool Seek(int64_t nRowId, CommonLib::CVariant& value) = 0;
			// the rows with a value in row id order (the first call - the first row)
			virtual bool Next(int64_t& nRowId, CommonLib::CVariant& value) = 0;
		};
		typedef std::shared_ptr<IValueReader> IValueReaderPtr;

		class IValueField : public ITree
		{
		public:
			virtual void Insert(int64_t nRowId, const CommonLib::CVariant& value) = 0;   // value of the field type, not NULL
			virtual bool Remove(int64_t nRowId) = 0;
			virtual bool Find(int64_t nRowId, CommonLib::CVariant& value) = 0;
			virtual IValueReaderPtr CreateReader() = 0;
		};
		typedef std::shared_ptr<IValueField> IValueFieldPtr;

		class IIndexTree : public ITree
		{
		public:
			virtual void Insert(const CommonLib::CVariant& key, int64_t nRowId) = 0;
			virtual bool Remove(const CommonLib::CVariant& key, int64_t nRowId) = 0;
			// is there an entry with the key (pRowId - the first row with it)
			virtual bool Exists(const CommonLib::CVariant& key, int64_t* pRowId = nullptr) = 0;
			// rows with "value op key", in the order of the index
			virtual IRowSourcePtr Query(const CommonLib::CVariant& key, OpType op) = 0;
			// the keys of the next inserts are ascending: full leaves stay full (TBPlusTreeSetBase::SetMinSplit)
			virtual void SetMinSplit(bool bMinSplit) = 0;
			// Builds an empty index from the values of a field (pReader->Next: the rows with a value): the keys
			// are sorted and bulk loaded (full pages, every node written once). bUnique: a key in several rows
			// throws. bChanged is set when the tree starts to change: an exception before (a duplicate)
			// leaves the tree as it was.
			virtual uint64_t Build(IValueReaderPtr pReader, bool bUnique, bool& bChanged) = 0;
		};
		typedef std::shared_ptr<IIndexTree> IIndexTreePtr;

		class ISpatialIndexTree : public ITree
		{
		public:
			virtual void Insert(const CommonLib::bbox& bb, int64_t nRowId) = 0;
			virtual bool Remove(const CommonLib::bbox& bb, int64_t nRowId) = 0;
			// candidates of a query (sorted, unique); bExact - the index checked the bounding boxes itself
			virtual void Query(const CommonLib::bbox& extent, eSpatialQueryMode mode, std::vector<int64_t>& rows, bool& bExact) = 0;
			// builds an empty index from all the entries at once (R-tree: STR packing, Z-order: sorted keys);
			// the vector is used up
			virtual uint64_t Build(std::vector<std::pair<CommonLib::bbox, int64_t> >& entries) = 0;
		};
		typedef std::shared_ptr<ISpatialIndexTree> ISpatialIndexTreePtr;

		// Storage of a field's values, chosen by the type and the length of the field.
		enum EValueStorage
		{
			evsNumber = 1,       // fixed size numbers in the leaves
			evsString = 2,       // strings in the leaves (up to MaxInlineString())
			evsBlob = 3          // blobs, long strings, geometry: inline or in stream pages
		};
		EValueStorage ChooseValueStorage(eDataTypes type, uint32_t nLength, uint32_t nPageSize);
		uint32_t MaxInlineString(uint32_t nPageSize);

		// The trees are opened on a pager: the pager of a write transaction (its pages are private until the
		// commit) or the committed pages. nInfoPage == -1 creates a new tree (GetInfoPage gives its address).
		IRowTreePtr CreateRowTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, int64_t nInfoPage);
		IValueFieldPtr CreateValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage);
		IIndexTreePtr CreateIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage);

		// the parameters of a spatial index stored in the schema
		struct SSpatialIndexParams
		{
			eIndexTypes m_type{ itSpatialRTree };
			CommonLib::bbox m_extent;      // itSpatialZOrder
			double m_dCellSize{ 0 };       // itSpatialZOrder: the cell of the grid (> 0)
		};
		ISpatialIndexTreePtr CreateSpatialIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, const SSpatialIndexParams& params, int64_t nInfoPage);
	}
}
