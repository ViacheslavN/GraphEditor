#include "Statement.h"
#include "../Cursors/Row.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include <atomic>
#include <unordered_map>
#include <set>

namespace bptreedb
{
	namespace sql
	{
		namespace
		{
			std::atomic<uint64_t> g_nExecution{ 0 };

			bool EqualNoCase(const std::string& a, const std::string& b)
			{
				if (a.size() != b.size())
					return false;
				for (size_t i = 0; i < a.size(); ++i)
					if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i]))
						return false;
				return true;
			}

			IFieldPtr FindField(const ITablePtr& ptrTable, const std::string& sName)
			{
				IFieldPtr ptrField = ptrTable->GetField(sName);
				if (ptrField.get())
					return ptrField;
				IFieldsPtr ptrFields = ptrTable->GetFields();
				for (int32_t i = 0; i < ptrFields->GetFieldCount(); ++i)
					if (EqualNoCase(ptrFields->GetField(i)->GetName(), sName))
						return ptrFields->GetField(i);
				return IFieldPtr();
			}
		}

		CSQLStatement::CSQLStatement(ITransactionPtr ptrTran, ISchemaPtr ptrSchema, SStatementPtr ptrAst, uint32_t nParameters, const std::string& sSQL) :
			m_ptrTran(ptrTran), m_ptrSchema(ptrSchema), m_ptrAst(ptrAst), m_sSQL(sSQL), m_params(nParameters), m_bound(nParameters, false)
		{
			try
			{
				Compile();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("SQL: failed to compile '{0}'", m_sSQL, excSrc);
				throw;
			}
		}

		void CSQLStatement::Compile()
		{
			CQueryCompiler compiler(m_ptrSchema);
			switch (m_ptrAst->m_type)
			{
			case eStatementType::Select:
				m_select = compiler.CompileSelect(static_cast<const SSelectStmt&>(*m_ptrAst));
				break;
			case eStatementType::Update:
				m_modify = compiler.CompileUpdate(static_cast<const SUpdateStmt&>(*m_ptrAst));
				break;
			case eStatementType::Delete:
				m_modify = compiler.CompileDelete(static_cast<const SDeleteStmt&>(*m_ptrAst));
				break;
			case eStatementType::Insert:
				{
					const SInsertStmt& insert = static_cast<const SInsertStmt&>(*m_ptrAst);
					ITablePtr ptrTable = compiler.GetTable(insert.m_sTable, insert.m_pos);
					m_sInsertTable = ptrTable->GetName();
					if (insert.m_columns.empty())
					{
						IFieldsPtr ptrFields = ptrTable->GetFields();
						for (int32_t i = 0; i < ptrFields->GetFieldCount(); ++i)
							m_insertColumns.push_back(ptrFields->GetField(i)->GetName());
					}
					else
					{
						for (const std::string& sColumn : insert.m_columns)
						{
							IFieldPtr ptrField = FindField(ptrTable, sColumn);
							if (ptrField.get() == nullptr)
								ThrowAt(insert.m_pos, "table '" + m_sInsertTable + "' has no column '" + sColumn + "'");
							if (std::find(m_insertColumns.begin(), m_insertColumns.end(), ptrField->GetName()) != m_insertColumns.end())
								ThrowAt(insert.m_pos, "column '" + sColumn + "' is twice in INSERT");
							m_insertColumns.push_back(ptrField->GetName());
						}
					}

					size_t nColumns = m_insertColumns.size();
					if (insert.m_ptrSelect.get())
					{
						m_ptrInsertSelect = std::make_shared<SSelectPlan>(compiler.CompileSelect(*insert.m_ptrSelect));
						if (m_ptrInsertSelect->m_nColumns != nColumns)
							ThrowAt(insert.m_pos, "INSERT has " + std::to_string(nColumns) + " columns, the SELECT " + std::to_string(m_ptrInsertSelect->m_nColumns));
						// the table is read and changed: the rows are read first
						m_bInsertMaterialize = compiler.TablesRead().count(m_sInsertTable) != 0;
					}
					else
					{
						for (const TExprList& row : insert.m_rows)
						{
							if (row.size() != nColumns)
								ThrowAt(row.empty() ? insert.m_pos : row[0]->m_pos, "INSERT has " + std::to_string(nColumns) + " columns, a row of VALUES " + std::to_string(row.size()));
							std::vector<CExprPtr> values;
							for (const SExprPtr& ptrValue : row)
								values.push_back(compiler.CompileValue(ptrValue));
							m_insertRows.push_back(std::move(values));
						}
					}
				}
				break;
			default:
				break;     // DDL: when it runs
			}
		}

		std::string CSQLStatement::GetPlan() const
		{
			std::string sText;
			switch (m_ptrAst->m_type)
			{
			case eStatementType::Select:
				m_select.m_ptrRoot->Explain(sText, 0);
				break;
			case eStatementType::Update:
			case eStatementType::Delete:
				sText = std::string(m_ptrAst->m_type == eStatementType::Update ? "update " : "delete from ") + m_modify.m_sTable + "\n";
				m_modify.m_ptrRoot->Explain(sText, 1);
				break;
			case eStatementType::Insert:
				sText = "insert into " + m_sInsertTable + (m_bInsertMaterialize ? " (the rows read first)" : "") + "\n";
				if (m_ptrInsertSelect.get())
					m_ptrInsertSelect->m_ptrRoot->Explain(sText, 1);
				else
					sText += "  values: " + std::to_string(m_insertRows.size()) + " row(s)\n";
				break;
			default:
				sText = "DDL\n";
				break;
			}
			return sText;
		}

		void CSQLStatement::BindParameter(uint32_t nIndex, const CommonLib::CVariant& value)
		{
			if (nIndex >= m_params.size())
				throw CommonLib::CExcBase("SQL: parameter {0} is out of the range, the statement has {1}", nIndex, m_params.size());
			m_params[nIndex] = NormalizeValue(value);
			m_bound[nIndex] = true;
		}

		void CSQLStatement::ClearParameters()
		{
			for (size_t i = 0; i < m_params.size(); ++i)
			{
				m_params[i] = TValue();
				m_bound[i] = false;
			}
		}

		void CSQLStatement::NewExecution(SExecContext& exec)
		{
			if (!m_ptrTran->IsActive())
				throw CommonLib::CExcBase("the transaction isn't active");
			for (size_t i = 0; i < m_bound.size(); ++i)
				if (!m_bound[i])
					throw CommonLib::CExcBase("parameter {0} isn't bound", i);
			exec.m_ptrTran = m_ptrTran;
			exec.m_params = m_params;
			exec.m_nExecution = ++g_nExecution;
			++m_nGeneration;
		}

		ISelectCursorPtr CSQLStatement::executeQuery()
		{
			try
			{
				if (!IsQuery())
					throw CommonLib::CExcBase("not a SELECT: use execute");
				SExecContext exec;
				NewExecution(exec);
				auto ptrCursor = std::make_shared<CSQLCursor>(shared_from_this(), m_select);
				ptrCursor->Start(exec);
				return ptrCursor;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("SQL: failed to execute '{0}'", m_sSQL, excSrc);
				throw;
			}
		}

		int64_t CSQLStatement::execute()
		{
			try
			{
				if (IsQuery())
				{
					ISelectCursorPtr ptrCursor = executeQuery();
					while (ptrCursor->Next())
					{
					}
					return 0;
				}

				SExecContext exec;
				NewExecution(exec);
				switch (m_ptrAst->m_type)
				{
				case eStatementType::Insert: return Insert(exec);
				case eStatementType::Update: return Update(exec);
				case eStatementType::Delete: return Delete(exec);
				default: return ExecuteDDL(exec);
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("SQL: failed to execute '{0}'", m_sSQL, excSrc);
				throw;
			}
		}

		int64_t CSQLStatement::Insert(SExecContext& exec)
		{
			IFieldSetPtr ptrFieldSet = IFieldSet::Create(m_insertColumns);
			IInsertCursorPtr ptrCursor = m_ptrTran->createInsertCursor(m_sInsertTable, ptrFieldSet);
			int64_t nRows = 0;
			auto Insert = [&](const TRow& row)
			{
				for (size_t i = 0; i < row.size(); ++i)
				{
					if (row[i].IsNull())
						ptrCursor->BindNull((int32_t)i);
					else
						ptrCursor->BindValue((int32_t)i, row[i]);
				}
				ptrCursor->Next();
				++nRows;
			};

			if (m_ptrInsertSelect.get() == nullptr)
			{
				TRow empty;
				SEvalContext ctx;
				ctx.m_pRow = &empty;
				ctx.m_pExec = &exec;
				for (const std::vector<CExprPtr>& values : m_insertRows)
				{
					TRow row;
					for (const CExprPtr& ptrValue : values)
						row.push_back(ptrValue->Eval(ctx));
					Insert(row);
				}
			}
			else
			{
				COperator& root = *m_ptrInsertSelect->m_ptrRoot;
				uint32_t nColumns = m_ptrInsertSelect->m_nColumns;
				std::vector<TRow> rows;
				root.Open(exec, nullptr);
				while (root.Next())
				{
					TRow row(root.Row().begin(), root.Row().begin() + nColumns);
					if (m_bInsertMaterialize)
						rows.push_back(std::move(row));
					else
						Insert(row);
				}
				root.Close();
				for (const TRow& row : rows)
					Insert(row);
			}
			ptrCursor->Finish();
			return nRows;
		}

		int64_t CSQLStatement::Update(SExecContext& exec)
		{
			// the rows and their new values first: the changes don't change which rows are updated
			std::vector<int64_t> rowIds;
			std::vector<TRow> values;
			COperator& root = *m_modify.m_ptrRoot;
			root.Open(exec, nullptr);
			while (root.Next())
			{
				const TRow& row = root.Row();
				SEvalContext ctx;
				ctx.m_pRow = &row;
				ctx.m_pExec = &exec;
				rowIds.push_back(row[m_modify.m_nRowIdIndex].Get<int64_t>());
				TRow rowValues;
				for (const CExprPtr& ptrValue : m_modify.m_values)
					rowValues.push_back(ptrValue->Eval(ctx));
				values.push_back(std::move(rowValues));
			}
			root.Close();
			if (rowIds.empty())
				return 0;

			std::unordered_map<int64_t, size_t> index;
			for (size_t i = 0; i < rowIds.size(); ++i)
				index[rowIds[i]] = i;
			IUpdateCursorPtr ptrCursor = m_ptrTran->createUpdateCursor(m_modify.m_sTable, IFieldSet::Create(m_modify.m_columns), rowIds);
			IRowPtr ptrRow = ptrCursor->CreateRow();
			int64_t nRows = 0;
			while (ptrCursor->Next())
			{
				const TRow& rowValues = values[index.at(ptrCursor->GetRowID())];
				for (size_t i = 0; i < rowValues.size(); ++i)
					ptrRow->SetValue((int32_t)i, rowValues[i]);
				ptrCursor->UpdateRow(ptrRow);
				++nRows;
			}
			return nRows;
		}

		int64_t CSQLStatement::Delete(SExecContext& exec)
		{
			std::vector<int64_t> rowIds;
			COperator& root = *m_modify.m_ptrRoot;
			root.Open(exec, nullptr);
			while (root.Next())
				rowIds.push_back(root.Row()[m_modify.m_nRowIdIndex].Get<int64_t>());
			root.Close();

			int64_t nRows = 0;
			if (rowIds.empty())
				return 0;
			IDeleteCursorPtr ptrCursor = m_ptrTran->createDeleteCursor(m_modify.m_sTable);
			for (int64_t nRowId : rowIds)
				if (ptrCursor->DeleteRow(nRowId))
					++nRows;
			return nRows;
		}

		SFieldProp CSQLStatement::FieldProp(const SColumnDef& column, SExecContext& exec)
		{
			SFieldProp prop;
			prop.m_sName = column.m_sName;
			bool bKnown = false;
			try
			{
				bKnown = MapTypeName(column.m_type, prop.m_type, prop.m_nLength);
			}
			catch (std::exception& e)
			{
				ThrowAt(column.m_pos, e.what());
			}
			if (!bKnown)
				ThrowAt(column.m_pos, "unknown type " + column.m_type.m_sName);
			prop.m_bNotNull = column.m_bNotNull;
			prop.m_bUnique = column.m_bUnique;
			if (column.m_ptrDefault.get())
			{
				CQueryCompiler compiler(m_ptrSchema);
				CExprPtr ptrDefault = compiler.CompileValue(column.m_ptrDefault);
				TRow empty;
				SEvalContext ctx;
				ctx.m_pRow = &empty;
				ctx.m_pExec = &exec;
				TValue value = ptrDefault->Eval(ctx);
				if (!value.IsNull())
					prop.m_defaultValue = db::ToFieldValue(value, prop.m_type, prop.m_nLength);
			}
			return prop;
		}

		int64_t CSQLStatement::ExecuteDDL(SExecContext& exec)
		{
			CQueryCompiler compiler(m_ptrSchema);
			switch (m_ptrAst->m_type)
			{
			case eStatementType::CreateTable:
				{
					const SCreateTableStmt& create = static_cast<const SCreateTableStmt&>(*m_ptrAst);
					if (compiler.FindTable(create.m_sTable).get())
					{
						if (create.m_bIfNotExists)
							return 0;
						ThrowAt(create.m_pos, "table '" + create.m_sTable + "' exists");
					}
					std::vector<SFieldProp> props;
					for (size_t i = 0; i < create.m_columns.size(); ++i)
					{
						for (size_t j = 0; j < i; ++j)
							if (EqualNoCase(create.m_columns[i].m_sName, create.m_columns[j].m_sName))
								ThrowAt(create.m_columns[i].m_pos, "column '" + create.m_columns[i].m_sName + "' is twice");
						props.push_back(FieldProp(create.m_columns[i], exec));
					}
					ITablePtr ptrTable = m_ptrSchema->CreateTable(create.m_sTable, m_ptrTran);
					for (const SFieldProp& prop : props)
						ptrTable->CreateField(prop, m_ptrTran);
				}
				break;
			case eStatementType::DropTable:
				{
					const SDropTableStmt& drop = static_cast<const SDropTableStmt&>(*m_ptrAst);
					ITablePtr ptrTable = compiler.FindTable(drop.m_sTable);
					if (ptrTable.get() == nullptr)
					{
						if (drop.m_bIfExists)
							return 0;
						ThrowAt(drop.m_pos, "no table '" + drop.m_sTable + "'");
					}
					m_ptrSchema->DropTable(ptrTable->GetName(), m_ptrTran);
				}
				break;
			case eStatementType::AlterTable:
				{
					const SAlterTableStmt& alter = static_cast<const SAlterTableStmt&>(*m_ptrAst);
					ITablePtr ptrTable = compiler.GetTable(alter.m_sTable, alter.m_pos);
					if (alter.m_action == SAlterTableStmt::AddColumn)
					{
						if (FindField(ptrTable, alter.m_column.m_sName).get())
							ThrowAt(alter.m_column.m_pos, "column '" + alter.m_column.m_sName + "' exists");
						ptrTable->CreateField(FieldProp(alter.m_column, exec), m_ptrTran);
					}
					else
					{
						IFieldPtr ptrField = FindField(ptrTable, alter.m_sColumn);
						if (ptrField.get() == nullptr)
							ThrowAt(alter.m_pos, "table '" + ptrTable->GetName() + "' has no column '" + alter.m_sColumn + "'");
						ptrTable->DropField(ptrField->GetName(), m_ptrTran);
					}
				}
				break;
			case eStatementType::CreateIndex:
				{
					const SCreateIndexStmt& create = static_cast<const SCreateIndexStmt&>(*m_ptrAst);
					ITablePtr ptrTable = compiler.GetTable(create.m_sTable, create.m_pos);
					if (create.m_columns.size() != 1)
						ThrowAt(create.m_pos, "an index is of one column");
					IFieldPtr ptrField = FindField(ptrTable, create.m_columns[0]);
					if (ptrField.get() == nullptr)
						ThrowAt(create.m_pos, "table '" + ptrTable->GetName() + "' has no column '" + create.m_columns[0] + "'");
					if (ptrField->GetIndex().get())
					{
						if (create.m_bIfNotExists)
							return 0;
						ThrowAt(create.m_pos, "column '" + ptrField->GetName() + "' has an index");
					}

					SIndexProp prop;
					const std::string& sMethod = create.m_sMethod;
					bool bGeometry = ptrField->GetType() == dtGeometry;
					if (sMethod == "RTREE" || (sMethod.empty() && (create.m_kind == SCreateIndexStmt::Spatial || bGeometry)))
						prop.m_type = itSpatialRTree;
					else if (sMethod == "ZORDER")
					{
						prop.m_type = itSpatialZOrder;
						const std::vector<double>& p = create.m_methodParams;
						if (p.size() != 4 && p.size() != 5)
							ThrowAt(create.m_pos, "ZORDER(xmin, ymin, xmax, ymax [, cell size]): the extent of the grid");
						prop.m_extent.xMin = p[0];
						prop.m_extent.yMin = p[1];
						prop.m_extent.xMax = p[2];
						prop.m_extent.yMax = p[3];
						if (p.size() == 5)
							prop.m_dCellSize = p[4];
					}
					else if (sMethod.empty() || sMethod == "BTREE")
						prop.m_type = create.m_kind == SCreateIndexStmt::Unique ? itUnique : itMultiRegular;
					else
						ThrowAt(create.m_pos, "unknown index method " + sMethod + ": BTREE, RTREE, ZORDER");

					bool bSpatial = prop.m_type == itSpatialRTree || prop.m_type == itSpatialZOrder;
					if (bSpatial && create.m_kind == SCreateIndexStmt::Unique)
						ThrowAt(create.m_pos, "a spatial index can't be unique");
					if (!bSpatial && create.m_kind == SCreateIndexStmt::Spatial)
						ThrowAt(create.m_pos, "SPATIAL INDEX with " + sMethod);
					if (bSpatial != bGeometry)
						ThrowAt(create.m_pos, bSpatial ? "a spatial index of a column that isn't a geometry" : "a geometry column needs a spatial index");
					ptrTable->CreateIndex(ptrField->GetName(), prop, m_ptrTran);
				}
				break;
			case eStatementType::DropIndex:
				{
					const SDropIndexStmt& drop = static_cast<const SDropIndexStmt&>(*m_ptrAst);
					ITablePtr ptrTable = compiler.GetTable(drop.m_sTable, drop.m_pos);
					if (drop.m_columns.size() != 1)
						ThrowAt(drop.m_pos, "an index is of one column");
					IFieldPtr ptrField = FindField(ptrTable, drop.m_columns[0]);
					if (ptrField.get() == nullptr)
						ThrowAt(drop.m_pos, "table '" + ptrTable->GetName() + "' has no column '" + drop.m_columns[0] + "'");
					if (ptrField->GetIndex().get() == nullptr)
					{
						if (drop.m_bIfExists)
							return 0;
						ThrowAt(drop.m_pos, "column '" + ptrField->GetName() + "' has no index");
					}
					ptrTable->DropIndex(ptrField->GetName(), m_ptrTran);
				}
				break;
			default:
				throw CommonLib::CExcBase("unexpected statement");
			}
			return 0;
		}

		//------------------------------------------------------------------------------------------
		// cursor

		CSQLCursor::CSQLCursor(std::shared_ptr<CSQLStatement> ptrStatement, const SSelectPlan& plan) :
			m_ptrStatement(ptrStatement), m_plan(plan)
		{
			// the field set needs unique names: a repeated name gets a suffix (ColumnName is the name of the query)
			std::vector<std::string> names;
			std::set<std::string> used;
			for (uint32_t i = 0; i < m_plan.m_nColumns; ++i)
			{
				std::string sName = m_plan.m_names[i];
				for (int n = 2; used.count(sName); ++n)
					sName = m_plan.m_names[i] + "_" + std::to_string(n);
				used.insert(sName);
				names.push_back(sName);
			}
			m_ptrFieldSet = IFieldSet::Create(names);
		}

		CSQLCursor::~CSQLCursor()
		{
			if (m_bOpen && m_ptrStatement->Generation() == m_nGeneration)
			{
				try
				{
					m_plan.m_ptrRoot->Close();
				}
				catch (...)
				{
				}
			}
		}

		void CSQLCursor::Start(const SExecContext& exec)
		{
			m_exec = exec;
			m_nGeneration = m_ptrStatement->Generation();
			m_bOpen = true;
			m_plan.m_ptrRoot->Open(m_exec, nullptr);
		}

		void CSQLCursor::CheckValid() const
		{
			if (m_ptrStatement->Generation() != m_nGeneration)
				throw CommonLib::CExcBase("SQL: the cursor isn't valid: its statement was executed again");
			if (!m_exec.m_ptrTran->IsActive())
				throw CommonLib::CExcBase("SQL: the transaction of the cursor isn't active");
		}

		bool CSQLCursor::Next()
		{
			CheckValid();
			if (m_bEnd)
				return false;
			try
			{
				m_bRow = m_plan.m_ptrRoot->Next();
			}
			catch (std::exception& excSrc)
			{
				m_bRow = false;
				CommonLib::CExcBase::RegenExcT("SQL: failed to read the next row of '{0}'", m_ptrStatement->GetSQL(), excSrc);
				throw;
			}
			if (!m_bRow)
			{
				m_bEnd = true;
				m_bOpen = false;
				m_plan.m_ptrRoot->Close();
			}
			return m_bRow;
		}

		const TValue& CSQLCursor::Cell(int32_t col) const
		{
			CheckValid();
			if (!m_bRow)
				throw CommonLib::CExcBase(m_bEnd ? "the cursor is at the end" : "no current row: call Next()");
			if (col < 0 || col >= (int32_t)m_plan.m_nColumns)
				throw CommonLib::CExcBase("column {0} is out of the range, the cursor has {1}", col, m_plan.m_nColumns);
			return m_plan.m_ptrRoot->Row()[col];
		}

		const TValue& CSQLCursor::Value(int32_t col) const
		{
			const TValue& value = Cell(col);
			if (value.IsNull())
				throw CommonLib::CExcBase("column '{0}' is NULL", m_plan.m_names[col]);
			return value;
		}

		template <class T>
		T CSQLCursor::Number(int32_t col) const
		{
			try
			{
				const TValue& value = Value(col);
				if (ValueKind(value) == eValueKind::String)
				{
					TValue number;
					if (!ParseNumber(value.Get<CommonLib::astr_t>(), number))
						throw CommonLib::CExcBase("'{0}' isn't a number", value.Get<CommonLib::astr_t>());
					return db::VariantToNumber<T>(number);
				}
				return db::VariantToNumber<T>(value);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to read column {0}", col, excSrc);
				throw;
			}
		}

		std::string CSQLCursor::ColumnName(int32_t col) const
		{
			if (col < 0 || col >= (int32_t)m_plan.m_nColumns)
				throw CommonLib::CExcBase("column {0} is out of the range, the cursor has {1}", col, m_plan.m_nColumns);
			return m_plan.m_names[col];
		}

		bool CSQLCursor::ColumnIsNull(int32_t col) const
		{
			return Cell(col).IsNull();
		}

		eDataTypes CSQLCursor::GetColumnType(int32_t col) const
		{
			if (col < 0 || col >= (int32_t)m_plan.m_nColumns)
				throw CommonLib::CExcBase("column {0} is out of the range, the cursor has {1}", col, m_plan.m_nColumns);
			if (m_plan.m_types[col] != dtUnknown || !m_bRow)
				return m_plan.m_types[col];
			return ValueType(m_plan.m_ptrRoot->Row()[col]);
		}

		int32_t CSQLCursor::GetColumnBytes(int32_t col) const
		{
			return db::ValueBytes(Cell(col));
		}

		int32_t CSQLCursor::FindFieldByName(const std::string& name) const
		{
			for (uint32_t i = 0; i < m_plan.m_nColumns; ++i)
				if (m_plan.m_names[i] == name)
					return (int32_t)i;
			for (uint32_t i = 0; i < m_plan.m_nColumns; ++i)
				if (EqualNoCase(m_plan.m_names[i], name))
					return (int32_t)i;
			return -1;
		}

		IFieldSetPtr CSQLCursor::GetFieldSet() const
		{
			return m_ptrFieldSet;
		}

		int64_t CSQLCursor::GetRowID() const
		{
			if (m_plan.m_nRowIdColumn < 0 || !m_bRow)
				return -1;
			CheckValid();
			const TValue& value = m_plan.m_ptrRoot->Row()[m_plan.m_nRowIdColumn];
			return value.IsNull() ? -1 : value.Get<int64_t>();
		}

		std::string CSQLCursor::ReadText(int32_t col) const
		{
			try
			{
				return ToText(Value(col));
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to read column {0}", col, excSrc);
				throw;
			}
		}

		std::wstring CSQLCursor::ReadTextW(int32_t col) const
		{
			return CommonLib::StringEncoding::str_utf82w_safe(ReadText(col));
		}

		void CSQLCursor::ReadBlob(int32_t col, const byte_t** pBuf, int32_t& size) const
		{
			const TValue& value = Value(col);
			switch (ValueKind(value))
			{
			case eValueKind::Blob:
				*pBuf = value.Get<CommonLib::CBlobPtr_t>()->Buffer();
				size = (int32_t)value.Get<CommonLib::CBlobPtr_t>()->Size();
				break;
			case eValueKind::String:
				*pBuf = (const byte_t*)value.Get<CommonLib::astr_t>().data();
				size = (int32_t)value.Get<CommonLib::astr_t>().size();
				break;
			case eValueKind::Geometry:
				*pBuf = value.Get<CommonLib::IGeoShapePtr>()->Data();
				size = (int32_t)value.Get<CommonLib::IGeoShapePtr>()->Size();
				break;
			default:
				throw CommonLib::CExcBase("column {0} isn't a blob", col);
			}
		}

		CommonLib::IGeoShapePtr CSQLCursor::ReadShape(int32_t col) const
		{
			const TValue& value = Value(col);
			if (ValueKind(value) != eValueKind::Geometry)
				throw CommonLib::CExcBase("column {0} isn't a geometry", col);
			return value.Get<CommonLib::IGeoShapePtr>();
		}

		const CommonLib::CVariant& CSQLCursor::ReadValue(int32_t col) const
		{
			return Cell(col);
		}

		IRowPtr CSQLCursor::CreateRow() const
		{
			return std::make_shared<db::CRow>(m_ptrFieldSet);
		}

		void CSQLCursor::FillRow(IRowPtr ptrRow) const
		{
			if (ptrRow.get() == nullptr || ptrRow->GetCount() != (int32_t)m_plan.m_nColumns)
				throw CommonLib::CExcBase("the row doesn't have the columns of the cursor");
			for (uint32_t i = 0; i < m_plan.m_nColumns; ++i)
				ptrRow->SetValue((int32_t)i, Cell((int32_t)i));
			ptrRow->SetRowID(GetRowID());
		}
	}
}
