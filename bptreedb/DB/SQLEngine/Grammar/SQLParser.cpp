// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.





#include "SQLParser.hpp"


// Unqualified %code blocks.

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



#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif


// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (false)
# endif


// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << '\n';                       \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yy_stack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YY_USE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

namespace bptreedb { namespace sql {

  /// Build a parser object.
  CSQLParser::CSQLParser (CSQLScanner& scanner_yyarg, CSQLDriver& driver_yyarg)
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      scanner (scanner_yyarg),
      driver (driver_yyarg)
  {}

  CSQLParser::~CSQLParser ()
  {}

  CSQLParser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/



  // by_state.
  CSQLParser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  CSQLParser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  CSQLParser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  CSQLParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  CSQLParser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  CSQLParser::symbol_kind_type
  CSQLParser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  CSQLParser::stack_symbol_type::stack_symbol_type ()
  {}

  CSQLParser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state), YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.YY_MOVE_OR_COPY< SColumnDef > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.YY_MOVE_OR_COPY< SCreateIndexStmt::eKind > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.YY_MOVE_OR_COPY< SExprPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.YY_MOVE_OR_COPY< SIndexMethod > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.YY_MOVE_OR_COPY< SJoinSpec > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.YY_MOVE_OR_COPY< SLimit > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_order_item: // order_item
        value.YY_MOVE_OR_COPY< SOrderItem > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_core: // select_core
        value.YY_MOVE_OR_COPY< SSelectCore > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_item: // select_item
        value.YY_MOVE_OR_COPY< SSelectItem > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.YY_MOVE_OR_COPY< SSelectStmtPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_set_item: // set_item
        value.YY_MOVE_OR_COPY< SSetClause > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_insert_stmt: // insert_stmt
      case symbol_kind::S_update_stmt: // update_stmt
      case symbol_kind::S_delete_stmt: // delete_stmt
      case symbol_kind::S_create_table_stmt: // create_table_stmt
      case symbol_kind::S_drop_table_stmt: // drop_table_stmt
      case symbol_kind::S_alter_table_stmt: // alter_table_stmt
      case symbol_kind::S_create_index_stmt: // create_index_stmt
      case symbol_kind::S_drop_index_stmt: // drop_index_stmt
        value.YY_MOVE_OR_COPY< SStatementPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.YY_MOVE_OR_COPY< STableRefPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_type_name: // type_name
        value.YY_MOVE_OR_COPY< STypeName > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.YY_MOVE_OR_COPY< TExprList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.YY_MOVE_OR_COPY< TNameList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_row_list: // row_list
        value.YY_MOVE_OR_COPY< TRowList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.YY_MOVE_OR_COPY< bool > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.YY_MOVE_OR_COPY< double > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.YY_MOVE_OR_COPY< eCompoundOp > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_join_op: // join_op
        value.YY_MOVE_OR_COPY< eJoinType > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.YY_MOVE_OR_COPY< int64_t > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.YY_MOVE_OR_COPY< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.YY_MOVE_OR_COPY< std::vector<SColumnDef> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.YY_MOVE_OR_COPY< std::vector<SOrderItem> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_list: // select_list
        value.YY_MOVE_OR_COPY< std::vector<SSelectItem> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_set_list: // set_list
        value.YY_MOVE_OR_COPY< std::vector<SSetClause> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.YY_MOVE_OR_COPY< std::vector<STableRefPtr> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_when_list: // when_list
        value.YY_MOVE_OR_COPY< std::vector<SWhenClause> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_number_list: // number_list
        value.YY_MOVE_OR_COPY< std::vector<double> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.YY_MOVE_OR_COPY< std::vector<int64_t> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_PARAM: // "?"
        value.YY_MOVE_OR_COPY< uint32_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  CSQLParser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s, YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.move< SColumnDef > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.move< SCreateIndexStmt::eKind > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.move< SExprPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.move< SIndexMethod > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.move< SJoinSpec > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.move< SLimit > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_order_item: // order_item
        value.move< SOrderItem > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_core: // select_core
        value.move< SSelectCore > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_item: // select_item
        value.move< SSelectItem > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.move< SSelectStmtPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_set_item: // set_item
        value.move< SSetClause > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_insert_stmt: // insert_stmt
      case symbol_kind::S_update_stmt: // update_stmt
      case symbol_kind::S_delete_stmt: // delete_stmt
      case symbol_kind::S_create_table_stmt: // create_table_stmt
      case symbol_kind::S_drop_table_stmt: // drop_table_stmt
      case symbol_kind::S_alter_table_stmt: // alter_table_stmt
      case symbol_kind::S_create_index_stmt: // create_index_stmt
      case symbol_kind::S_drop_index_stmt: // drop_index_stmt
        value.move< SStatementPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.move< STableRefPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_type_name: // type_name
        value.move< STypeName > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.move< TExprList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.move< TNameList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_row_list: // row_list
        value.move< TRowList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.move< bool > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.move< double > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.move< eCompoundOp > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_join_op: // join_op
        value.move< eJoinType > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.move< int64_t > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.move< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.move< std::vector<SColumnDef> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.move< std::vector<SOrderItem> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_list: // select_list
        value.move< std::vector<SSelectItem> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_set_list: // set_list
        value.move< std::vector<SSetClause> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.move< std::vector<STableRefPtr> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_when_list: // when_list
        value.move< std::vector<SWhenClause> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_number_list: // number_list
        value.move< std::vector<double> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.move< std::vector<int64_t> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_PARAM: // "?"
        value.move< uint32_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  CSQLParser::stack_symbol_type&
  CSQLParser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.copy< SColumnDef > (that.value);
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.copy< SCreateIndexStmt::eKind > (that.value);
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.copy< SExprPtr > (that.value);
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.copy< SIndexMethod > (that.value);
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.copy< SJoinSpec > (that.value);
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.copy< SLimit > (that.value);
        break;

      case symbol_kind::S_order_item: // order_item
        value.copy< SOrderItem > (that.value);
        break;

      case symbol_kind::S_select_core: // select_core
        value.copy< SSelectCore > (that.value);
        break;

      case symbol_kind::S_select_item: // select_item
        value.copy< SSelectItem > (that.value);
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.copy< SSelectStmtPtr > (that.value);
        break;

      case symbol_kind::S_set_item: // set_item
        value.copy< SSetClause > (that.value);
        break;

      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_insert_stmt: // insert_stmt
      case symbol_kind::S_update_stmt: // update_stmt
      case symbol_kind::S_delete_stmt: // delete_stmt
      case symbol_kind::S_create_table_stmt: // create_table_stmt
      case symbol_kind::S_drop_table_stmt: // drop_table_stmt
      case symbol_kind::S_alter_table_stmt: // alter_table_stmt
      case symbol_kind::S_create_index_stmt: // create_index_stmt
      case symbol_kind::S_drop_index_stmt: // drop_index_stmt
        value.copy< SStatementPtr > (that.value);
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.copy< STableRefPtr > (that.value);
        break;

      case symbol_kind::S_type_name: // type_name
        value.copy< STypeName > (that.value);
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.copy< TExprList > (that.value);
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.copy< TNameList > (that.value);
        break;

      case symbol_kind::S_row_list: // row_list
        value.copy< TRowList > (that.value);
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.copy< bool > (that.value);
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.copy< double > (that.value);
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.copy< eCompoundOp > (that.value);
        break;

      case symbol_kind::S_join_op: // join_op
        value.copy< eJoinType > (that.value);
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.copy< int64_t > (that.value);
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.copy< std::string > (that.value);
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.copy< std::vector<SColumnDef> > (that.value);
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.copy< std::vector<SOrderItem> > (that.value);
        break;

      case symbol_kind::S_select_list: // select_list
        value.copy< std::vector<SSelectItem> > (that.value);
        break;

      case symbol_kind::S_set_list: // set_list
        value.copy< std::vector<SSetClause> > (that.value);
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.copy< std::vector<STableRefPtr> > (that.value);
        break;

      case symbol_kind::S_when_list: // when_list
        value.copy< std::vector<SWhenClause> > (that.value);
        break;

      case symbol_kind::S_number_list: // number_list
        value.copy< std::vector<double> > (that.value);
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.copy< std::vector<int64_t> > (that.value);
        break;

      case symbol_kind::S_PARAM: // "?"
        value.copy< uint32_t > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    return *this;
  }

  CSQLParser::stack_symbol_type&
  CSQLParser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.move< SColumnDef > (that.value);
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.move< SCreateIndexStmt::eKind > (that.value);
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.move< SExprPtr > (that.value);
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.move< SIndexMethod > (that.value);
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.move< SJoinSpec > (that.value);
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.move< SLimit > (that.value);
        break;

      case symbol_kind::S_order_item: // order_item
        value.move< SOrderItem > (that.value);
        break;

      case symbol_kind::S_select_core: // select_core
        value.move< SSelectCore > (that.value);
        break;

      case symbol_kind::S_select_item: // select_item
        value.move< SSelectItem > (that.value);
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.move< SSelectStmtPtr > (that.value);
        break;

      case symbol_kind::S_set_item: // set_item
        value.move< SSetClause > (that.value);
        break;

      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_insert_stmt: // insert_stmt
      case symbol_kind::S_update_stmt: // update_stmt
      case symbol_kind::S_delete_stmt: // delete_stmt
      case symbol_kind::S_create_table_stmt: // create_table_stmt
      case symbol_kind::S_drop_table_stmt: // drop_table_stmt
      case symbol_kind::S_alter_table_stmt: // alter_table_stmt
      case symbol_kind::S_create_index_stmt: // create_index_stmt
      case symbol_kind::S_drop_index_stmt: // drop_index_stmt
        value.move< SStatementPtr > (that.value);
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.move< STableRefPtr > (that.value);
        break;

      case symbol_kind::S_type_name: // type_name
        value.move< STypeName > (that.value);
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.move< TExprList > (that.value);
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.move< TNameList > (that.value);
        break;

      case symbol_kind::S_row_list: // row_list
        value.move< TRowList > (that.value);
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.move< bool > (that.value);
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.move< double > (that.value);
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.move< eCompoundOp > (that.value);
        break;

      case symbol_kind::S_join_op: // join_op
        value.move< eJoinType > (that.value);
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.move< int64_t > (that.value);
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.move< std::string > (that.value);
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.move< std::vector<SColumnDef> > (that.value);
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.move< std::vector<SOrderItem> > (that.value);
        break;

      case symbol_kind::S_select_list: // select_list
        value.move< std::vector<SSelectItem> > (that.value);
        break;

      case symbol_kind::S_set_list: // set_list
        value.move< std::vector<SSetClause> > (that.value);
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.move< std::vector<STableRefPtr> > (that.value);
        break;

      case symbol_kind::S_when_list: // when_list
        value.move< std::vector<SWhenClause> > (that.value);
        break;

      case symbol_kind::S_number_list: // number_list
        value.move< std::vector<double> > (that.value);
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.move< std::vector<int64_t> > (that.value);
        break;

      case symbol_kind::S_PARAM: // "?"
        value.move< uint32_t > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  CSQLParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  CSQLParser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YY_USE (yyoutput);
    if (yysym.empty ())
      yyo << "empty symbol";
    else
      {
        symbol_kind_type yykind = yysym.kind ();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm")
            << ' ' << yysym.name () << " ("
            << yysym.location << ": ";
        YY_USE (yykind);
        yyo << ')';
      }
  }
