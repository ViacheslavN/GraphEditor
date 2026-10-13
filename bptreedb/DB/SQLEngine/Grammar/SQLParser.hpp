// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton interface for Bison LALR(1) parsers in C++

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


/**
 ** \file SQLParser.hpp
 ** Define the bptreedb::sql::parser class.
 */

// C++ LALR(1) parser skeleton written by Akim Demaille.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

#ifndef YY_YY_SQLPARSER_HPP_INCLUDED
# define YY_YY_SQLPARSER_HPP_INCLUDED
// "%code requires" blocks.

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


# include <cassert>
# include <cstdlib> // std::abort
# include <iostream>
# include <stdexcept>
# include <string>
# include <vector>

#if defined __cplusplus
# define YY_CPLUSPLUS __cplusplus
#else
# define YY_CPLUSPLUS 199711L
#endif

// Support move semantics when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_MOVE           std::move
# define YY_MOVE_OR_COPY   move
# define YY_MOVE_REF(Type) Type&&
# define YY_RVREF(Type)    Type&&
# define YY_COPY(Type)     Type
#else
# define YY_MOVE
# define YY_MOVE_OR_COPY   copy
# define YY_MOVE_REF(Type) Type&
# define YY_RVREF(Type)    const Type&
# define YY_COPY(Type)     const Type&
#endif

// Support noexcept when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_NOEXCEPT noexcept
# define YY_NOTHROW
#else
# define YY_NOEXCEPT
# define YY_NOTHROW throw ()
#endif

// Support constexpr when possible.
#if 201703 <= YY_CPLUSPLUS
# define YY_CONSTEXPR constexpr
#else
# define YY_CONSTEXPR
#endif

#include <typeinfo>
#ifndef YY_ASSERT
# include <cassert>
# define YY_ASSERT assert
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

namespace bptreedb { namespace sql {


  /// A point in a source file.
  class position
  {
  public:
    /// Type for file name.
    typedef const std::string filename_type;
    /// Type for line and column numbers.
    typedef int counter_type;

    /// Construct a position.
    explicit position (filename_type* f = YY_NULLPTR,
                       counter_type l = 1,
                       counter_type c = 1)
      : filename (f)
      , line (l)
      , column (c)
    {}


    /// Initialization.
    void initialize (filename_type* fn = YY_NULLPTR,
                     counter_type l = 1,
                     counter_type c = 1)
    {
      filename = fn;
      line = l;
      column = c;
    }

    /** \name Line and Column related manipulators
     ** \{ */
    /// (line related) Advance to the COUNT next lines.
    void lines (counter_type count = 1)
    {
      if (count)
        {
          column = 1;
          line = add_ (line, count, 1);
        }
    }

    /// (column related) Advance to the COUNT next columns.
    void columns (counter_type count = 1)
    {
      column = add_ (column, count, 1);
    }
    /** \} */

    /// File name to which this position refers.
    filename_type* filename;
    /// Current line number.
    counter_type line;
    /// Current column number.
    counter_type column;

  private:
    /// Compute max (min, lhs+rhs).
    static counter_type add_ (counter_type lhs, counter_type rhs, counter_type min)
    {
      return lhs + rhs < min ? min : lhs + rhs;
    }
  };

  /// Add \a width columns, in place.
  inline position&
  operator+= (position& res, position::counter_type width)
  {
    res.columns (width);
    return res;
  }

  /// Add \a width columns.
  inline position
  operator+ (position res, position::counter_type width)
  {
    return res += width;
  }

  /// Subtract \a width columns, in place.
  inline position&
  operator-= (position& res, position::counter_type width)
  {
    return res += -width;
  }

  /// Subtract \a width columns.
  inline position
  operator- (position res, position::counter_type width)
  {
    return res -= width;
  }

  /** \brief Intercept output stream redirection.
   ** \param ostr the destination output stream
   ** \param pos a reference to the position to redirect
   */
  template <typename YYChar>
  std::basic_ostream<YYChar>&
  operator<< (std::basic_ostream<YYChar>& ostr, const position& pos)
  {
    if (pos.filename)
      ostr << *pos.filename << ':';
    return ostr << pos.line << '.' << pos.column;
  }

  /// Two points in a source file.
  class location
  {
  public:
    /// Type for file name.
    typedef position::filename_type filename_type;
    /// Type for line and column numbers.
    typedef position::counter_type counter_type;

    /// Construct a location from \a b to \a e.
    location (const position& b, const position& e)
      : begin (b)
      , end (e)
    {}

    /// Construct a 0-width location in \a p.
    explicit location (const position& p = position ())
      : begin (p)
      , end (p)
    {}

    /// Construct a 0-width location in \a f, \a l, \a c.
    explicit location (filename_type* f,
                       counter_type l = 1,
                       counter_type c = 1)
      : begin (f, l, c)
      , end (f, l, c)
    {}


    /// Initialization.
    void initialize (filename_type* f = YY_NULLPTR,
                     counter_type l = 1,
                     counter_type c = 1)
    {
      begin.initialize (f, l, c);
      end = begin;
    }

    /** \name Line and Column related manipulators
     ** \{ */
  public:
    /// Reset initial location to final location.
    void step ()
    {
      begin = end;
    }

    /// Extend the current location to the COUNT next columns.
    void columns (counter_type count = 1)
    {
      end += count;
    }

    /// Extend the current location to the COUNT next lines.
    void lines (counter_type count = 1)
    {
      end.lines (count);
    }
    /** \} */


  public:
    /// Beginning of the located region.
    position begin;
    /// End of the located region.
    position end;
  };

  /// Join two locations, in place.
  inline location&
  operator+= (location& res, const location& end)
  {
    res.end = end.end;
    return res;
  }

  /// Join two locations.
  inline location
  operator+ (location res, const location& end)
  {
    return res += end;
  }

  /// Add \a width columns to the end position, in place.
  inline location&
  operator+= (location& res, location::counter_type width)
  {
    res.columns (width);
    return res;
  }

  /// Add \a width columns to the end position.
  inline location
  operator+ (location res, location::counter_type width)
  {
    return res += width;
  }

  /// Subtract \a width columns to the end position, in place.
  inline location&
  operator-= (location& res, location::counter_type width)
  {
    return res += -width;
  }

  /// Subtract \a width columns to the end position.
  inline location
  operator- (location res, location::counter_type width)
  {
    return res -= width;
  }

  /** \brief Intercept output stream redirection.
   ** \param ostr the destination output stream
   ** \param loc a reference to the location to redirect
   **
   ** Avoid duplicate information.
   */
  template <typename YYChar>
  std::basic_ostream<YYChar>&
  operator<< (std::basic_ostream<YYChar>& ostr, const location& loc)
  {
    location::counter_type end_col
      = 0 < loc.end.column ? loc.end.column - 1 : 0;
    ostr << loc.begin;
    if (loc.end.filename
        && (!loc.begin.filename
            || *loc.begin.filename != *loc.end.filename))
      ostr << '-' << loc.end.filename << ':' << loc.end.line << '.' << end_col;
    else if (loc.begin.line < loc.end.line)
      ostr << '-' << loc.end.line << '.' << end_col;
    else if (loc.begin.column < end_col)
      ostr << '-' << end_col;
    return ostr;
  }


  /// A Bison parser.
  class CSQLParser
  {
  public:
#ifdef YYSTYPE
# ifdef __GNUC__
#  pragma GCC message "bison: do not #define YYSTYPE in C++, use %define api.value.type"
# endif
    typedef YYSTYPE value_type;
#else
  /// A buffer to store and retrieve objects.
  ///
  /// Sort of a variant, but does not keep track of the nature
  /// of the stored data, since that knowledge is available
  /// via the current parser state.
  class value_type
  {
  public:
    /// Type of *this.
    typedef value_type self_type;

    /// Empty construction.
    value_type () YY_NOEXCEPT
      : yyraw_ ()
      , yytypeid_ (YY_NULLPTR)
    {}

    /// Construct and fill.
    template <typename T>
    value_type (YY_RVREF (T) t)
      : yytypeid_ (&typeid (T))
    {
      YY_ASSERT (sizeof (T) <= size);
      new (yyas_<T> ()) T (YY_MOVE (t));
    }

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    value_type (const self_type&) = delete;
    /// Non copyable.
    self_type& operator= (const self_type&) = delete;
#endif

    /// Destruction, allowed only if empty.
    ~value_type () YY_NOEXCEPT
    {
      YY_ASSERT (!yytypeid_);
    }

# if 201103L <= YY_CPLUSPLUS
    /// Instantiate a \a T in here from \a t.
    template <typename T, typename... U>
    T&
    emplace (U&&... u)
    {
      YY_ASSERT (!yytypeid_);
      YY_ASSERT (sizeof (T) <= size);
      yytypeid_ = & typeid (T);
      return *new (yyas_<T> ()) T (std::forward <U>(u)...);
    }
# else
    /// Instantiate an empty \a T in here.
    template <typename T>
    T&
    emplace ()
    {
      YY_ASSERT (!yytypeid_);
      YY_ASSERT (sizeof (T) <= size);
      yytypeid_ = & typeid (T);
      return *new (yyas_<T> ()) T ();
    }

    /// Instantiate a \a T in here from \a t.
    template <typename T>
    T&
    emplace (const T& t)
    {
      YY_ASSERT (!yytypeid_);
      YY_ASSERT (sizeof (T) <= size);
      yytypeid_ = & typeid (T);
      return *new (yyas_<T> ()) T (t);
    }
# endif

