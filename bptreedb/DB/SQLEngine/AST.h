#pragma once

// Syntax tree of SQL statements, built by the grammar (Grammar/SQLParser.y) through CSQLDriver.
//
// The tree is pure syntax: names aren't resolved and types aren't checked (the binder does it against the
// schema). Every node keeps its position in the text for the error messages of the later stages.
// ToSQL prints a node back as SQL with every operator in parentheses: the tests compare it, and it shows how
// the parser grouped an expression.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <ostream>

namespace bptreedb
{
	namespace sql
	{
		struct SSourcePos
		{
			uint32_t m_nLine{ 0 };
			uint32_t m_nColumn{ 0 };
		};

		struct SNode
		{
			virtual ~SNode() {}
			virtual void ToSQL(std::ostream& stream) const = 0;
			std::string ToSQL() const;

			SSourcePos m_pos;
		};

		//------------------------------------------------------------------------------------------
		// expressions

		enum class eExprType
		{
			Literal,
			Parameter,
			Column,
			Star,           // * or t.* : the select list and COUNT(*) only
			Unary,
			Binary,
			Function,
			IsNull,
			Between,
			InList,
			InSelect,
			Like,
			Case,
			Cast,
			Exists,
			Subquery        // a scalar subquery
		};

		enum class eUnaryOp { Minus, Plus, Not };

		enum class eBinaryOp
		{
			Or, And,
			Equal, NotEqual, Less, LessOrEqual, Greater, GreaterOrEqual,
			Add, Subtract, Multiply, Divide, Modulo, Concat
		};

		const char* ToString(eUnaryOp op);
		const char* ToString(eBinaryOp op);

		struct SExpr : SNode
		{
			explicit SExpr(eExprType type) : m_type(type) {}
			const eExprType m_type;
		};
		typedef std::shared_ptr<SExpr> SExprPtr;
		typedef std::vector<SExprPtr> TExprList;

		typedef std::shared_ptr<struct SSelectStmt> SSelectStmtPtr;

		struct SLiteral : SExpr
		{
			enum eKind { Null, Integer, Double, String, Bool };

			SLiteral() : SExpr(eExprType::Literal) {}
			static std::shared_ptr<SLiteral> MakeNull();
			static std::shared_ptr<SLiteral> MakeInteger(int64_t nValue);
			static std::shared_ptr<SLiteral> MakeDouble(double dValue);
			static std::shared_ptr<SLiteral> MakeString(const std::string& sValue);
			static std::shared_ptr<SLiteral> MakeBool(bool bValue);
			void ToSQL(std::ostream& stream) const override;

			eKind m_kind{ Null };
			int64_t m_nValue{ 0 };      // Integer, Bool (0 / 1)
			double m_dValue{ 0 };       // Double
			std::string m_sValue;       // String
		};

		// "?": the parameters are numbered from 0 in the order of the text (of the whole script)
		struct SParameter : SExpr
		{
			explicit SParameter(uint32_t nIndex) : SExpr(eExprType::Parameter), m_nIndex(nIndex) {}
			void ToSQL(std::ostream& stream) const override;
			uint32_t m_nIndex;
		};

		struct SColumnRef : SExpr
		{
			SColumnRef(const std::string& sTable, const std::string& sColumn) : SExpr(eExprType::Column), m_sTable(sTable), m_sColumn(sColumn) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;       // a table name or an alias, empty if not qualified
			std::string m_sColumn;
		};

		struct SStar : SExpr
		{
			explicit SStar(const std::string& sTable = std::string()) : SExpr(eExprType::Star), m_sTable(sTable) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;       // t.* , empty for *
		};

		struct SUnaryExpr : SExpr
		{
			SUnaryExpr(eUnaryOp op, SExprPtr ptrOperand) : SExpr(eExprType::Unary), m_op(op), m_ptrOperand(ptrOperand) {}
			void ToSQL(std::ostream& stream) const override;
			eUnaryOp m_op;
			SExprPtr m_ptrOperand;
		};

