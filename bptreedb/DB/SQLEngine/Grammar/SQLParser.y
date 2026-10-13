/*
 * The SQL grammar: bison (3.2+) generates the C++ parser SQLParser.cpp / SQLParser.hpp (class
 * bptreedb::sql::CSQLParser). Regenerate with generate.bat / generate.sh after a change; the generated files
 * are kept in the repository, so the build doesn't need bison.
 *
 * The tokens come from CSQLScanner (SQLLexer.l), the statements go to CSQLDriver, the nodes are AST.h.
 *
 * Expressions are in two levels (as in PostgreSQL): b_expr - the arithmetic (+ - * / % ||, unary - +) over the
 * primaries; expr - the conditions over b_expr (comparisons, IS NULL, LIKE, IN, BETWEEN, NOT, AND, OR).
 * BETWEEN takes b_expr bounds: "a BETWEEN 1 AND 5 AND b" is "(a BETWEEN 1 AND 5) AND b" without a conflict.
 * The grammar has no conflicts (%expect 0).
 */

%skeleton "lalr1.cc"
%require "3.2"
%language "c++"

%define api.namespace {bptreedb::sql}
%define api.parser.class {CSQLParser}
%define api.token.constructor
%define api.value.type variant
/* the enumerators are TOK_KW_DELETE...: no clash with the macros of windows.h (DELETE, IN, ...) */
%define api.token.prefix {TOK_}
%define api.location.file none
%define parse.assert
%define parse.error detailed
%locations
%expect 0

%parse-param {CSQLScanner& scanner} {CSQLDriver& driver}
%lex-param {CSQLDriver& driver}

%code requires
{
#include "../AST.h"

namespace bptreedb
{
	namespace sql
	{
		class CSQLScanner;
		class CSQLDriver;

		// the parts of the grammar that aren't nodes
		struct SJoinSpec
		{
			SExprPtr m_ptrCondition;
			std::vector<std::string> m_using;
		};

		struct SLimit
		{
			SExprPtr m_ptrLimit;
			SExprPtr m_ptrOffset;
		};

		struct SIndexMethod
		{
			std::string m_sName;
			std::vector<double> m_params;
		};

		typedef std::vector<std::string> TNameList;
		typedef std::vector<TExprList> TRowList;
	}
}
}

%code
{
#include "../SQLScanner.h"
#include "../SQLDriver.h"
#include <algorithm>
#include <cctype>

#undef yylex
#define yylex scanner.ScanToken

namespace
{
	using namespace bptreedb::sql;

	SSourcePos Pos(const CSQLParser::location_type& loc)
	{
		SSourcePos pos;
		pos.m_nLine = (uint32_t)loc.begin.line;
		pos.m_nColumn = (uint32_t)loc.begin.column;
		return pos;
	}

	template<class TNode>
	std::shared_ptr<TNode> At(std::shared_ptr<TNode> ptrNode, const CSQLParser::location_type& loc)
	{
		ptrNode->m_pos = Pos(loc);
		return ptrNode;
	}

	std::string Upper(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
		return s;
	}

	// -literal is a negative literal
	SExprPtr Negate(SExprPtr ptrOperand, const CSQLParser::location_type& loc)
	{
		if (ptrOperand->m_type == eExprType::Literal)
		{
			SLiteral* pLiteral = static_cast<SLiteral*>(ptrOperand.get());
			// 9223372036854775808 is a double for the scanner, -9223372036854775808 the minimum integer
			if (pLiteral->m_kind == SLiteral::Double && pLiteral->m_dValue == 9223372036854775808.0)
			{
				pLiteral->m_kind = SLiteral::Integer;
				pLiteral->m_nValue = INT64_MIN;
				pLiteral->m_dValue = 0;
				pLiteral->m_pos = Pos(loc);
				return ptrOperand;
			}
			if (pLiteral->m_kind == SLiteral::Integer && pLiteral->m_nValue != INT64_MIN)
			{
				pLiteral->m_nValue = -pLiteral->m_nValue;
				pLiteral->m_pos = Pos(loc);
				return ptrOperand;
			}
			if (pLiteral->m_kind == SLiteral::Double)
			{
				pLiteral->m_dValue = -pLiteral->m_dValue;
				pLiteral->m_pos = Pos(loc);
				return ptrOperand;
			}
		}
		return At(std::make_shared<SUnaryExpr>(eUnaryOp::Minus, ptrOperand), loc);
	}

	SExprPtr Binary(eBinaryOp op, SExprPtr ptrLeft, SExprPtr ptrRight, const CSQLParser::location_type& loc)
	{
		return At(std::make_shared<SBinaryExpr>(op, ptrLeft, ptrRight), loc);
	}
}
}

