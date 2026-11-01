/*
 * The SQL expression of a row filter (the condition of a WHERE clause). bison (3.6+) generates the C++ parser
 * SQLExprParser.cpp / SQLExprParser.hpp; the tokens come from the flex scanner SQLExprLexer.l.
 * Regenerate with generate.bat / generate.sh (or GRAPHENGINE_GENERATE_SQL_GRAMMAR=ON in CMake).
 *
 *   logic:       OR, AND, NOT
 *   comparison:  = (==), <> (!=), <, <=, >, >=, IS [NOT] NULL, [NOT] LIKE p [ESCAPE c], [NOT] BETWEEN a AND b,
 *                [NOT] IN (list)
 *   arithmetic:  + - * / % (unary + -), || (concatenation)
 *   operands:    numbers, 'strings', NULL, TRUE, FALSE, fields: name, "name", [name], `name`, prefix.name (the
 *                prefix of the table or of a join), functions, CAST(x AS type), CASE ... END, (expr)
 */

%skeleton "lalr1.cc"
%require "3.6"
%language "c++"

%define api.namespace {GraphEngine::GeoDatabase::SQLFilter}
%define api.parser.class {CSQLExprParser}
%define api.token.constructor
%define api.value.type variant
%define api.token.prefix {TOK_}
%define parse.assert
%define parse.error detailed
%expect 0

%parse-param {CSQLExprDriver& driver} {void* scanner}
%lex-param {CSQLExprDriver& driver} {void* scanner}

%code requires
{
#include "../SQLExpr.h"
#include <cstdint>
#include <string>
#include <vector>

namespace GraphEngine { namespace GeoDatabase { namespace SQLFilter { class CSQLExprDriver; } } }
}

%code
{
#include "../SQLExprDriver.h"

GraphEngine::GeoDatabase::SQLFilter::CSQLExprParser::symbol_type
    sqlexpr_lex(GraphEngine::GeoDatabase::SQLFilter::CSQLExprDriver& driver, void* yyscanner);
#define yylex sqlexpr_lex

namespace
{
    using namespace GraphEngine::GeoDatabase::SQLFilter;

    CSQLExprPtr MakeBinary(CBinaryExpr::eOp op, CSQLExprPtr& left, CSQLExprPtr& right)
    {
        return std::make_shared<CBinaryExpr>(op, std::move(left), std::move(right));
    }
}
}

%token END_OF_INPUT 0 "end of input"
%token <std::string> IDENT "identifier"
%token <std::string> STRING "string"
%token <int64_t> INTEGER "integer"
%token <double> FLOAT "number"

%token
    KW_AND "AND"
    KW_OR "OR"
    KW_NOT "NOT"
    KW_IS "IS"
    KW_NULL "NULL"
    KW_LIKE "LIKE"
    KW_ESCAPE "ESCAPE"
    KW_BETWEEN "BETWEEN"
    KW_IN "IN"
    KW_TRUE "TRUE"
    KW_FALSE "FALSE"
    KW_CAST "CAST"
    KW_AS "AS"
    KW_CASE "CASE"
    KW_WHEN "WHEN"
    KW_THEN "THEN"
    KW_ELSE "ELSE"
    KW_END "END"
;

%token
    EQ "="
    NE "<>"
    LT "<"
    LE "<="
    GT ">"
    GE ">="
    PLUS "+"
    MINUS "-"
    STAR "*"
    SLASH "/"
    PERCENT "%"
    CONCAT "||"
    LPAREN "("
    RPAREN ")"
    COMMA ","
    DOT "."
;

%left KW_OR
%left KW_AND
%right KW_NOT
%nonassoc KW_IS KW_LIKE KW_BETWEEN KW_IN EQ NE
%nonassoc LT LE GT GE
%left PLUS MINUS
%left STAR SLASH PERCENT
%left CONCAT
%precedence UMINUS

%type <CSQLExprPtr> expr b_expr primary opt_escape opt_operand opt_else
%type <TExprList> expr_list
%type <std::vector<CCaseExpr::SWhen>> when_list

%start input

%%

input:
    expr                                            { driver.SetResult(std::move($1)); }
;

