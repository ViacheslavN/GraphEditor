#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../QueryFilter.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "TestingData.h"
#include "../../Join.h"
#include "../../FieldSet.h"

void SelectData( GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor)
{
    while (ptrCursor->Next())
    {
        int colNumber = ptrCursor->ColumnCount();
        std::string text;
        int64_t  val;
        for(int col = 0;  col < colNumber; ++col)
        {
            GraphEngine::GeoDatabase::eDataTypes type =  ptrCursor->GetColumnType(col);
            std::string filedName = ptrCursor->ColumnName(col);
            switch (type)
            {
                case GraphEngine::GeoDatabase::eDataTypes::dtInteger64:
                    val = ptrCursor->ReadInt64(col);
                    break;
                case GraphEngine::GeoDatabase::eDataTypes::dtString:
                    text = ptrCursor->ReadText(col);
                    break;
            }
        }
    }
}

void SelectFromDatabase() {
    try
    {

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrWks = GraphEngine::GeoDatabase::CSQLiteWorkspace::Open(CTestingData::m_sDatabaseName.c_str(), CTestingData::m_sDatabasePath.c_str(), 1);
        GraphEngine::GeoDatabase::ITablePtr ptrTable1 = ptrWks->GetTable(CTestingData::m_sTestTableName_1);
        GraphEngine::GeoDatabase::ITablePtr ptrTable2 = ptrWks->GetTable(CTestingData::m_sTestTableName_2);
        GraphEngine::GeoDatabase::ITablePtr ptrTable3 = ptrWks->GetTable(CTestingData::m_sTestTableName_3);

        {
            GraphEngine::GeoDatabase::IJoinPtr ptrJoin = std::make_shared<GraphEngine::GeoDatabase::CJoin>();
            ptrJoin->SetTable(ptrTable2);
            ptrJoin->SetTablePrefix("varTable2");
            ptrJoin->SetFirstField("OID_2");
            ptrJoin->SetSecondField("OID");


            GraphEngine::GeoDatabase::IJoinPtr ptrJoin2 = std::make_shared<GraphEngine::GeoDatabase::CJoin>();
            ptrJoin2->SetTable(ptrTable3);
            ptrJoin2->SetTablePrefix("varTable3");
            ptrJoin2->SetFirstField("OID_3");
            ptrJoin2->SetSecondField("OID");

            GraphEngine::GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();
            ptrFilter->SetTablePrefix("varTable1");
            ptrFilter->SetJoins({ptrJoin, ptrJoin2});
            ptrFilter->SetWhereClause("varTable1.OID != 5");

            GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable1->Search(ptrFilter);
            SelectData(ptrCursor);

        }


        {
            GraphEngine::GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();
            GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable1->Search(ptrFilter);
            SelectData(ptrCursor);
        }


        {
            GraphEngine::GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();
            ptrFilter->SetTablePrefix("tableI");
            GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable1->Search(ptrFilter);
            SelectData(ptrCursor);
        }

        {
            GraphEngine::GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();
            ptrFilter->SetTablePrefix("tableI");
            ptrFilter->SetWhereClause("tableI.OID != 5");
            GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable1->Search(ptrFilter);
            SelectData(ptrCursor);
        }

        {

            GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable2->Select("SELECT *  FROM  TestTable_2");
            SelectData(ptrCursor);
        }




    }

    catch(std::exception& exc)
    {
        CommonLib::CExcBase::RegenExc("Failed to select from database", exc);
        throw ;
    }
}