%token END_OF_INPUT 0 "end of input"
%token <std::string> IDENT "identifier"
%token <std::string> STRING "string"
%token <int64_t> INTEGER "integer"
%token <double> FLOAT "number"
%token <uint32_t> PARAM "?"

%token
	LPAREN "("
	RPAREN ")"
	COMMA ","
	DOT "."
	SEMICOLON ";"
	STAR "*"
	PLUS "+"
	MINUS "-"
	SLASH "/"
	PERCENT "%"
	CONCAT "||"
	EQ "="
	NE "<>"
	LT "<"
	LE "<="
	GT ">"
	GE ">="

%token
	KW_ADD "ADD"
	KW_ALL "ALL"
	KW_ALTER "ALTER"
	KW_AND "AND"
	KW_AS "AS"
	KW_ASC "ASC"
	KW_BETWEEN "BETWEEN"
	KW_BY "BY"
	KW_CASE "CASE"
	KW_CAST "CAST"
	KW_COLUMN "COLUMN"
	KW_CREATE "CREATE"
	KW_CROSS "CROSS"
	KW_DEFAULT "DEFAULT"
	KW_DELETE "DELETE"
	KW_DESC "DESC"
	KW_DISTINCT "DISTINCT"
	KW_DROP "DROP"
	KW_ELSE "ELSE"
	KW_END "END"
	KW_ESCAPE "ESCAPE"
	KW_EXCEPT "EXCEPT"
	KW_EXISTS "EXISTS"
	KW_FALSE "FALSE"
	KW_FROM "FROM"
	KW_FULL "FULL"
	KW_GROUP "GROUP"
	KW_HAVING "HAVING"
	KW_IF "IF"
	KW_IN "IN"
	KW_INDEX "INDEX"
	KW_INNER "INNER"
	KW_INSERT "INSERT"
	KW_INTERSECT "INTERSECT"
	KW_INTO "INTO"
	KW_IS "IS"
	KW_JOIN "JOIN"
	KW_LEFT "LEFT"
	KW_LIKE "LIKE"
	KW_LIMIT "LIMIT"
	KW_NOT "NOT"
	KW_NULL "NULL"
	KW_OFFSET "OFFSET"
	KW_ON "ON"
	KW_OR "OR"
	KW_ORDER "ORDER"
	KW_OUTER "OUTER"
	KW_RIGHT "RIGHT"
	KW_SELECT "SELECT"
	KW_SET "SET"
	KW_SPATIAL "SPATIAL"
	KW_TABLE "TABLE"
	KW_THEN "THEN"
	KW_TRUE "TRUE"
	KW_UNION "UNION"
	KW_UNIQUE "UNIQUE"
	KW_UPDATE "UPDATE"
	KW_USING "USING"
	KW_VALUES "VALUES"
	KW_WHEN "WHEN"
	KW_WHERE "WHERE"

/* conditions (expr) */
%left KW_OR
%left KW_AND
%right KW_NOT
%nonassoc KW_IS KW_LIKE KW_BETWEEN KW_IN EQ NE
%precedence KW_ESCAPE
%nonassoc LT LE GT GE
/* arithmetic (b_expr) */
%left PLUS MINUS
%left STAR SLASH PERCENT
%left CONCAT
%precedence UMINUS

