#include "Table.h"
#include "Transactions/Transaction.h"
#include "Values.h"

namespace bptreedb
{
	namespace db
	{
		namespace
		{
			bool BBoxMatches(const CommonLib::bbox& bb, const CommonLib::bbox& extent, eSpatialQueryMode mode)
			{
				switch (mode)
				{
				case sqmIntersect: return !(bb.xMax < extent.xMin || extent.xMax < bb.xMin || bb.yMax < extent.yMin || extent.yMax < bb.yMin);
				case sqmOver: return bb.xMin <= extent.xMin && bb.yMin <= extent.yMin && extent.xMax <= bb.xMax && extent.yMax <= bb.yMax;
				case sqmInside: return extent.xMin <= bb.xMin && extent.yMin <= bb.yMin && bb.xMax <= extent.xMax && bb.yMax <= extent.yMax;
				default: return false;
				}
			}

			bool InExtent(const CommonLib::bbox& bb, const CommonLib::bbox& extent)
			{
				return BBoxMatches(bb, extent, sqmInside);
			}

			// rows of a scan of a value tree where "value op key"
			class CFieldScanSource : public IRowSource
			{
			public:
				CFieldScanSource(IValueReaderPtr ptrReader, const CommonLib::CVariant& key, OpType op, eDataTypes type) :
					m_ptrReader(ptrReader), m_key(key), m_op(op), m_type(type)
				{
				}

				virtual bool Next(int64_t& nRowId)
				{
					CommonLib::CVariant value;
					while (m_ptrReader->Next(nRowId, value))
					{
						if (MatchOp(value, m_key, m_op, m_type))
							return true;
					}
					return false;
				}

			private:
				IValueReaderPtr m_ptrReader;
				CommonLib::CVariant m_key;
				OpType m_op;
				eDataTypes m_type;
			};

			bool ValuesEqual(const CommonLib::CVariant& a, const CommonLib::CVariant& b, eDataTypes type)
			{
				if (a.IsNull() || b.IsNull())
					return a.IsNull() && b.IsNull();
				if (IsNumberType(type) || type == dtString)
					return CompareFieldValues(a, b, type) == 0;
				return false;   // blobs, geometry: always written
			}
		}

		CTable::CTable(SDBContextPtr ptrContext) : m_ptrContext(ptrContext)
		{
		}

		CTable::~CTable()
		{
		}

		//------------------------------------------------------------------------------------------------
		// metadata

		void CTable::Create(uint32_t nId, const std::string& sName, STableTrees& trees)
		{
			try
			{
				CheckName(sName, "table");
				m_nId = nId;
				m_sName = sName;
				trees.m_ptrRowTree = CreateRowTree(m_ptrContext, trees.m_ptrPager, -1);
				m_nRowTreePage = trees.m_ptrRowTree->GetInfoPage();
				m_bDirty = true;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to create table '{0}'", sName, excSrc);
				throw;
			}
		}

		void CTable::Write(CommonLib::IWriteStream* pStream, const STableTrees* pTrees) const
		{
			bool bWrite = pTrees != nullptr && pTrees->m_bWrite;
			pStream->Write(m_nId);
			pStream->Write(m_sName);
			pStream->Write(m_nRowTreePage);
			pStream->Write(bWrite ? pTrees->m_nNextRowId : m_nNextRowId);
			pStream->Write(bWrite ? m_nRowCount + pTrees->m_nRowCountDelta : m_nRowCount);
			pStream->Write(m_nNextFieldId);
			pStream->Write((uint32_t)m_fields.size());
			for (const CFieldPtr& ptrField : m_fields)
				ptrField->Write(pStream);
		}

		void CTable::Read(CommonLib::IReadStream* pStream)
		{
			m_nId = pStream->ReadIntu32();
			pStream->Read(m_sName);
			m_nRowTreePage = pStream->ReadInt64();
			m_nNextRowId = pStream->ReadInt64();
			m_nRowCount = pStream->ReadInt64();
			m_nNextFieldId = pStream->ReadIntu32();
			uint32_t nFields = pStream->ReadIntu32();
			m_fields.clear();
			for (uint32_t i = 0; i < nFields; ++i)
				m_fields.push_back(CField::Read(pStream));
		}

