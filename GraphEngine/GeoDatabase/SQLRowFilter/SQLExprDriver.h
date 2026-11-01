#pragma once

#include "SQLExpr.h"
#include <string>
#include <vector>

namespace GraphEngine {
    namespace GeoDatabase {
        namespace SQLFilter {

            // a field the expression references: [prefix.]name
            struct SFieldRef
            {
                std::string sPrefix;   // the table prefix (an alias of a table or a join), empty if not qualified
                std::string sName;

                std::string GetFullName() const { return sPrefix.empty() ? sName : sPrefix + "." + sName; }
            };

            // parses a SQL expression (a WHERE condition) into a tree: the bison parser SQLExprParser.y with the flex
            // scanner SQLExprLexer.l. The fields are numbered by their first appearance, names are case-insensitive.
            class CSQLExprDriver
            {
            public:
                CSQLExprDriver() {}

                // throws CSQLExprError
                CSQLExprPtr                   Parse(const std::string& sExpr);
                const std::vector<SFieldRef>& GetFields() const { return m_vecFields; }

                // used by the grammar
                CSQLExprPtr MakeField(const std::string& sPrefix, const std::string& sName);
                CSQLExprPtr MakeFunction(const std::string& sName, TExprList args);
                CSQLExprPtr MakeCast(CSQLExprPtr ptrExpr, const std::string& sType);
                void        SetResult(CSQLExprPtr ptrExpr) { m_ptrResult = std::move(ptrExpr); }
                void        SetError(const std::string& sMessage);
                [[noreturn]] void Fail(const std::string& sMessage) const;

                // used by the scanner: the token starts at the current position and takes nLen bytes
                void   Advance(size_t nLen) { m_nTokenPos = m_nPos; m_nPos += nLen; }
                size_t GetTokenPosition() const { return m_nTokenPos; }

            private:
                std::vector<SFieldRef> m_vecFields;
                CSQLExprPtr            m_ptrResult;
                std::string            m_sError;
                size_t                 m_nErrorPos = 0;
                size_t                 m_nPos = 0;
                size_t                 m_nTokenPos = 0;
            };
        }
    }
}
