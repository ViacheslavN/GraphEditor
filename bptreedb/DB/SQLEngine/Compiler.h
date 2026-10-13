#pragma once

// The compiler of statements: resolves the names of the syntax tree against the schema (tables, columns,
// aliases, outer columns of correlated subqueries), checks the aggregation, and builds the plan:
//
//   FROM:     the sources (tables, subqueries) and the joins as written (left deep for a chain);
//             the conjuncts of WHERE / ON are pushed down to the lowest join or table they need (not into the
//             NULL side of an outer join); a table is read by the best index its conditions allow (row id,
//             unique / regular index =, IN, spatial, range) or scanned.
//   joins:    INNER / LEFT with an equality on an indexed column of a right table - index join (the index is
//             read for every left row; a spatial index for ST_Intersects of two geometries), other equalities -
//             hash join, otherwise nested loop. RIGHT / FULL by the hash / nested loop joins.
//   then:     GROUP BY / aggregates, HAVING, the projection, DISTINCT, ORDER BY, LIMIT; UNION / INTERSECT /
//             EXCEPT of the parts.
//
// The plan is built once (a prepared statement) and opened by every execution.

#include "Operators.h"
#include "AST.h"
#include <set>

namespace bptreedb
{
	namespace sql
	{
		struct SSelectPlan
		{
			COperatorPtr m_ptrRoot;
			uint32_t m_nColumns{ 0 };               // the visible columns (the row may have hidden ones after them)
			std::vector<std::string> m_names;
			std::vector<eDataTypes> m_types;
			int32_t m_nRowIdColumn{ -1 };           // a hidden column with the row id (a query of one table)
		};

		// UPDATE / DELETE: the rows to change (a table read with the conditions), their row ids and new values
		struct SModifyPlan
		{
			std::string m_sTable;
			COperatorPtr m_ptrRoot;
			int32_t m_nRowIdIndex{ -1 };            // in the row of m_ptrRoot
			std::vector<std::string> m_columns;     // UPDATE: the fields
			std::vector<CExprPtr> m_values;         //         and the values (on the row of m_ptrRoot)
		};

		class CScope;

		class CQueryCompiler
		{
		public:
			explicit CQueryCompiler(ISchemaPtr ptrSchema) : m_ptrSchema(ptrSchema) {}

			SSelectPlan CompileSelect(const SSelectStmt& stmt);
			SModifyPlan CompileUpdate(const SUpdateStmt& stmt);
			SModifyPlan CompileDelete(const SDeleteStmt& stmt);
			// an expression without columns (VALUES, DEFAULT): parameters and subqueries are allowed
			CExprPtr CompileValue(const SExprPtr& ptrExpr);

			// the tables the compiled statements read
			const std::set<std::string>& TablesRead() const { return m_tablesRead; }

			ITablePtr FindTable(const std::string& sName) const;      // exact, then case-insensitive; null if none
			ITablePtr GetTable(const std::string& sName, const SSourcePos& pos) const;

		private:
			struct SCollector
			{
				SSubqueryPlan* m_pPlan;
				uint32_t m_nLevel;           // the level of the scope that contains the subquery
			};

			SSelectPlan CompileSelect(const SSelectStmt& stmt, CScope* pParent);
			SSelectPlan CompileCore(const SSelectCore& core, const std::vector<SOrderItem>* pOrder, const SExprPtr& ptrLimit,
				const SExprPtr& ptrOffset, CScope* pParent);
			SSubqueryPlanPtr CompileSubquery(const SSelectStmt& stmt, CScope& scope, bool bOneColumn);

			struct SFromNode;
			std::unique_ptr<SFromNode> CompileFrom(const STableRefPtr& ptrRef, CScope& scope);
			void PlaceOn(const CExprPtr& ptrCond, SFromNode& node, CScope& scope);
			void Place(const CExprPtr& ptrCond, SFromNode& node, CScope& scope);
			COperatorPtr Build(SFromNode& node, CScope& scope);
			SAccessPath ChooseAccessPath(CScope& scope, int32_t nSource);

			CExprPtr Expr(const SExprPtr& ptrExpr, CScope& scope, bool bAggregates);
			CExprPtr Column(const std::string& sTable, const std::string& sColumn, CScope& scope, const SSourcePos& pos);
			CExprPtr Function(const SFunctionCall& call, CScope& scope, bool bAggregates);
			CExprPtr Spatial(const SFunctionCall& call, CScope& scope, bool bAggregates);

			uint64_t MaskOf(const CExprPtr& ptrExpr, const CScope& scope) const;
			static void SplitAnd(const CExprPtr& ptrExpr, std::vector<CExprPtr>& conjuncts);

			ISchemaPtr m_ptrSchema;
			std::vector<SCollector> m_collectors;
			std::set<std::string> m_tablesRead;
		};

		// an error of a statement at a position of the text
		[[noreturn]] void ThrowAt(const SSourcePos& pos, const std::string& sMessage);
	}
}