%type <SStatementPtr> stmt insert_stmt update_stmt delete_stmt create_table_stmt drop_table_stmt alter_table_stmt
%type <SStatementPtr> create_index_stmt drop_index_stmt
%type <SSelectStmtPtr> select_stmt select_compound
%type <SSelectCore> select_core
%type <eCompoundOp> compound_op
%type <bool> opt_distinct opt_if_exists opt_if_not_exists opt_desc
%type <std::vector<SSelectItem>> select_list
%type <SSelectItem> select_item
%type <std::vector<STableRefPtr>> opt_from from_list
%type <STableRefPtr> table_ref table_primary
%type <eJoinType> join_op
%type <SJoinSpec> join_spec
%type <std::string> opt_alias opt_name
%type <SExprPtr> expr b_expr primary literal opt_where opt_having opt_case_operand opt_else
%type <TExprList> expr_list opt_group_by
%type <std::vector<SOrderItem>> opt_order_by order_list
%type <SOrderItem> order_item
%type <SLimit> opt_limit
%type <std::vector<SWhenClause>> when_list
%type <STypeName> type_name
%type <std::vector<int64_t>> integer_list
%type <TNameList> name_list opt_column_list
%type <TRowList> row_list
%type <TExprList> row
%type <std::vector<SSetClause>> set_list
%type <SSetClause> set_item
%type <SColumnDef> column_def column_constraints
%type <std::vector<SColumnDef>> column_defs
%type <SCreateIndexStmt::eKind> index_kind
%type <SIndexMethod> opt_index_method
%type <std::vector<double>> number_list
%type <double> number

%start script

%%

/*------------------------------------------------------------------------------------------------
 * script: statements separated by ";", empty statements are skipped */

script
	: stmt_seq
	;

stmt_seq
	: opt_stmt
	| stmt_seq ";" opt_stmt
	;

opt_stmt
	: %empty
	| stmt  { driver.AddStatement($1); }
	;

stmt
	: select_stmt  { $$ = $1; }
	| insert_stmt
	| update_stmt
	| delete_stmt
	| create_table_stmt
	| drop_table_stmt
	| alter_table_stmt
	| create_index_stmt
	| drop_index_stmt
	;

/*------------------------------------------------------------------------------------------------
 * SELECT */

select_stmt
	: select_compound opt_order_by opt_limit
		{
			$$ = $1;
			$$->m_orderBy = std::move($2);
			$$->m_ptrLimit = $3.m_ptrLimit;
			$$->m_ptrOffset = $3.m_ptrOffset;
		}
	;

select_compound
	: select_core
		{
			$$ = At(std::make_shared<SSelectStmt>(), @$);
			$$->m_parts.emplace_back();
			$$->m_parts.back().m_core = std::move($1);
		}
	| select_compound compound_op select_core
		{
			$$ = $1;
			$$->m_parts.emplace_back();
			$$->m_parts.back().m_op = $2;
			$$->m_parts.back().m_core = std::move($3);
		}
	;

compound_op
	: "UNION"            { $$ = eCompoundOp::Union; }
	| "UNION" "ALL"      { $$ = eCompoundOp::UnionAll; }
	| "INTERSECT"        { $$ = eCompoundOp::Intersect; }
	| "EXCEPT"           { $$ = eCompoundOp::Except; }
	;

select_core
	: "SELECT" opt_distinct select_list opt_from opt_where opt_group_by opt_having
		{
			$$.m_bDistinct = $2;
			$$.m_items = std::move($3);
			$$.m_from = std::move($4);
			$$.m_ptrWhere = $5;
			$$.m_groupBy = std::move($6);
			$$.m_ptrHaving = $7;
		}
	;

