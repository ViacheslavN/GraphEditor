#include "SQLDriver.h"
#include "SQLScanner.h"
#include "../DBCommon.h"
#include <sstream>

namespace bptreedb
{
	namespace sql
	{
		TStatementList CSQLDriver::Parse(const std::string& sSQL)
		{
			m_statements.clear();
			m_nParameters = 0;
			m_bError = false;
			m_sError.clear();

			std::istringstream in(sSQL);
			CSQLScanner scanner(in);
			CSQLParser parser(scanner, *this);
			int nResult = parser.parse();
			if (nResult != 0 || m_bError)
			{
				m_statements.clear();
				if (!m_bError)
					throw CommonLib::CExcBase("SQL parser failed ({0})", nResult);
				throw CommonLib::CExcBase("SQL: line {0}, column {1}: {2}", m_nErrorLine, m_nErrorColumn, m_sError);
			}

			TStatementList statements;
			statements.swap(m_statements);
			return statements;
		}

		SStatementPtr CSQLDriver::ParseStatement(const std::string& sSQL)
		{
			TStatementList statements = Parse(sSQL);
			if (statements.size() != 1)
				throw CommonLib::CExcBase("one SQL statement is expected, the text has {0}", statements.size());
			return statements[0];
		}

		void CSQLDriver::AddStatement(SStatementPtr ptrStatement)
		{
			m_statements.push_back(ptrStatement);
		}

		void CSQLDriver::SetError(uint32_t nLine, uint32_t nColumn, const std::string& sMessage)
		{
			if (m_bError)
				return;          // the first error
			m_bError = true;
			m_nErrorLine = nLine;
			m_nErrorColumn = nColumn;
			m_sError = sMessage;
		}
	}
}