/* a condition: logic and comparisons over the arithmetic of b_expr */
expr:
    b_expr                                          { $$ = std::move($1); }
  | expr KW_OR expr                                 { $$ = MakeBinary(CBinaryExpr::Or, $1, $3); }
  | expr KW_AND expr                                { $$ = MakeBinary(CBinaryExpr::And, $1, $3); }
  | KW_NOT expr                                     { $$ = std::make_shared<CUnaryExpr>(CUnaryExpr::Not, std::move($2)); }
  | expr EQ expr                                    { $$ = MakeBinary(CBinaryExpr::Equal, $1, $3); }
  | expr NE expr                                    { $$ = MakeBinary(CBinaryExpr::NotEqual, $1, $3); }
  | expr LT expr                                    { $$ = MakeBinary(CBinaryExpr::Less, $1, $3); }
  | expr LE expr                                    { $$ = MakeBinary(CBinaryExpr::LessOrEqual, $1, $3); }
  | expr GT expr                                    { $$ = MakeBinary(CBinaryExpr::Greater, $1, $3); }
  | expr GE expr                                    { $$ = MakeBinary(CBinaryExpr::GreaterOrEqual, $1, $3); }
  | expr KW_IS KW_NULL                              { $$ = std::make_shared<CIsNullExpr>(std::move($1), false); }
  | expr KW_IS KW_NOT KW_NULL                       { $$ = std::make_shared<CIsNullExpr>(std::move($1), true); }
  | expr KW_LIKE b_expr opt_escape                  { $$ = std::make_shared<CLikeExpr>(std::move($1), std::move($3), std::move($4), false); }
  | expr KW_NOT KW_LIKE b_expr opt_escape           { $$ = std::make_shared<CLikeExpr>(std::move($1), std::move($4), std::move($5), true); }
  | expr KW_BETWEEN b_expr KW_AND b_expr            { $$ = std::make_shared<CBetweenExpr>(std::move($1), std::move($3), std::move($5), false); }
  | expr KW_NOT KW_BETWEEN b_expr KW_AND b_expr     { $$ = std::make_shared<CBetweenExpr>(std::move($1), std::move($4), std::move($6), true); }
  | expr KW_IN LPAREN expr_list RPAREN              { $$ = std::make_shared<CInExpr>(std::move($1), std::move($4), false); }
  | expr KW_NOT KW_IN LPAREN expr_list RPAREN       { $$ = std::make_shared<CInExpr>(std::move($1), std::move($5), true); }
;

opt_escape:
    %empty                                          { $$ = CSQLExprPtr(); }
  | KW_ESCAPE b_expr                                { $$ = std::move($2); }
;

b_expr:
    primary                                         { $$ = std::move($1); }
  | b_expr PLUS b_expr                              { $$ = MakeBinary(CBinaryExpr::Add, $1, $3); }
  | b_expr MINUS b_expr                             { $$ = MakeBinary(CBinaryExpr::Subtract, $1, $3); }
  | b_expr STAR b_expr                              { $$ = MakeBinary(CBinaryExpr::Multiply, $1, $3); }
  | b_expr SLASH b_expr                             { $$ = MakeBinary(CBinaryExpr::Divide, $1, $3); }
  | b_expr PERCENT b_expr                           { $$ = MakeBinary(CBinaryExpr::Modulo, $1, $3); }
  | b_expr CONCAT b_expr                            { $$ = MakeBinary(CBinaryExpr::Concat, $1, $3); }
  | MINUS b_expr %prec UMINUS                       { $$ = std::make_shared<CUnaryExpr>(CUnaryExpr::Negate, std::move($2)); }
  | PLUS b_expr %prec UMINUS                        { $$ = std::make_shared<CUnaryExpr>(CUnaryExpr::Plus, std::move($2)); }
;

primary:
    INTEGER                                         { $$ = std::make_shared<CLiteralExpr>(CSQLValue::MakeInteger($1)); }
  | FLOAT                                           { $$ = std::make_shared<CLiteralExpr>(CSQLValue::MakeReal($1)); }
  | STRING                                          { $$ = std::make_shared<CLiteralExpr>(CSQLValue::MakeText(std::move($1))); }
  | KW_NULL                                         { $$ = std::make_shared<CLiteralExpr>(CSQLValue()); }
  | KW_TRUE                                         { $$ = std::make_shared<CLiteralExpr>(CSQLValue::MakeBool(true)); }
  | KW_FALSE                                        { $$ = std::make_shared<CLiteralExpr>(CSQLValue::MakeBool(false)); }
  | IDENT                                           { $$ = driver.MakeField(std::string(), $1); }
  | IDENT DOT IDENT                                 { $$ = driver.MakeField($1, $3); }
  | IDENT LPAREN RPAREN                             { $$ = driver.MakeFunction($1, TExprList()); }
  | IDENT LPAREN expr_list RPAREN                   { $$ = driver.MakeFunction($1, std::move($3)); }
  | KW_CAST LPAREN expr KW_AS IDENT RPAREN          { $$ = driver.MakeCast(std::move($3), $5); }
  | KW_CASE opt_operand when_list opt_else KW_END   { $$ = std::make_shared<CCaseExpr>(std::move($2), std::move($3), std::move($4)); }
  | LPAREN expr RPAREN                              { $$ = std::move($2); }
;

expr_list:
    expr                                            { $$.push_back(std::move($1)); }
  | expr_list COMMA expr                            { $$ = std::move($1); $$.push_back(std::move($3)); }
;

opt_operand:
    %empty                                          { $$ = CSQLExprPtr(); }
  | expr                                            { $$ = std::move($1); }
;

when_list:
    KW_WHEN expr KW_THEN expr                       { $$.push_back(CCaseExpr::SWhen{std::move($2), std::move($4)}); }
  | when_list KW_WHEN expr KW_THEN expr             { $$ = std::move($1); $$.push_back(CCaseExpr::SWhen{std::move($3), std::move($5)}); }
;

opt_else:
    %empty                                          { $$ = CSQLExprPtr(); }
  | KW_ELSE expr                                    { $$ = std::move($2); }
;

%%

void GraphEngine::GeoDatabase::SQLFilter::CSQLExprParser::error(const std::string& sMessage)
{
    driver.SetError(sMessage);
}