		struct SBinaryExpr : SExpr
		{
			SBinaryExpr(eBinaryOp op, SExprPtr ptrLeft, SExprPtr ptrRight) : SExpr(eExprType::Binary), m_op(op), m_ptrLeft(ptrLeft), m_ptrRight(ptrRight) {}
			void ToSQL(std::ostream& stream) const override;
			eBinaryOp m_op;
			SExprPtr m_ptrLeft;
			SExprPtr m_ptrRight;
		};

		// name(args), name(DISTINCT args), name(*): the aggregates are functions too, the binder knows them
		struct SFunctionCall : SExpr
		{
			SFunctionCall() : SExpr(eExprType::Function) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sName;        // upper case
			TExprList m_args;
			bool m_bDistinct{ false };
			bool m_bStar{ false };      // COUNT(*)
		};

		struct SIsNullExpr : SExpr
		{
			SIsNullExpr(SExprPtr ptrOperand, bool bNot) : SExpr(eExprType::IsNull), m_ptrOperand(ptrOperand), m_bNot(bNot) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;
			bool m_bNot;
		};

		struct SBetweenExpr : SExpr
		{
			SBetweenExpr(SExprPtr ptrOperand, SExprPtr ptrLow, SExprPtr ptrHigh, bool bNot) : SExpr(eExprType::Between),
				m_ptrOperand(ptrOperand), m_ptrLow(ptrLow), m_ptrHigh(ptrHigh), m_bNot(bNot) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;
			SExprPtr m_ptrLow;
			SExprPtr m_ptrHigh;
			bool m_bNot;
		};

		struct SInListExpr : SExpr
		{
			SInListExpr(SExprPtr ptrOperand, const TExprList& list, bool bNot) : SExpr(eExprType::InList), m_ptrOperand(ptrOperand), m_list(list), m_bNot(bNot) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;
			TExprList m_list;
			bool m_bNot;
		};

		struct SInSelectExpr : SExpr
		{
			SInSelectExpr(SExprPtr ptrOperand, SSelectStmtPtr ptrSelect, bool bNot) : SExpr(eExprType::InSelect), m_ptrOperand(ptrOperand), m_ptrSelect(ptrSelect), m_bNot(bNot) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;
			SSelectStmtPtr m_ptrSelect;
			bool m_bNot;
		};

		struct SLikeExpr : SExpr
		{
			SLikeExpr(SExprPtr ptrOperand, SExprPtr ptrPattern, SExprPtr ptrEscape, bool bNot) : SExpr(eExprType::Like),
				m_ptrOperand(ptrOperand), m_ptrPattern(ptrPattern), m_ptrEscape(ptrEscape), m_bNot(bNot) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;
			SExprPtr m_ptrPattern;
			SExprPtr m_ptrEscape;       // null if no ESCAPE
			bool m_bNot;
		};

		struct SWhenClause
		{
			SExprPtr m_ptrWhen;
			SExprPtr m_ptrThen;
		};

		// CASE [operand] WHEN .. THEN .. [ELSE ..] END
		struct SCaseExpr : SExpr
		{
			SCaseExpr() : SExpr(eExprType::Case) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;      // null: the WHENs are conditions
			std::vector<SWhenClause> m_whens;
			SExprPtr m_ptrElse;         // null: ELSE NULL
		};

		// a type in CREATE TABLE / CAST: the name as written and the numbers in parentheses (VARCHAR(64));
		// the binder maps it to eDataTypes
		struct STypeName
		{
			std::string m_sName;        // upper case
			std::vector<int64_t> m_params;
			void ToSQL(std::ostream& stream) const;
		};

		struct SCastExpr : SExpr
		{
			SCastExpr(SExprPtr ptrOperand, const STypeName& type) : SExpr(eExprType::Cast), m_ptrOperand(ptrOperand), m_type(type) {}
			void ToSQL(std::ostream& stream) const override;
			SExprPtr m_ptrOperand;
			STypeName m_type;
		};

