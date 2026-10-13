/*
 * The SQL tokens: flex (2.6+) generates the C++ scanner SQLLexer.cpp, the class CSQLScanner (SQLScanner.h)
 * derived from yyFlexLexer (FlexLexer.h of flex is kept here). Regenerate with generate.bat / generate.sh.
 *
 * Keywords are case-insensitive; identifiers keep their case: plain, "quoted", [quoted] or `quoted`.
 * Strings: '...' ('' is a quote). Numbers: integers (a too big one is a double), decimals, exponents.
 * Comments: -- to the end of the line, / * ... * /. Parameters: ? (numbered by the driver).
 */

%{
#include "../SQLScanner.h"
#include "../SQLDriver.h"
#include <cerrno>
#include <cstdlib>
#include <string>

#undef YY_DECL
#define YY_DECL bptreedb::sql::CSQLParser::symbol_type bptreedb::sql::CSQLScanner::ScanToken(bptreedb::sql::CSQLDriver& driver)

/* the location advances by the token */
#define YY_USER_ACTION m_location.columns((int)yyleng);

#define yyterminate() return CSQLParser::make_END_OF_INPUT(m_location)

using bptreedb::sql::CSQLParser;

namespace
{
	// 'it''s' -> it's ; "a""b" -> a"b ; [a] -> a
	std::string Unquote(const char* pText, size_t nLen)
	{
		std::string s;
		char quote = pText[0] == '[' ? ']' : pText[0];
		for (size_t i = 1; i + 1 < nLen; ++i)
		{
			s += pText[i];
			if (pText[i] == quote && i + 2 < nLen && pText[i + 1] == quote)
				++i;
		}
		return s;
	}
}
%}

%option c++
%option yyclass="bptreedb::sql::CSQLScanner"
%option noyywrap nounput noinput batch nounistd never-interactive nodefault
%option case-insensitive
%option warn

%x COMMENT

DIGIT       [0-9]
IDSTART     [A-Za-z_\x80-\xff]
IDCHAR      [A-Za-z0-9_\x80-\xff]
EXPONENT    [eE][+-]?{DIGIT}+

%%

%{
	m_location.step();
%}

[ \t\r\f]+          { m_location.step(); }
\n+                 { m_location.lines((int)yyleng); m_location.step(); }
"--"[^\n]*          { m_location.step(); }

"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); m_location.step(); }
<COMMENT>[^*\n]+    { }
<COMMENT>"*"        { }
<COMMENT>\n         { m_location.lines(1); }
<COMMENT><<EOF>>    { throw CSQLParser::syntax_error(m_location, "the comment isn't closed"); }

"ADD"               { return CSQLParser::make_KW_ADD(m_location); }
"ALL"               { return CSQLParser::make_KW_ALL(m_location); }
"ALTER"             { return CSQLParser::make_KW_ALTER(m_location); }
"AND"               { return CSQLParser::make_KW_AND(m_location); }
"AS"                { return CSQLParser::make_KW_AS(m_location); }
"ASC"               { return CSQLParser::make_KW_ASC(m_location); }
"BETWEEN"           { return CSQLParser::make_KW_BETWEEN(m_location); }
"BY"                { return CSQLParser::make_KW_BY(m_location); }
"CASE"              { return CSQLParser::make_KW_CASE(m_location); }
"CAST"              { return CSQLParser::make_KW_CAST(m_location); }
"COLUMN"            { return CSQLParser::make_KW_COLUMN(m_location); }
"CREATE"            { return CSQLParser::make_KW_CREATE(m_location); }
"CROSS"             { return CSQLParser::make_KW_CROSS(m_location); }
"DEFAULT"           { return CSQLParser::make_KW_DEFAULT(m_location); }
"DELETE"            { return CSQLParser::make_KW_DELETE(m_location); }
"DESC"              { return CSQLParser::make_KW_DESC(m_location); }
"DISTINCT"          { return CSQLParser::make_KW_DISTINCT(m_location); }
"DROP"              { return CSQLParser::make_KW_DROP(m_location); }
"ELSE"              { return CSQLParser::make_KW_ELSE(m_location); }
"END"               { return CSQLParser::make_KW_END(m_location); }
"ESCAPE"            { return CSQLParser::make_KW_ESCAPE(m_location); }
"EXCEPT"            { return CSQLParser::make_KW_EXCEPT(m_location); }
"EXISTS"            { return CSQLParser::make_KW_EXISTS(m_location); }
"FALSE"             { return CSQLParser::make_KW_FALSE(m_location); }
"FROM"              { return CSQLParser::make_KW_FROM(m_location); }
"FULL"              { return CSQLParser::make_KW_FULL(m_location); }
"GROUP"             { return CSQLParser::make_KW_GROUP(m_location); }
"HAVING"            { return CSQLParser::make_KW_HAVING(m_location); }
"IF"                { return CSQLParser::make_KW_IF(m_location); }
"IN"                { return CSQLParser::make_KW_IN(m_location); }
"INDEX"             { return CSQLParser::make_KW_INDEX(m_location); }
"INNER"             { return CSQLParser::make_KW_INNER(m_location); }
"INSERT"            { return CSQLParser::make_KW_INSERT(m_location); }
"INTERSECT"         { return CSQLParser::make_KW_INTERSECT(m_location); }
"INTO"              { return CSQLParser::make_KW_INTO(m_location); }
"IS"                { return CSQLParser::make_KW_IS(m_location); }
"JOIN"              { return CSQLParser::make_KW_JOIN(m_location); }
"LEFT"              { return CSQLParser::make_KW_LEFT(m_location); }
"LIKE"              { return CSQLParser::make_KW_LIKE(m_location); }
"LIMIT"             { return CSQLParser::make_KW_LIMIT(m_location); }
"NOT"               { return CSQLParser::make_KW_NOT(m_location); }
"NULL"              { return CSQLParser::make_KW_NULL(m_location); }
"OFFSET"            { return CSQLParser::make_KW_OFFSET(m_location); }
"ON"                { return CSQLParser::make_KW_ON(m_location); }
"OR"                { return CSQLParser::make_KW_OR(m_location); }
"ORDER"             { return CSQLParser::make_KW_ORDER(m_location); }
"OUTER"             { return CSQLParser::make_KW_OUTER(m_location); }
"RIGHT"             { return CSQLParser::make_KW_RIGHT(m_location); }
"SELECT"            { return CSQLParser::make_KW_SELECT(m_location); }
"SET"               { return CSQLParser::make_KW_SET(m_location); }
"SPATIAL"           { return CSQLParser::make_KW_SPATIAL(m_location); }
"TABLE"             { return CSQLParser::make_KW_TABLE(m_location); }
"THEN"              { return CSQLParser::make_KW_THEN(m_location); }
"TRUE"              { return CSQLParser::make_KW_TRUE(m_location); }
"UNION"             { return CSQLParser::make_KW_UNION(m_location); }
"UNIQUE"            { return CSQLParser::make_KW_UNIQUE(m_location); }
"UPDATE"            { return CSQLParser::make_KW_UPDATE(m_location); }
"USING"             { return CSQLParser::make_KW_USING(m_location); }
"VALUES"            { return CSQLParser::make_KW_VALUES(m_location); }
"WHEN"              { return CSQLParser::make_KW_WHEN(m_location); }
"WHERE"             { return CSQLParser::make_KW_WHERE(m_location); }