    /// Instantiate an empty \a T in here.
    /// Obsolete, use emplace.
    template <typename T>
    T&
    build ()
    {
      return emplace<T> ();
    }

    /// Instantiate a \a T in here from \a t.
    /// Obsolete, use emplace.
    template <typename T>
    T&
    build (const T& t)
    {
      return emplace<T> (t);
    }

    /// Accessor to a built \a T.
    template <typename T>
    T&
    as () YY_NOEXCEPT
    {
      YY_ASSERT (yytypeid_);
      YY_ASSERT (*yytypeid_ == typeid (T));
      YY_ASSERT (sizeof (T) <= size);
      return *yyas_<T> ();
    }

    /// Const accessor to a built \a T (for %printer).
    template <typename T>
    const T&
    as () const YY_NOEXCEPT
    {
      YY_ASSERT (yytypeid_);
      YY_ASSERT (*yytypeid_ == typeid (T));
      YY_ASSERT (sizeof (T) <= size);
      return *yyas_<T> ();
    }

    /// Swap the content with \a that, of same type.
    ///
    /// Both variants must be built beforehand, because swapping the actual
    /// data requires reading it (with as()), and this is not possible on
    /// unconstructed variants: it would require some dynamic testing, which
    /// should not be the variant's responsibility.
    /// Swapping between built and (possibly) non-built is done with
    /// self_type::move ().
    template <typename T>
    void
    swap (self_type& that) YY_NOEXCEPT
    {
      YY_ASSERT (yytypeid_);
      YY_ASSERT (*yytypeid_ == *that.yytypeid_);
      std::swap (as<T> (), that.as<T> ());
    }

    /// Move the content of \a that to this.
    ///
    /// Destroys \a that.
    template <typename T>
    void
    move (self_type& that)
    {
# if 201103L <= YY_CPLUSPLUS
      emplace<T> (std::move (that.as<T> ()));
# else
      emplace<T> ();
      swap<T> (that);
# endif
      that.destroy<T> ();
    }

# if 201103L <= YY_CPLUSPLUS
    /// Move the content of \a that to this.
    template <typename T>
    void
    move (self_type&& that)
    {
      emplace<T> (std::move (that.as<T> ()));
      that.destroy<T> ();
    }
#endif

    /// Copy the content of \a that to this.
    template <typename T>
    void
    copy (const self_type& that)
    {
      emplace<T> (that.as<T> ());
    }

    /// Destroy the stored \a T.
    template <typename T>
    void
    destroy ()
    {
      as<T> ().~T ();
      yytypeid_ = YY_NULLPTR;
    }

  private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    value_type (const self_type&);
    /// Non copyable.
    self_type& operator= (const self_type&);
#endif

    /// Accessor to raw memory as \a T.
    template <typename T>
    T*
    yyas_ () YY_NOEXCEPT
    {
      void *yyp = yyraw_;
      return static_cast<T*> (yyp);
     }

    /// Const accessor to raw memory as \a T.
    template <typename T>
    const T*
    yyas_ () const YY_NOEXCEPT
    {
      const void *yyp = yyraw_;
      return static_cast<const T*> (yyp);
     }

    /// An auxiliary type to compute the largest semantic type.
    union union_type
    {
      // column_def
      // column_constraints
      char dummy1[sizeof (SColumnDef)];

      // index_kind
      char dummy2[sizeof (SCreateIndexStmt::eKind)];

      // opt_where
      // opt_having
      // expr
      // b_expr
      // primary
      // literal
      // opt_case_operand
      // opt_else
      char dummy3[sizeof (SExprPtr)];

      // opt_index_method
      char dummy4[sizeof (SIndexMethod)];

      // join_spec
      char dummy5[sizeof (SJoinSpec)];

      // opt_limit
      char dummy6[sizeof (SLimit)];

      // order_item
      char dummy7[sizeof (SOrderItem)];

      // select_core
      char dummy8[sizeof (SSelectCore)];

      // select_item
      char dummy9[sizeof (SSelectItem)];

      // select_stmt
      // select_compound
      char dummy10[sizeof (SSelectStmtPtr)];

      // set_item
      char dummy11[sizeof (SSetClause)];

      // stmt
      // insert_stmt
      // update_stmt
      // delete_stmt
      // create_table_stmt
      // drop_table_stmt
      // alter_table_stmt
      // create_index_stmt
      // drop_index_stmt
      char dummy12[sizeof (SStatementPtr)];

      // table_ref
      // table_primary
      char dummy13[sizeof (STableRefPtr)];

      // type_name
      char dummy14[sizeof (STypeName)];

      // opt_group_by
      // row
      // expr_list
      char dummy15[sizeof (TExprList)];

      // opt_column_list
      // name_list
      char dummy16[sizeof (TNameList)];

      // row_list
      char dummy17[sizeof (TRowList)];

      // opt_distinct
      // opt_desc
      // opt_if_exists
      // opt_if_not_exists
      char dummy18[sizeof (bool)];

      // "number"
      // number
      char dummy19[sizeof (double)];

      // compound_op
      char dummy20[sizeof (eCompoundOp)];

      // join_op
      char dummy21[sizeof (eJoinType)];

      // "integer"
      char dummy22[sizeof (int64_t)];

      // "identifier"
      // "string"
      // opt_alias
      // opt_name
      char dummy23[sizeof (std::string)];

      // column_defs
      char dummy24[sizeof (std::vector<SColumnDef>)];

      // opt_order_by
      // order_list
      char dummy25[sizeof (std::vector<SOrderItem>)];

      // select_list
      char dummy26[sizeof (std::vector<SSelectItem>)];

      // set_list
      char dummy27[sizeof (std::vector<SSetClause>)];

      // opt_from
      // from_list
      char dummy28[sizeof (std::vector<STableRefPtr>)];

      // when_list
      char dummy29[sizeof (std::vector<SWhenClause>)];

      // number_list
      char dummy30[sizeof (std::vector<double>)];

      // integer_list
      char dummy31[sizeof (std::vector<int64_t>)];

      // "?"
      char dummy32[sizeof (uint32_t)];
    };

    /// The size of the largest semantic type.
    enum { size = sizeof (union_type) };

    /// A buffer to store semantic values.
    union
    {
      /// Strongest alignment constraints.
      long double yyalign_me_;
      /// A buffer large enough to store any of the semantic values.
      char yyraw_[size];
    };

    /// Whether the content is built: if defined, the name of the stored type.
    const std::type_info *yytypeid_;
  };

#endif
    /// Backward compatibility (Bison 3.8).
    typedef value_type semantic_type;

    /// Symbol locations.
    typedef location location_type;

    /// Syntax errors thrown from user actions.
    struct syntax_error : std::runtime_error
    {
      syntax_error (const location_type& l, const std::string& m)
        : std::runtime_error (m)
        , location (l)
      {}

      syntax_error (const syntax_error& s)
        : std::runtime_error (s.what ())
        , location (s.location)
      {}

      ~syntax_error () YY_NOEXCEPT YY_NOTHROW;

      location_type location;
    };

