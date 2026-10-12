#pragma once

#include "../GeoDatabase.h"
#include "../../CommonLib/sqlitelib/Database.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteTransaction : public ITransaction
        {
        public:

            CSQLiteTransaction( CommonLib::database::IDatabasePtr ptrDatabase);
            virtual ~CSQLiteTransaction();

            virtual void Commit() ;
            virtual void Rollback();

            virtual IInsertCursorPtr CreateInsertCusor(const std::string&  sqlInsertQuery);
            virtual IInsertCursorPtr CreateInsertCusor(ITablePtr pTable);
            virtual IUpdateCursorPtr CreateUpdateCusor(const std::string&  sqlUpdateQuery);
            virtual IUpdateCursorPtr CreateUpdateCusor(ITablePtr pTable, const std::string& whereClause = "");
            virtual IDeleteCursorPtr CreateDeleteCusor(ITablePtr pTable, IFieldSetPtr ptrFileds = nullptr);
            virtual void Executequery(const std::string& sqlQuery);
        private:
            CommonLib::database::ITransactionPtr m_ptrTran;
            CommonLib::database::IDatabasePtr   m_ptrDatabase;
        };


    }
    }