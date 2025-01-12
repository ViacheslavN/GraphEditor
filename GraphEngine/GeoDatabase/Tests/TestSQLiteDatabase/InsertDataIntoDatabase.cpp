#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../QueryFilter.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "TestingData.h"

void InsertDateInDatabase()
{
    try
    {

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrWks = GraphEngine::GeoDatabase::CSQLiteWorkspace::Open(CTestingData::m_sDatabaseName.c_str(), CTestingData::m_sDatabasePath.c_str(), 1);
        GraphEngine::GeoDatabase::ITablePtr ptrTable1 = ptrWks->GetTable(CTestingData::m_sTestTableName_1);
        GraphEngine::GeoDatabase::ITablePtr ptrTable2 = ptrWks->GetTable(CTestingData::m_sTestTableName_2);

        GraphEngine::GeoDatabase::ITransactionPtr ptrTran = ptrWks->StartTransaction(GraphEngine::GeoDatabase::eTransactionType::ttModify);

        {
            GraphEngine::GeoDatabase::IInsertCursorPtr ptrCursor =  ptrTran->CreateInsertCusor(ptrTable1);
            for(int i = 0; i < 10; ++i)
            {
                int colNumber =  ptrCursor->ColumnCount();
                for(int col = 0; col < colNumber; ++col)
                {
                    GraphEngine::GeoDatabase::eDataTypes type =  ptrCursor->GetColumnType(col);

                    switch (type)
                    {
                        case GraphEngine::GeoDatabase::eDataTypes::dtInteger64:
                            ptrCursor->BindInt64(col, i);
                            break;
                        case GraphEngine::GeoDatabase::eDataTypes::dtString:
                            ptrCursor->BindText(col,  CommonLib::str_format::AStrFormatSafeT("Text_{0}", i), true);
                            break;
                    }
                }

                ptrCursor->Next();

            }
        }

        {
            GraphEngine::GeoDatabase::IInsertCursorPtr ptrCursor =  ptrTran->CreateInsertCusor("INSERT INTO  TestTable_2 ( OID_2, StringText) VALUES ( ?, ?)");
            for(int i = 0; i < 10; ++i) {
                ptrCursor->BindInt64(0, i);
                ptrCursor->BindText(1, CommonLib::str_format::AStrFormatSafeT("Text_2_{0}", i), true);
                ptrCursor->Next();
            }

        }

        {
            GraphEngine::GeoDatabase::IInsertCursorPtr ptrCursor =  ptrTran->CreateInsertCusor("INSERT INTO  TestTable_3 ( OID_3, StringText) VALUES ( ?, ?)");
            for(int i = 0; i < 10; ++i) {
                ptrCursor->BindInt64(0, i);
                ptrCursor->BindText(1, CommonLib::str_format::AStrFormatSafeT("Text_3_{0}", i), true);
                ptrCursor->Next();
            }

        }


        ptrTran->Commit();

    }
    catch(std::exception& exc)
    {
        CommonLib::CExcBase::RegenExc("Failed to insert into database", exc);
        throw ;
    }
}