    /// Token kinds.
    struct token
    {
      enum token_kind_type
      {
        TOK_YYEMPTY = -2,
    TOK_END_OF_INPUT = 0,          // "end of input"
    TOK_YYerror = 256,             // error
    TOK_YYUNDEF = 257,             // "invalid token"
    TOK_IDENT = 258,               // "identifier"
    TOK_STRING = 259,              // "string"
    TOK_INTEGER = 260,             // "integer"
    TOK_FLOAT = 261,               // "number"
    TOK_PARAM = 262,               // "?"
    TOK_LPAREN = 263,              // "("
    TOK_RPAREN = 264,              // ")"
    TOK_COMMA = 265,               // ","
    TOK_DOT = 266,                 // "."
    TOK_SEMICOLON = 267,           // ";"
    TOK_STAR = 268,                // "*"
    TOK_PLUS = 269,                // "+"
    TOK_MINUS = 270,               // "-"
    TOK_SLASH = 271,               // "/"
    TOK_PERCENT = 272,             // "%"
    TOK_CONCAT = 273,              // "||"
    TOK_EQ = 274,                  // "="
    TOK_NE = 275,                  // "<>"
    TOK_LT = 276,                  // "<"
    TOK_LE = 277,                  // "<="
    TOK_GT = 278,                  // ">"
    TOK_GE = 279,                  // ">="
    TOK_KW_ADD = 280,              // "ADD"
    TOK_KW_ALL = 281,              // "ALL"
    TOK_KW_ALTER = 282,            // "ALTER"
    TOK_KW_AND = 283,              // "AND"
    TOK_KW_AS = 284,               // "AS"
    TOK_KW_ASC = 285,              // "ASC"
    TOK_KW_BETWEEN = 286,          // "BETWEEN"
    TOK_KW_BY = 287,               // "BY"
    TOK_KW_CASE = 288,             // "CASE"
    TOK_KW_CAST = 289,             // "CAST"
    TOK_KW_COLUMN = 290,           // "COLUMN"
    TOK_KW_CREATE = 291,           // "CREATE"
    TOK_KW_CROSS = 292,            // "CROSS"
    TOK_KW_DEFAULT = 293,          // "DEFAULT"
    TOK_KW_DELETE = 294,           // "DELETE"
    TOK_KW_DESC = 295,             // "DESC"
    TOK_KW_DISTINCT = 296,         // "DISTINCT"
    TOK_KW_DROP = 297,             // "DROP"
    TOK_KW_ELSE = 298,             // "ELSE"
    TOK_KW_END = 299,              // "END"
    TOK_KW_ESCAPE = 300,           // "ESCAPE"
    TOK_KW_EXCEPT = 301,           // "EXCEPT"
    TOK_KW_EXISTS = 302,           // "EXISTS"
    TOK_KW_FALSE = 303,            // "FALSE"
    TOK_KW_FROM = 304,             // "FROM"
    TOK_KW_FULL = 305,             // "FULL"
    TOK_KW_GROUP = 306,            // "GROUP"
    TOK_KW_HAVING = 307,           // "HAVING"
    TOK_KW_IF = 308,               // "IF"
    TOK_KW_IN = 309,               // "IN"
    TOK_KW_INDEX = 310,            // "INDEX"
    TOK_KW_INNER = 311,            // "INNER"
    TOK_KW_INSERT = 312,           // "INSERT"
    TOK_KW_INTERSECT = 313,        // "INTERSECT"
    TOK_KW_INTO = 314,             // "INTO"
    TOK_KW_IS = 315,               // "IS"
    TOK_KW_JOIN = 316,             // "JOIN"
    TOK_KW_LEFT = 317,             // "LEFT"
    TOK_KW_LIKE = 318,             // "LIKE"
    TOK_KW_LIMIT = 319,            // "LIMIT"
    TOK_KW_NOT = 320,              // "NOT"
    TOK_KW_NULL = 321,             // "NULL"
    TOK_KW_OFFSET = 322,           // "OFFSET"
    TOK_KW_ON = 323,               // "ON"
    TOK_KW_OR = 324,               // "OR"
    TOK_KW_ORDER = 325,            // "ORDER"
    TOK_KW_OUTER = 326,            // "OUTER"
    TOK_KW_RIGHT = 327,            // "RIGHT"
    TOK_KW_SELECT = 328,           // "SELECT"
    TOK_KW_SET = 329,              // "SET"
    TOK_KW_SPATIAL = 330,          // "SPATIAL"
    TOK_KW_TABLE = 331,            // "TABLE"
    TOK_KW_THEN = 332,             // "THEN"
    TOK_KW_TRUE = 333,             // "TRUE"
    TOK_KW_UNION = 334,            // "UNION"
    TOK_KW_UNIQUE = 335,           // "UNIQUE"
    TOK_KW_UPDATE = 336,           // "UPDATE"
    TOK_KW_USING = 337,            // "USING"
    TOK_KW_VALUES = 338,           // "VALUES"
    TOK_KW_WHEN = 339,             // "WHEN"
    TOK_KW_WHERE = 340,            // "WHERE"
    TOK_UMINUS = 341               // UMINUS
      };
      /// Backward compatibility alias (Bison 3.6).
      typedef token_kind_type yytokentype;
    };

    /// Token kind, as returned by yylex.
    typedef token::token_kind_type token_kind_type;

    /// Backward compatibility alias (Bison 3.6).
    typedef token_kind_type token_type;

    /// Symbol kinds.
    struct symbol_kind
    {
      enum symbol_kind_type
      {
        YYNTOKENS = 87, ///< Number of tokens.
        S_YYEMPTY = -2,
        S_YYEOF = 0,                             // "end of input"
        S_YYerror = 1,                           // error
        S_YYUNDEF = 2,                           // "invalid token"
        S_IDENT = 3,                             // "identifier"
        S_STRING = 4,                            // "string"
        S_INTEGER = 5,                           // "integer"
        S_FLOAT = 6,                             // "number"
        S_PARAM = 7,                             // "?"
        S_LPAREN = 8,                            // "("
        S_RPAREN = 9,                            // ")"
        S_COMMA = 10,                            // ","
        S_DOT = 11,                              // "."
        S_SEMICOLON = 12,                        // ";"
        S_STAR = 13,                             // "*"
        S_PLUS = 14,                             // "+"
        S_MINUS = 15,                            // "-"
        S_SLASH = 16,                            // "/"
        S_PERCENT = 17,                          // "%"
        S_CONCAT = 18,                           // "||"
        S_EQ = 19,                               // "="
        S_NE = 20,                               // "<>"
        S_LT = 21,                               // "<"
        S_LE = 22,                               // "<="
        S_GT = 23,                               // ">"
        S_GE = 24,                               // ">="
        S_KW_ADD = 25,                           // "ADD"
        S_KW_ALL = 26,                           // "ALL"
        S_KW_ALTER = 27,                         // "ALTER"
        S_KW_AND = 28,                           // "AND"
        S_KW_AS = 29,                            // "AS"
        S_KW_ASC = 30,                           // "ASC"
        S_KW_BETWEEN = 31,                       // "BETWEEN"
        S_KW_BY = 32,                            // "BY"
        S_KW_CASE = 33,                          // "CASE"
        S_KW_CAST = 34,                          // "CAST"
        S_KW_COLUMN = 35,                        // "COLUMN"
        S_KW_CREATE = 36,                        // "CREATE"
        S_KW_CROSS = 37,                         // "CROSS"
        S_KW_DEFAULT = 38,                       // "DEFAULT"
        S_KW_DELETE = 39,                        // "DELETE"
        S_KW_DESC = 40,                          // "DESC"
        S_KW_DISTINCT = 41,                      // "DISTINCT"
        S_KW_DROP = 42,                          // "DROP"
        S_KW_ELSE = 43,                          // "ELSE"
        S_KW_END = 44,                           // "END"
        S_KW_ESCAPE = 45,                        // "ESCAPE"
        S_KW_EXCEPT = 46,                        // "EXCEPT"
        S_KW_EXISTS = 47,                        // "EXISTS"
        S_KW_FALSE = 48,                         // "FALSE"
        S_KW_FROM = 49,                          // "FROM"
        S_KW_FULL = 50,                          // "FULL"
        S_KW_GROUP = 51,                         // "GROUP"
        S_KW_HAVING = 52,                        // "HAVING"
        S_KW_IF = 53,                            // "IF"
        S_KW_IN = 54,                            // "IN"
        S_KW_INDEX = 55,                         // "INDEX"
        S_KW_INNER = 56,                         // "INNER"
        S_KW_INSERT = 57,                        // "INSERT"
        S_KW_INTERSECT = 58,                     // "INTERSECT"
        S_KW_INTO = 59,                          // "INTO"
        S_KW_IS = 60,                            // "IS"
        S_KW_JOIN = 61,                          // "JOIN"
        S_KW_LEFT = 62,                          // "LEFT"
        S_KW_LIKE = 63,                          // "LIKE"
        S_KW_LIMIT = 64,                         // "LIMIT"
        S_KW_NOT = 65,                           // "NOT"
        S_KW_NULL = 66,                          // "NULL"
        S_KW_OFFSET = 67,                        // "OFFSET"
        S_KW_ON = 68,                            // "ON"
        S_KW_OR = 69,                            // "OR"
        S_KW_ORDER = 70,                         // "ORDER"
        S_KW_OUTER = 71,                         // "OUTER"
        S_KW_RIGHT = 72,                         // "RIGHT"
        S_KW_SELECT = 73,                        // "SELECT"
        S_KW_SET = 74,                           // "SET"
        S_KW_SPATIAL = 75,                       // "SPATIAL"
        S_KW_TABLE = 76,                         // "TABLE"
        S_KW_THEN = 77,                          // "THEN"
        S_KW_TRUE = 78,                          // "TRUE"
        S_KW_UNION = 79,                         // "UNION"
        S_KW_UNIQUE = 80,                        // "UNIQUE"
        S_KW_UPDATE = 81,                        // "UPDATE"
        S_KW_USING = 82,                         // "USING"
        S_KW_VALUES = 83,                        // "VALUES"
        S_KW_WHEN = 84,                          // "WHEN"
        S_KW_WHERE = 85,                         // "WHERE"
        S_UMINUS = 86,                           // UMINUS
        S_YYACCEPT = 87,                         // $accept
        S_script = 88,                           // script
        S_stmt_seq = 89,                         // stmt_seq
        S_opt_stmt = 90,                         // opt_stmt
        S_stmt = 91,                             // stmt
        S_select_stmt = 92,                      // select_stmt
        S_select_compound = 93,                  // select_compound
        S_compound_op = 94,                      // compound_op
        S_select_core = 95,                      // select_core
        S_opt_distinct = 96,                     // opt_distinct
        S_select_list = 97,                      // select_list
        S_select_item = 98,                      // select_item
        S_opt_alias = 99,                        // opt_alias
        S_opt_from = 100,                        // opt_from
        S_from_list = 101,                       // from_list
        S_table_ref = 102,                       // table_ref
        S_join_op = 103,                         // join_op
        S_opt_outer = 104,                       // opt_outer
        S_join_spec = 105,                       // join_spec
        S_table_primary = 106,                   // table_primary
        S_opt_where = 107,                       // opt_where
        S_opt_group_by = 108,                    // opt_group_by
        S_opt_having = 109,                      // opt_having
        S_opt_order_by = 110,                    // opt_order_by
        S_order_list = 111,                      // order_list
        S_order_item = 112,                      // order_item
        S_opt_desc = 113,                        // opt_desc
        S_opt_limit = 114,                       // opt_limit
        S_insert_stmt = 115,                     // insert_stmt
        S_opt_column_list = 116,                 // opt_column_list
        S_row_list = 117,                        // row_list
        S_row = 118,                             // row
        S_update_stmt = 119,                     // update_stmt
        S_set_list = 120,                        // set_list
        S_set_item = 121,                        // set_item
        S_delete_stmt = 122,                     // delete_stmt
        S_create_table_stmt = 123,               // create_table_stmt
        S_column_defs = 124,                     // column_defs
        S_column_def = 125,                      // column_def
        S_column_constraints = 126,              // column_constraints
        S_type_name = 127,                       // type_name
        S_integer_list = 128,                    // integer_list
        S_drop_table_stmt = 129,                 // drop_table_stmt
        S_alter_table_stmt = 130,                // alter_table_stmt
        S_opt_column = 131,                      // opt_column
        S_create_index_stmt = 132,               // create_index_stmt
        S_index_kind = 133,                      // index_kind
        S_opt_index_method = 134,                // opt_index_method
        S_number_list = 135,                     // number_list
        S_number = 136,                          // number
        S_drop_index_stmt = 137,                 // drop_index_stmt
        S_opt_name = 138,                        // opt_name
        S_opt_if_exists = 139,                   // opt_if_exists
        S_opt_if_not_exists = 140,               // opt_if_not_exists
        S_name_list = 141,                       // name_list
        S_expr = 142,                            // expr
        S_b_expr = 143,                          // b_expr
        S_primary = 144,                         // primary
        S_literal = 145,                         // literal
        S_opt_case_operand = 146,                // opt_case_operand
        S_when_list = 147,                       // when_list
        S_opt_else = 148,                        // opt_else
        S_expr_list = 149                        // expr_list
      };
    };

