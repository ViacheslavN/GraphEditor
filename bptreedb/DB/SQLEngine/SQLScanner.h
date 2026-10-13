#pragma once

// The scanner of SQL: flex generates CSQLScanner::ScanToken from Grammar/SQLLexer.l (Grammar/SQLLexer.cpp).
// One scanner reads one text: it isn't shared between threads.

#if !defined(yyFlexLexerOnce)
#include "Grammar/FlexLexer.h"
#endif

#include "Grammar/SQLParser.hpp"
#include <istream>

namespace bptreedb
{
	namespace sql
	{
		class CSQLScanner : public yyFlexLexer
		{
		public:
			explicit CSQLScanner(std::istream& in) : yyFlexLexer(&in) {}

			// the next token (END_OF_INPUT at the end); throws CSQLParser::syntax_error for a wrong token
			CSQLParser::symbol_type ScanToken(CSQLDriver& driver);

		private:
			CSQLParser::location_type m_location;
		};
	}
}
