#pragma once

#include "../TableBase.h"
#include "../../CommonLib/sqlitelib/Database.h"
#include "SQLiteUtils.h"
#include "SQLiteSelectCursor.h"


namespace GraphEngine {
    namespace GeoDatabase {

        template<class I>
        class CSQLiteTableBase : public ITableBase<I>
        {
        public:

            typedef ITableBase<I> TBase;

            CSQLiteTableBase(eDatasetType datasetType, const std::string& tableName,  const std::string& viewName,
                             CommonLib::database::IDatabasePtr ptrDatabase) :
                    TBase(datasetType, tableName, viewName),
                    m_ptrDatabase(ptrDatabase)
            {
                try
                {
                   m_pFields =  CSQLiteUtils::ReadFields(tableName, m_ptrDatabase);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("Failed read sqlite table", exc);
                }
            }

            virtual ISelectCursorPtr Select(const std::string& sqlSelectQuery)
            {
                try
                {

                    return std::make_shared<CSQLiteSelectCursor>(sqlSelectQuery, m_ptrDatabase);

                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("CSQLiteTable: Failed to Select", exc);
                    throw;
                }

            }

            virtual ISelectCursorPtr	Search(IQueryFilterPtr ptrFilter)
            {
                try
                {
                    return std::make_shared<CSQLiteSelectCursor>(GetDatasetName(), GetOIDFieldName(), GetSpatialIndexName(), GetFields(), ptrFilter, GetSpatialReference() , m_ptrDatabase);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("CSQLiteTable: Failed to Search", exc);
                    throw;
                }

            }


        protected:




        protected:
            CommonLib::database::IDatabasePtr m_ptrDatabase;
        };

    }
}