/*
 * The tokens of a row filter expression: flex (2.6+) generates the reentrant C scanner SQLExprLexer.cpp
 * (prefix sqlexpr_, no global state: filters can be parsed from several threads). Regenerate with
 * generate.bat / generate.sh.
 *
 * Keywords are case-insensitive; identifiers keep their case: plain, "quoted", [quoted] or `quoted`.
 * Strings: '...' ('' is a quote). Numbers: integers (a too big one is a real), decimals, exponents.
 * Comments: -- to the end of the line.
 */

%{
#include "SQLExprParser.hpp"
#include "../SQLExprDriver.h"
#include <cerrno>
#include <cstdlib>
#include <string>

using GraphEngine::GeoDatabase::SQLFilter::CSQLExprParser;
using GraphEngine::GeoDatabase::SQLFilter::CSQLExprDriver;

#define YY_DECL CSQLExprParser::symbol_type sqlexpr_lex(CSQLExprDriver& driver, yyscan_t yyscanner)

/* the position of the token in the expression, for the error messages */
#define YY_USER_ACTION driver.Advance((size_t)yyleng);

#define yyterminate() return CSQLExprParser::make_END_OF_INPUT()

namespace
{
    // 'it''s' -> it's ; "a""b" -> a"b ; [a] -> a
    std::string Unquote(const char* pText, size_t nLen)
    {
        std::string s;
        char quote = pText[0] == '[' ? ']' : pText[0];
        for(size_t i = 1; i + 1 < nLen; ++i)
        {
            s += pText[i];
            if(pText[i] == quote && i + 2 < nLen && pText[i + 1] == quote)
                ++i;
        }
        return s;
    }
}
%}

%option reentrant
%option prefix="sqlexpr_"
%option noyywrap nounput noinput batch nounistd never-interactive nodefault
%option case-insensitive
%option warn

DIGIT       [0-9]
IDSTART     [A-Za-z_\x80-\xff]
IDCHAR      [A-Za-z0-9_\x80-\xff]
EXPONENT    [eE][+-]?{DIGIT}+

%%

[ \t\r\n\f\v]+          { /* spaces */ }
"--"[^\n]*              { /* a comment */ }

AND                     { return CSQLExprParser::make_KW_AND(); }
OR                      { return CSQLExprParser::make_KW_OR(); }
NOT                     { return CSQLExprParser::make_KW_NOT(); }
IS                      { return CSQLExprParser::make_KW_IS(); }
NULL                    { return CSQLExprParser::make_KW_NULL(); }
LIKE                    { return CSQLExprParser::make_KW_LIKE(); }
ESCAPE                  { return CSQLExprParser::make_KW_ESCAPE(); }
BETWEEN                 { return CSQLExprParser::make_KW_BETWEEN(); }
IN                      { return CSQLExprParser::make_KW_IN(); }
TRUE                    { return CSQLExprParser::make_KW_TRUE(); }
FALSE                   { return CSQLExprParser::make_KW_FALSE(); }
CAST                    { return CSQLExprParser::make_KW_CAST(); }
AS                      { return CSQLExprParser::make_KW_AS(); }
CASE                    { return CSQLExprParser::make_KW_CASE(); }
WHEN                    { return CSQLExprParser::make_KW_WHEN(); }
THEN                    { return CSQLExprParser::make_KW_THEN(); }
ELSE                    { return CSQLExprParser::make_KW_ELSE(); }
END                     { return CSQLExprParser::make_KW_END(); }

"=="|"="                { return CSQLExprParser::make_EQ(); }
"<>"|"!="               { return CSQLExprParser::make_NE(); }
"<="                    { return CSQLExprParser::make_LE(); }
">="                    { return CSQLExprParser::make_GE(); }
"<"                     { return CSQLExprParser::make_LT(); }
">"                     { return CSQLExprParser::make_GT(); }
"+"                     { return CSQLExprParser::make_PLUS(); }
"-"                     { return CSQLExprParser::make_MINUS(); }
"*"                     { return CSQLExprParser::make_STAR(); }
"/"                     { return CSQLExprParser::make_SLASH(); }
"%"                     { return CSQLExprParser::make_PERCENT(); }
"||"                    { return CSQLExprParser::make_CONCAT(); }
"("                     { return CSQLExprParser::make_LPAREN(); }
")"                     { return CSQLExprParser::make_RPAREN(); }
","                     { return CSQLExprParser::make_COMMA(); }
"."                     { return CSQLExprParser::make_DOT(); }

{DIGIT}+                {
                            errno = 0;
                            long long n = strtoll(yytext, nullptr, 10);
                            if(errno == ERANGE)
                                return CSQLExprParser::make_FLOAT(strtod(yytext, nullptr));
                            return CSQLExprParser::make_INTEGER((int64_t)n);
                        }

{DIGIT}+"."{DIGIT}*{EXPONENT}?|"."{DIGIT}+{EXPONENT}?|{DIGIT}+{EXPONENT} {
                            return CSQLExprParser::make_FLOAT(strtod(yytext, nullptr));
                        }

'([^']|'')*'            { return CSQLExprParser::make_STRING(Unquote(yytext, (size_t)yyleng)); }
'([^']|'')*             { driver.Fail("unterminated string"); }

\"([^"]|\"\")*\"        { return CSQLExprParser::make_IDENT(Unquote(yytext, (size_t)yyleng)); }
\"([^"]|\"\")*          { driver.Fail("unterminated quoted identifier"); }
\[[^\]]*\]              { return CSQLExprParser::make_IDENT(Unquote(yytext, (size_t)yyleng)); }
\[[^\]]*                { driver.Fail("unterminated quoted identifier"); }
`([^`]|``)*`            { return CSQLExprParser::make_IDENT(Unquote(yytext, (size_t)yyleng)); }
`([^`]|``)*             { driver.Fail("unterminated quoted identifier"); }

{IDSTART}{IDCHAR}*      { return CSQLExprParser::make_IDENT(std::string(yytext, (size_t)yyleng)); }

.                       { driver.Fail(std::string("unexpected character '") + yytext + "'"); }

%%

namespace GraphEngine {
    namespace GeoDatabase {
        namespace SQLFilter {

            // scans and parses sExpr: 0 - parsed, otherwise the parser has reported the error to the driver
            int RunSQLExprParser(CSQLExprDriver& driver, const std::string& sExpr)
            {
                yyscan_t scanner = nullptr;
                if(yylex_init(&scanner) != 0)
                    throw std::runtime_error("failed to create the SQL expression scanner");

                struct SScannerGuard
                {
                    yyscan_t        scanner;
                    YY_BUFFER_STATE buffer;
                    ~SScannerGuard()
                    {
                        if(buffer)
                            yy_delete_buffer(buffer, scanner);
                        yylex_destroy(scanner);
                    }
                } guard{scanner, nullptr};

                guard.buffer = yy_scan_bytes(sExpr.data(), (int)sExpr.size(), scanner);
                CSQLExprParser parser(driver, scanner);
                return parser.parse();
            }
        }
    }
}