    /// (Internal) symbol kind.
    typedef symbol_kind::symbol_kind_type symbol_kind_type;

    /// The number of tokens.
    static const symbol_kind_type YYNTOKENS = symbol_kind::YYNTOKENS;

    /// A complete symbol.
    ///
    /// Expects its Base type to provide access to the symbol kind
    /// via kind ().
    ///
    /// Provide access to semantic value and location.
    template <typename Base>
    struct basic_symbol : Base
    {
      /// Alias to Base.
      typedef Base super_type;

      /// Default constructor.
      basic_symbol () YY_NOEXCEPT
        : value ()
        , location ()
      {}

#if 201103L <= YY_CPLUSPLUS
      /// Move constructor.
      basic_symbol (basic_symbol&& that)
        : Base (std::move (that))
        , value ()
        , location (std::move (that.location))
      {
        switch (this->kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.move< SColumnDef > (std::move (that.value));
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.move< SCreateIndexStmt::eKind > (std::move (that.value));
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.move< SExprPtr > (std::move (that.value));
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.move< SIndexMethod > (std::move (that.value));
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.move< SJoinSpec > (std::move (that.value));
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.move< SLimit > (std::move (that.value));
        break;

      case symbol_kind::S_order_item: // order_item
        value.move< SOrderItem > (std::move (that.value));
        break;

      case symbol_kind::S_select_core: // select_core
        value.move< SSelectCore > (std::move (that.value));
        break;

      case symbol_kind::S_select_item: // select_item
        value.move< SSelectItem > (std::move (that.value));
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.move< SSelectStmtPtr > (std::move (that.value));
        break;

      case symbol_kind::S_set_item: // set_item
        value.move< SSetClause > (std::move (that.value));
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
        value.move< SStatementPtr > (std::move (that.value));
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.move< STableRefPtr > (std::move (that.value));
        break;

      case symbol_kind::S_type_name: // type_name
        value.move< STypeName > (std::move (that.value));
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.move< TExprList > (std::move (that.value));
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.move< TNameList > (std::move (that.value));
        break;

      case symbol_kind::S_row_list: // row_list
        value.move< TRowList > (std::move (that.value));
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.move< bool > (std::move (that.value));
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.move< double > (std::move (that.value));
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.move< eCompoundOp > (std::move (that.value));
        break;

      case symbol_kind::S_join_op: // join_op
        value.move< eJoinType > (std::move (that.value));
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.move< int64_t > (std::move (that.value));
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.move< std::string > (std::move (that.value));
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.move< std::vector<SColumnDef> > (std::move (that.value));
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.move< std::vector<SOrderItem> > (std::move (that.value));
        break;

      case symbol_kind::S_select_list: // select_list
        value.move< std::vector<SSelectItem> > (std::move (that.value));
        break;

      case symbol_kind::S_set_list: // set_list
        value.move< std::vector<SSetClause> > (std::move (that.value));
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.move< std::vector<STableRefPtr> > (std::move (that.value));
        break;

      case symbol_kind::S_when_list: // when_list
        value.move< std::vector<SWhenClause> > (std::move (that.value));
        break;

      case symbol_kind::S_number_list: // number_list
        value.move< std::vector<double> > (std::move (that.value));
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.move< std::vector<int64_t> > (std::move (that.value));
        break;

      case symbol_kind::S_PARAM: // "?"
        value.move< uint32_t > (std::move (that.value));
        break;

      default:
        break;
    }

      }
#endif

      /// Copy constructor.
      basic_symbol (const basic_symbol& that);

      /// Constructors for typed symbols.
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, location_type&& l)
        : Base (t)
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const location_type& l)
        : Base (t)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SColumnDef&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SColumnDef& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SCreateIndexStmt::eKind&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SCreateIndexStmt::eKind& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SExprPtr&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SExprPtr& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SIndexMethod&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SIndexMethod& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SJoinSpec&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SJoinSpec& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SLimit&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SLimit& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SOrderItem&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SOrderItem& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SSelectCore&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SSelectCore& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SSelectItem&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SSelectItem& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SSelectStmtPtr&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SSelectStmtPtr& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SSetClause&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SSetClause& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, SStatementPtr&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const SStatementPtr& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, STableRefPtr&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const STableRefPtr& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, STypeName&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const STypeName& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, TExprList&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const TExprList& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, TNameList&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const TNameList& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, TRowList&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const TRowList& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, bool&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const bool& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, double&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const double& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, eCompoundOp&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const eCompoundOp& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, eJoinType&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const eJoinType& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, int64_t&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const int64_t& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::string&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::string& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<SColumnDef>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<SColumnDef>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<SOrderItem>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<SOrderItem>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<SSelectItem>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<SSelectItem>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<SSetClause>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<SSetClause>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<STableRefPtr>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<STableRefPtr>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<SWhenClause>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<SWhenClause>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<double>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<double>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::vector<int64_t>&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::vector<int64_t>& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, uint32_t&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const uint32_t& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

      /// Destroy the symbol.
      ~basic_symbol ()
      {
        clear ();
      }



      /// Destroy contents, and record that is empty.
      void clear () YY_NOEXCEPT
      {
        // User destructor.
        symbol_kind_type yykind = this->kind ();
        basic_symbol<Base>& yysym = *this;
        (void) yysym;
        switch (yykind)
        {
       default:
          break;
        }

        // Value type destructor.
switch (yykind)
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.template destroy< SColumnDef > ();
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.template destroy< SCreateIndexStmt::eKind > ();
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.template destroy< SExprPtr > ();
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.template destroy< SIndexMethod > ();
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.template destroy< SJoinSpec > ();
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.template destroy< SLimit > ();
        break;

      case symbol_kind::S_order_item: // order_item
        value.template destroy< SOrderItem > ();
        break;

      case symbol_kind::S_select_core: // select_core
        value.template destroy< SSelectCore > ();
        break;

      case symbol_kind::S_select_item: // select_item
        value.template destroy< SSelectItem > ();
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.template destroy< SSelectStmtPtr > ();
        break;

      case symbol_kind::S_set_item: // set_item
        value.template destroy< SSetClause > ();
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
        value.template destroy< SStatementPtr > ();
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.template destroy< STableRefPtr > ();
        break;

      case symbol_kind::S_type_name: // type_name
        value.template destroy< STypeName > ();
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.template destroy< TExprList > ();
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.template destroy< TNameList > ();
        break;

      case symbol_kind::S_row_list: // row_list
        value.template destroy< TRowList > ();
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.template destroy< bool > ();
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.template destroy< double > ();
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.template destroy< eCompoundOp > ();
        break;

      case symbol_kind::S_join_op: // join_op
        value.template destroy< eJoinType > ();
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.template destroy< int64_t > ();
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.template destroy< std::string > ();
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.template destroy< std::vector<SColumnDef> > ();
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.template destroy< std::vector<SOrderItem> > ();
        break;

      case symbol_kind::S_select_list: // select_list
        value.template destroy< std::vector<SSelectItem> > ();
        break;

      case symbol_kind::S_set_list: // set_list
        value.template destroy< std::vector<SSetClause> > ();
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.template destroy< std::vector<STableRefPtr> > ();
        break;

      case symbol_kind::S_when_list: // when_list
        value.template destroy< std::vector<SWhenClause> > ();
        break;

      case symbol_kind::S_number_list: // number_list
        value.template destroy< std::vector<double> > ();
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.template destroy< std::vector<int64_t> > ();
        break;

      case symbol_kind::S_PARAM: // "?"
        value.template destroy< uint32_t > ();
        break;

      default:
        break;
    }

