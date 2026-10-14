#include "BPtreeDBTransaction.h"
#include "BPtreeDBInsertCursor.h"
#include "BPtreeDBUtils.h"


namespace GraphEngine {
    namespace GeoDatabase {

        CBPtreeDBTransaction::CBPtreeDBTransaction(CBPtreeDBContextPtr ptrContext, eTransactionType type) :
                m_ptrContext(ptrContext)
        {
            m_bWrite = type != ttSelect;

            // bptreedb: one write transaction at a time, a thread mustn't start a second one (it would wait
            // for itself); a read transaction of a thread that has a DDL one would wait too
            bptreedb::ITransactionPtr ptrThreadTran = m_ptrContext->GetThreadWriteTransaction();
            if(ptrThreadTran.get())
            {
                if(m_bWrite)
                    throw CommonLib::CExcBase("the thread has an open write transaction of the workspace");
                if(ptrThreadTran->GetTransactionDataType() == bptreedb::eTT_DDL)
                    throw CommonLib::CExcBase("the thread has an open DDL transaction of the workspace (it is exclusive): read in it");
            }

            if(!m_bWrite)
            {
                m_ptrTran = m_ptrContext->GetDatabase()->StartTransaction(bptreedb::eTT_SELECT, bptreedb::eTTDirectRead);
                return;
            }

            bptreedb::eTransactionDataType dataType = bptreedb::eTT_MODIFY;
            if(type == ttDDL || m_ptrContext->IsBulkLoad())
                dataType = bptreedb::eTT_DDL;

            m_ptrTran = m_ptrContext->GetDatabase()->StartTransaction(dataType, bptreedb::eTT2PL);
            m_ptrTran->SetInsertOnRight(m_ptrContext->IsInsertOnRight());
            m_ptrContext->RegisterWriteTransaction(m_ptrTran);
        }

        CBPtreeDBTransaction::~CBPtreeDBTransaction()
        {
            try
            {
                if(m_ptrTran.get() && m_ptrTran->IsActive())
                    Rollback();
            }
            catch (...)
            {
            }
            End();
        }

        void CBPtreeDBTransaction::End()
        {
            if(m_bWrite && m_ptrTran.get())
                m_ptrContext->UnregisterWriteTransaction(m_ptrTran.get());
        }

        void CBPtreeDBTransaction::Commit()
        {
            try
            {
                // the indexes of a bulk load that weren't built by IInsertCursor::Finish
                if(m_bWrite)
                    m_ptrContext->BuildAllPendingIndexes(m_ptrTran);

                m_ptrTran->Commit();
                End();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBTransaction: failed to commit", exc);
                throw;
            }
        }

        void CBPtreeDBTransaction::Rollback()
        {
            try
            {
                if(m_bWrite)
                    m_ptrContext->ClearPendingIndexes();

                m_ptrTran->Rollback();
                End();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBTransaction: failed to roll back", exc);
                throw;
            }
        }

        IInsertCursorPtr CBPtreeDBTransaction::CreateInsertCusor(const std::string&  sqlInsertQuery)
        {
            return std::make_shared<CBPtreeDBInsertCursor>(sqlInsertQuery, m_ptrTran, m_ptrContext);
        }

        IInsertCursorPtr CBPtreeDBTransaction::CreateInsertCusor(ITablePtr ptrTable)
        {
            return std::make_shared<CBPtreeDBInsertCursor>(ptrTable, m_ptrTran, m_ptrContext);
        }

        IUpdateCursorPtr CBPtreeDBTransaction::CreateUpdateCusor(const std::string&  sqlUpdateQuery)
        {
            return IUpdateCursorPtr();
        }

        IUpdateCursorPtr CBPtreeDBTransaction::CreateUpdateCusor(ITablePtr ptrTable, const std::string& whereClause)
        {
            return IUpdateCursorPtr();
        }

        IDeleteCursorPtr CBPtreeDBTransaction::CreateDeleteCusor(ITablePtr ptrTable, IFieldSetPtr ptrFileds)
        {
            return std::make_shared<CBPtreeDBDeleteCursor>(ptrTable->GetDatasetName(), ptrTable->GetOIDFieldName(), m_ptrTran, m_ptrContext);
        }

        void CBPtreeDBTransaction::Executequery(const std::string& sqlQuery)
        {
            try
            {
                m_ptrContext->InvalidatePendingBoxes();   // the SQL can insert / delete rows
                m_ptrTran->execute(sqlQuery);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBTransaction: failed to execute {0}", sqlQuery, exc);
                throw;
            }
        }

        CBPtreeDBDeleteCursor::CBPtreeDBDeleteCursor(const std::string& sTable, const std::string& sOIDField, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContextPtr ptrContext) :
                m_ptrContext(ptrContext),
                m_ptrTran(ptrTran),
                m_sTable(sTable),
                m_sOIDField(sOIDField)
        {
            m_ptrCursor = m_ptrTran->createDeleteCursor(sTable);
            m_ptrContext->InvalidatePendingBoxes();   // the deleted rows would stay in the boxes
        }

        CBPtreeDBDeleteCursor::~CBPtreeDBDeleteCursor()
        {

        }

        void CBPtreeDBDeleteCursor::DeleteRow(int64_t oid)
        {
            try
            {
                if(m_sOIDField.empty() || CBPtreeDBUtils::IsRowIdName(m_sOIDField))
                {
                    m_ptrCursor->DeleteRow(oid);
                    return;
                }

                // the rows of the OID value (by the index of the field if it has one)
                std::vector<int64_t> vecRows;
                {
                    bptreedb::ISelectCursorPtr ptrCursor = m_ptrTran->executeSelectQuery(m_sTable, bptreedb::IFieldSet::Create(std::vector<std::string>{ m_sOIDField }),
                                                                                         m_sOIDField, CommonLib::CVariant(oid), bptreedb::OpEqual);
                    while(ptrCursor->Next())
                        vecRows.push_back(ptrCursor->GetRowID());
                }

                for(int64_t nRowID : vecRows)
                    m_ptrCursor->DeleteRow(nRowID);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExcT("CBPtreeDBDeleteCursor: failed to delete {0} from {1}", oid, m_sTable, exc);
                throw;
            }
        }

    }
}