opt_distinct
	: %empty         { $$ = false; }
	| "DISTINCT"     { $$ = true; }
	| "ALL"          { $$ = false; }
	;

select_list
	: select_item                    { $$.push_back(std::move($1)); }
	| select_list "," select_item    { $$ = std::move($1); $$.push_back(std::move($3)); }
	;

select_item
	: expr opt_alias                 { $$.m_ptrExpr = $1; $$.m_sAlias = $2; }
	| "*"                            { $$.m_ptrExpr = At(std::make_shared<SStar>(), @$); }
	| IDENT "." "*"                  { $$.m_ptrExpr = At(std::make_shared<SStar>($1), @$); }
	;

opt_alias
	: %empty         { }
	| "AS" IDENT     { $$ = $2; }
	| IDENT          { $$ = $1; }
	;

opt_from
	: %empty                 { }
	| "FROM" from_list       { $$ = std::move($2); }
	;

from_list
	: table_ref                  { $$.push_back($1); }
	| from_list "," table_ref    { $$ = std::move($1); $$.push_back($3); }
	;

/* the joins are left deep: a JOIN b ON .. JOIN c ON .. is (a JOIN b) JOIN c */
table_ref
	: table_primary  { $$ = $1; }
	| table_ref join_op table_primary join_spec
		{
			auto ptrJoin = At(std::make_shared<SJoinRef>(), @$);
			ptrJoin->m_joinType = $2;
			ptrJoin->m_ptrLeft = $1;
			ptrJoin->m_ptrRight = $3;
			ptrJoin->m_ptrCondition = $4.m_ptrCondition;
			ptrJoin->m_using = std::move($4.m_using);
			if ($2 == eJoinType::Cross && (ptrJoin->m_ptrCondition.get() || !ptrJoin->m_using.empty()))
				throw syntax_error(@4, "CROSS JOIN has no ON / USING");
			if (($2 == eJoinType::Left || $2 == eJoinType::Right || $2 == eJoinType::Full) &&
				ptrJoin->m_ptrCondition.get() == nullptr && ptrJoin->m_using.empty())
				throw syntax_error(@3, "an outer join needs ON or USING");
			$$ = ptrJoin;
		}
	;

join_op
	: "JOIN"                         { $$ = eJoinType::Inner; }
	| "INNER" "JOIN"                 { $$ = eJoinType::Inner; }
	| "LEFT" opt_outer "JOIN"        { $$ = eJoinType::Left; }
	| "RIGHT" opt_outer "JOIN"       { $$ = eJoinType::Right; }
	| "FULL" opt_outer "JOIN"        { $$ = eJoinType::Full; }
	| "CROSS" "JOIN"                 { $$ = eJoinType::Cross; }
	;

opt_outer
	: %empty
	| "OUTER"
	;

join_spec
	: %empty                         { }
	| "ON" expr                      { $$.m_ptrCondition = $2; }
	| "USING" "(" name_list ")"      { $$.m_using = std::move($3); }
	;

table_primary
	: IDENT opt_alias                        { $$ = At(std::make_shared<STableName>($1, $2), @$); }
	| "(" select_stmt ")" opt_alias          { $$ = At(std::make_shared<SSubqueryRef>($2, $4), @$); }
	;

opt_where
	: %empty             { }
	| "WHERE" expr       { $$ = $2; }
	;

opt_group_by
	: %empty                 { }
	| "GROUP" "BY" expr_list { $$ = std::move($3); }
	;

opt_having
	: %empty             { }
	| "HAVING" expr      { $$ = $2; }
	;

opt_order_by
	: %empty                     { }
	| "ORDER" "BY" order_list    { $$ = std::move($3); }
	;

order_list
	: order_item                 { $$.push_back(std::move($1)); }
	| order_list "," order_item  { $$ = std::move($1); $$.push_back(std::move($3)); }
	;

order_item
	: expr opt_desc  { $$.m_ptrExpr = $1; $$.m_bDesc = $2; }
	;

