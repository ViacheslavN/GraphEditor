#include "TestCommon.h"
#include "../../Join.h"
#include "../../SQLRowFilter/SQLRowFilter.h"

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

namespace
{
    IRowPtr CreateRoadRow()
    {
        IFieldsPtr ptrFields = std::make_shared<CFields>();
        ptrFields->AddField(CreateField("OID", dtInteger64, true));
        ptrFields->AddField(CreateField("Name", dtString));
        ptrFields->AddField(CreateField("Pop", dtInteger32));
        ptrFields->AddField(CreateField("Area", dtDouble));
        ptrFields->AddField(CreateField("t2.Type", dtString));   // a column of a join, as a joined cursor names it
        ptrFields->AddField(CreateField("Note", dtString));

        IRowPtr ptrRow = std::make_shared<CRow>(ptrFields);
        ptrRow->SetInt64(0, 1);
        ptrRow->SetText(1, "Main Street");
        ptrRow->SetInt32(2, 1500);
        ptrRow->SetDouble(3, 2.5);
        ptrRow->SetText(4, "road");
        ptrRow->SetNull(5);
        return ptrRow;
    }

    bool Eval(const std::string& sExpr, IRowPtr ptrRow)
    {
        return CSQLRowFilter::Create(sExpr)->EvaluateRow(ptrRow);
    }
}

TEST_CASE("SQLRowFilter: comparisons, logic and NULL", "[geodatabase][sqlrowfilter]")
{
    IRowPtr ptrRow = CreateRoadRow();

    REQUIRE(Eval("Pop > 1000 AND Name LIKE 'main%'", ptrRow));
    REQUIRE(Eval("pop >= 1500 and NAME like '%STREET'", ptrRow));   // names and LIKE ignore case
    REQUIRE(Eval("Area BETWEEN 2 AND 3", ptrRow));
    REQUIRE(Eval("OID IN (1, 2, 3)", ptrRow));
    REQUIRE(Eval("Name <> 'Elm' OR Pop < 0", ptrRow));
    REQUIRE(Eval("Note IS NULL", ptrRow));
    REQUIRE(Eval("COALESCE(Note, 'n/a') = 'n/a'", ptrRow));
    REQUIRE(Eval("Pop * 2 + 1 = 3001", ptrRow));
    REQUIRE(Eval("Name || '!' = 'Main Street!'", ptrRow));

    REQUIRE_FALSE(Eval("Note = 'x'", ptrRow));            // NULL: unknown, doesn't pass
    REQUIRE_FALSE(Eval("NOT (Note = 'x')", ptrRow));      // still unknown
    REQUIRE_FALSE(Eval("Pop NOT BETWEEN 1000 AND 2000", ptrRow));
}

TEST_CASE("SQLRowFilter: qualified fields of a join", "[geodatabase][sqlrowfilter]")
{
    IRowPtr ptrRow = CreateRoadRow();

    ISQLRowFilterPtr ptrFilter = CSQLRowFilter::Create("t2.Type = 'road' AND t1.Pop > 10");
    REQUIRE(ptrFilter->EvaluateRow(ptrRow));    // t1.Pop: no column "t1.Pop", the main column "Pop"

    REQUIRE(ptrFilter->GetFieldCount() == 2);
    REQUIRE(ptrFilter->GetFieldPrefix(0) == "t2");
    REQUIRE(ptrFilter->GetFieldName(0) == "Type");
    REQUIRE(ptrFilter->GetFieldPrefix(1) == "t1");
    REQUIRE(ptrFilter->GetFieldName(1) == "Pop");

    REQUIRE(Eval("\"t2\".[Type] IN ('rail', 'road')", ptrRow));
}

