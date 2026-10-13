#pragma once

// A compiled SQL statement (IStatement) and the cursor of a SELECT (ISelectCursor over the plan).

#include "Compiler.h"
#include "../Values.h"

namespace bptreedb
{
	namespace sql
	{
		class CSQLStatement : public IStatement, public std::enable_shared_from_this<CSQLStatement>
		{
		public:
			// compiles the statement against the schema (DDL is checked when it runs)
			CSQLStatement(ITransactionPtr ptrTran, ISchemaPtr ptrSchema, SStatementPtr ptrAst, uint32_t nParameters, const std::string& sSQL);
			virtual ~CSQLStatement() {}

			// IStatement
			virtual uint32_t GetParameterCount() const { return (uint32_t)m_params.size(); }
			virtual void BindParameter(uint32_t nIndex, const CommonLib::CVariant& value);
			virtual void ClearParameters();
			virtual bool IsQuery() const { return m_ptrAst->m_type == eStatementType::Select; }
			virtual ISelectCursorPtr executeQuery();
			virtual int64_t execute();
			virtual std::string GetSQL() const { return m_sSQL; }
			virtual std::string GetPlan() const;

			uint64_t Generation() const { return m_nGeneration; }

		private:
			void Compile();
			void NewExecution(SExecContext& exec);
			int64_t Insert(SExecContext& exec);
			int64_t Update(SExecContext& exec);
			int64_t Delete(SExecContext& exec);
			int64_t ExecuteDDL(SExecContext& exec);
			SFieldProp FieldProp(const SColumnDef& column, SExecContext& exec);

			ITransactionPtr m_ptrTran;
			ISchemaPtr m_ptrSchema;
			SStatementPtr m_ptrAst;
			std::string m_sSQL;
			std::vector<TValue> m_params;
			std::vector<bool> m_bound;
			uint64_t m_nGeneration{ 0 };

			// SELECT
			SSelectPlan m_select;
			// UPDATE / DELETE
			SModifyPlan m_modify;
			// INSERT
			std::string m_sInsertTable;
			std::vector<std::string> m_insertColumns;
			std::vector<std::vector<CExprPtr> > m_insertRows;
			std::shared_ptr<SSelectPlan> m_ptrInsertSelect;
			bool m_bInsertMaterialize{ false };
		};

		class CSQLCursor : public ISelectCursor
		{
		public:
			CSQLCursor(std::shared_ptr<CSQLStatement> ptrStatement, const SSelectPlan& plan);
			virtual ~CSQLCursor();

			void Start(const SExecContext& exec);    // opens the plan

			// ICursor
			virtual int32_t ColumnCount() const { return (int32_t)m_plan.m_nColumns; }
			virtual std::string ColumnName(int32_t col) const;
			virtual bool ColumnIsNull(int32_t col) const;
			virtual eDataTypes GetColumnType(int32_t col) const;
			virtual int32_t GetColumnBytes(int32_t col) const;
			virtual int32_t FindFieldByName(const std::string& name) const;
			virtual IFieldSetPtr GetFieldSet() const;
			virtual bool Next();
			virtual int64_t GetRowID() const;

			// ISelectCursor
			virtual int8_t ReadInt8(int32_t col) const { return Number<int8_t>(col); }
			virtual uint8_t ReadUInt8(int32_t col) const { return Number<uint8_t>(col); }
			virtual int16_t ReadInt16(int32_t col) const { return Number<int16_t>(col); }
			virtual uint16_t ReadUInt16(int32_t col) const { return Number<uint16_t>(col); }
			virtual int32_t ReadInt32(int32_t col) const { return Number<int32_t>(col); }
			virtual uint32_t ReadUInt32(int32_t col) const { return Number<uint32_t>(col); }
			virtual int64_t ReadInt64(int32_t col) const { return Number<int64_t>(col); }
			virtual uint64_t ReadUInt64(int32_t col) const { return Number<uint64_t>(col); }
			virtual float ReadFloat(int32_t col) const { return Number<float>(col); }
			virtual double ReadDouble(int32_t col) const { return Number<double>(col); }
			virtual void ReadText(int32_t col, std::string& text) const { text = ReadText(col); }
			virtual std::string ReadText(int32_t col) const;
			virtual void ReadTextW(int32_t col, std::wstring& text) const { text = ReadTextW(col); }
			virtual std::wstring ReadTextW(int32_t col) const;
			virtual void ReadBlob(int32_t col, const byte_t** pBuf, int32_t& size) const;
			virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const;
			virtual const CommonLib::CVariant& ReadValue(int32_t col) const;
			virtual IRowPtr CreateRow() const;
			virtual void FillRow(IRowPtr ptrRow) const;

		private:
			void CheckValid() const;
			const TValue& Cell(int32_t col) const;        // may be NULL
			const TValue& Value(int32_t col) const;       // not NULL
			template <class T>
			T Number(int32_t col) const;

			std::shared_ptr<CSQLStatement> m_ptrStatement;
			SSelectPlan m_plan;
			SExecContext m_exec;
			uint64_t m_nGeneration{ 0 };
			IFieldSetPtr m_ptrFieldSet;
			bool m_bOpen{ false };
			bool m_bRow{ false };
			bool m_bEnd{ false };
		};
	}
}