opt_desc
	: %empty     { $$ = false; }
	| "ASC"      { $$ = false; }
	| "DESC"     { $$ = true; }
	;

/* LIMIT n, LIMIT n OFFSET m, LIMIT m, n (the MySQL order: offset, count) */
opt_limit
	: %empty                         { }
	| "LIMIT" expr                   { $$.m_ptrLimit = $2; }
	| "LIMIT" expr "OFFSET" expr     { $$.m_ptrLimit = $2; $$.m_ptrOffset = $4; }
	| "LIMIT" expr "," expr          { $$.m_ptrLimit = $4; $$.m_ptrOffset = $2; }
	;

/*------------------------------------------------------------------------------------------------
 * INSERT, UPDATE, DELETE */

insert_stmt
	: "INSERT" "INTO" IDENT opt_column_list "VALUES" row_list
		{
			auto ptrStmt = At(std::make_shared<SInsertStmt>(), @$);
			ptrStmt->m_sTable = $3;
			ptrStmt->m_columns = std::move($4);
			ptrStmt->m_rows = std::move($6);
			$$ = ptrStmt;
		}
	| "INSERT" "INTO" IDENT opt_column_list select_stmt
		{
			auto ptrStmt = At(std::make_shared<SInsertStmt>(), @$);
			ptrStmt->m_sTable = $3;
			ptrStmt->m_columns = std::move($4);
			ptrStmt->m_ptrSelect = $5;
			$$ = ptrStmt;
		}
	;

opt_column_list
	: %empty             { }
	| "(" name_list ")"  { $$ = std::move($2); }
	;

row_list
	: row                { $$.push_back(std::move($1)); }
	| row_list "," row   { $$ = std::move($1); $$.push_back(std::move($3)); }
	;

row
	: "(" expr_list ")"  { $$ = std::move($2); }
	;

update_stmt
	: "UPDATE" IDENT "SET" set_list opt_where
		{
			auto ptrStmt = At(std::make_shared<SUpdateStmt>(), @$);
			ptrStmt->m_sTable = $2;
			ptrStmt->m_set = std::move($4);
			ptrStmt->m_ptrWhere = $5;
			$$ = ptrStmt;
		}
	;

set_list
	: set_item               { $$.push_back(std::move($1)); }
	| set_list "," set_item  { $$ = std::move($1); $$.push_back(std::move($3)); }
	;

set_item
	: IDENT "=" expr     { $$.m_sColumn = $1; $$.m_ptrValue = $3; }
	;

delete_stmt
	: "DELETE" "FROM" IDENT opt_where
		{
			auto ptrStmt = At(std::make_shared<SDeleteStmt>(), @$);
			ptrStmt->m_sTable = $3;
			ptrStmt->m_ptrWhere = $4;
			$$ = ptrStmt;
		}
	;

/*------------------------------------------------------------------------------------------------
 * DDL */

create_table_stmt
	: "CREATE" "TABLE" opt_if_not_exists IDENT "(" column_defs ")"
		{
			auto ptrStmt = At(std::make_shared<SCreateTableStmt>(), @$);
			ptrStmt->m_bIfNotExists = $3;
			ptrStmt->m_sTable = $4;
			ptrStmt->m_columns = std::move($6);
			$$ = ptrStmt;
		}
	;

column_defs
	: column_def                     { $$.push_back(std::move($1)); }
	| column_defs "," column_def     { $$ = std::move($1); $$.push_back(std::move($3)); }
	;

column_def
	: IDENT type_name column_constraints
		{
			$$ = std::move($3);
			$$.m_sName = $1;
			$$.m_type = std::move($2);
			$$.m_pos = Pos(@$);
		}
	;

column_constraints
	: %empty                                 { }
	| column_constraints "NOT" "NULL"        { $$ = std::move($1); $$.m_bNotNull = true; }
	| column_constraints "NULL"              { $$ = std::move($1); $$.m_bNotNull = false; }
	| column_constraints "UNIQUE"            { $$ = std::move($1); $$.m_bUnique = true; }
	| column_constraints "DEFAULT" b_expr    { $$ = std::move($1); $$.m_ptrDefault = $3; }
	;