        Base::clear ();
      }

      /// The user-facing name of this symbol.
      const char *name () const YY_NOEXCEPT
      {
        return CSQLParser::symbol_name (this->kind ());
      }

      /// Backward compatibility (Bison 3.6).
      symbol_kind_type type_get () const YY_NOEXCEPT;

      /// Whether empty.
      bool empty () const YY_NOEXCEPT;

      /// Destructive move, \a s is emptied into this.
      void move (basic_symbol& s);

      /// The semantic value.
      value_type value;

      /// The location.
      location_type location;

    private:
#if YY_CPLUSPLUS < 201103L
      /// Assignment operator.
      basic_symbol& operator= (const basic_symbol& that);
#endif
    };

    /// Type access provider for token (enum) based symbols.
    struct by_kind
    {
      /// The symbol kind as needed by the constructor.
      typedef token_kind_type kind_type;

      /// Default constructor.
      by_kind () YY_NOEXCEPT;

#if 201103L <= YY_CPLUSPLUS
      /// Move constructor.
      by_kind (by_kind&& that) YY_NOEXCEPT;
#endif

      /// Copy constructor.
      by_kind (const by_kind& that) YY_NOEXCEPT;

      /// Constructor from (external) token numbers.
      by_kind (kind_type t) YY_NOEXCEPT;



      /// Record that this symbol is empty.
      void clear () YY_NOEXCEPT;

      /// Steal the symbol kind from \a that.
      void move (by_kind& that);

      /// The (internal) type number (corresponding to \a type).
      /// \a empty when empty.
      symbol_kind_type kind () const YY_NOEXCEPT;

      /// Backward compatibility (Bison 3.6).
      symbol_kind_type type_get () const YY_NOEXCEPT;

      /// The symbol kind.
      /// \a S_YYEMPTY when empty.
      symbol_kind_type kind_;
    };

    /// Backward compatibility for a private implementation detail (Bison 3.6).
    typedef by_kind by_type;

    /// "External" symbols: returned by the scanner.
    struct symbol_type : basic_symbol<by_kind>
    {
      /// Superclass.
      typedef basic_symbol<by_kind> super_type;

      /// Empty symbol.
      symbol_type () YY_NOEXCEPT {}

      /// Constructor for valueless symbols, and symbols from each type.
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, location_type l)
        : super_type (token_kind_type (tok), std::move (l))
#else
      symbol_type (int tok, const location_type& l)
        : super_type (token_kind_type (tok), l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT (tok == token::TOK_END_OF_INPUT
                   || (token::TOK_YYerror <= tok && tok <= token::TOK_YYUNDEF)
                   || (token::TOK_LPAREN <= tok && tok <= token::TOK_UMINUS));
#endif
      }
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, double v, location_type l)
        : super_type (token_kind_type (tok), std::move (v), std::move (l))
#else
      symbol_type (int tok, const double& v, const location_type& l)
        : super_type (token_kind_type (tok), v, l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT (tok == token::TOK_FLOAT);
#endif
      }
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, int64_t v, location_type l)
        : super_type (token_kind_type (tok), std::move (v), std::move (l))
#else
      symbol_type (int tok, const int64_t& v, const location_type& l)
        : super_type (token_kind_type (tok), v, l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT (tok == token::TOK_INTEGER);
#endif
      }
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, std::string v, location_type l)
        : super_type (token_kind_type (tok), std::move (v), std::move (l))
#else
      symbol_type (int tok, const std::string& v, const location_type& l)
        : super_type (token_kind_type (tok), v, l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT ((token::TOK_IDENT <= tok && tok <= token::TOK_STRING));
#endif
      }
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, uint32_t v, location_type l)
        : super_type (token_kind_type (tok), std::move (v), std::move (l))
#else
      symbol_type (int tok, const uint32_t& v, const location_type& l)
        : super_type (token_kind_type (tok), v, l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT (tok == token::TOK_PARAM);
#endif
      }
    };

    /// Build a parser object.
    CSQLParser (CSQLScanner& scanner_yyarg, CSQLDriver& driver_yyarg);
    virtual ~CSQLParser ();

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    CSQLParser (const CSQLParser&) = delete;
    /// Non copyable.
    CSQLParser& operator= (const CSQLParser&) = delete;
#endif

    /// Parse.  An alias for parse ().
    /// \returns  0 iff parsing succeeded.
    int operator() ();

    /// Parse.
    /// \returns  0 iff parsing succeeded.
    virtual int parse ();

#if YYDEBUG
    /// The current debugging stream.
    std::ostream& debug_stream () const YY_ATTRIBUTE_PURE;
    /// Set the current debugging stream.
    void set_debug_stream (std::ostream &);

    /// Type for debugging levels.
    typedef int debug_level_type;
    /// The current debugging level.
    debug_level_type debug_level () const YY_ATTRIBUTE_PURE;
    /// Set the current debugging level.
    void set_debug_level (debug_level_type l);
#endif

    /// Report a syntax error.
    /// \param loc    where the syntax error is found.
    /// \param msg    a description of the syntax error.
    virtual void error (const location_type& loc, const std::string& msg);

    /// Report a syntax error.
    void error (const syntax_error& err);

    /// The user-facing name of the symbol whose (internal) number is
    /// YYSYMBOL.  No bounds checking.
    static const char *symbol_name (symbol_kind_type yysymbol);

    // Implementation of make_symbol for each token kind.
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_END_OF_INPUT (location_type l)
      {
        return symbol_type (token::TOK_END_OF_INPUT, std::move (l));
      }