		struct SExistsExpr : SExpr
		{
			explicit SExistsExpr(SSelectStmtPtr ptrSelect) : SExpr(eExprType::Exists), m_ptrSelect(ptrSelect) {}
			void ToSQL(std::ostream& stream) const override;
			SSelectStmtPtr m_ptrSelect;
		};

		struct SSubqueryExpr : SExpr
		{
			explicit SSubqueryExpr(SSelectStmtPtr ptrSelect) : SExpr(eExprType::Subquery), m_ptrSelect(ptrSelect) {}
			void ToSQL(std::ostream& stream) const override;
			SSelectStmtPtr m_ptrSelect;
		};

		//------------------------------------------------------------------------------------------
		// FROM

		enum class eTableRefType { Table, Subquery, Join };
		enum class eJoinType { Inner, Left, Right, Full, Cross };
		const char* ToString(eJoinType type);

		struct STableRef : SNode
		{
			explicit STableRef(eTableRefType type) : m_type(type) {}
			const eTableRefType m_type;
		};
		typedef std::shared_ptr<STableRef> STableRefPtr;

		struct STableName : STableRef
		{
			STableName(const std::string& sName, const std::string& sAlias) : STableRef(eTableRefType::Table), m_sName(sName), m_sAlias(sAlias) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sName;
			std::string m_sAlias;       // empty if none
		};

		struct SSubqueryRef : STableRef
		{
			SSubqueryRef(SSelectStmtPtr ptrSelect, const std::string& sAlias) : STableRef(eTableRefType::Subquery), m_ptrSelect(ptrSelect), m_sAlias(sAlias) {}
			void ToSQL(std::ostream& stream) const override;
			SSelectStmtPtr m_ptrSelect;
			std::string m_sAlias;
		};

		// left JOIN right ON cond / USING (columns); a join of no condition (CROSS JOIN, or JOIN without ON):
		// m_ptrCondition null and m_using empty
		struct SJoinRef : STableRef
		{
			SJoinRef() : STableRef(eTableRefType::Join) {}
			void ToSQL(std::ostream& stream) const override;
			eJoinType m_joinType{ eJoinType::Inner };
			STableRefPtr m_ptrLeft;
			STableRefPtr m_ptrRight;
			SExprPtr m_ptrCondition;
			std::vector<std::string> m_using;
		};

		//------------------------------------------------------------------------------------------
		// statements

		enum class eStatementType
		{
			Select,
			Insert,
			Update,
			Delete,
			CreateTable,
			DropTable,
			AlterTable,
			CreateIndex,
			DropIndex
		};

		struct SStatement : SNode
		{
			explicit SStatement(eStatementType type) : m_type(type) {}
			const eStatementType m_type;
		};
		typedef std::shared_ptr<SStatement> SStatementPtr;
		typedef std::vector<SStatementPtr> TStatementList;

		struct SSelectItem
		{
			SExprPtr m_ptrExpr;         // SStar for * / t.*
			std::string m_sAlias;
		};

		struct SOrderItem
		{
			SExprPtr m_ptrExpr;
			bool m_bDesc{ false };
		};

		// SELECT [DISTINCT] list [FROM ..] [WHERE ..] [GROUP BY .. [HAVING ..]]
		struct SSelectCore
		{
			bool m_bDistinct{ false };
			std::vector<SSelectItem> m_items;
			std::vector<STableRefPtr> m_from;   // "a, b": the cross join of the items
			SExprPtr m_ptrWhere;
			TExprList m_groupBy;
			SExprPtr m_ptrHaving;
			void ToSQL(std::ostream& stream) const;
		};

		enum class eCompoundOp { None, Union, UnionAll, Intersect, Except };
		const char* ToString(eCompoundOp op);

		// core [op core ...] [ORDER BY ..] [LIMIT n [OFFSET m]]: ORDER BY and LIMIT of the whole compound
		struct SSelectStmt : SStatement
		{
			struct SPart
			{
				eCompoundOp m_op{ eCompoundOp::None };      // with the previous part (None for the first)
				SSelectCore m_core;
			};

