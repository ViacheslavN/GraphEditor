#include "SQLiteTransaction.h"
#include "SQLiteInsertCursor.h"



namespace GraphEngine {
    namespace GeoDatabase {

        CSQLiteTransaction::CSQLiteTransaction( CommonLib::database::IDatabasePtr ptrDatabase)
        {
            m_ptrDatabase = ptrDatabase;
            m_ptrTran = m_ptrDatabase->CreateTransaction();
            m_ptrTran->Begin();
        }

        CSQLiteTransaction::~CSQLiteTransaction()
        {

        }

        void CSQLiteTransaction::Commit()
        {
             m_ptrTran->Commit();
        }

        void CSQLiteTransaction::CSQLiteTransaction::Rollback()
        {
            m_ptrTran->Rollback();
        }

        IInsertCursorPtr CSQLiteTransaction::CreateInsertCusor(const std::string&  sqlInsertQuery)
        {
            return std::make_shared<CSQLiteInsertCursor>(sqlInsertQuery, m_ptrDatabase);
        }

        IInsertCursorPtr CSQLiteTransaction::CreateInsertCusor(ITablePtr ptrTable)
        {
            return std::make_shared<CSQLiteInsertCursor>(ptrTable, m_ptrDatabase);
        }

        IUpdateCursorPtr CSQLiteTransaction::CreateUpdateCusor(const std::string&  sqlUpdateQuery)
        {
            return IUpdateCursorPtr();
        }

        IUpdateCursorPtr CSQLiteTransaction::CreateUpdateCusor(ITablePtr ptrTable, const std::string& whereClause)
        {
            return IUpdateCursorPtr();
        }

        IDeleteCursorPtr CSQLiteTransaction::CreateDeleteCusor(ITablePtr ptrTable, IFieldSetPtr ptrFileds)
        {
            return IDeleteCursorPtr();
        }

        void CSQLiteTransaction::Executequery(const std::string& sqlQuery)
        {
            m_ptrDatabase->Execute(sqlQuery.c_str());
        }

    }
    }