type_name
	: IDENT                          { $$.m_sName = Upper($1); }
	| IDENT "(" integer_list ")"     { $$.m_sName = Upper($1); $$.m_params = std::move($3); }
	;

integer_list
	: INTEGER                        { $$.push_back($1); }
	| integer_list "," INTEGER       { $$ = std::move($1); $$.push_back($3); }
	;

drop_table_stmt
	: "DROP" "TABLE" opt_if_exists IDENT
		{
			auto ptrStmt = At(std::make_shared<SDropTableStmt>(), @$);
			ptrStmt->m_bIfExists = $3;
			ptrStmt->m_sTable = $4;
			$$ = ptrStmt;
		}
	;

alter_table_stmt
	: "ALTER" "TABLE" IDENT "ADD" opt_column column_def
		{
			auto ptrStmt = At(std::make_shared<SAlterTableStmt>(), @$);
			ptrStmt->m_sTable = $3;
			ptrStmt->m_action = SAlterTableStmt::AddColumn;
			ptrStmt->m_column = std::move($6);
			$$ = ptrStmt;
		}
	| "ALTER" "TABLE" IDENT "DROP" opt_column IDENT
		{
			auto ptrStmt = At(std::make_shared<SAlterTableStmt>(), @$);
			ptrStmt->m_sTable = $3;
			ptrStmt->m_action = SAlterTableStmt::DropColumn;
			ptrStmt->m_sColumn = $6;
			$$ = ptrStmt;
		}
	;

opt_column
	: %empty
	| "COLUMN"
	;

create_index_stmt
	: "CREATE" index_kind "INDEX" opt_if_not_exists opt_name "ON" IDENT "(" name_list ")" opt_index_method
		{
			auto ptrStmt = At(std::make_shared<SCreateIndexStmt>(), @$);
			ptrStmt->m_kind = $2;
			ptrStmt->m_bIfNotExists = $4;
			ptrStmt->m_sName = $5;
			ptrStmt->m_sTable = $7;
			ptrStmt->m_columns = std::move($9);
			ptrStmt->m_sMethod = $11.m_sName;
			ptrStmt->m_methodParams = std::move($11.m_params);
			$$ = ptrStmt;
		}
	;

index_kind
	: %empty         { $$ = SCreateIndexStmt::Regular; }
	| "UNIQUE"       { $$ = SCreateIndexStmt::Unique; }
	| "SPATIAL"      { $$ = SCreateIndexStmt::Spatial; }
	;

opt_index_method
	: %empty                             { }
	| "USING" IDENT                      { $$.m_sName = Upper($2); }
	| "USING" IDENT "(" number_list ")"  { $$.m_sName = Upper($2); $$.m_params = std::move($4); }
	;

number_list
	: number                     { $$.push_back($1); }
	| number_list "," number     { $$ = std::move($1); $$.push_back($3); }
	;

number
	: INTEGER        { $$ = (double)$1; }
	| FLOAT          { $$ = $1; }
	| "-" INTEGER    { $$ = -(double)$2; }
	| "-" FLOAT      { $$ = -$2; }
	;

drop_index_stmt
	: "DROP" "INDEX" opt_if_exists opt_name "ON" IDENT "(" name_list ")"
		{
			auto ptrStmt = At(std::make_shared<SDropIndexStmt>(), @$);
			ptrStmt->m_bIfExists = $3;
			ptrStmt->m_sName = $4;
			ptrStmt->m_sTable = $6;
			ptrStmt->m_columns = std::move($8);
			$$ = ptrStmt;
		}
	;

opt_name
	: %empty     { }
	| IDENT      { $$ = $1; }
	;

