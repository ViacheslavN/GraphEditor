#pragma once

#include "../GeoDatabase.h"
#include "SQLExprDriver.h"

namespace GraphEngine {
    namespace GeoDatabase {

        // ISQLRowFilter on the expression tree of SQLFilter (parsed by bison / flex, SQLRowFilter/Grammar).
        //
        // The fields of the expression are found in the row by the column name, ignoring case:
        //   prefix.name - the column "prefix.name" (a column of a join, see CreateSelectPartQuery), then the column "name"
        //                 (the main table queried with a prefix gives the columns without it)
        //   name        - the column "name"
        // A field missing in the row is an error of EvaluateRow. The column indexes are cached and checked by the name
        // on each row, so one filter works with the rows of different cursors. Not thread safe: a filter per thread.
        class CSQLRowFilter : public ISQLRowFilter
        {
        public:
            CSQLRowFilter();
            explicit CSQLRowFilter(const std::string& sExpr);   // throws on a wrong expression
            virtual ~CSQLRowFilter();

            static ISQLRowFilterPtr Create(const std::string& sExpr);

        private:
            CSQLRowFilter(const CSQLRowFilter&);
            CSQLRowFilter& operator=(const CSQLRowFilter&);

        public:
            // ISQLRowFilter
            virtual bool               EvaluateRow(IRowPtr ptrRow) const;
            virtual const std::string& GetSQLExpression() const;
            virtual void               SetSQLExpression(const std::string& sExpr);
            virtual int                GetFieldCount() const;
            virtual const std::string& GetFieldName(int nIndex) const;
            virtual const std::string& GetFieldPrefix(int nIndex) const;

        private:
            class CRowValues;
            int32_t FindColumn(IRow* pRow, size_t nField) const;
            static SQLFilter::CSQLValue GetColumnValue(IRow* pRow, int32_t nColumn);

        private:
            std::string                       m_sExpr;
            SQLFilter::CSQLExprPtr            m_ptrExpr;     // empty for an empty expression: every row passes
            std::vector<SQLFilter::SFieldRef> m_vecFields;
            mutable std::vector<int32_t>      m_vecColumns;  // the cached column of each field, -1 - not found yet
        };
    }
}
