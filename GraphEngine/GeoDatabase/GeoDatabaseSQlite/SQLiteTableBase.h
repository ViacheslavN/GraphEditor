#pragma once

#include "../TableBase.h"
#include "../../CommonLib/sqlitelib/Database.h"
#include "SQLiteUtils.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteTableBase : public ITableBase<ITable>
        {
        public:

            typedef ITableBase<ITable> TBase;

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



            virtual  ~CSQLiteTableBase()
            {

            }


        private:
            CommonLib::database::IDatabasePtr m_ptrDatabase;
        };

    }
}