opt_if_exists
	: %empty             { $$ = false; }
	| "IF" "EXISTS"      { $$ = true; }
	;

opt_if_not_exists
	: %empty                 { $$ = false; }
	| "IF" "NOT" "EXISTS"    { $$ = true; }
	;

name_list
	: IDENT                  { $$.push_back($1); }
	| name_list "," IDENT    { $$ = std::move($1); $$.push_back($3); }
	;

/*------------------------------------------------------------------------------------------------
 * expressions */

expr
	: b_expr                                     { $$ = $1; }
	| expr "OR" expr                             { $$ = Binary(eBinaryOp::Or, $1, $3, @$); }
	| expr "AND" expr                            { $$ = Binary(eBinaryOp::And, $1, $3, @$); }
	| "NOT" expr                                 { $$ = At(std::make_shared<SUnaryExpr>(eUnaryOp::Not, $2), @$); }
	| expr "=" expr                              { $$ = Binary(eBinaryOp::Equal, $1, $3, @$); }
	| expr "<>" expr                             { $$ = Binary(eBinaryOp::NotEqual, $1, $3, @$); }
	| expr "<" expr                              { $$ = Binary(eBinaryOp::Less, $1, $3, @$); }
	| expr "<=" expr                             { $$ = Binary(eBinaryOp::LessOrEqual, $1, $3, @$); }
	| expr ">" expr                              { $$ = Binary(eBinaryOp::Greater, $1, $3, @$); }
	| expr ">=" expr                             { $$ = Binary(eBinaryOp::GreaterOrEqual, $1, $3, @$); }
	| expr "IS" "NULL" %prec KW_IS               { $$ = At(std::make_shared<SIsNullExpr>($1, false), @$); }
	| expr "IS" "NOT" "NULL" %prec KW_IS         { $$ = At(std::make_shared<SIsNullExpr>($1, true), @$); }
	| expr "LIKE" expr                           { $$ = At(std::make_shared<SLikeExpr>($1, $3, nullptr, false), @$); }
	| expr "LIKE" expr "ESCAPE" expr             { $$ = At(std::make_shared<SLikeExpr>($1, $3, $5, false), @$); }
	| expr "NOT" "LIKE" expr %prec KW_LIKE       { $$ = At(std::make_shared<SLikeExpr>($1, $4, nullptr, true), @$); }
	| expr "NOT" "LIKE" expr "ESCAPE" expr       { $$ = At(std::make_shared<SLikeExpr>($1, $4, $6, true), @$); }
	| expr "BETWEEN" b_expr "AND" b_expr %prec KW_BETWEEN
		{ $$ = At(std::make_shared<SBetweenExpr>($1, $3, $5, false), @$); }
	| expr "NOT" "BETWEEN" b_expr "AND" b_expr %prec KW_BETWEEN
		{ $$ = At(std::make_shared<SBetweenExpr>($1, $4, $6, true), @$); }
	| expr "IN" "(" expr_list ")" %prec KW_IN                { $$ = At(std::make_shared<SInListExpr>($1, $4, false), @$); }
	| expr "NOT" "IN" "(" expr_list ")" %prec KW_IN          { $$ = At(std::make_shared<SInListExpr>($1, $5, true), @$); }
	| expr "IN" "(" select_stmt ")" %prec KW_IN              { $$ = At(std::make_shared<SInSelectExpr>($1, $4, false), @$); }
	| expr "NOT" "IN" "(" select_stmt ")" %prec KW_IN        { $$ = At(std::make_shared<SInSelectExpr>($1, $5, true), @$); }
	;