TEST_CASE("SQLRowFilter: expression errors", "[geodatabase][sqlrowfilter]")
{
    IRowPtr ptrRow = CreateRoadRow();

    REQUIRE_THROWS(CSQLRowFilter::Create("Pop >"));
    REQUIRE_THROWS(CSQLRowFilter::Create("Name = 'abc"));
    REQUIRE_THROWS(CSQLRowFilter::Create("NoSuchFunc(Pop)"));

    // a wrong expression leaves the filter as it was
    ISQLRowFilterPtr ptrFilter = CSQLRowFilter::Create("Pop = 1500");
    REQUIRE_THROWS(ptrFilter->SetSQLExpression("Pop = = 1"));
    REQUIRE(ptrFilter->GetSQLExpression() == "Pop = 1500");
    REQUIRE(ptrFilter->EvaluateRow(ptrRow));

    // an empty expression: every row
    ptrFilter->SetSQLExpression("  ");
    REQUIRE(ptrFilter->EvaluateRow(ptrRow));

    // a field missing in the row
    REQUIRE_THROWS(Eval("Missing = 1", ptrRow));
}

TEST_CASE("SQLRowFilter: rows of a SQLite cursor with a join", "[geodatabase][sqlrowfilter][sqlite]")
{
    std::string sPath = (TestDir() / "sqlrowfilter_join.sqlite").string();
    RemoveDatabase(sPath);
    IDatabaseWorkspacePtr ptrWorkspace = CSQLiteWorkspace::Create("test", sPath.c_str(), CommonLib::CGuid::CreateNew());

    IFieldsPtr ptrRoadFields = std::make_shared<CFields>();
    ptrRoadFields->AddField(CreateField("OID", dtInteger64, true));
    ptrRoadFields->AddField(CreateField("TypeID", dtInteger64));
    ptrRoadFields->AddField(CreateField("Name", dtString));
    ITablePtr ptrRoads = ptrWorkspace->CreateTable("roads", "roads", ptrRoadFields);

    IFieldsPtr ptrTypeFields = std::make_shared<CFields>();
    ptrTypeFields->AddField(CreateField("TID", dtInteger64, true));
    ptrTypeFields->AddField(CreateField("Name", dtString));   // the same name as in roads
    ITablePtr ptrTypes = ptrWorkspace->CreateTable("road_types", "road_types", ptrTypeFields);

    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTypes);
        const char* types[] = {"highway", "street"};
        for(int i = 0; i < 2; ++i)
        {
            ptrInsert->BindInt64(0, i + 1);
            ptrInsert->BindText(1, types[i], true);
            ptrInsert->Next();
        }

        ptrInsert = ptrTransaction->CreateInsertCusor(ptrRoads);
        for(int i = 0; i < 6; ++i)
        {
            ptrInsert->BindInt64(0, i + 1);
            ptrInsert->BindInt64(1, i % 2 + 1);
            ptrInsert->BindText(2, "Road_" + std::to_string(i + 1), true);
            ptrInsert->Next();
        }
        ptrTransaction->Commit();
    }

    IJoinPtr ptrJoin = std::make_shared<CJoin>();
    ptrJoin->SetTableName("road_types");
    ptrJoin->SetTablePrefix("t2");
    ptrJoin->SetFirstField("TID");
    ptrJoin->SetSecondField("TypeID");

    IQueryFilterPtr ptrQuery = std::make_shared<CQueryFilter>();
    ptrQuery->SetTablePrefix("t1");
    ptrQuery->SetJoins({ptrJoin});
    ptrQuery->GetFieldSet()->Add("OID");
    ptrQuery->GetFieldSet()->Add("Name");
    ptrJoin->GetFieldSet()->Add("Name");   // what the expression selector adds for t2.Name

    ISQLRowFilterPtr ptrFilter = CSQLRowFilter::Create("t2.Name = 'highway' AND Name <> 'Road_5'");

    ISelectCursorPtr ptrCursor = ptrRoads->Search(ptrQuery);
    IRowPtr ptrRow = ptrCursor->CreateRow();
    std::vector<int64_t> vecPassed;
    while(ptrCursor->Next())
    {
        ptrCursor->FillRow(ptrRow);
        if(ptrFilter->EvaluateRow(ptrRow))
            vecPassed.push_back(ptrCursor->ReadInt64(0));
    }

    // highway: roads 1, 3, 5; road 5 is out by its own name
    REQUIRE(vecPassed == std::vector<int64_t>{1, 3});

    ptrCursor.reset();
    ptrRoads.reset();
    ptrTypes.reset();
    ptrWorkspace.reset();
    RemoveDatabase(sPath);
}