		void CTable::Assign(const CTable& table)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_ptrDirect.reset();
			m_nId = table.m_nId;
			m_sName = table.m_sName;
			m_nRowTreePage = table.m_nRowTreePage;
			m_nNextRowId = table.m_nNextRowId;
			m_nRowCount = table.m_nRowCount;
			m_nNextFieldId = table.m_nNextFieldId;
			m_fields = table.m_fields;
			m_bDirty = false;
			m_bDropped = false;
		}

		void CTable::SetDropped()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_ptrDirect.reset();
			m_bDropped = true;
		}

		void CTable::ApplyWrite(const STableTrees& trees)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (!trees.m_bWrite)
				return;
			m_nNextRowId = trees.m_nNextRowId;
			m_nRowCount += trees.m_nRowCountDelta;
		}

		STableTreesPtr CTable::DirectTrees()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			CheckNotDropped();
			uint64_t nCommits = m_ptrContext->m_nCommits;
			if (m_ptrDirect.get() == nullptr || m_nDirectCommits != nCommits)
			{
				// the trees of the last commit (the old set stays with the cursors that hold it)
				m_ptrDirect = std::make_shared<STableTrees>(m_ptrContext->m_ptrPager);
				m_nDirectCommits = nCommits;
			}
			return m_ptrDirect;
		}

		void CTable::CloseDirectTrees()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			m_ptrDirect.reset();
		}

		void STableTrees::Flush()
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			if (m_ptrRowTree.get())
				m_ptrRowTree->Flush();
			for (auto& item : m_fields)
			{
				if (item.second.m_ptrValues.get())
					item.second.m_ptrValues->Flush();
				if (item.second.m_ptrIndex.get())
					item.second.m_ptrIndex->Flush();
				if (item.second.m_ptrSpatial.get())
					item.second.m_ptrSpatial->Flush();
			}
		}

		void CTable::CheckNotDropped() const
		{
			if (m_bDropped)
				throw CommonLib::CExcBase("table '{0}' was dropped", m_sName);
			m_ptrContext->CheckOpen();
		}

		const std::string& CTable::GetName() const
		{
			return m_sName;
		}

		int64_t CTable::GetRowCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_nRowCount;
		}

		std::vector<CFieldPtr> CTable::GetFieldList() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			return m_fields;
		}

		IFieldsPtr CTable::GetFields() const
		{
			return std::make_shared<CFields>(GetFieldList());
		}

		CFieldPtr CTable::FindField(const std::string& sName) const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			for (const CFieldPtr& ptrField : m_fields)
				if (ptrField->GetName() == sName)
					return ptrField;
			return CFieldPtr();
		}

		CFieldPtr CTable::GetFieldThrow(const std::string& sName) const
		{
			CFieldPtr ptrField = FindField(sName);
			if (ptrField.get() == nullptr)
				throw CommonLib::CExcBase("table '{0}' has no field '{1}'", m_sName, sName);
			return ptrField;
		}

		IFieldPtr CTable::GetField(const std::string& sName) const
		{
			return FindField(sName);
		}

		int32_t CTable::GetIndexCount() const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			int32_t n = 0;
			for (const CFieldPtr& ptrField : m_fields)
				if (ptrField->GetIndexImpl().get())
					++n;
			return n;
		}

		IIndexPtr CTable::GetIndex(int32_t nIndex) const
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			int32_t n = 0;
			for (const CFieldPtr& ptrField : m_fields)
			{
				if (ptrField->GetIndexImpl().get() && n++ == nIndex)
					return ptrField->GetIndexImpl();
			}
			throw CommonLib::CExcBase("index {0} is out of range, table '{1}' has {2} indexes", nIndex, m_sName, n);
		}

		IIndexPtr CTable::GetIndex(const std::string& sField) const
		{
			CFieldPtr ptrField = FindField(sField);
			return ptrField.get() ? IIndexPtr(ptrField->GetIndexImpl()) : IIndexPtr();
		}

		CTransaction& CTable::CheckTran(ITransactionPtr ptrTran, bool bDDL) const
		{
			CTransaction* pTran = dynamic_cast<CTransaction*>(ptrTran.get());
			if (pTran == nullptr)
				throw CommonLib::CExcBase("no transaction of the database");
			if (pTran->Context() != m_ptrContext.get())
				throw CommonLib::CExcBase("the transaction belongs to another database");
			if (bDDL)
				pTran->CheckDDL();
			else
				pTran->CheckWrite();
			CheckNotDropped();
			return *pTran;
		}

		void CTable::CheckField(const CFieldPtr& ptrField) const
		{
			for (const CFieldPtr& f : m_fields)
				if (f == ptrField)
					return;
			throw CommonLib::CExcBase("field '{0}' isn't a field of table '{1}' (dropped?)", ptrField.get() ? ptrField->GetName() : std::string(), m_sName);
		}

		//------------------------------------------------------------------------------------------------
		// DDL

		IFieldPtr CTable::CreateField(const SFieldProp& prop, ITransactionPtr ptrTran)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			CTransaction* pTran = nullptr;
			try
			{
				pTran = &CheckTran(ptrTran, true);
				STableTreesPtr ptrTrees = pTran->Trees(*this);
				std::lock_guard<std::recursive_mutex> treesLocker(ptrTrees->m_mutex);
				if (FindField(prop.m_sName).get())
					throw CommonLib::CExcBase("the field exists already");

				CFieldPtr ptrField = std::make_shared<CField>(m_nNextFieldId, prop, m_ptrContext->m_nPageSize);
				const CommonLib::CVariant& def = ptrField->GetDefaultValue();
				int64_t nRows = m_nRowCount + (ptrTrees->m_bWrite ? ptrTrees->m_nRowCountDelta : 0);
				if (nRows > 0)
				{
					if (ptrField->GetIsNotNull() && def.IsNull())
						throw CommonLib::CExcBase("a NOT NULL field without a default value can't be added to a table with rows");
					if (ptrField->GetIsUnique() && !def.IsNull() && nRows > 1)
						throw CommonLib::CExcBase("a unique field with a default value can't be added to a table with several rows");
				}

				// changes
				try
				{
					IValueFieldPtr ptrValues = CreateValueField(m_ptrContext, ptrTrees->m_ptrPager, ptrField->GetType(), ptrField->GetValueStorage(), ptrField->GetLength(), -1);
					ptrField->SetValuePage(ptrValues->GetInfoPage());
					ptrTrees->m_fields[ptrField->GetId()].m_ptrValues = ptrValues;
					++m_nNextFieldId;
					m_fields.push_back(ptrField);
					m_bDirty = true;

					if (!def.IsNull() && nRows > 0)
					{
						IRowSourcePtr ptrRows = RowTree(*ptrTrees)->Scan();
						int64_t nRowId = 0;
						while (ptrRows->Next(nRowId))
							ptrValues->Insert(nRowId, def);
					}

					if (ptrField->GetIsUnique())
					{
						CIndexPtr ptrIndex = std::make_shared<CIndex>(itUnique, ptrField);
						ptrIndex->SetFormat(CIndex::NewFormat(ptrField->GetType()));
						SFieldTrees& trees = ptrTrees->m_fields[ptrField->GetId()];
						trees.m_ptrIndex = CreateIndexTree(m_ptrContext, ptrTrees->m_ptrPager, ptrField->GetType(), -1, ptrIndex->GetFormat());
						ptrIndex->SetTreePage(trees.m_ptrIndex->GetInfoPage());
						bool bChanged = false;
						BuildIndex(ptrField, ptrIndex, trees, bChanged);
						ptrField->SetIndex(ptrIndex);
					}
				}
				catch (...)
				{
					pTran->SetFailed();
					throw;
				}
				return ptrField;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to create field '{1}'", m_sName, prop.m_sName, excSrc);
				throw;
			}
		}

		void CTable::DropField(const std::string& sName, ITransactionPtr ptrTran)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				CTransaction& tran = CheckTran(ptrTran, true);
				CFieldPtr ptrField = GetFieldThrow(sName);
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> treesLocker(ptrTrees->m_mutex);
				ptrTrees->m_fields.erase(ptrField->GetId());
				m_fields.erase(std::find(m_fields.begin(), m_fields.end(), ptrField));
				m_bDirty = true;
				// the pages of the trees aren't reused yet (no free page management)
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to drop field '{1}'", m_sName, sName, excSrc);
				throw;
			}
		}

		IIndexPtr CTable::CreateIndex(const std::string& sField, const SIndexProp& prop, ITransactionPtr ptrTran)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				CTransaction& tran = CheckTran(ptrTran, true);
				CFieldPtr ptrField = GetFieldThrow(sField);
				if (ptrField->GetIndexImpl().get())
					throw CommonLib::CExcBase("the field has an index already");

				eDataTypes type = ptrField->GetType();
				SSpatialIndexParams spatial;
				spatial.m_type = prop.m_type;
				switch (prop.m_type)
				{
				case itUnique:
				case itMultiRegular:
					if (type == dtBlob || type == dtGeometry)
						throw CommonLib::CExcBase("a {0} field can't have a {1} index", DataTypeName(type), prop.m_type == itUnique ? "unique" : "regular");
					if (type == dtString && (ptrField->GetLength() == 0 || ptrField->GetLength() > MaxIndexStringLength))
						throw CommonLib::CExcBase("an indexed string field has to have a length 1..{0}, the field length is {1}", MaxIndexStringLength, ptrField->GetLength());
					break;
				case itSpatialRTree:
				case itSpatialZOrder:
					if (type != dtGeometry)
						throw CommonLib::CExcBase("a spatial index needs a geometry field, the field is {0}", DataTypeName(type));
					if (prop.m_type == itSpatialZOrder)
					{
						const CommonLib::bbox& e = prop.m_extent;
						if (!(e.xMin <= e.xMax && e.yMin <= e.yMax))
							throw CommonLib::CExcBase("a Z-order index needs the extent of the data");
						spatial.m_extent = e;
						spatial.m_dCellSize = prop.m_dCellSize;
						if (spatial.m_dCellSize <= 0)
						{
							if (prop.m_nGridCells == 0)
								throw CommonLib::CExcBase("a Z-order index needs a cell size or the number of cells");
							double dSize = (std::max)(e.xMax - e.xMin, e.yMax - e.yMin);
							spatial.m_dCellSize = dSize > 0 ? dSize / prop.m_nGridCells : 1.0;
						}
					}
					break;
				default:
					throw CommonLib::CExcBase("index type {0} isn't supported", (int)prop.m_type);
				}

				CIndexPtr ptrIndex = std::make_shared<CIndex>(prop.m_type, ptrField);
				if (!ptrIndex->IsSpatial())
					ptrIndex->SetFormat(CIndex::NewFormat(type));
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> treesLocker(ptrTrees->m_mutex);
				SFieldTrees& trees = Trees(*ptrTrees, ptrField);
				try
				{
					if (ptrIndex->IsSpatial())
					{
						ptrIndex->SetSpatialParams(spatial);
						trees.m_ptrSpatial = CreateSpatialIndexTree(m_ptrContext, ptrTrees->m_ptrPager, spatial, -1);
						ptrIndex->SetTreePage(trees.m_ptrSpatial->GetInfoPage());
					}
					else
					{
						trees.m_ptrIndex = CreateIndexTree(m_ptrContext, ptrTrees->m_ptrPager, type, -1, ptrIndex->GetFormat());
						ptrIndex->SetTreePage(trees.m_ptrIndex->GetInfoPage());
					}
				}
				catch (...)
				{
					tran.SetFailed();
					throw;
				}

				// the rows of the table into the index at once
				bool bChanged = false;
				try
				{
					BuildIndex(ptrField, ptrIndex, trees, bChanged, ptrIndex->IsSpatial() ? prop.m_pSpatialEntries : nullptr);
				}
				catch (...)
				{
					// a duplicate / a geometry outside the extent: found before the index changed, the
					// transaction goes on without the index (its new pages are left unused)
					if (bChanged)
						tran.SetFailed();
					trees.m_ptrIndex.reset();
					trees.m_ptrSpatial.reset();
					throw;
				}
				ptrField->SetIndex(ptrIndex);
				m_bDirty = true;
				return ptrIndex;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to create the index of field '{1}'", m_sName, sField, excSrc);
				throw;
			}
		}

		void CTable::BuildIndex(const CFieldPtr& ptrField, const CIndexPtr& ptrIndex, SFieldTrees& trees, bool& bChanged,
		                        std::vector<std::pair<CommonLib::bbox, int64_t> >* pSpatialEntries)
		{
			bool bZOrder = ptrIndex->GetType() == itSpatialZOrder;
			const CommonLib::bbox& extent = ptrIndex->GetSpatialParams().m_extent;
			if (ptrIndex->IsSpatial() && pSpatialEntries)
			{
				// the boxes of the caller
				if (bZOrder)
				{
					for (const auto& e : *pSpatialEntries)
					{
						if (!InExtent(e.first, extent))
							throw CommonLib::CExcBase("the geometry of row {0} is outside the extent of the Z-order index", e.second);
					}
				}
				if (pSpatialEntries->empty())
					return;

				bChanged = true;
				trees.m_ptrSpatial->Build(*pSpatialEntries);
				return;
			}

			IValueReaderPtr ptrReader = trees.m_ptrValues->CreateReader();
			if (!ptrIndex->IsSpatial())
			{
				trees.m_ptrIndex->Build(ptrReader, ptrIndex->IsUnique(), bChanged);
				return;
			}

			std::vector<std::pair<CommonLib::bbox, int64_t> > entries;
			int64_t nRowId = 0;
			CommonLib::CVariant value;
			while (ptrReader->Next(nRowId, value))
			{
				CommonLib::bbox bb;
				if (!GetShapeBBox(value, bb))
					continue;
				if (bZOrder && !InExtent(bb, extent))
					throw CommonLib::CExcBase("the geometry of row {0} is outside the extent of the Z-order index", nRowId);
				entries.emplace_back(bb, nRowId);
			}
			if (entries.empty())
				return;

			bChanged = true;
			trees.m_ptrSpatial->Build(entries);
		}

		void CTable::DropIndex(const std::string& sField, ITransactionPtr ptrTran)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				CTransaction& tran = CheckTran(ptrTran, true);
				CFieldPtr ptrField = GetFieldThrow(sField);
				if (ptrField->GetIndexImpl().get() == nullptr)
					throw CommonLib::CExcBase("the field has no index");
				if (ptrField->GetIsUnique())
					throw CommonLib::CExcBase("the index of a unique field can't be dropped");

				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> treesLocker(ptrTrees->m_mutex);
				auto it = ptrTrees->m_fields.find(ptrField->GetId());
				if (it != ptrTrees->m_fields.end())
				{
					it->second.m_ptrIndex.reset();
					it->second.m_ptrSpatial.reset();
				}
				ptrField->SetIndex(CIndexPtr());
				m_bDirty = true;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to drop the index of field '{1}'", m_sName, sField, excSrc);
				throw;
			}
		}

		//------------------------------------------------------------------------------------------------
		// trees

		IRowTreePtr CTable::RowTree(STableTrees& trees)
		{
			CheckNotDropped();
			if (trees.m_ptrRowTree.get() == nullptr)
				trees.m_ptrRowTree = CreateRowTree(m_ptrContext, trees.m_ptrPager, m_nRowTreePage);
			return trees.m_ptrRowTree;
		}

		SFieldTrees& CTable::Trees(STableTrees& tableTrees, const CFieldPtr& ptrField)
		{
			CheckNotDropped();
			CheckField(ptrField);
			SFieldTrees& trees = tableTrees.m_fields[ptrField->GetId()];
			if (trees.m_ptrValues.get() == nullptr)
				trees.m_ptrValues = CreateValueField(m_ptrContext, tableTrees.m_ptrPager, ptrField->GetType(), ptrField->GetValueStorage(), ptrField->GetLength(), ptrField->GetValuePage());

			CIndexPtr ptrIndex = ptrField->GetIndexImpl();
			if (ptrIndex.get())
			{
				if (ptrIndex->IsSpatial())
				{
					if (trees.m_ptrSpatial.get() == nullptr)
						trees.m_ptrSpatial = CreateSpatialIndexTree(m_ptrContext, tableTrees.m_ptrPager, ptrIndex->GetSpatialParams(), ptrIndex->GetTreePage());
				}
				else if (trees.m_ptrIndex.get() == nullptr)
					trees.m_ptrIndex = CreateIndexTree(m_ptrContext, tableTrees.m_ptrPager, ptrField->GetType(), ptrIndex->GetTreePage(), ptrIndex->GetFormat());
			}
			return trees;
		}

		//------------------------------------------------------------------------------------------------
		// data

		void CTable::CheckValue(const CFieldPtr& ptrField, SFieldTrees& trees, const CommonLib::CVariant& value, int64_t nRowId)
		{
			if (value.IsNull())
			{
				if (ptrField->GetIsNotNull())
					throw CommonLib::CExcBase("field '{0}' can't be NULL", ptrField->GetName());
				return;
			}

			CIndexPtr ptrIndex = ptrField->GetIndexImpl();
			if (ptrIndex.get() == nullptr)
				return;

			if (ptrIndex->IsUnique())
			{
				int64_t nExisting = -1;
				if (trees.m_ptrIndex->Exists(value, &nExisting) && nExisting != nRowId)
					throw CommonLib::CExcBase("field '{0}' is unique, the value is in row {1}", ptrField->GetName(), nExisting);
			}
			else if (ptrIndex->GetType() == itSpatialZOrder)
			{
				CommonLib::bbox bb;
				if (GetShapeBBox(value, bb) && !InExtent(bb, ptrIndex->GetSpatialParams().m_extent))
					throw CommonLib::CExcBase("the geometry of field '{0}' is outside the extent of its Z-order index", ptrField->GetName());
			}
		}

		void CTable::InsertValue(const CFieldPtr& ptrField, SFieldTrees& trees, int64_t nRowId, const CommonLib::CVariant& value)
		{
			if (value.IsNull())
				return;
			trees.m_ptrValues->Insert(nRowId, value);
			if (trees.m_ptrIndex.get())
				trees.m_ptrIndex->Insert(value, nRowId);
			if (trees.m_ptrSpatial.get())
			{
				CommonLib::bbox bb;
				if (GetShapeBBox(value, bb))
					trees.m_ptrSpatial->Insert(bb, nRowId);
			}
		}

		void CTable::RemoveValue(const CFieldPtr& ptrField, SFieldTrees& trees, int64_t nRowId, const CommonLib::CVariant& value)
		{
			if (value.IsNull())
				return;
			if (trees.m_ptrIndex.get() && !trees.m_ptrIndex->Remove(value, nRowId))
				throw CommonLib::CExcBase("index of field '{0}' has no entry of row {1}", ptrField->GetName(), nRowId);
			if (trees.m_ptrSpatial.get())
			{
				CommonLib::bbox bb;
				if (GetShapeBBox(value, bb) && !trees.m_ptrSpatial->Remove(bb, nRowId))
					throw CommonLib::CExcBase("spatial index of field '{0}' has no entry of row {1}", ptrField->GetName(), nRowId);
			}
			if (!trees.m_ptrValues->Remove(nRowId))
				throw CommonLib::CExcBase("field '{0}' has no value of row {1}", ptrField->GetName(), nRowId);
		}

		int64_t CTable::InsertRow(CTransaction& tran, const std::vector<std::pair<CFieldPtr, CommonLib::CVariant> >& values, bool bMinSplit)
		{
			try
			{
				tran.CheckWrite();
				CheckNotDropped();
				bMinSplit = bMinSplit || tran.GetInsertOnRight();
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);

				// values of all fields, checked before anything changes
				std::vector<CommonLib::CVariant> rowValues(m_fields.size());
				std::vector<bool> bSet(m_fields.size(), false);
				for (const auto& item : values)
				{
					size_t i = std::find(m_fields.begin(), m_fields.end(), item.first) - m_fields.begin();
					if (i == m_fields.size())
						CheckField(item.first);
					rowValues[i] = item.first->Convert(item.second);
					bSet[i] = true;
				}

				std::vector<SFieldTrees*> trees(m_fields.size());
				for (size_t i = 0; i < m_fields.size(); ++i)
				{
					if (!bSet[i])
						rowValues[i] = m_fields[i]->GetDefaultValue();
					trees[i] = &Trees(*ptrTrees, m_fields[i]);
					CheckValue(m_fields[i], *trees[i], rowValues[i], -1);
				}
				IRowTreePtr ptrRowTree = RowTree(*ptrTrees);
				if (!ptrTrees->m_bWrite)
				{
					ptrTrees->m_bWrite = true;
					ptrTrees->m_nNextRowId = m_nNextRowId;
					ptrTrees->m_nRowCountDelta = 0;
				}

				try
				{
					int64_t nRowId = ptrTrees->m_nNextRowId++;
					ptrRowTree->Insert(nRowId);
					for (size_t i = 0; i < m_fields.size(); ++i)
					{
						if (bMinSplit && trees[i]->m_ptrIndex.get())
							trees[i]->m_ptrIndex->SetMinSplit(true);
						InsertValue(m_fields[i], *trees[i], nRowId, rowValues[i]);
						if (bMinSplit && trees[i]->m_ptrIndex.get())
							trees[i]->m_ptrIndex->SetMinSplit(false);
					}
					++ptrTrees->m_nRowCountDelta;
					return nRowId;
				}
				catch (...)
				{
					tran.SetFailed();
					for (SFieldTrees* pTrees : trees)
						if (pTrees->m_ptrIndex.get())
							pTrees->m_ptrIndex->SetMinSplit(false);
					throw;
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to insert a row", m_sName, excSrc);
				throw;
			}
		}

		bool CTable::DeleteRow(CTransaction& tran, int64_t nRowId)
		{
			try
			{
				tran.CheckWrite();
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				IRowTreePtr ptrRowTree = RowTree(*ptrTrees);
				if (!ptrRowTree->Exists(nRowId))
					return false;

				std::vector<SFieldTrees*> trees(m_fields.size());
				for (size_t i = 0; i < m_fields.size(); ++i)
					trees[i] = &Trees(*ptrTrees, m_fields[i]);
				if (!ptrTrees->m_bWrite)
				{
					ptrTrees->m_bWrite = true;
					ptrTrees->m_nNextRowId = m_nNextRowId;
					ptrTrees->m_nRowCountDelta = 0;
				}

				try
				{
					CommonLib::CVariant value;
					for (size_t i = 0; i < m_fields.size(); ++i)
					{
						if (trees[i]->m_ptrValues->Find(nRowId, value))
							RemoveValue(m_fields[i], *trees[i], nRowId, value);
					}
					ptrRowTree->Remove(nRowId);
					--ptrTrees->m_nRowCountDelta;
					return true;
				}
				catch (...)
				{
					tran.SetFailed();
					throw;
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to delete row {1}", m_sName, nRowId, excSrc);
				throw;
			}
		}

		void CTable::UpdateRow(CTransaction& tran, int64_t nRowId, const std::vector<std::pair<CFieldPtr, CommonLib::CVariant> >& values)
		{
			try
			{
				tran.CheckWrite();
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				if (!RowTree(*ptrTrees)->Exists(nRowId))
					throw CommonLib::CExcBase("no such row");

				struct SChange
				{
					CFieldPtr m_ptrField;
					SFieldTrees* m_pTrees;
					CommonLib::CVariant m_newValue;
					CommonLib::CVariant m_oldValue;
				};
				std::vector<SChange> changes;
				for (const auto& item : values)
				{
					SChange c;
					c.m_ptrField = item.first;
					c.m_pTrees = &Trees(*ptrTrees, item.first);
					c.m_newValue = item.first->Convert(item.second);
					c.m_pTrees->m_ptrValues->Find(nRowId, c.m_oldValue);
					if (ValuesEqual(c.m_oldValue, c.m_newValue, item.first->GetType()))
						continue;
					CheckValue(c.m_ptrField, *c.m_pTrees, c.m_newValue, nRowId);
					changes.push_back(std::move(c));
				}

				try
				{
					bool bMinSplit = tran.GetInsertOnRight();
					for (SChange& c : changes)
					{
						RemoveValue(c.m_ptrField, *c.m_pTrees, nRowId, c.m_oldValue);
						if (bMinSplit && c.m_pTrees->m_ptrIndex.get())
							c.m_pTrees->m_ptrIndex->SetMinSplit(true);
						InsertValue(c.m_ptrField, *c.m_pTrees, nRowId, c.m_newValue);
						if (bMinSplit && c.m_pTrees->m_ptrIndex.get())
							c.m_pTrees->m_ptrIndex->SetMinSplit(false);
					}
				}
				catch (...)
				{
					tran.SetFailed();
					throw;
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to update row {1}", m_sName, nRowId, excSrc);
				throw;
			}
		}

		bool CTable::RowExists(CTransaction& tran, int64_t nRowId)
		{
			STableTreesPtr ptrTrees = tran.Trees(*this);
			std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
			return RowTree(*ptrTrees)->Exists(nRowId);
		}

		IRowSourcePtr CTable::ScanRows(CTransaction& tran)
		{
			STableTreesPtr ptrTrees = tran.Trees(*this);
			std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
			return RowTree(*ptrTrees)->Scan();
		}

		void CTable::GetSpace(CTransaction& tran, bool bBlobs, std::vector<SSpaceInfo>& space)
		{
			std::lock_guard<std::recursive_mutex> locker(m_mutex);
			try
			{
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> treesLocker(ptrTrees->m_mutex);

				auto add = [&](const std::string& sObject, const char* pszKind, eDataTypes type, ITree* pTree)
				{
					STreeSpace tree;
					pTree->GetSpace(tree, bBlobs);
					SSpaceInfo info;
					info.m_sTable = m_sName;
					info.m_sObject = sObject;
					info.m_sKind = pszKind;
					info.m_type = type;
					info.m_nLevels = tree.m_nLevels;
					info.m_nInnerPages = tree.m_nInnerPages;
					info.m_nLeafPages = tree.m_nLeafPages;
					info.m_nBlobPages = tree.m_nBlobPages;
					info.m_nInfoPages = tree.m_nInfoPages;
					info.m_bBlobsCounted = tree.m_bBlobsCounted;
					space.push_back(info);
				};

				add(std::string(), "rows", dtUnknown, RowTree(*ptrTrees).get());
				for (const CFieldPtr& ptrField : m_fields)
				{
					SFieldTrees& trees = Trees(*ptrTrees, ptrField);
					add(ptrField->GetName(), "values", ptrField->GetType(), trees.m_ptrValues.get());

					CIndexPtr ptrIndex = ptrField->GetIndexImpl();
					if (trees.m_ptrIndex.get())
						add(ptrField->GetName(), ptrIndex.get() && ptrIndex->GetType() == itUnique ? "unique index" : "index", ptrField->GetType(), trees.m_ptrIndex.get());
					if (trees.m_ptrSpatial.get())
						add(ptrField->GetName(), ptrIndex.get() && ptrIndex->GetType() == itSpatialZOrder ? "spatial index Z-order" : "spatial index R-tree",
							ptrField->GetType(), trees.m_ptrSpatial.get());
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to count the pages", m_sName, excSrc);
				throw;
			}
		}

		IValueReaderPtr CTable::CreateReader(CTransaction& tran, CFieldPtr ptrField)
		{
			STableTreesPtr ptrTrees = tran.Trees(*this);
			std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
			return Trees(*ptrTrees, ptrField).m_ptrValues->CreateReader();
		}

		IRowSourcePtr CTable::Select(CTransaction& tran, CFieldPtr ptrField, const CommonLib::CVariant& key, OpType op)
		{
			try
			{
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				eDataTypes type = ptrField->GetType();
				if (!IsNumberType(type) && type != dtString)
					throw CommonLib::CExcBase("values of a {0} field can't be compared", DataTypeName(type));
				if (key.IsNull())
					throw CommonLib::CExcBase("the key is NULL (NULL values never match)");
				if (op < OpEqual || op > OpGreaterOrEqual)
					throw CommonLib::CExcBase("unknown operation {0}", (int)op);

				CommonLib::CVariant fieldKey = ToFieldValue(key, type, 0);
				SFieldTrees& trees = Trees(*ptrTrees, ptrField);
				if (trees.m_ptrIndex.get() && !(type == dtString && fieldKey.Get<CommonLib::astr_t>().size() > MaxIndexStringLength))
					return trees.m_ptrIndex->Query(fieldKey, op);
				return std::make_shared<CFieldScanSource>(trees.m_ptrValues->CreateReader(), fieldKey, op, type);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed to select by field '{1}' {2}", m_sName, ptrField->GetName(), OpTypeName(op), excSrc);
				throw;
			}
		}

		IRowSourcePtr CTable::SpatialSelect(CTransaction& tran, CFieldPtr ptrField, const CommonLib::bbox& extent, eSpatialQueryMode mode)
		{
			try
			{
				STableTreesPtr ptrTrees = tran.Trees(*this);
				std::lock_guard<std::recursive_mutex> locker(ptrTrees->m_mutex);
				if (mode != sqmIntersect && mode != sqmOver && mode != sqmInside)
					throw CommonLib::CExcBase("spatial query mode {0} isn't supported", (int)mode);
				if (!(extent.xMin <= extent.xMax && extent.yMin <= extent.yMax))
					throw CommonLib::CExcBase("wrong extent");

				CIndexPtr ptrIndex = ptrField->GetIndexImpl();
				if (ptrIndex.get() == nullptr || !ptrIndex->IsSpatial())
					throw CommonLib::CExcBase("the field has no spatial index");

				SFieldTrees& trees = Trees(*ptrTrees, ptrField);
				std::vector<int64_t> rows;
				bool bExact = false;
				trees.m_ptrSpatial->Query(extent, mode, rows, bExact);
				if (!bExact)
				{
					// candidates by the grid cells: check the bounding boxes
					IValueReaderPtr ptrReader = trees.m_ptrValues->CreateReader();
					CommonLib::CVariant value;
					size_t n = 0;
					for (int64_t nRowId : rows)
					{
						CommonLib::bbox bb;
						if (ptrReader->Seek(nRowId, value) && GetShapeBBox(value, bb) && BBoxMatches(bb, extent, mode))
							rows[n++] = nRowId;
					}
					rows.resize(n);
				}
				return std::make_shared<CListRowSource>(std::move(rows));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("table '{0}': failed spatial query of field '{1}'", m_sName, ptrField->GetName(), excSrc);
				throw;
			}
		}
	}
}