b_expr
	: primary                        { $$ = $1; }
	| b_expr "+" b_expr              { $$ = Binary(eBinaryOp::Add, $1, $3, @$); }
	| b_expr "-" b_expr              { $$ = Binary(eBinaryOp::Subtract, $1, $3, @$); }
	| b_expr "*" b_expr              { $$ = Binary(eBinaryOp::Multiply, $1, $3, @$); }
	| b_expr "/" b_expr              { $$ = Binary(eBinaryOp::Divide, $1, $3, @$); }
	| b_expr "%" b_expr              { $$ = Binary(eBinaryOp::Modulo, $1, $3, @$); }
	| b_expr "||" b_expr             { $$ = Binary(eBinaryOp::Concat, $1, $3, @$); }
	| "-" b_expr %prec UMINUS        { $$ = Negate($2, @$); }
	| "+" b_expr %prec UMINUS        { $$ = At(std::make_shared<SUnaryExpr>(eUnaryOp::Plus, $2), @$); }
	;

primary
	: literal                        { $$ = $1; }
	| PARAM                          { $$ = At(std::make_shared<SParameter>($1), @$); }
	| IDENT                          { $$ = At(std::make_shared<SColumnRef>(std::string(), $1), @$); }
	| IDENT "." IDENT                { $$ = At(std::make_shared<SColumnRef>($1, $3), @$); }
	| IDENT "(" ")"
		{
			auto ptrCall = At(std::make_shared<SFunctionCall>(), @$);
			ptrCall->m_sName = Upper($1);
			$$ = ptrCall;
		}
	| IDENT "(" "*" ")"
		{
			auto ptrCall = At(std::make_shared<SFunctionCall>(), @$);
			ptrCall->m_sName = Upper($1);
			ptrCall->m_bStar = true;
			$$ = ptrCall;
		}
	| IDENT "(" opt_distinct expr_list ")"
		{
			auto ptrCall = At(std::make_shared<SFunctionCall>(), @$);
			ptrCall->m_sName = Upper($1);
			ptrCall->m_bDistinct = $3;
			ptrCall->m_args = std::move($4);
			$$ = ptrCall;
		}
	| "(" expr ")"                   { $$ = $2; }
	| "(" select_stmt ")"            { $$ = At(std::make_shared<SSubqueryExpr>($2), @$); }
	| "EXISTS" "(" select_stmt ")"   { $$ = At(std::make_shared<SExistsExpr>($3), @$); }
	| "CASE" opt_case_operand when_list opt_else "END"
		{
			auto ptrCase = At(std::make_shared<SCaseExpr>(), @$);
			ptrCase->m_ptrOperand = $2;
			ptrCase->m_whens = std::move($3);
			ptrCase->m_ptrElse = $4;
			$$ = ptrCase;
		}
	| "CAST" "(" expr "AS" type_name ")" { $$ = At(std::make_shared<SCastExpr>($3, $5), @$); }
	;

literal
	: INTEGER        { $$ = At(SLiteral::MakeInteger($1), @$); }
	| FLOAT          { $$ = At(SLiteral::MakeDouble($1), @$); }
	| STRING         { $$ = At(SLiteral::MakeString($1), @$); }
	| "NULL"         { $$ = At(SLiteral::MakeNull(), @$); }
	| "TRUE"         { $$ = At(SLiteral::MakeBool(true), @$); }
	| "FALSE"        { $$ = At(SLiteral::MakeBool(false), @$); }
	;

opt_case_operand
	: %empty     { }
	| expr       { $$ = $1; }
	;

when_list
	: "WHEN" expr "THEN" expr                { $$.push_back(SWhenClause{ $2, $4 }); }
	| when_list "WHEN" expr "THEN" expr      { $$ = std::move($1); $$.push_back(SWhenClause{ $3, $5 }); }
	;

opt_else
	: %empty         { }
	| "ELSE" expr    { $$ = $2; }
	;

expr_list
	: expr                   { $$.push_back($1); }
	| expr_list "," expr     { $$ = std::move($1); $$.push_back($3); }
	;

%%

void bptreedb::sql::CSQLParser::error(const location_type& loc, const std::string& sMessage)
{
	driver.SetError((uint32_t)loc.begin.line, (uint32_t)loc.begin.column, sMessage);
}