			SSelectStmt() : SStatement(eStatementType::Select) {}
			void ToSQL(std::ostream& stream) const override;
			std::vector<SPart> m_parts;
			std::vector<SOrderItem> m_orderBy;
			SExprPtr m_ptrLimit;
			SExprPtr m_ptrOffset;
		};

		// INSERT INTO t [(columns)] VALUES (..), (..) | INSERT INTO t [(columns)] SELECT ..
		struct SInsertStmt : SStatement
		{
			SInsertStmt() : SStatement(eStatementType::Insert) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;
			std::vector<std::string> m_columns;         // empty: all the columns in the order of the table
			std::vector<TExprList> m_rows;
			SSelectStmtPtr m_ptrSelect;                 // instead of m_rows
		};

		struct SSetClause
		{
			std::string m_sColumn;
			SExprPtr m_ptrValue;
		};

		struct SUpdateStmt : SStatement
		{
			SUpdateStmt() : SStatement(eStatementType::Update) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;
			std::vector<SSetClause> m_set;
			SExprPtr m_ptrWhere;
		};

		struct SDeleteStmt : SStatement
		{
			SDeleteStmt() : SStatement(eStatementType::Delete) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;
			SExprPtr m_ptrWhere;
		};

		// name type [NOT NULL | NULL] [UNIQUE] [DEFAULT expr]
		struct SColumnDef
		{
			std::string m_sName;
			STypeName m_type;
			bool m_bNotNull{ false };
			bool m_bUnique{ false };
			SExprPtr m_ptrDefault;
			SSourcePos m_pos;
			void ToSQL(std::ostream& stream) const;
		};

		struct SCreateTableStmt : SStatement
		{
			SCreateTableStmt() : SStatement(eStatementType::CreateTable) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;
			bool m_bIfNotExists{ false };
			std::vector<SColumnDef> m_columns;
		};

		struct SDropTableStmt : SStatement
		{
			SDropTableStmt() : SStatement(eStatementType::DropTable) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;
			bool m_bIfExists{ false };
		};

		// ALTER TABLE t ADD [COLUMN] def | ALTER TABLE t DROP [COLUMN] name
		struct SAlterTableStmt : SStatement
		{
			enum eAction { AddColumn, DropColumn };

			SAlterTableStmt() : SStatement(eStatementType::AlterTable) {}
			void ToSQL(std::ostream& stream) const override;
			std::string m_sTable;
			eAction m_action{ AddColumn };
			SColumnDef m_column;                        // AddColumn
			std::string m_sColumn;                      // DropColumn
		};

		// CREATE [UNIQUE | SPATIAL] INDEX [IF NOT EXISTS] [name] ON t (columns) [USING method [(numbers)]]
		// An index is of one field: the name is optional (the field names it), the binder checks the columns.
		// USING: BTREE, RTREE, ZORDER (ZORDER(xmin, ymin, xmax, ymax [, cell size]) - the grid).
		struct SCreateIndexStmt : SStatement
		{
			enum eKind { Regular, Unique, Spatial };

			SCreateIndexStmt() : SStatement(eStatementType::CreateIndex) {}
			void ToSQL(std::ostream& stream) const override;
			eKind m_kind{ Regular };
			bool m_bIfNotExists{ false };
			std::string m_sName;
			std::string m_sTable;
			std::vector<std::string> m_columns;
			std::string m_sMethod;                      // upper case, empty if no USING
			std::vector<double> m_methodParams;
		};

		// DROP INDEX [IF EXISTS] [name] ON t (column)
		struct SDropIndexStmt : SStatement
		{
			SDropIndexStmt() : SStatement(eStatementType::DropIndex) {}
			void ToSQL(std::ostream& stream) const override;
			bool m_bIfExists{ false };
			std::string m_sName;
			std::string m_sTable;
			std::vector<std::string> m_columns;
		};

		// an identifier as SQL: quoted if it isn't a plain name or is a keyword
		void WriteIdentifier(std::ostream& stream, const std::string& sName);
		void WriteString(std::ostream& stream, const std::string& sValue);
	}
}