#else
      static
      symbol_type
      make_END_OF_INPUT (const location_type& l)
      {
        return symbol_type (token::TOK_END_OF_INPUT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_YYerror (location_type l)
      {
        return symbol_type (token::TOK_YYerror, std::move (l));
      }
#else
      static
      symbol_type
      make_YYerror (const location_type& l)
      {
        return symbol_type (token::TOK_YYerror, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_YYUNDEF (location_type l)
      {
        return symbol_type (token::TOK_YYUNDEF, std::move (l));
      }
#else
      static
      symbol_type
      make_YYUNDEF (const location_type& l)
      {
        return symbol_type (token::TOK_YYUNDEF, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_IDENT (std::string v, location_type l)
      {
        return symbol_type (token::TOK_IDENT, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_IDENT (const std::string& v, const location_type& l)
      {
        return symbol_type (token::TOK_IDENT, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_STRING (std::string v, location_type l)
      {
        return symbol_type (token::TOK_STRING, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_STRING (const std::string& v, const location_type& l)
      {
        return symbol_type (token::TOK_STRING, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_INTEGER (int64_t v, location_type l)
      {
        return symbol_type (token::TOK_INTEGER, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_INTEGER (const int64_t& v, const location_type& l)
      {
        return symbol_type (token::TOK_INTEGER, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_FLOAT (double v, location_type l)
      {
        return symbol_type (token::TOK_FLOAT, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_FLOAT (const double& v, const location_type& l)
      {
        return symbol_type (token::TOK_FLOAT, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_PARAM (uint32_t v, location_type l)
      {
        return symbol_type (token::TOK_PARAM, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_PARAM (const uint32_t& v, const location_type& l)
      {
        return symbol_type (token::TOK_PARAM, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_LPAREN (location_type l)
      {
        return symbol_type (token::TOK_LPAREN, std::move (l));
      }
#else
      static
      symbol_type
      make_LPAREN (const location_type& l)
      {
        return symbol_type (token::TOK_LPAREN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_RPAREN (location_type l)
      {
        return symbol_type (token::TOK_RPAREN, std::move (l));
      }
#else
      static
      symbol_type
      make_RPAREN (const location_type& l)
      {
        return symbol_type (token::TOK_RPAREN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_COMMA (location_type l)
      {
        return symbol_type (token::TOK_COMMA, std::move (l));
      }
#else
      static
      symbol_type
      make_COMMA (const location_type& l)
      {
        return symbol_type (token::TOK_COMMA, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_DOT (location_type l)
      {
        return symbol_type (token::TOK_DOT, std::move (l));
      }
#else
      static
      symbol_type
      make_DOT (const location_type& l)
      {
        return symbol_type (token::TOK_DOT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_SEMICOLON (location_type l)
      {
        return symbol_type (token::TOK_SEMICOLON, std::move (l));
      }
#else
      static
      symbol_type
      make_SEMICOLON (const location_type& l)
      {
        return symbol_type (token::TOK_SEMICOLON, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_STAR (location_type l)
      {
        return symbol_type (token::TOK_STAR, std::move (l));
      }
#else
      static
      symbol_type
      make_STAR (const location_type& l)
      {
        return symbol_type (token::TOK_STAR, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_PLUS (location_type l)
      {
        return symbol_type (token::TOK_PLUS, std::move (l));
      }
#else
      static
      symbol_type
      make_PLUS (const location_type& l)
      {
        return symbol_type (token::TOK_PLUS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_MINUS (location_type l)
      {
        return symbol_type (token::TOK_MINUS, std::move (l));
      }
#else
      static
      symbol_type
      make_MINUS (const location_type& l)
      {
        return symbol_type (token::TOK_MINUS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_SLASH (location_type l)
      {
        return symbol_type (token::TOK_SLASH, std::move (l));
      }
#else
      static
      symbol_type
      make_SLASH (const location_type& l)
      {
        return symbol_type (token::TOK_SLASH, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_PERCENT (location_type l)
      {
        return symbol_type (token::TOK_PERCENT, std::move (l));
      }
#else
      static
      symbol_type
      make_PERCENT (const location_type& l)
      {
        return symbol_type (token::TOK_PERCENT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_CONCAT (location_type l)
      {
        return symbol_type (token::TOK_CONCAT, std::move (l));
      }
#else
      static
      symbol_type
      make_CONCAT (const location_type& l)
      {
        return symbol_type (token::TOK_CONCAT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_EQ (location_type l)
      {
        return symbol_type (token::TOK_EQ, std::move (l));
      }
#else
      static
      symbol_type
      make_EQ (const location_type& l)
      {
        return symbol_type (token::TOK_EQ, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_NE (location_type l)
      {
        return symbol_type (token::TOK_NE, std::move (l));
      }
#else
      static
      symbol_type
      make_NE (const location_type& l)
      {
        return symbol_type (token::TOK_NE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_LT (location_type l)
      {
        return symbol_type (token::TOK_LT, std::move (l));
      }
#else
      static
      symbol_type
      make_LT (const location_type& l)
      {
        return symbol_type (token::TOK_LT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_LE (location_type l)
      {
        return symbol_type (token::TOK_LE, std::move (l));
      }
#else
      static
      symbol_type
      make_LE (const location_type& l)
      {
        return symbol_type (token::TOK_LE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_GT (location_type l)
      {
        return symbol_type (token::TOK_GT, std::move (l));
      }
#else
      static
      symbol_type
      make_GT (const location_type& l)
      {
        return symbol_type (token::TOK_GT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_GE (location_type l)
      {
        return symbol_type (token::TOK_GE, std::move (l));
      }
#else
      static
      symbol_type
      make_GE (const location_type& l)
      {
        return symbol_type (token::TOK_GE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ADD (location_type l)
      {
        return symbol_type (token::TOK_KW_ADD, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ADD (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ADD, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ALL (location_type l)
      {
        return symbol_type (token::TOK_KW_ALL, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ALL (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ALL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ALTER (location_type l)
      {
        return symbol_type (token::TOK_KW_ALTER, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ALTER (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ALTER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_AND (location_type l)
      {
        return symbol_type (token::TOK_KW_AND, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_AND (const location_type& l)
      {
        return symbol_type (token::TOK_KW_AND, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_AS (location_type l)
      {
        return symbol_type (token::TOK_KW_AS, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_AS (const location_type& l)
      {
        return symbol_type (token::TOK_KW_AS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ASC (location_type l)
      {
        return symbol_type (token::TOK_KW_ASC, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ASC (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ASC, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_BETWEEN (location_type l)
      {
        return symbol_type (token::TOK_KW_BETWEEN, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_BETWEEN (const location_type& l)
      {
        return symbol_type (token::TOK_KW_BETWEEN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_BY (location_type l)
      {
        return symbol_type (token::TOK_KW_BY, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_BY (const location_type& l)
      {
        return symbol_type (token::TOK_KW_BY, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_CASE (location_type l)
      {
        return symbol_type (token::TOK_KW_CASE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_CASE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_CASE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_CAST (location_type l)
      {
        return symbol_type (token::TOK_KW_CAST, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_CAST (const location_type& l)
      {
        return symbol_type (token::TOK_KW_CAST, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_COLUMN (location_type l)
      {
        return symbol_type (token::TOK_KW_COLUMN, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_COLUMN (const location_type& l)
      {
        return symbol_type (token::TOK_KW_COLUMN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_CREATE (location_type l)
      {
        return symbol_type (token::TOK_KW_CREATE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_CREATE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_CREATE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_CROSS (location_type l)
      {
        return symbol_type (token::TOK_KW_CROSS, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_CROSS (const location_type& l)
      {
        return symbol_type (token::TOK_KW_CROSS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_DEFAULT (location_type l)
      {
        return symbol_type (token::TOK_KW_DEFAULT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_DEFAULT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_DEFAULT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_DELETE (location_type l)
      {
        return symbol_type (token::TOK_KW_DELETE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_DELETE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_DELETE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_DESC (location_type l)
      {
        return symbol_type (token::TOK_KW_DESC, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_DESC (const location_type& l)
      {
        return symbol_type (token::TOK_KW_DESC, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_DISTINCT (location_type l)
      {
        return symbol_type (token::TOK_KW_DISTINCT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_DISTINCT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_DISTINCT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_DROP (location_type l)
      {
        return symbol_type (token::TOK_KW_DROP, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_DROP (const location_type& l)
      {
        return symbol_type (token::TOK_KW_DROP, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ELSE (location_type l)
      {
        return symbol_type (token::TOK_KW_ELSE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ELSE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ELSE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_END (location_type l)
      {
        return symbol_type (token::TOK_KW_END, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_END (const location_type& l)
      {
        return symbol_type (token::TOK_KW_END, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ESCAPE (location_type l)
      {
        return symbol_type (token::TOK_KW_ESCAPE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ESCAPE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ESCAPE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_EXCEPT (location_type l)
      {
        return symbol_type (token::TOK_KW_EXCEPT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_EXCEPT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_EXCEPT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_EXISTS (location_type l)
      {
        return symbol_type (token::TOK_KW_EXISTS, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_EXISTS (const location_type& l)
      {
        return symbol_type (token::TOK_KW_EXISTS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_FALSE (location_type l)
      {
        return symbol_type (token::TOK_KW_FALSE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_FALSE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_FALSE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_FROM (location_type l)
      {
        return symbol_type (token::TOK_KW_FROM, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_FROM (const location_type& l)
      {
        return symbol_type (token::TOK_KW_FROM, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_FULL (location_type l)
      {
        return symbol_type (token::TOK_KW_FULL, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_FULL (const location_type& l)
      {
        return symbol_type (token::TOK_KW_FULL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_GROUP (location_type l)
      {
        return symbol_type (token::TOK_KW_GROUP, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_GROUP (const location_type& l)
      {
        return symbol_type (token::TOK_KW_GROUP, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_HAVING (location_type l)
      {
        return symbol_type (token::TOK_KW_HAVING, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_HAVING (const location_type& l)
      {
        return symbol_type (token::TOK_KW_HAVING, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_IF (location_type l)
      {
        return symbol_type (token::TOK_KW_IF, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_IF (const location_type& l)
      {
        return symbol_type (token::TOK_KW_IF, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_IN (location_type l)
      {
        return symbol_type (token::TOK_KW_IN, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_IN (const location_type& l)
      {
        return symbol_type (token::TOK_KW_IN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_INDEX (location_type l)
      {
        return symbol_type (token::TOK_KW_INDEX, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_INDEX (const location_type& l)
      {
        return symbol_type (token::TOK_KW_INDEX, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_INNER (location_type l)
      {
        return symbol_type (token::TOK_KW_INNER, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_INNER (const location_type& l)
      {
        return symbol_type (token::TOK_KW_INNER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_INSERT (location_type l)
      {
        return symbol_type (token::TOK_KW_INSERT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_INSERT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_INSERT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_INTERSECT (location_type l)
      {
        return symbol_type (token::TOK_KW_INTERSECT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_INTERSECT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_INTERSECT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_INTO (location_type l)
      {
        return symbol_type (token::TOK_KW_INTO, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_INTO (const location_type& l)
      {
        return symbol_type (token::TOK_KW_INTO, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_IS (location_type l)
      {
        return symbol_type (token::TOK_KW_IS, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_IS (const location_type& l)
      {
        return symbol_type (token::TOK_KW_IS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_JOIN (location_type l)
      {
        return symbol_type (token::TOK_KW_JOIN, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_JOIN (const location_type& l)
      {
        return symbol_type (token::TOK_KW_JOIN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_LEFT (location_type l)
      {
        return symbol_type (token::TOK_KW_LEFT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_LEFT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_LEFT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_LIKE (location_type l)
      {
        return symbol_type (token::TOK_KW_LIKE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_LIKE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_LIKE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_LIMIT (location_type l)
      {
        return symbol_type (token::TOK_KW_LIMIT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_LIMIT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_LIMIT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_NOT (location_type l)
      {
        return symbol_type (token::TOK_KW_NOT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_NOT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_NOT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_NULL (location_type l)
      {
        return symbol_type (token::TOK_KW_NULL, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_NULL (const location_type& l)
      {
        return symbol_type (token::TOK_KW_NULL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_OFFSET (location_type l)
      {
        return symbol_type (token::TOK_KW_OFFSET, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_OFFSET (const location_type& l)
      {
        return symbol_type (token::TOK_KW_OFFSET, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ON (location_type l)
      {
        return symbol_type (token::TOK_KW_ON, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ON (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ON, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_OR (location_type l)
      {
        return symbol_type (token::TOK_KW_OR, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_OR (const location_type& l)
      {
        return symbol_type (token::TOK_KW_OR, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_ORDER (location_type l)
      {
        return symbol_type (token::TOK_KW_ORDER, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_ORDER (const location_type& l)
      {
        return symbol_type (token::TOK_KW_ORDER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_OUTER (location_type l)
      {
        return symbol_type (token::TOK_KW_OUTER, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_OUTER (const location_type& l)
      {
        return symbol_type (token::TOK_KW_OUTER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_RIGHT (location_type l)
      {
        return symbol_type (token::TOK_KW_RIGHT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_RIGHT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_RIGHT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_SELECT (location_type l)
      {
        return symbol_type (token::TOK_KW_SELECT, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_SELECT (const location_type& l)
      {
        return symbol_type (token::TOK_KW_SELECT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_SET (location_type l)
      {
        return symbol_type (token::TOK_KW_SET, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_SET (const location_type& l)
      {
        return symbol_type (token::TOK_KW_SET, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_SPATIAL (location_type l)
      {
        return symbol_type (token::TOK_KW_SPATIAL, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_SPATIAL (const location_type& l)
      {
        return symbol_type (token::TOK_KW_SPATIAL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_TABLE (location_type l)
      {
        return symbol_type (token::TOK_KW_TABLE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_TABLE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_TABLE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_THEN (location_type l)
      {
        return symbol_type (token::TOK_KW_THEN, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_THEN (const location_type& l)
      {
        return symbol_type (token::TOK_KW_THEN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_TRUE (location_type l)
      {
        return symbol_type (token::TOK_KW_TRUE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_TRUE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_TRUE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_UNION (location_type l)
      {
        return symbol_type (token::TOK_KW_UNION, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_UNION (const location_type& l)
      {
        return symbol_type (token::TOK_KW_UNION, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_UNIQUE (location_type l)
      {
        return symbol_type (token::TOK_KW_UNIQUE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_UNIQUE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_UNIQUE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_UPDATE (location_type l)
      {
        return symbol_type (token::TOK_KW_UPDATE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_UPDATE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_UPDATE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_USING (location_type l)
      {
        return symbol_type (token::TOK_KW_USING, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_USING (const location_type& l)
      {
        return symbol_type (token::TOK_KW_USING, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_VALUES (location_type l)
      {
        return symbol_type (token::TOK_KW_VALUES, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_VALUES (const location_type& l)
      {
        return symbol_type (token::TOK_KW_VALUES, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_WHEN (location_type l)
      {
        return symbol_type (token::TOK_KW_WHEN, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_WHEN (const location_type& l)
      {
        return symbol_type (token::TOK_KW_WHEN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_KW_WHERE (location_type l)
      {
        return symbol_type (token::TOK_KW_WHERE, std::move (l));
      }
#else
      static
      symbol_type
      make_KW_WHERE (const location_type& l)
      {
        return symbol_type (token::TOK_KW_WHERE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_UMINUS (location_type l)
      {
        return symbol_type (token::TOK_UMINUS, std::move (l));
      }
#else
      static
      symbol_type
      make_UMINUS (const location_type& l)
      {
        return symbol_type (token::TOK_UMINUS, l);
      }
#endif


    class context
    {
    public:
      context (const CSQLParser& yyparser, const symbol_type& yyla);
      const symbol_type& lookahead () const YY_NOEXCEPT { return yyla_; }
      symbol_kind_type token () const YY_NOEXCEPT { return yyla_.kind (); }
      const location_type& location () const YY_NOEXCEPT { return yyla_.location; }

      /// Put in YYARG at most YYARGN of the expected tokens, and return the
      /// number of tokens stored in YYARG.  If YYARG is null, return the
      /// number of expected tokens (guaranteed to be less than YYNTOKENS).
      int expected_tokens (symbol_kind_type yyarg[], int yyargn) const;

    private:
      const CSQLParser& yyparser_;
      const symbol_type& yyla_;
    };

  private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    CSQLParser (const CSQLParser&);
    /// Non copyable.
    CSQLParser& operator= (const CSQLParser&);
#endif


    /// Stored state numbers (used for stacks).
    typedef short state_type;

    /// The arguments of the error message.
    int yy_syntax_error_arguments_ (const context& yyctx,
                                    symbol_kind_type yyarg[], int yyargn) const;

    /// Generate an error message.
    /// \param yyctx     the context in which the error occurred.
    virtual std::string yysyntax_error_ (const context& yyctx) const;
    /// Compute post-reduction state.
    /// \param yystate   the current state
    /// \param yysym     the nonterminal to push on the stack
    static state_type yy_lr_goto_state_ (state_type yystate, int yysym);

    /// Whether the given \c yypact_ value indicates a defaulted state.
    /// \param yyvalue   the value to check
    static bool yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT;

    /// Whether the given \c yytable_ value indicates a syntax error.
    /// \param yyvalue   the value to check
    static bool yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT;

    static const short yypact_ninf_;
    static const signed char yytable_ninf_;

    /// Convert a scanner token kind \a t to a symbol kind.
    /// In theory \a t should be a token_kind_type, but character literals
    /// are valid, yet not members of the token_kind_type enum.
    static symbol_kind_type yytranslate_ (int t) YY_NOEXCEPT;



    // Tables.
    // YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
    // STATE-NUM.
    static const short yypact_[];

    // YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
    // Performed when YYTABLE does not specify something else to do.  Zero
    // means the default is an error.
    static const unsigned char yydefact_[];

    // YYPGOTO[NTERM-NUM].
    static const short yypgoto_[];

    // YYDEFGOTO[NTERM-NUM].
    static const short yydefgoto_[];

    // YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
    // positive, shift that token.  If negative, reduce the rule whose
    // number is the opposite.  If YYTABLE_NINF, syntax error.
    static const short yytable_[];

    static const short yycheck_[];

    // YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
    // state STATE-NUM.
    static const unsigned char yystos_[];

    // YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.
    static const unsigned char yyr1_[];

    // YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.
    static const signed char yyr2_[];


#if YYDEBUG
    // YYRLINE[YYN] -- Source line where rule number YYN was defined.
    static const short yyrline_[];
    /// Report on the debug stream that the rule \a r is going to be reduced.
    virtual void yy_reduce_print_ (int r) const;
    /// Print the state stack on the debug stream.
    virtual void yy_stack_print_ () const;

    /// Debugging level.
    int yydebug_;
    /// Debug stream.
    std::ostream* yycdebug_;

    /// \brief Display a symbol kind, value and location.
    /// \param yyo    The output stream.
    /// \param yysym  The symbol.
    template <typename Base>
    void yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const;
#endif

    /// \brief Reclaim the memory associated to a symbol.
    /// \param yymsg     Why this token is reclaimed.
    ///                  If null, print nothing.
    /// \param yysym     The symbol.
    template <typename Base>
    void yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const;

  private:
    /// Type access provider for state based symbols.
    struct by_state
    {
      /// Default constructor.
      by_state () YY_NOEXCEPT;

      /// The symbol kind as needed by the constructor.
      typedef state_type kind_type;

      /// Constructor.
      by_state (kind_type s) YY_NOEXCEPT;

      /// Copy constructor.
      by_state (const by_state& that) YY_NOEXCEPT;

      /// Record that this symbol is empty.
      void clear () YY_NOEXCEPT;

      /// Steal the symbol kind from \a that.
      void move (by_state& that);

      /// The symbol kind (corresponding to \a state).
      /// \a symbol_kind::S_YYEMPTY when empty.
      symbol_kind_type kind () const YY_NOEXCEPT;

      /// The state number used to denote an empty symbol.
      /// We use the initial state, as it does not have a value.
      enum { empty_state = 0 };

      /// The state.
      /// \a empty when empty.
      state_type state;
    };

    /// "Internal" symbol: element of the stack.
    struct stack_symbol_type : basic_symbol<by_state>
    {
      /// Superclass.
      typedef basic_symbol<by_state> super_type;
      /// Construct an empty symbol.
      stack_symbol_type ();
      /// Move or copy construction.
      stack_symbol_type (YY_RVREF (stack_symbol_type) that);
      /// Steal the contents from \a sym to build this.
      stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) sym);
#if YY_CPLUSPLUS < 201103L
      /// Assignment, needed by push_back by some old implementations.
      /// Moves the contents of that.
      stack_symbol_type& operator= (stack_symbol_type& that);

      /// Assignment, needed by push_back by other implementations.
      /// Needed by some other old implementations.
      stack_symbol_type& operator= (const stack_symbol_type& that);
#endif
    };

    /// A stack with random access from its top.
    template <typename T, typename S = std::vector<T> >
    class stack
    {
    public:
      // Hide our reversed order.
      typedef typename S::iterator iterator;
      typedef typename S::const_iterator const_iterator;
      typedef typename S::size_type size_type;
      typedef typename std::ptrdiff_t index_type;

      stack (size_type n = 200) YY_NOEXCEPT
        : seq_ (n)
      {}

#if 201103L <= YY_CPLUSPLUS
      /// Non copyable.
      stack (const stack&) = delete;
      /// Non copyable.
      stack& operator= (const stack&) = delete;
#endif

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      const T&
      operator[] (index_type i) const
      {
        return seq_[size_type (size () - 1 - i)];
      }

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      T&
      operator[] (index_type i)
      {
        return seq_[size_type (size () - 1 - i)];
      }

      /// Steal the contents of \a t.
      ///
      /// Close to move-semantics.
      void
      push (YY_MOVE_REF (T) t)
      {
        seq_.push_back (T ());
        operator[] (0).move (t);
      }

      /// Pop elements from the stack.
      void
      pop (std::ptrdiff_t n = 1) YY_NOEXCEPT
      {
        for (; 0 < n; --n)
          seq_.pop_back ();
      }

      /// Pop all elements from the stack.
      void
      clear () YY_NOEXCEPT
      {
        seq_.clear ();
      }

      /// Number of elements on the stack.
      index_type
      size () const YY_NOEXCEPT
      {
        return index_type (seq_.size ());
      }

      /// Iterator on top of the stack (going downwards).
      const_iterator
      begin () const YY_NOEXCEPT
      {
        return seq_.begin ();
      }

      /// Bottom of the stack.
      const_iterator
      end () const YY_NOEXCEPT
      {
        return seq_.end ();
      }

      /// Present a slice of the top of a stack.
      class slice
      {
      public:
        slice (const stack& stack, index_type range) YY_NOEXCEPT
          : stack_ (stack)
          , range_ (range)
        {}

        const T&
        operator[] (index_type i) const
        {
          return stack_[range_ - i];
        }

      private:
        const stack& stack_;
        index_type range_;
      };

    private:
#if YY_CPLUSPLUS < 201103L
      /// Non copyable.
      stack (const stack&);
      /// Non copyable.
      stack& operator= (const stack&);
#endif
      /// The wrapped container.
      S seq_;
    };


    /// Stack type.
    typedef stack<stack_symbol_type> stack_type;

    /// The stack.
    stack_type yystack_;

    /// Push a new state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param sym  the symbol
    /// \warning the contents of \a s.value is stolen.
    void yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym);

    /// Push a new look ahead token on the state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param s    the state
    /// \param sym  the symbol (for its value and location).
    /// \warning the contents of \a sym.value is stolen.
    void yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym);

    /// Pop \a n symbols from the stack.
    void yypop_ (int n = 1) YY_NOEXCEPT;

    /// Constants.
    enum
    {
      yylast_ = 587,     ///< Last index in yytable_.
      yynnts_ = 63,  ///< Number of nonterminal symbols.
      yyfinal_ = 36 ///< Termination state number.
    };


    // User arguments.
    CSQLScanner& scanner;
    CSQLDriver& driver;

  };

  inline
  CSQLParser::symbol_kind_type
  CSQLParser::yytranslate_ (int t) YY_NOEXCEPT
  {
    // YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to
    // TOKEN-NUM as returned by yylex.
    static
    const signed char
    translate_table[] =
    {
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86
    };
    // Last valid token kind.
    const int code_max = 341;

    if (t <= 0)
      return symbol_kind::S_YYEOF;
    else if (t <= code_max)
      return static_cast <symbol_kind_type> (translate_table[t]);
    else
      return symbol_kind::S_YYUNDEF;
  }

  // basic_symbol.
  template <typename Base>
  CSQLParser::basic_symbol<Base>::basic_symbol (const basic_symbol& that)
    : Base (that)
    , value ()
    , location (that.location)
  {
    switch (this->kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.copy< SColumnDef > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.copy< SCreateIndexStmt::eKind > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.copy< SExprPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.copy< SIndexMethod > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.copy< SJoinSpec > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.copy< SLimit > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_order_item: // order_item
        value.copy< SOrderItem > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_core: // select_core
        value.copy< SSelectCore > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_item: // select_item
        value.copy< SSelectItem > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.copy< SSelectStmtPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_set_item: // set_item
        value.copy< SSetClause > (YY_MOVE (that.value));
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
        value.copy< SStatementPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.copy< STableRefPtr > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_type_name: // type_name
        value.copy< STypeName > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.copy< TExprList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.copy< TNameList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_row_list: // row_list
        value.copy< TRowList > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.copy< bool > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.copy< double > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.copy< eCompoundOp > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_join_op: // join_op
        value.copy< eJoinType > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.copy< int64_t > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.copy< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.copy< std::vector<SColumnDef> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.copy< std::vector<SOrderItem> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_select_list: // select_list
        value.copy< std::vector<SSelectItem> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_set_list: // set_list
        value.copy< std::vector<SSetClause> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.copy< std::vector<STableRefPtr> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_when_list: // when_list
        value.copy< std::vector<SWhenClause> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_number_list: // number_list
        value.copy< std::vector<double> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.copy< std::vector<int64_t> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_PARAM: // "?"
        value.copy< uint32_t > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

  }




  template <typename Base>
  CSQLParser::symbol_kind_type
  CSQLParser::basic_symbol<Base>::type_get () const YY_NOEXCEPT
  {
    return this->kind ();
  }


  template <typename Base>
  bool
  CSQLParser::basic_symbol<Base>::empty () const YY_NOEXCEPT
  {
    return this->kind () == symbol_kind::S_YYEMPTY;
  }

  template <typename Base>
  void
  CSQLParser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move (s);
    switch (this->kind ())
    {
      case symbol_kind::S_column_def: // column_def
      case symbol_kind::S_column_constraints: // column_constraints
        value.move< SColumnDef > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_index_kind: // index_kind
        value.move< SCreateIndexStmt::eKind > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_where: // opt_where
      case symbol_kind::S_opt_having: // opt_having
      case symbol_kind::S_expr: // expr
      case symbol_kind::S_b_expr: // b_expr
      case symbol_kind::S_primary: // primary
      case symbol_kind::S_literal: // literal
      case symbol_kind::S_opt_case_operand: // opt_case_operand
      case symbol_kind::S_opt_else: // opt_else
        value.move< SExprPtr > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_index_method: // opt_index_method
        value.move< SIndexMethod > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_join_spec: // join_spec
        value.move< SJoinSpec > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_limit: // opt_limit
        value.move< SLimit > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_order_item: // order_item
        value.move< SOrderItem > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_select_core: // select_core
        value.move< SSelectCore > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_select_item: // select_item
        value.move< SSelectItem > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_select_stmt: // select_stmt
      case symbol_kind::S_select_compound: // select_compound
        value.move< SSelectStmtPtr > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_set_item: // set_item
        value.move< SSetClause > (YY_MOVE (s.value));
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
        value.move< SStatementPtr > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_table_ref: // table_ref
      case symbol_kind::S_table_primary: // table_primary
        value.move< STableRefPtr > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_type_name: // type_name
        value.move< STypeName > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_group_by: // opt_group_by
      case symbol_kind::S_row: // row
      case symbol_kind::S_expr_list: // expr_list
        value.move< TExprList > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_column_list: // opt_column_list
      case symbol_kind::S_name_list: // name_list
        value.move< TNameList > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_row_list: // row_list
        value.move< TRowList > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_distinct: // opt_distinct
      case symbol_kind::S_opt_desc: // opt_desc
      case symbol_kind::S_opt_if_exists: // opt_if_exists
      case symbol_kind::S_opt_if_not_exists: // opt_if_not_exists
        value.move< bool > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_FLOAT: // "number"
      case symbol_kind::S_number: // number
        value.move< double > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_compound_op: // compound_op
        value.move< eCompoundOp > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_join_op: // join_op
        value.move< eJoinType > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_INTEGER: // "integer"
        value.move< int64_t > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_IDENT: // "identifier"
      case symbol_kind::S_STRING: // "string"
      case symbol_kind::S_opt_alias: // opt_alias
      case symbol_kind::S_opt_name: // opt_name
        value.move< std::string > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_column_defs: // column_defs
        value.move< std::vector<SColumnDef> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_order_by: // opt_order_by
      case symbol_kind::S_order_list: // order_list
        value.move< std::vector<SOrderItem> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_select_list: // select_list
        value.move< std::vector<SSelectItem> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_set_list: // set_list
        value.move< std::vector<SSetClause> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_opt_from: // opt_from
      case symbol_kind::S_from_list: // from_list
        value.move< std::vector<STableRefPtr> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_when_list: // when_list
        value.move< std::vector<SWhenClause> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_number_list: // number_list
        value.move< std::vector<double> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_integer_list: // integer_list
        value.move< std::vector<int64_t> > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_PARAM: // "?"
        value.move< uint32_t > (YY_MOVE (s.value));
        break;

      default:
        break;
    }

    location = YY_MOVE (s.location);
  }

  // by_kind.
  inline
  CSQLParser::by_kind::by_kind () YY_NOEXCEPT
    : kind_ (symbol_kind::S_YYEMPTY)
  {}

#if 201103L <= YY_CPLUSPLUS
  inline
  CSQLParser::by_kind::by_kind (by_kind&& that) YY_NOEXCEPT
    : kind_ (that.kind_)
  {
    that.clear ();
  }
#endif

  inline
  CSQLParser::by_kind::by_kind (const by_kind& that) YY_NOEXCEPT
    : kind_ (that.kind_)
  {}

  inline
  CSQLParser::by_kind::by_kind (token_kind_type t) YY_NOEXCEPT
    : kind_ (yytranslate_ (t))
  {}



  inline
  void
  CSQLParser::by_kind::clear () YY_NOEXCEPT
  {
    kind_ = symbol_kind::S_YYEMPTY;
  }

  inline
  void
  CSQLParser::by_kind::move (by_kind& that)
  {
    kind_ = that.kind_;
    that.clear ();
  }

  inline
  CSQLParser::symbol_kind_type
  CSQLParser::by_kind::kind () const YY_NOEXCEPT
  {
    return kind_;
  }


  inline
  CSQLParser::symbol_kind_type
  CSQLParser::by_kind::type_get () const YY_NOEXCEPT
  {
    return this->kind ();
  }


} } // bptreedb::sql




#endif // !YY_YY_SQLPARSER_HPP_INCLUDED
