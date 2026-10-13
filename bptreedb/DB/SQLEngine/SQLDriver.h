#pragma once

// The front end of SQL: the text -> the syntax tree (AST.h).
//
//   CSQLDriver driver;
//   sql::TStatementList statements = driver.Parse("CREATE TABLE t (id INT); SELECT * FROM t WHERE id = ?");
//
// The parser (bison, Grammar/SQLParser.y) and the scanner (flex, Grammar/SQLLexer.l) are generated C++
// classes; the driver runs them over a text and collects the statements. A driver parses one text at a
// time; different drivers are independent (the scanner and the parser have no global state).

#include "AST.h"

namespace bptreedb
{
	namespace sql
	{
		class CSQLDriver
		{
		public:
			// The statements of a script, separated by ";" (empty statements are skipped). A syntax error throws
			// CommonLib::CExcBase "SQL: line L, column C: syntax error, ...".
			TStatementList Parse(const std::string& sSQL);

			// Exactly one statement (a ";" at the end is allowed).
			SStatementPtr ParseStatement(const std::string& sSQL);

			// the "?" parameters of the last parsed text
			uint32_t GetParameterCount() const { return m_nParameters; }

			// used by the grammar
			void AddStatement(SStatementPtr ptrStatement);
			uint32_t NewParameter() { return m_nParameters++; }
			void SetError(uint32_t nLine, uint32_t nColumn, const std::string& sMessage);

		private:
			TStatementList m_statements;
			uint32_t m_nParameters{ 0 };
			bool m_bError{ false };
			uint32_t m_nErrorLine{ 0 };
			uint32_t m_nErrorColumn{ 0 };
			std::string m_sError;
		};
	}
}