#endif

  void
  CSQLParser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  CSQLParser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  CSQLParser::yypop_ (int n) YY_NOEXCEPT
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  CSQLParser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  CSQLParser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  CSQLParser::debug_level_type
  CSQLParser::debug_level () const
  {
    return yydebug_;
  }

  void
  CSQLParser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  CSQLParser::state_type
  CSQLParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  CSQLParser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  CSQLParser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  CSQLParser::operator() ()
  {
    return parse ();
  }

  int
  CSQLParser::parse ()
  {
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int (yystack_[0].state) << '\n';
    YY_STACK_PRINT ();

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            symbol_type yylookahead (yylex (driver));
            yyla.move (yylookahead);
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    if (yyla.kind () == symbol_kind::S_YYerror)
    {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind ())
      {
        goto yydefault;
      }

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", state_type (yyn), YY_MOVE (yyla));
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        yylhs.value.emplace< SColumnDef > ();
        break;

      case symbol_kind::S_index_kind: // index_kind
        yylhs.value.emplace< SCreateIndexStmt::eKind > ();
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        yylhs.value.emplace< SExprPtr > ();
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        yylhs.value.emplace< SIndexMethod > ();
        break;

      case symbol_kind::S_join_spec: // join_spec
        yylhs.value.emplace< SJoinSpec > ();
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        yylhs.value.emplace< SLimit > ();
        break;

      case symbol_kind::S_order_item: // order_item
        yylhs.value.emplace< SOrderItem > ();
        break;

      case symbol_kind::S_select_core: // select_core
        yylhs.value.emplace< SSelectCore > ();
        break;

      case symbol_kind::S_select_item: // select_item
        yylhs.value.emplace< SSelectItem > ();
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        yylhs.value.emplace< SSelectStmtPtr > ();
        break;

      case symbol_kind::S_set_item: // set_item
        yylhs.value.emplace< SSetClause > ();
        break;

      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_insert_stmt: // insert_stmt
      case symbol_kind::S_update_stmt: // update_stmt
      case symbol_kind::S_delete_stmt: // delete_stmt
      case symbol_kind::S_create_table_stmt: // create_table_stmt
      case symbol_kind::S_drop_table_stmt: // drop_table_stmt
      case symbol_kind::S_alter_table_stmt: // alter_table_stmt
      case symbol_kind::S_create_index_stmt: // create_index_stmt
      case symbol_kind::S_drop_index_stmt: // drop_index_stmt
        yylhs.value.emplace< SStatementPtr > ();
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        yylhs.value.emplace< STableRefPtr > ();
        break;

      case symbol_kind::S_type_name: // type_name
        yylhs.value.emplace< STypeName > ();
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        yylhs.value.emplace< TExprList > ();
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        yylhs.value.emplace< TNameList > ();
        break;

      case symbol_kind::S_row_list: // row_list
        yylhs.value.emplace< TRowList > ();
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        yylhs.value.emplace< bool > ();
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        yylhs.value.emplace< double > ();
        break;

      case symbol_kind::S_compound_op: // compound_op
        yylhs.value.emplace< eCompoundOp > ();
        break;

      case symbol_kind::S_join_op: // join_op
        yylhs.value.emplace< eJoinType > ();
        break;

      case symbol_kind::S_INTEGER: // "integer"
        yylhs.value.emplace< int64_t > ();
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        yylhs.value.emplace< std::string > ();
        break;

      case symbol_kind::S_column_defs: // column_defs
        yylhs.value.emplace< std::vector<SColumnDef> > ();
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        yylhs.value.emplace< std::vector<SOrderItem> > ();
        break;

      case symbol_kind::S_select_list: // select_list
        yylhs.value.emplace< std::vector<SSelectItem> > ();
        break;

      case symbol_kind::S_set_list: // set_list
        yylhs.value.emplace< std::vector<SSetClause> > ();
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        yylhs.value.emplace< std::vector<STableRefPtr> > ();
        break;

      case symbol_kind::S_when_list: // when_list
        yylhs.value.emplace< std::vector<SWhenClause> > ();
        break;

      case symbol_kind::S_number_list: // number_list
        yylhs.value.emplace< std::vector<double> > ();
        break;

      case symbol_kind::S_integer_list: // integer_list
        yylhs.value.emplace< std::vector<int64_t> > ();
        break;

      case symbol_kind::S_PARAM: // "?"
        yylhs.value.emplace< uint32_t > ();
        break;

      default:
        break;
    }


      // Default location.
      {
        stack_type::slice range (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, range, yylen);
        yyerror_range[1].location = yylhs.location;
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 6: // opt_stmt: stmt
                { driver.AddStatement(yystack_[0].value.as < SStatementPtr > ()); }
    break;

  case 7: // stmt: select_stmt
                       { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SSelectStmtPtr > (); }
    break;

  case 8: // stmt: insert_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 9: // stmt: update_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 10: // stmt: delete_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 11: // stmt: create_table_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 12: // stmt: drop_table_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 13: // stmt: alter_table_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 14: // stmt: create_index_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 15: // stmt: drop_index_stmt
          { yylhs.value.as < SStatementPtr > () = yystack_[0].value.as < SStatementPtr > (); }
    break;

  case 16: // select_stmt: select_compound opt_order_by opt_limit
                {
			yylhs.value.as < SSelectStmtPtr > () = yystack_[2].value.as < SSelectStmtPtr > ();
			yylhs.value.as < SSelectStmtPtr > ()->m_orderBy = std::move(yystack_[1].value.as < std::vector<SOrderItem> > ());
			yylhs.value.as < SSelectStmtPtr > ()->m_ptrLimit = yystack_[0].value.as < SLimit > ().m_ptrLimit;
			yylhs.value.as < SSelectStmtPtr > ()->m_ptrOffset = yystack_[0].value.as < SLimit > ().m_ptrOffset;
		}
    break;

  case 17: // select_compound: select_core
                {
			yylhs.value.as < SSelectStmtPtr > () = At(std::make_shared<SSelectStmt>(), yylhs.location);
			yylhs.value.as < SSelectStmtPtr > ()->m_parts.emplace_back();
			yylhs.value.as < SSelectStmtPtr > ()->m_parts.back().m_core = std::move(yystack_[0].value.as < SSelectCore > ());
		}
    break;

  case 18: // select_compound: select_compound compound_op select_core
                {
			yylhs.value.as < SSelectStmtPtr > () = yystack_[2].value.as < SSelectStmtPtr > ();
			yylhs.value.as < SSelectStmtPtr > ()->m_parts.emplace_back();
			yylhs.value.as < SSelectStmtPtr > ()->m_parts.back().m_op = yystack_[1].value.as < eCompoundOp > ();
			yylhs.value.as < SSelectStmtPtr > ()->m_parts.back().m_core = std::move(yystack_[0].value.as < SSelectCore > ());
		}
    break;

  case 19: // compound_op: "UNION"
                             { yylhs.value.as < eCompoundOp > () = eCompoundOp::Union; }
    break;

  case 20: // compound_op: "UNION" "ALL"
                             { yylhs.value.as < eCompoundOp > () = eCompoundOp::UnionAll; }
    break;

  case 21: // compound_op: "INTERSECT"
                             { yylhs.value.as < eCompoundOp > () = eCompoundOp::Intersect; }
    break;

  case 22: // compound_op: "EXCEPT"
                             { yylhs.value.as < eCompoundOp > () = eCompoundOp::Except; }
    break;

  case 23: // select_core: "SELECT" opt_distinct select_list opt_from opt_where opt_group_by opt_having
                {
			yylhs.value.as < SSelectCore > ().m_bDistinct = yystack_[5].value.as < bool > ();
			yylhs.value.as < SSelectCore > ().m_items = std::move(yystack_[4].value.as < std::vector<SSelectItem> > ());
			yylhs.value.as < SSelectCore > ().m_from = std::move(yystack_[3].value.as < std::vector<STableRefPtr> > ());
			yylhs.value.as < SSelectCore > ().m_ptrWhere = yystack_[2].value.as < SExprPtr > ();
			yylhs.value.as < SSelectCore > ().m_groupBy = std::move(yystack_[1].value.as < TExprList > ());
			yylhs.value.as < SSelectCore > ().m_ptrHaving = yystack_[0].value.as < SExprPtr > ();
		}
    break;

  case 24: // opt_distinct: %empty
                         { yylhs.value.as < bool > () = false; }
    break;

  case 25: // opt_distinct: "DISTINCT"
                         { yylhs.value.as < bool > () = true; }
    break;

  case 26: // opt_distinct: "ALL"
                         { yylhs.value.as < bool > () = false; }
    break;

  case 27: // select_list: select_item
                                         { yylhs.value.as < std::vector<SSelectItem> > ().push_back(std::move(yystack_[0].value.as < SSelectItem > ())); }
    break;

  case 28: // select_list: select_list "," select_item
                                         { yylhs.value.as < std::vector<SSelectItem> > () = std::move(yystack_[2].value.as < std::vector<SSelectItem> > ()); yylhs.value.as < std::vector<SSelectItem> > ().push_back(std::move(yystack_[0].value.as < SSelectItem > ())); }
    break;

  case 29: // select_item: expr opt_alias
                                         { yylhs.value.as < SSelectItem > ().m_ptrExpr = yystack_[1].value.as < SExprPtr > (); yylhs.value.as < SSelectItem > ().m_sAlias = yystack_[0].value.as < std::string > (); }
    break;

  case 30: // select_item: "*"
                                         { yylhs.value.as < SSelectItem > ().m_ptrExpr = At(std::make_shared<SStar>(), yylhs.location); }
    break;

  case 31: // select_item: "identifier" "." "*"
                                         { yylhs.value.as < SSelectItem > ().m_ptrExpr = At(std::make_shared<SStar>(yystack_[2].value.as < std::string > ()), yylhs.location); }
    break;

  case 32: // opt_alias: %empty
                         { }
    break;

  case 33: // opt_alias: "AS" "identifier"
                         { yylhs.value.as < std::string > () = yystack_[0].value.as < std::string > (); }
    break;

  case 34: // opt_alias: "identifier"
                         { yylhs.value.as < std::string > () = yystack_[0].value.as < std::string > (); }
    break;

  case 35: // opt_from: %empty
                                 { }
    break;

  case 36: // opt_from: "FROM" from_list
                                 { yylhs.value.as < std::vector<STableRefPtr> > () = std::move(yystack_[0].value.as < std::vector<STableRefPtr> > ()); }
    break;

  case 37: // from_list: table_ref
                                     { yylhs.value.as < std::vector<STableRefPtr> > ().push_back(yystack_[0].value.as < STableRefPtr > ()); }
    break;

  case 38: // from_list: from_list "," table_ref
                                     { yylhs.value.as < std::vector<STableRefPtr> > () = std::move(yystack_[2].value.as < std::vector<STableRefPtr> > ()); yylhs.value.as < std::vector<STableRefPtr> > ().push_back(yystack_[0].value.as < STableRefPtr > ()); }
    break;

  case 39: // table_ref: table_primary
                         { yylhs.value.as < STableRefPtr > () = yystack_[0].value.as < STableRefPtr > (); }
    break;

  case 40: // table_ref: table_ref join_op table_primary join_spec
                {
			auto ptrJoin = At(std::make_shared<SJoinRef>(), yylhs.location);
			ptrJoin->m_joinType = yystack_[2].value.as < eJoinType > ();
			ptrJoin->m_ptrLeft = yystack_[3].value.as < STableRefPtr > ();
			ptrJoin->m_ptrRight = yystack_[1].value.as < STableRefPtr > ();
			ptrJoin->m_ptrCondition = yystack_[0].value.as < SJoinSpec > ().m_ptrCondition;
			ptrJoin->m_using = std::move(yystack_[0].value.as < SJoinSpec > ().m_using);
			if (yystack_[2].value.as < eJoinType > () == eJoinType::Cross && (ptrJoin->m_ptrCondition.get() || !ptrJoin->m_using.empty()))
				throw syntax_error(yystack_[0].location, "CROSS JOIN has no ON / USING");
			if ((yystack_[2].value.as < eJoinType > () == eJoinType::Left || yystack_[2].value.as < eJoinType > () == eJoinType::Right || yystack_[2].value.as < eJoinType > () == eJoinType::Full) &&
				ptrJoin->m_ptrCondition.get() == nullptr && ptrJoin->m_using.empty())
				throw syntax_error(yystack_[1].location, "an outer join needs ON or USING");
			yylhs.value.as < STableRefPtr > () = ptrJoin;
		}
    break;

  case 41: // join_op: "JOIN"
                                         { yylhs.value.as < eJoinType > () = eJoinType::Inner; }
    break;

  case 42: // join_op: "INNER" "JOIN"
                                         { yylhs.value.as < eJoinType > () = eJoinType::Inner; }
    break;

  case 43: // join_op: "LEFT" opt_outer "JOIN"
                                         { yylhs.value.as < eJoinType > () = eJoinType::Left; }
    break;

  case 44: // join_op: "RIGHT" opt_outer "JOIN"
                                         { yylhs.value.as < eJoinType > () = eJoinType::Right; }
    break;

  case 45: // join_op: "FULL" opt_outer "JOIN"
                                         { yylhs.value.as < eJoinType > () = eJoinType::Full; }
    break;

  case 46: // join_op: "CROSS" "JOIN"
                                         { yylhs.value.as < eJoinType > () = eJoinType::Cross; }
    break;

  case 49: // join_spec: %empty
                                         { }
    break;

  case 50: // join_spec: "ON" expr
                                         { yylhs.value.as < SJoinSpec > ().m_ptrCondition = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 51: // join_spec: "USING" "(" name_list ")"
                                         { yylhs.value.as < SJoinSpec > ().m_using = std::move(yystack_[1].value.as < TNameList > ()); }
    break;

  case 52: // table_primary: "identifier" opt_alias
                                                 { yylhs.value.as < STableRefPtr > () = At(std::make_shared<STableName>(yystack_[1].value.as < std::string > (), yystack_[0].value.as < std::string > ()), yylhs.location); }
    break;

  case 53: // table_primary: "(" select_stmt ")" opt_alias
                                                 { yylhs.value.as < STableRefPtr > () = At(std::make_shared<SSubqueryRef>(yystack_[2].value.as < SSelectStmtPtr > (), yystack_[0].value.as < std::string > ()), yylhs.location); }
    break;

  case 54: // opt_where: %empty
                             { }
    break;

  case 55: // opt_where: "WHERE" expr
                             { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 56: // opt_group_by: %empty
                                 { }
    break;

  case 57: // opt_group_by: "GROUP" "BY" expr_list
                                 { yylhs.value.as < TExprList > () = std::move(yystack_[0].value.as < TExprList > ()); }
    break;

  case 58: // opt_having: %empty
                             { }
    break;

  case 59: // opt_having: "HAVING" expr
                             { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 60: // opt_order_by: %empty
                                     { }
    break;

  case 61: // opt_order_by: "ORDER" "BY" order_list
                                     { yylhs.value.as < std::vector<SOrderItem> > () = std::move(yystack_[0].value.as < std::vector<SOrderItem> > ()); }
    break;

  case 62: // order_list: order_item
                                     { yylhs.value.as < std::vector<SOrderItem> > ().push_back(std::move(yystack_[0].value.as < SOrderItem > ())); }
    break;

  case 63: // order_list: order_list "," order_item
                                     { yylhs.value.as < std::vector<SOrderItem> > () = std::move(yystack_[2].value.as < std::vector<SOrderItem> > ()); yylhs.value.as < std::vector<SOrderItem> > ().push_back(std::move(yystack_[0].value.as < SOrderItem > ())); }
    break;

  case 64: // order_item: expr opt_desc
                         { yylhs.value.as < SOrderItem > ().m_ptrExpr = yystack_[1].value.as < SExprPtr > (); yylhs.value.as < SOrderItem > ().m_bDesc = yystack_[0].value.as < bool > (); }
    break;

  case 65: // opt_desc: %empty
                     { yylhs.value.as < bool > () = false; }
    break;

  case 66: // opt_desc: "ASC"
                     { yylhs.value.as < bool > () = false; }
    break;

  case 67: // opt_desc: "DESC"
                     { yylhs.value.as < bool > () = true; }
    break;

  case 68: // opt_limit: %empty
                                         { }
    break;

  case 69: // opt_limit: "LIMIT" expr
                                         { yylhs.value.as < SLimit > ().m_ptrLimit = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 70: // opt_limit: "LIMIT" expr "OFFSET" expr
                                         { yylhs.value.as < SLimit > ().m_ptrLimit = yystack_[2].value.as < SExprPtr > (); yylhs.value.as < SLimit > ().m_ptrOffset = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 71: // opt_limit: "LIMIT" expr "," expr
                                         { yylhs.value.as < SLimit > ().m_ptrLimit = yystack_[0].value.as < SExprPtr > (); yylhs.value.as < SLimit > ().m_ptrOffset = yystack_[2].value.as < SExprPtr > (); }
    break;

  case 72: // insert_stmt: "INSERT" "INTO" "identifier" opt_column_list "VALUES" row_list
                {
			auto ptrStmt = At(std::make_shared<SInsertStmt>(), yylhs.location);
			ptrStmt->m_sTable = yystack_[3].value.as < std::string > ();
			ptrStmt->m_columns = std::move(yystack_[2].value.as < TNameList > ());
			ptrStmt->m_rows = std::move(yystack_[0].value.as < TRowList > ());
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 73: // insert_stmt: "INSERT" "INTO" "identifier" opt_column_list select_stmt
                {
			auto ptrStmt = At(std::make_shared<SInsertStmt>(), yylhs.location);
			ptrStmt->m_sTable = yystack_[2].value.as < std::string > ();
			ptrStmt->m_columns = std::move(yystack_[1].value.as < TNameList > ());
			ptrStmt->m_ptrSelect = yystack_[0].value.as < SSelectStmtPtr > ();
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 74: // opt_column_list: %empty
                             { }
    break;

  case 75: // opt_column_list: "(" name_list ")"
                             { yylhs.value.as < TNameList > () = std::move(yystack_[1].value.as < TNameList > ()); }
    break;

  case 76: // row_list: row
                             { yylhs.value.as < TRowList > ().push_back(std::move(yystack_[0].value.as < TExprList > ())); }
    break;

  case 77: // row_list: row_list "," row
                             { yylhs.value.as < TRowList > () = std::move(yystack_[2].value.as < TRowList > ()); yylhs.value.as < TRowList > ().push_back(std::move(yystack_[0].value.as < TExprList > ())); }
    break;

  case 78: // row: "(" expr_list ")"
                             { yylhs.value.as < TExprList > () = std::move(yystack_[1].value.as < TExprList > ()); }
    break;

  case 79: // update_stmt: "UPDATE" "identifier" "SET" set_list opt_where
                {
			auto ptrStmt = At(std::make_shared<SUpdateStmt>(), yylhs.location);
			ptrStmt->m_sTable = yystack_[3].value.as < std::string > ();
			ptrStmt->m_set = std::move(yystack_[1].value.as < std::vector<SSetClause> > ());
			ptrStmt->m_ptrWhere = yystack_[0].value.as < SExprPtr > ();
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 80: // set_list: set_item
                                 { yylhs.value.as < std::vector<SSetClause> > ().push_back(std::move(yystack_[0].value.as < SSetClause > ())); }
    break;

  case 81: // set_list: set_list "," set_item
                                 { yylhs.value.as < std::vector<SSetClause> > () = std::move(yystack_[2].value.as < std::vector<SSetClause> > ()); yylhs.value.as < std::vector<SSetClause> > ().push_back(std::move(yystack_[0].value.as < SSetClause > ())); }
    break;

  case 82: // set_item: "identifier" "=" expr
                             { yylhs.value.as < SSetClause > ().m_sColumn = yystack_[2].value.as < std::string > (); yylhs.value.as < SSetClause > ().m_ptrValue = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 83: // delete_stmt: "DELETE" "FROM" "identifier" opt_where
                {
			auto ptrStmt = At(std::make_shared<SDeleteStmt>(), yylhs.location);
			ptrStmt->m_sTable = yystack_[1].value.as < std::string > ();
			ptrStmt->m_ptrWhere = yystack_[0].value.as < SExprPtr > ();
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 84: // create_table_stmt: "CREATE" "TABLE" opt_if_not_exists "identifier" "(" column_defs ")"
                {
			auto ptrStmt = At(std::make_shared<SCreateTableStmt>(), yylhs.location);
			ptrStmt->m_bIfNotExists = yystack_[4].value.as < bool > ();
			ptrStmt->m_sTable = yystack_[3].value.as < std::string > ();
			ptrStmt->m_columns = std::move(yystack_[1].value.as < std::vector<SColumnDef> > ());
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 85: // column_defs: column_def
                                         { yylhs.value.as < std::vector<SColumnDef> > ().push_back(std::move(yystack_[0].value.as < SColumnDef > ())); }
    break;

  case 86: // column_defs: column_defs "," column_def
                                         { yylhs.value.as < std::vector<SColumnDef> > () = std::move(yystack_[2].value.as < std::vector<SColumnDef> > ()); yylhs.value.as < std::vector<SColumnDef> > ().push_back(std::move(yystack_[0].value.as < SColumnDef > ())); }
    break;

  case 87: // column_def: "identifier" type_name column_constraints
                {
			yylhs.value.as < SColumnDef > () = std::move(yystack_[0].value.as < SColumnDef > ());
			yylhs.value.as < SColumnDef > ().m_sName = yystack_[2].value.as < std::string > ();
			yylhs.value.as < SColumnDef > ().m_type = std::move(yystack_[1].value.as < STypeName > ());
			yylhs.value.as < SColumnDef > ().m_pos = Pos(yylhs.location);
		}
    break;

  case 88: // column_constraints: %empty
                                                 { }
    break;

  case 89: // column_constraints: column_constraints "NOT" "NULL"
                                                 { yylhs.value.as < SColumnDef > () = std::move(yystack_[2].value.as < SColumnDef > ()); yylhs.value.as < SColumnDef > ().m_bNotNull = true; }
    break;

  case 90: // column_constraints: column_constraints "NULL"
                                                 { yylhs.value.as < SColumnDef > () = std::move(yystack_[1].value.as < SColumnDef > ()); yylhs.value.as < SColumnDef > ().m_bNotNull = false; }
    break;

  case 91: // column_constraints: column_constraints "UNIQUE"
                                                 { yylhs.value.as < SColumnDef > () = std::move(yystack_[1].value.as < SColumnDef > ()); yylhs.value.as < SColumnDef > ().m_bUnique = true; }
    break;

  case 92: // column_constraints: column_constraints "DEFAULT" b_expr
                                                 { yylhs.value.as < SColumnDef > () = std::move(yystack_[2].value.as < SColumnDef > ()); yylhs.value.as < SColumnDef > ().m_ptrDefault = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 93: // type_name: "identifier"
                                         { yylhs.value.as < STypeName > ().m_sName = Upper(yystack_[0].value.as < std::string > ()); }
    break;

  case 94: // type_name: "identifier" "(" integer_list ")"
                                         { yylhs.value.as < STypeName > ().m_sName = Upper(yystack_[3].value.as < std::string > ()); yylhs.value.as < STypeName > ().m_params = std::move(yystack_[1].value.as < std::vector<int64_t> > ()); }
    break;

  case 95: // integer_list: "integer"
                                         { yylhs.value.as < std::vector<int64_t> > ().push_back(yystack_[0].value.as < int64_t > ()); }
    break;

  case 96: // integer_list: integer_list "," "integer"
                                         { yylhs.value.as < std::vector<int64_t> > () = std::move(yystack_[2].value.as < std::vector<int64_t> > ()); yylhs.value.as < std::vector<int64_t> > ().push_back(yystack_[0].value.as < int64_t > ()); }
    break;

  case 97: // drop_table_stmt: "DROP" "TABLE" opt_if_exists "identifier"
                {
			auto ptrStmt = At(std::make_shared<SDropTableStmt>(), yylhs.location);
			ptrStmt->m_bIfExists = yystack_[1].value.as < bool > ();
			ptrStmt->m_sTable = yystack_[0].value.as < std::string > ();
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 98: // alter_table_stmt: "ALTER" "TABLE" "identifier" "ADD" opt_column column_def
                {
			auto ptrStmt = At(std::make_shared<SAlterTableStmt>(), yylhs.location);
			ptrStmt->m_sTable = yystack_[3].value.as < std::string > ();
			ptrStmt->m_action = SAlterTableStmt::AddColumn;
			ptrStmt->m_column = std::move(yystack_[0].value.as < SColumnDef > ());
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 99: // alter_table_stmt: "ALTER" "TABLE" "identifier" "DROP" opt_column "identifier"
                {
			auto ptrStmt = At(std::make_shared<SAlterTableStmt>(), yylhs.location);
			ptrStmt->m_sTable = yystack_[3].value.as < std::string > ();
			ptrStmt->m_action = SAlterTableStmt::DropColumn;
			ptrStmt->m_sColumn = yystack_[0].value.as < std::string > ();
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 102: // create_index_stmt: "CREATE" index_kind "INDEX" opt_if_not_exists opt_name "ON" "identifier" "(" name_list ")" opt_index_method
                {
			auto ptrStmt = At(std::make_shared<SCreateIndexStmt>(), yylhs.location);
			ptrStmt->m_kind = yystack_[9].value.as < SCreateIndexStmt::eKind > ();
			ptrStmt->m_bIfNotExists = yystack_[7].value.as < bool > ();
			ptrStmt->m_sName = yystack_[6].value.as < std::string > ();
			ptrStmt->m_sTable = yystack_[4].value.as < std::string > ();
			ptrStmt->m_columns = std::move(yystack_[2].value.as < TNameList > ());
			ptrStmt->m_sMethod = yystack_[0].value.as < SIndexMethod > ().m_sName;
			ptrStmt->m_methodParams = std::move(yystack_[0].value.as < SIndexMethod > ().m_params);
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 103: // index_kind: %empty
                         { yylhs.value.as < SCreateIndexStmt::eKind > () = SCreateIndexStmt::Regular; }
    break;

  case 104: // index_kind: "UNIQUE"
                         { yylhs.value.as < SCreateIndexStmt::eKind > () = SCreateIndexStmt::Unique; }
    break;

  case 105: // index_kind: "SPATIAL"
                         { yylhs.value.as < SCreateIndexStmt::eKind > () = SCreateIndexStmt::Spatial; }
    break;

  case 106: // opt_index_method: %empty
                                             { }
    break;

  case 107: // opt_index_method: "USING" "identifier"
                                             { yylhs.value.as < SIndexMethod > ().m_sName = Upper(yystack_[0].value.as < std::string > ()); }
    break;

  case 108: // opt_index_method: "USING" "identifier" "(" number_list ")"
                                             { yylhs.value.as < SIndexMethod > ().m_sName = Upper(yystack_[3].value.as < std::string > ()); yylhs.value.as < SIndexMethod > ().m_params = std::move(yystack_[1].value.as < std::vector<double> > ()); }
    break;

  case 109: // number_list: number
                                     { yylhs.value.as < std::vector<double> > ().push_back(yystack_[0].value.as < double > ()); }
    break;

  case 110: // number_list: number_list "," number
                                     { yylhs.value.as < std::vector<double> > () = std::move(yystack_[2].value.as < std::vector<double> > ()); yylhs.value.as < std::vector<double> > ().push_back(yystack_[0].value.as < double > ()); }
    break;

  case 111: // number: "integer"
                         { yylhs.value.as < double > () = (double)yystack_[0].value.as < int64_t > (); }
    break;

  case 112: // number: "number"
                         { yylhs.value.as < double > () = yystack_[0].value.as < double > (); }
    break;

  case 113: // number: "-" "integer"
                         { yylhs.value.as < double > () = -(double)yystack_[0].value.as < int64_t > (); }
    break;

  case 114: // number: "-" "number"
                         { yylhs.value.as < double > () = -yystack_[0].value.as < double > (); }
    break;

  case 115: // drop_index_stmt: "DROP" "INDEX" opt_if_exists opt_name "ON" "identifier" "(" name_list ")"
                {
			auto ptrStmt = At(std::make_shared<SDropIndexStmt>(), yylhs.location);
			ptrStmt->m_bIfExists = yystack_[6].value.as < bool > ();
			ptrStmt->m_sName = yystack_[5].value.as < std::string > ();
			ptrStmt->m_sTable = yystack_[3].value.as < std::string > ();
			ptrStmt->m_columns = std::move(yystack_[1].value.as < TNameList > ());
			yylhs.value.as < SStatementPtr > () = ptrStmt;
		}
    break;

  case 116: // opt_name: %empty
                     { }
    break;

  case 117: // opt_name: "identifier"
                     { yylhs.value.as < std::string > () = yystack_[0].value.as < std::string > (); }
    break;

  case 118: // opt_if_exists: %empty
                             { yylhs.value.as < bool > () = false; }
    break;

  case 119: // opt_if_exists: "IF" "EXISTS"
                             { yylhs.value.as < bool > () = true; }
    break;

  case 120: // opt_if_not_exists: %empty
                                 { yylhs.value.as < bool > () = false; }
    break;

  case 121: // opt_if_not_exists: "IF" "NOT" "EXISTS"
                                 { yylhs.value.as < bool > () = true; }
    break;

  case 122: // name_list: "identifier"
                                 { yylhs.value.as < TNameList > ().push_back(yystack_[0].value.as < std::string > ()); }
    break;

  case 123: // name_list: name_list "," "identifier"
                                 { yylhs.value.as < TNameList > () = std::move(yystack_[2].value.as < TNameList > ()); yylhs.value.as < TNameList > ().push_back(yystack_[0].value.as < std::string > ()); }
    break;

  case 124: // expr: b_expr
                                                     { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 125: // expr: expr "OR" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Or, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 126: // expr: expr "AND" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::And, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 127: // expr: "NOT" expr
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SUnaryExpr>(eUnaryOp::Not, yystack_[0].value.as < SExprPtr > ()), yylhs.location); }
    break;

  case 128: // expr: expr "=" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Equal, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 129: // expr: expr "<>" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::NotEqual, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 130: // expr: expr "<" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Less, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 131: // expr: expr "<=" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::LessOrEqual, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 132: // expr: expr ">" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Greater, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 133: // expr: expr ">=" expr
                                                     { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::GreaterOrEqual, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 134: // expr: expr "IS" "NULL"
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SIsNullExpr>(yystack_[2].value.as < SExprPtr > (), false), yylhs.location); }
    break;

  case 135: // expr: expr "IS" "NOT" "NULL"
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SIsNullExpr>(yystack_[3].value.as < SExprPtr > (), true), yylhs.location); }
    break;

  case 136: // expr: expr "LIKE" expr
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SLikeExpr>(yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), nullptr, false), yylhs.location); }
    break;

  case 137: // expr: expr "LIKE" expr "ESCAPE" expr
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SLikeExpr>(yystack_[4].value.as < SExprPtr > (), yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), false), yylhs.location); }
    break;

  case 138: // expr: expr "NOT" "LIKE" expr
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SLikeExpr>(yystack_[3].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), nullptr, true), yylhs.location); }
    break;

  case 139: // expr: expr "NOT" "LIKE" expr "ESCAPE" expr
                                                     { yylhs.value.as < SExprPtr > () = At(std::make_shared<SLikeExpr>(yystack_[5].value.as < SExprPtr > (), yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), true), yylhs.location); }
    break;

  case 140: // expr: expr "BETWEEN" b_expr "AND" b_expr
                { yylhs.value.as < SExprPtr > () = At(std::make_shared<SBetweenExpr>(yystack_[4].value.as < SExprPtr > (), yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), false), yylhs.location); }
    break;

  case 141: // expr: expr "NOT" "BETWEEN" b_expr "AND" b_expr
                { yylhs.value.as < SExprPtr > () = At(std::make_shared<SBetweenExpr>(yystack_[5].value.as < SExprPtr > (), yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), true), yylhs.location); }
    break;

  case 142: // expr: expr "IN" "(" expr_list ")"
                                                                 { yylhs.value.as < SExprPtr > () = At(std::make_shared<SInListExpr>(yystack_[4].value.as < SExprPtr > (), yystack_[1].value.as < TExprList > (), false), yylhs.location); }
    break;

  case 143: // expr: expr "NOT" "IN" "(" expr_list ")"
                                                                 { yylhs.value.as < SExprPtr > () = At(std::make_shared<SInListExpr>(yystack_[5].value.as < SExprPtr > (), yystack_[1].value.as < TExprList > (), true), yylhs.location); }
    break;

  case 144: // expr: expr "IN" "(" select_stmt ")"
                                                                 { yylhs.value.as < SExprPtr > () = At(std::make_shared<SInSelectExpr>(yystack_[4].value.as < SExprPtr > (), yystack_[1].value.as < SSelectStmtPtr > (), false), yylhs.location); }
    break;

  case 145: // expr: expr "NOT" "IN" "(" select_stmt ")"
                                                                 { yylhs.value.as < SExprPtr > () = At(std::make_shared<SInSelectExpr>(yystack_[5].value.as < SExprPtr > (), yystack_[1].value.as < SSelectStmtPtr > (), true), yylhs.location); }
    break;

  case 146: // b_expr: primary
                                         { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 147: // b_expr: b_expr "+" b_expr
                                         { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Add, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 148: // b_expr: b_expr "-" b_expr
                                         { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Subtract, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 149: // b_expr: b_expr "*" b_expr
                                         { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Multiply, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 150: // b_expr: b_expr "/" b_expr
                                         { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Divide, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 151: // b_expr: b_expr "%" b_expr
                                         { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Modulo, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 152: // b_expr: b_expr "||" b_expr
                                         { yylhs.value.as < SExprPtr > () = Binary(eBinaryOp::Concat, yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 153: // b_expr: "-" b_expr
                                         { yylhs.value.as < SExprPtr > () = Negate(yystack_[0].value.as < SExprPtr > (), yylhs.location); }
    break;

  case 154: // b_expr: "+" b_expr
                                         { yylhs.value.as < SExprPtr > () = At(std::make_shared<SUnaryExpr>(eUnaryOp::Plus, yystack_[0].value.as < SExprPtr > ()), yylhs.location); }
    break;

  case 155: // primary: literal
                                         { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 156: // primary: "?"
                                         { yylhs.value.as < SExprPtr > () = At(std::make_shared<SParameter>(yystack_[0].value.as < uint32_t > ()), yylhs.location); }
    break;

  case 157: // primary: "identifier"
                                         { yylhs.value.as < SExprPtr > () = At(std::make_shared<SColumnRef>(std::string(), yystack_[0].value.as < std::string > ()), yylhs.location); }
    break;

  case 158: // primary: "identifier" "." "identifier"
                                         { yylhs.value.as < SExprPtr > () = At(std::make_shared<SColumnRef>(yystack_[2].value.as < std::string > (), yystack_[0].value.as < std::string > ()), yylhs.location); }
    break;

  case 159: // primary: "identifier" "(" ")"
                {
			auto ptrCall = At(std::make_shared<SFunctionCall>(), yylhs.location);
			ptrCall->m_sName = Upper(yystack_[2].value.as < std::string > ());
			yylhs.value.as < SExprPtr > () = ptrCall;
		}
    break;

  case 160: // primary: "identifier" "(" "*" ")"
                {
			auto ptrCall = At(std::make_shared<SFunctionCall>(), yylhs.location);
			ptrCall->m_sName = Upper(yystack_[3].value.as < std::string > ());
			ptrCall->m_bStar = true;
			yylhs.value.as < SExprPtr > () = ptrCall;
		}
    break;

  case 161: // primary: "identifier" "(" opt_distinct expr_list ")"
                {
			auto ptrCall = At(std::make_shared<SFunctionCall>(), yylhs.location);
			ptrCall->m_sName = Upper(yystack_[4].value.as < std::string > ());
			ptrCall->m_bDistinct = yystack_[2].value.as < bool > ();
			ptrCall->m_args = std::move(yystack_[1].value.as < TExprList > ());
			yylhs.value.as < SExprPtr > () = ptrCall;
		}
    break;

  case 162: // primary: "(" expr ")"
                                         { yylhs.value.as < SExprPtr > () = yystack_[1].value.as < SExprPtr > (); }
    break;

  case 163: // primary: "(" select_stmt ")"
                                         { yylhs.value.as < SExprPtr > () = At(std::make_shared<SSubqueryExpr>(yystack_[1].value.as < SSelectStmtPtr > ()), yylhs.location); }
    break;

  case 164: // primary: "EXISTS" "(" select_stmt ")"
                                         { yylhs.value.as < SExprPtr > () = At(std::make_shared<SExistsExpr>(yystack_[1].value.as < SSelectStmtPtr > ()), yylhs.location); }
    break;

  case 165: // primary: "CASE" opt_case_operand when_list opt_else "END"
                {
			auto ptrCase = At(std::make_shared<SCaseExpr>(), yylhs.location);
			ptrCase->m_ptrOperand = yystack_[3].value.as < SExprPtr > ();
			ptrCase->m_whens = std::move(yystack_[2].value.as < std::vector<SWhenClause> > ());
			ptrCase->m_ptrElse = yystack_[1].value.as < SExprPtr > ();
			yylhs.value.as < SExprPtr > () = ptrCase;
		}
    break;

  case 166: // primary: "CAST" "(" expr "AS" type_name ")"
                                             { yylhs.value.as < SExprPtr > () = At(std::make_shared<SCastExpr>(yystack_[3].value.as < SExprPtr > (), yystack_[1].value.as < STypeName > ()), yylhs.location); }
    break;

  case 167: // literal: "integer"
                         { yylhs.value.as < SExprPtr > () = At(SLiteral::MakeInteger(yystack_[0].value.as < int64_t > ()), yylhs.location); }
    break;

  case 168: // literal: "number"
                         { yylhs.value.as < SExprPtr > () = At(SLiteral::MakeDouble(yystack_[0].value.as < double > ()), yylhs.location); }
    break;

  case 169: // literal: "string"
                         { yylhs.value.as < SExprPtr > () = At(SLiteral::MakeString(yystack_[0].value.as < std::string > ()), yylhs.location); }
    break;

  case 170: // literal: "NULL"
                         { yylhs.value.as < SExprPtr > () = At(SLiteral::MakeNull(), yylhs.location); }
    break;

  case 171: // literal: "TRUE"
                         { yylhs.value.as < SExprPtr > () = At(SLiteral::MakeBool(true), yylhs.location); }
    break;

  case 172: // literal: "FALSE"
                         { yylhs.value.as < SExprPtr > () = At(SLiteral::MakeBool(false), yylhs.location); }
    break;

  case 173: // opt_case_operand: %empty
                     { }
    break;

  case 174: // opt_case_operand: expr
                     { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 175: // when_list: "WHEN" expr "THEN" expr
                                                 { yylhs.value.as < std::vector<SWhenClause> > ().push_back(SWhenClause{ yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > () }); }
    break;

  case 176: // when_list: when_list "WHEN" expr "THEN" expr
                                                 { yylhs.value.as < std::vector<SWhenClause> > () = std::move(yystack_[4].value.as < std::vector<SWhenClause> > ()); yylhs.value.as < std::vector<SWhenClause> > ().push_back(SWhenClause{ yystack_[2].value.as < SExprPtr > (), yystack_[0].value.as < SExprPtr > () }); }
    break;

  case 177: // opt_else: %empty
                         { }
    break;

  case 178: // opt_else: "ELSE" expr
                         { yylhs.value.as < SExprPtr > () = yystack_[0].value.as < SExprPtr > (); }
    break;

  case 179: // expr_list: expr
                                 { yylhs.value.as < TExprList > ().push_back(yystack_[0].value.as < SExprPtr > ()); }
    break;

  case 180: // expr_list: expr_list "," expr
                                 { yylhs.value.as < TExprList > () = std::move(yystack_[2].value.as < TExprList > ()); yylhs.value.as < TExprList > ().push_back(yystack_[0].value.as < SExprPtr > ()); }
    break;



            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
    }
    goto yynewstate;


  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        context yyctx (*this, yyla);
        std::string msg = yysyntax_error_ (yyctx);
        error (yyla.location, YY_MOVE (msg));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.kind () == symbol_kind::S_YYEOF)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;)
      {
        yyn = yypact_[+yystack_[0].state];
        if (!yy_pact_value_is_default_ (yyn))
          {
            yyn += symbol_kind::S_YYerror;
            if (0 <= yyn && yyn <= yylast_
                && yycheck_[yyn] == symbol_kind::S_YYerror)
              {
                yyn = yytable_[yyn];
                if (0 < yyn)
                  break;
              }
          }

        // Pop the current state because it cannot handle the error token.
        if (yystack_.size () == 1)
          YYABORT;

        yyerror_range[1].location = yystack_[0].location;
        yy_destroy_ ("Error: popping", yystack_[0]);
        yypop_ ();
        YY_STACK_PRINT ();
      }
    {
      stack_symbol_type error_token;

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = state_type (yyn);
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    YY_STACK_PRINT ();
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  CSQLParser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what ());
  }

  const char *
  CSQLParser::symbol_name (symbol_kind_type yysymbol)
  {
    static const char *const yy_sname[] =
    {
    "end of input", "error", "invalid token", "identifier", "string",
  "integer", "number", "?", "(", ")", ",", ".", ";", "*", "+", "-", "/",
  "%", "||", "=", "<>", "<", "<=", ">", ">=", "ADD", "ALL", "ALTER", "AND",
  "AS", "ASC", "BETWEEN", "BY", "CASE", "CAST", "COLUMN", "CREATE",
  "CROSS", "DEFAULT", "DELETE", "DESC", "DISTINCT", "DROP", "ELSE", "END",
  "ESCAPE", "EXCEPT", "EXISTS", "FALSE", "FROM", "FULL", "GROUP", "HAVING",
  "IF", "IN", "INDEX", "INNER", "INSERT", "INTERSECT", "INTO", "IS",
  "JOIN", "LEFT", "LIKE", "LIMIT", "NOT", "NULL", "OFFSET", "ON", "OR",
  "ORDER", "OUTER", "RIGHT", "SELECT", "SET", "SPATIAL", "TABLE", "THEN",
  "TRUE", "UNION", "UNIQUE", "UPDATE", "USING", "VALUES", "WHEN", "WHERE",
  "UMINUS", "$accept", "script", "stmt_seq", "opt_stmt", "stmt",
  "select_stmt", "select_compound", "compound_op", "select_core",
  "opt_distinct", "select_list", "select_item", "opt_alias", "opt_from",
  "from_list", "table_ref", "join_op", "opt_outer", "join_spec",
  "table_primary", "opt_where", "opt_group_by", "opt_having",
  "opt_order_by", "order_list", "order_item", "opt_desc", "opt_limit",
  "insert_stmt", "opt_column_list", "row_list", "row", "update_stmt",
  "set_list", "set_item", "delete_stmt", "create_table_stmt",
  "column_defs", "column_def", "column_constraints", "type_name",
  "integer_list", "drop_table_stmt", "alter_table_stmt", "opt_column",
  "create_index_stmt", "index_kind", "opt_index_method", "number_list",
  "number", "drop_index_stmt", "opt_name", "opt_if_exists",
  "opt_if_not_exists", "name_list", "expr", "b_expr", "primary", "literal",
  "opt_case_operand", "when_list", "opt_else", "expr_list", YY_NULLPTR
    };
    return yy_sname[yysymbol];
  }



  // CSQLParser::context.
  CSQLParser::context::context (const CSQLParser& yyparser, const symbol_type& yyla)
    : yyparser_ (yyparser)
    , yyla_ (yyla)
  {}

  int
  CSQLParser::context::expected_tokens (symbol_kind_type yyarg[], int yyargn) const
  {
    // Actual number of expected tokens
    int yycount = 0;

    const int yyn = yypact_[+yyparser_.yystack_[0].state];
    if (!yy_pact_value_is_default_ (yyn))
      {
        /* Start YYX at -YYN if negative to avoid negative indexes in
           YYCHECK.  In other words, skip the first -YYN actions for
           this state because they are default actions.  */
        const int yyxbegin = yyn < 0 ? -yyn : 0;
        // Stay within bounds of both yycheck and yytname.
        const int yychecklim = yylast_ - yyn + 1;
        const int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
        for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
          if (yycheck_[yyx + yyn] == yyx && yyx != symbol_kind::S_YYerror
              && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
            {
              if (!yyarg)
                ++yycount;
              else if (yycount == yyargn)
                return 0;
              else
                yyarg[yycount++] = YY_CAST (symbol_kind_type, yyx);
            }
      }

    if (yyarg && yycount == 0 && 0 < yyargn)
      yyarg[0] = symbol_kind::S_YYEMPTY;
    return yycount;
  }






  int
  CSQLParser::yy_syntax_error_arguments_ (const context& yyctx,
                                                 symbol_kind_type yyarg[], int yyargn) const
  {
    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state merging
         (from LALR or IELR) and default reductions corrupt the expected
         token list.  However, the list is correct for canonical LR with
         one exception: it will still contain any token that will not be
         accepted due to an error action in a later state.
    */

    if (!yyctx.lookahead ().empty ())
      {
        if (yyarg)
          yyarg[0] = yyctx.token ();
        int yyn = yyctx.expected_tokens (yyarg ? yyarg + 1 : yyarg, yyargn - 1);
        return yyn + 1;
      }
    return 0;
  }

  // Generate an error message.
  std::string
  CSQLParser::yysyntax_error_ (const context& yyctx) const
  {
    // Its maximum.
    enum { YYARGS_MAX = 5 };
    // Arguments of yyformat.
    symbol_kind_type yyarg[YYARGS_MAX];
    int yycount = yy_syntax_error_arguments_ (yyctx, yyarg, YYARGS_MAX);

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
      default: // Avoid compiler warnings.
        YYCASE_ (0, YY_("syntax error"));
        YYCASE_ (1, YY_("syntax error, unexpected %s"));
        YYCASE_ (2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_ (3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_ (4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_ (5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    std::ptrdiff_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += symbol_name (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const short CSQLParser::yypact_ninf_ = -248;

  const signed char CSQLParser::yytable_ninf_ = -1;

  const short
  CSQLParser::yypact_[] =
  {
      -9,   -56,   112,    10,    -8,     4,    34,    66,    76,    77,
    -248,  -248,  -248,   123,  -248,  -248,  -248,  -248,  -248,  -248,
    -248,  -248,  -248,    94,  -248,    46,  -248,    52,   120,    48,
      48,   126,  -248,  -248,   113,    41,  -248,    -9,  -248,  -248,
     136,   118,   104,   106,    12,   151,   198,    46,   138,   174,
     223,   238,   236,    97,  -248,  -248,  -248,  -248,   149,  -248,
       8,     8,   203,   240,   250,  -248,   203,  -248,  -248,    -5,
    -248,   111,   523,  -248,  -248,   257,  -248,   203,  -248,  -248,
     203,  -248,   231,   231,   239,   272,   223,   203,  -248,  -248,
    -248,   220,  -248,   286,   -52,   132,    54,   164,   281,   233,
    -248,  -248,   389,   208,   203,   104,   453,   113,   135,   138,
    -248,   203,   203,   203,   203,   203,   203,   203,   288,     8,
     287,   -57,   203,    -2,   203,  -248,     8,     8,     8,     8,
       8,     8,   278,     0,  -248,   289,  -248,   362,   284,  -248,
     297,   306,  -248,   297,   242,   389,   308,  -248,   127,   293,
    -248,  -248,   304,   203,  -248,  -248,   313,  -248,  -248,   203,
     -26,   375,   305,  -248,    16,   104,   314,    50,  -248,   274,
     524,   524,   255,   255,   255,   255,   453,  -248,   215,   149,
     262,  -248,   466,     8,   315,   203,   440,   309,   254,   254,
     309,   309,  -248,   203,   257,  -248,   203,  -248,  -248,  -248,
     203,   203,   333,  -248,  -248,   140,  -248,   334,   332,  -248,
     338,   203,   335,  -248,  -248,   389,   180,   298,   203,   203,
     299,   333,  -248,  -248,   337,   135,   294,   277,   295,  -248,
     277,   277,   135,   318,   302,     8,   348,   185,  -248,   203,
     465,   149,   511,   389,  -248,  -248,   389,   389,   351,  -248,
    -248,   297,   352,   286,  -248,   189,   293,  -248,   203,   203,
     389,   311,  -248,   353,    16,    50,  -248,  -248,   303,  -248,
     307,   312,   -17,   203,   203,  -248,   523,  -248,  -248,   261,
       8,   357,   194,   203,   364,   -31,  -248,   286,   210,  -248,
    -248,   389,   389,   203,  -248,  -248,  -248,  -248,  -248,   203,
     369,  -248,   360,   389,   523,  -248,  -248,   261,  -248,   225,
       8,   321,  -248,  -248,   229,  -248,   389,   389,   286,  -248,
     367,   523,  -248,   296,   237,  -248,   376,  -248,  -248,   381,
      98,  -248,  -248,   207,   253,  -248,  -248,  -248,  -248,    98,
    -248
  };

  const unsigned char
  CSQLParser::yydefact_[] =
  {
       5,     0,   103,     0,     0,     0,    24,     0,     0,     2,
       3,     6,     7,    60,    17,     8,     9,    10,    11,    12,
      13,    14,    15,     0,   105,   120,   104,     0,     0,   118,
     118,     0,    26,    25,     0,     0,     1,     5,    22,    21,
       0,    19,     0,    68,     0,     0,     0,   120,    54,     0,
     116,     0,    74,   157,   169,   167,   168,   156,     0,    30,
       0,     0,   173,     0,     0,   172,     0,   170,   171,    35,
      27,    32,   124,   146,   155,     0,     4,     0,    20,    18,
       0,    16,   100,   100,     0,     0,   116,     0,    83,   119,
     117,     0,    97,     0,     0,    24,     0,   157,     0,     0,
     154,   153,   174,     0,     0,     0,   127,     0,     0,    54,
      34,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    29,     0,     0,     0,     0,
       0,     0,     0,    54,    80,    61,    62,    65,    69,   101,
       0,     0,   121,     0,     0,    55,     0,   122,     0,     0,
      73,   159,     0,     0,   158,    31,     0,   163,   162,     0,
     177,     0,     0,    28,    32,     0,    36,    37,    39,    56,
     128,   129,   130,   131,   132,   133,   126,    33,     0,     0,
       0,   134,   136,     0,     0,     0,   125,   149,   147,   148,
     150,   151,   152,     0,     0,    79,     0,    66,    67,    64,
       0,     0,     0,    98,    99,     0,    85,     0,     0,    75,
       0,     0,    72,    76,   160,   179,     0,     0,     0,     0,
       0,     0,   164,    52,     0,     0,     0,    47,     0,    41,
      47,    47,     0,     0,    58,     0,     0,     0,   135,     0,
       0,     0,   138,    82,    81,    63,    71,    70,    93,    88,
      84,     0,     0,     0,   123,     0,     0,   161,     0,     0,
     178,     0,   165,     0,    32,    38,    46,    48,     0,    42,
       0,     0,    49,     0,     0,    23,   140,   144,   142,   137,
       0,     0,     0,     0,     0,    87,    86,     0,     0,    78,
      77,   180,   175,     0,   166,    53,    45,    43,    44,     0,
       0,    40,    57,    59,   141,   145,   143,   139,    95,     0,
       0,     0,    90,    91,     0,   115,   176,    50,     0,    94,
       0,    92,    89,   106,     0,    96,     0,   102,    51,   107,
       0,   111,   112,     0,     0,   109,   113,   114,   108,     0,
     110
  };

  const short
  CSQLParser::yypgoto_[] =
  {
    -248,  -248,  -248,   354,  -248,   -55,  -248,  -248,   358,   310,
    -248,   300,  -162,  -248,  -248,   176,  -248,    43,  -248,   182,
     -71,  -248,  -248,  -248,  -248,   219,  -248,  -248,  -248,  -248,
    -248,   162,  -248,  -248,   227,  -248,  -248,  -248,  -142,  -248,
     202,  -248,  -248,  -248,   336,  -248,  -248,  -248,  -248,    85,
    -248,   340,   398,   383,  -247,   -34,   -35,  -248,  -248,  -248,
    -248,  -248,  -175
  };

  const short
  CSQLParser::yydefgoto_[] =
  {
       0,     8,     9,    10,    11,    12,    13,    42,    14,    34,
      69,    70,   125,   109,   166,   167,   232,   268,   301,   168,
      88,   234,   275,    43,   135,   136,   199,    81,    15,    94,
     212,   213,    16,   133,   134,    17,    18,   205,   203,   285,
     249,   309,    19,    20,   140,    21,    27,   327,   334,   335,
      22,    91,    50,    46,   148,   215,    72,    73,    74,   103,
     160,   220,   216
  };

  const short
  CSQLParser::yytable_[] =
  {
      71,   206,   223,    98,   237,   107,   288,   310,   180,   181,
     194,    97,    54,    55,    56,    57,    58,   218,     1,   110,
      23,     6,    60,    61,    99,   100,   101,     2,   102,   183,
       3,   149,   106,     4,   311,   312,   255,    82,   169,   150,
     314,    62,    63,   137,   108,   118,   138,    29,     5,   313,
     162,   299,   184,   145,    83,    64,    65,   154,   219,    28,
      32,   185,   195,    31,     6,   300,   282,   155,    30,    35,
     161,   324,     7,    71,    67,    33,    36,   170,   171,   172,
     173,   174,   175,   176,   178,    87,    68,   226,   182,    37,
     186,   187,   188,   189,   190,   191,   192,    44,   302,    45,
     227,    49,   295,   331,   332,    95,   228,    47,    96,   286,
     224,   229,   230,   333,   110,    75,    53,    54,    55,    56,
      57,    58,   231,    48,   236,   217,    59,    60,    61,    52,
     111,   112,   113,   114,   115,   116,   209,   210,   164,   117,
     118,   151,   119,   165,    78,   152,    62,    63,   240,   250,
     251,   242,    97,    54,    55,    56,    57,    58,    32,   243,
      64,    65,   137,    60,    61,   120,   246,   247,    77,    38,
      80,   121,    95,    33,   122,   156,   123,     6,    66,    67,
     124,    39,    62,    63,   260,   261,   281,    24,    25,   257,
     258,    68,    26,    40,   278,   258,    64,    65,   289,   258,
     276,    85,    41,   306,   258,   279,    97,    54,    55,    56,
      57,    58,   336,   337,    66,    67,    84,    60,    61,   315,
     210,    89,     6,    87,   291,   292,    90,    68,   126,   127,
     128,   129,   130,   131,   319,   320,    62,    63,   323,   210,
     303,    92,   158,   235,    93,   304,   328,   210,   104,   307,
      64,    65,   111,   112,   113,   114,   115,   116,   105,   316,
     132,   117,   338,   339,   119,   317,   139,   126,    66,    67,
     129,   130,   131,   270,   271,   321,    -1,    -1,    -1,    -1,
     143,    68,   113,   114,   115,   116,   142,   120,   146,   147,
     157,   177,   159,   121,   200,   179,   122,   193,   123,   196,
     202,   211,   124,   111,   112,   113,   114,   115,   116,   204,
     207,   208,   117,   214,   222,   119,   154,   111,   112,   113,
     114,   115,   116,   241,   225,   233,   117,   131,   238,   119,
     111,   112,   113,   114,   115,   116,   248,   252,   120,   117,
     253,   254,   119,   262,   121,   256,   264,   122,   267,   123,
     273,   201,   120,   124,   274,   266,   269,   277,   121,   284,
     287,   122,   294,   123,   296,   120,   305,   124,   297,   308,
     258,   121,   325,   298,   122,   259,   123,   318,   326,   329,
     124,   111,   112,   113,   114,   115,   116,   322,   293,   330,
     117,    76,   197,   119,   111,   112,   113,   114,   115,   116,
      79,   265,   198,   117,   221,   153,   119,   163,   111,   112,
     113,   114,   115,   116,   272,   245,   120,   117,   290,   141,
     119,   244,   121,   263,   340,   122,   144,   123,    51,   120,
      86,   124,     0,     0,     0,   121,     0,     0,   122,     0,
     123,     0,     0,   120,   124,     0,     0,     0,     0,   121,
       0,     0,   122,     0,   123,     0,     0,     0,   124,   111,
     112,   113,   114,   115,   116,     0,     0,     0,   117,     0,
       0,   119,   111,   112,   113,   114,   115,   116,   126,   127,
     128,   129,   130,   131,   119,    -1,    -1,   113,   114,   115,
     116,     0,     0,   280,   120,     0,     0,    -1,     0,     0,
     121,     0,     0,   122,     0,   123,     0,   120,     0,     0,
       0,   239,     0,   121,     0,     0,   122,     0,   123,     0,
      -1,     0,     0,     0,     0,     0,    -1,     0,     0,    -1,
      -1,    -1,   113,   114,   115,   116,   126,   127,   128,   129,
     130,   131,    -1,    -1,    -1,   113,   114,   115,   116,     0,
       0,     0,     0,     0,     0,    -1,   283,     0,     0,     0,
       0,     0,     0,     0,     0,    -1,     0,     0,     0,     0,
       0,    -1,     0,     0,    -1,     0,     0,     0,    -1,     0,
       0,     0,     0,     0,    -1,     0,     0,    -1
  };

  const short
  CSQLParser::yycheck_[] =
  {
      34,   143,   164,    58,   179,    10,   253,    38,    65,    66,
      10,     3,     4,     5,     6,     7,     8,    43,    27,     3,
      76,    73,    14,    15,    58,    60,    61,    36,    62,    31,
      39,    83,    66,    42,    65,    66,   211,    25,   109,    94,
     287,    33,    34,    77,    49,    29,    80,    55,    57,    80,
     105,    68,    54,    87,    42,    47,    48,     3,    84,    49,
      26,    63,   133,    59,    73,    82,   241,    13,    76,     3,
     104,   318,    81,   107,    66,    41,     0,   111,   112,   113,
     114,   115,   116,   117,   119,    85,    78,    37,   122,    12,
     124,   126,   127,   128,   129,   130,   131,     3,   273,    53,
      50,    53,   264,     5,     6,     8,    56,    55,    11,   251,
     165,    61,    62,    15,     3,    74,     3,     4,     5,     6,
       7,     8,    72,     3,   179,   159,    13,    14,    15,     3,
      19,    20,    21,    22,    23,    24,     9,    10,     3,    28,
      29,     9,    31,     8,    26,    13,    33,    34,   183,     9,
      10,   185,     3,     4,     5,     6,     7,     8,    26,   193,
      47,    48,   196,    14,    15,    54,   200,   201,    32,    46,
      64,    60,     8,    41,    63,    11,    65,    73,    65,    66,
      69,    58,    33,    34,   218,   219,   241,    75,    76,     9,
      10,    78,    80,    70,     9,    10,    47,    48,     9,    10,
     235,     3,    79,     9,    10,   239,     3,     4,     5,     6,
       7,     8,     5,     6,    65,    66,    65,    14,    15,     9,
      10,    47,    73,    85,   258,   259,     3,    78,    13,    14,
      15,    16,    17,    18,     9,    10,    33,    34,     9,    10,
     274,     3,     9,    28,     8,   280,     9,    10,     8,   283,
      47,    48,    19,    20,    21,    22,    23,    24,     8,   293,
       3,    28,     9,    10,    31,   299,    35,    13,    65,    66,
      16,    17,    18,   230,   231,   310,    21,    22,    23,    24,
       8,    78,    21,    22,    23,    24,    47,    54,    68,     3,
       9,     3,    84,    60,    10,     8,    63,    19,    65,    10,
       3,     8,    69,    19,    20,    21,    22,    23,    24,     3,
      68,     3,    28,     9,     9,    31,     3,    19,    20,    21,
      22,    23,    24,     8,    10,    51,    28,    18,    66,    31,
      19,    20,    21,    22,    23,    24,     3,     3,    54,    28,
       8,     3,    31,    44,    60,    10,     9,    63,    71,    65,
      32,    67,    54,    69,    52,    61,    61,     9,    60,     8,
       8,    63,     9,    65,    61,    54,     9,    69,    61,     5,
      10,    60,     5,    61,    63,    77,    65,     8,    82,     3,
      69,    19,    20,    21,    22,    23,    24,    66,    77,     8,
      28,    37,    30,    31,    19,    20,    21,    22,    23,    24,
      42,   225,    40,    28,    29,    95,    31,   107,    19,    20,
      21,    22,    23,    24,   232,   196,    54,    28,   256,    83,
      31,   194,    60,   221,   339,    63,    86,    65,    30,    54,
      47,    69,    -1,    -1,    -1,    60,    -1,    -1,    63,    -1,
      65,    -1,    -1,    54,    69,    -1,    -1,    -1,    -1,    60,
      -1,    -1,    63,    -1,    65,    -1,    -1,    -1,    69,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    28,    -1,
      -1,    31,    19,    20,    21,    22,    23,    24,    13,    14,
      15,    16,    17,    18,    31,    19,    20,    21,    22,    23,
      24,    -1,    -1,    28,    54,    -1,    -1,    31,    -1,    -1,
      60,    -1,    -1,    63,    -1,    65,    -1,    54,    -1,    -1,
      -1,    45,    -1,    60,    -1,    -1,    63,    -1,    65,    -1,
      54,    -1,    -1,    -1,    -1,    -1,    60,    -1,    -1,    63,
      19,    20,    21,    22,    23,    24,    13,    14,    15,    16,
      17,    18,    31,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    -1,    31,    45,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    60,    -1,    -1,    63,    -1,    -1,    -1,    54,    -1,
      -1,    -1,    -1,    -1,    60,    -1,    -1,    63
  };

  const unsigned char
  CSQLParser::yystos_[] =
  {
       0,    27,    36,    39,    42,    57,    73,    81,    88,    89,
      90,    91,    92,    93,    95,   115,   119,   122,   123,   129,
     130,   132,   137,    76,    75,    76,    80,   133,    49,    55,
      76,    59,    26,    41,    96,     3,     0,    12,    46,    58,
      70,    79,    94,   110,     3,    53,   140,    55,     3,    53,
     139,   139,     3,     3,     4,     5,     6,     7,     8,    13,
      14,    15,    33,    34,    47,    48,    65,    66,    78,    97,
      98,   142,   143,   144,   145,    74,    90,    32,    26,    95,
      64,   114,    25,    42,    65,     3,   140,    85,   107,    47,
       3,   138,     3,     8,   116,     8,    11,     3,    92,   142,
     143,   143,   142,   146,     8,     8,   142,    10,    49,   100,
       3,    19,    20,    21,    22,    23,    24,    28,    29,    31,
      54,    60,    63,    65,    69,    99,    13,    14,    15,    16,
      17,    18,     3,   120,   121,   111,   112,   142,   142,    35,
     131,   131,    47,     8,   138,   142,    68,     3,   141,    83,
      92,     9,    13,    96,     3,    13,    11,     9,     9,    84,
     147,   142,    92,    98,     3,     8,   101,   102,   106,   107,
     142,   142,   142,   142,   142,   142,   142,     3,   143,     8,
      65,    66,   142,    31,    54,    63,   142,   143,   143,   143,
     143,   143,   143,    19,    10,   107,    10,    30,    40,   113,
      10,    67,     3,   125,     3,   124,   125,    68,     3,     9,
      10,     8,   117,   118,     9,   142,   149,   142,    43,    84,
     148,    29,     9,    99,    92,    10,    37,    50,    56,    61,
      62,    72,   103,    51,   108,    28,    92,   149,    66,    45,
     143,     8,   142,   142,   121,   112,   142,   142,     3,   127,
       9,    10,     3,     8,     3,   149,    10,     9,    10,    77,
     142,   142,    44,   127,     9,   102,    61,    71,   104,    61,
     104,   104,   106,    32,    52,   109,   143,     9,     9,   142,
      28,    92,   149,    45,     8,   126,   125,     8,   141,     9,
     118,   142,   142,    77,     9,    99,    61,    61,    61,    68,
      82,   105,   149,   142,   143,     9,     9,   142,     5,   128,
      38,    65,    66,    80,   141,     9,   142,   142,     8,     9,
      10,   143,    66,     9,   141,     5,    82,   134,     9,     3,
       8,     5,     6,    15,   135,   136,     5,     6,     9,    10,
     136
  };

  const unsigned char
  CSQLParser::yyr1_[] =
  {
       0,    87,    88,    89,    89,    90,    90,    91,    91,    91,
      91,    91,    91,    91,    91,    91,    92,    93,    93,    94,
      94,    94,    94,    95,    96,    96,    96,    97,    97,    98,
      98,    98,    99,    99,    99,   100,   100,   101,   101,   102,
     102,   103,   103,   103,   103,   103,   103,   104,   104,   105,
     105,   105,   106,   106,   107,   107,   108,   108,   109,   109,
     110,   110,   111,   111,   112,   113,   113,   113,   114,   114,
     114,   114,   115,   115,   116,   116,   117,   117,   118,   119,
     120,   120,   121,   122,   123,   124,   124,   125,   126,   126,
     126,   126,   126,   127,   127,   128,   128,   129,   130,   130,
     131,   131,   132,   133,   133,   133,   134,   134,   134,   135,
     135,   136,   136,   136,   136,   137,   138,   138,   139,   139,
     140,   140,   141,   141,   142,   142,   142,   142,   142,   142,
     142,   142,   142,   142,   142,   142,   142,   142,   142,   142,
     142,   142,   142,   142,   142,   142,   143,   143,   143,   143,
     143,   143,   143,   143,   143,   144,   144,   144,   144,   144,
     144,   144,   144,   144,   144,   144,   144,   145,   145,   145,
     145,   145,   145,   146,   146,   147,   147,   148,   148,   149,
     149
  };

  const signed char
  CSQLParser::yyr2_[] =
  {
       0,     2,     1,     1,     3,     0,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     3,     1,
       2,     1,     1,     7,     0,     1,     1,     1,     3,     2,
       1,     3,     0,     2,     1,     0,     2,     1,     3,     1,
       4,     1,     2,     3,     3,     3,     2,     0,     1,     0,
       2,     4,     2,     4,     0,     2,     0,     3,     0,     2,
       0,     3,     1,     3,     2,     0,     1,     1,     0,     2,
       4,     4,     6,     5,     0,     3,     1,     3,     3,     5,
       1,     3,     3,     4,     7,     1,     3,     3,     0,     3,
       2,     2,     3,     1,     4,     1,     3,     4,     6,     6,
       0,     1,    11,     0,     1,     1,     0,     2,     5,     1,
       3,     1,     1,     2,     2,     9,     0,     1,     0,     2,
       0,     3,     1,     3,     1,     3,     3,     2,     3,     3,
       3,     3,     3,     3,     3,     4,     3,     5,     4,     6,
       5,     6,     5,     6,     5,     6,     1,     3,     3,     3,
       3,     3,     3,     2,     2,     1,     1,     1,     3,     3,
       4,     5,     3,     3,     4,     5,     6,     1,     1,     1,
       1,     1,     1,     0,     1,     4,     5,     0,     2,     1,
       3
  };




#if YYDEBUG
  const short
  CSQLParser::yyrline_[] =
  {
       0,   285,   285,   289,   290,   294,   295,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   314,   324,   330,   340,
     341,   342,   343,   347,   359,   360,   361,   365,   366,   370,
     371,   372,   376,   377,   378,   382,   383,   387,   388,   393,
     394,   412,   413,   414,   415,   416,   417,   421,   422,   426,
     427,   428,   432,   433,   437,   438,   442,   443,   447,   448,
     452,   453,   457,   458,   462,   466,   467,   468,   473,   474,
     475,   476,   483,   491,   502,   503,   507,   508,   512,   516,
     527,   528,   532,   536,   549,   560,   561,   565,   575,   576,
     577,   578,   579,   583,   584,   588,   589,   593,   603,   611,
     622,   623,   627,   642,   643,   644,   648,   649,   650,   654,
     655,   659,   660,   661,   662,   666,   678,   679,   683,   684,
     688,   689,   693,   694,   701,   702,   703,   704,   705,   706,
     707,   708,   709,   710,   711,   712,   713,   714,   715,   716,
     717,   719,   721,   722,   723,   724,   728,   729,   730,   731,
     732,   733,   734,   735,   736,   740,   741,   742,   743,   744,
     750,   757,   765,   766,   767,   768,   776,   780,   781,   782,
     783,   784,   785,   789,   790,   794,   795,   799,   800,   804,
     805
  };

  void
  CSQLParser::yy_stack_print_ () const
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << int (i->state);
    *yycdebug_ << '\n';
  }

  void
  CSQLParser::yy_reduce_print_ (int yyrule) const
  {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG


} } // bptreedb::sql



void bptreedb::sql::CSQLParser::error(const location_type& loc, const std::string& sMessage)
{
	driver.SetError((uint32_t)loc.begin.line, (uint32_t)loc.begin.column, sMessage);
}