"("                 { return CSQLParser::make_LPAREN(m_location); }
")"                 { return CSQLParser::make_RPAREN(m_location); }
","                 { return CSQLParser::make_COMMA(m_location); }
"."                 { return CSQLParser::make_DOT(m_location); }
";"                 { return CSQLParser::make_SEMICOLON(m_location); }
"*"                 { return CSQLParser::make_STAR(m_location); }
"+"                 { return CSQLParser::make_PLUS(m_location); }
"-"                 { return CSQLParser::make_MINUS(m_location); }
"/"                 { return CSQLParser::make_SLASH(m_location); }
"%"                 { return CSQLParser::make_PERCENT(m_location); }
"||"                { return CSQLParser::make_CONCAT(m_location); }
"="|"=="            { return CSQLParser::make_EQ(m_location); }
"<>"|"!="           { return CSQLParser::make_NE(m_location); }
"<"                 { return CSQLParser::make_LT(m_location); }
"<="                { return CSQLParser::make_LE(m_location); }
">"                 { return CSQLParser::make_GT(m_location); }
">="                { return CSQLParser::make_GE(m_location); }
"?"                 { return CSQLParser::make_PARAM(driver.NewParameter(), m_location); }

{DIGIT}+ {
	errno = 0;
	unsigned long long nValue = std::strtoull(yytext, nullptr, 10);
	if (errno == ERANGE || nValue > (unsigned long long)INT64_MAX)    // a double (-9223372036854775808: the grammar)
		return CSQLParser::make_FLOAT(std::strtod(yytext, nullptr), m_location);
	return CSQLParser::make_INTEGER((int64_t)nValue, m_location);
}

{DIGIT}+"."{DIGIT}*{EXPONENT}?|"."{DIGIT}+{EXPONENT}?|{DIGIT}+{EXPONENT} {
	return CSQLParser::make_FLOAT(std::strtod(yytext, nullptr), m_location);
}

'([^']|'')*'        { return CSQLParser::make_STRING(Unquote(yytext, yyleng), m_location); }
'([^']|'')*         { throw CSQLParser::syntax_error(m_location, "the string isn't closed"); }

\"([^"]|\"\")*\"    { return CSQLParser::make_IDENT(Unquote(yytext, yyleng), m_location); }
\[[^\]]*\]          { return CSQLParser::make_IDENT(Unquote(yytext, yyleng), m_location); }
`([^`]|``)*`        { return CSQLParser::make_IDENT(Unquote(yytext, yyleng), m_location); }
\"([^"]|\"\")*|\[[^\]]*|`([^`]|``)* { throw CSQLParser::syntax_error(m_location, "the quoted identifier isn't closed"); }

{IDSTART}{IDCHAR}*  { return CSQLParser::make_IDENT(std::string(yytext, yyleng), m_location); }

.                   { throw CSQLParser::syntax_error(m_location, std::string("unexpected character '") + yytext + "'"); }

<<EOF>>             { return CSQLParser::make_END_OF_INPUT(m_location); }

%%
