#include "TestCommon.h"
#include "../../../GisGeometry/Envelope.h"

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

namespace
{
    IDatabaseWorkspacePtr CreateDatabase(const std::string& sFileName, std::string& sPath)
    {
        sPath = (TestDir() / sFileName).string();
        RemoveDatabase(sPath);
        return CSQLiteWorkspace::Create("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    }

    IFieldsPtr CreatePeopleFields()
    {
        IFieldsPtr ptrFields = std::make_shared<CFields>();
        ptrFields->AddField(CreateField("OID", dtInteger64, true));
        ptrFields->AddField(CreateField("Name", dtString));
        ptrFields->AddField(CreateField("Score", dtDouble));
        return ptrFields;
    }

    void InsertPeople(IDatabaseWorkspacePtr ptrWorkspace, ITablePtr ptrTable, int nCount, bool bCommit)
    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        int32_t nOid = ptrTable->GetFields()->FindField("OID");
        int32_t nName = ptrTable->GetFields()->FindField("Name");
        int32_t nScore = ptrTable->GetFields()->FindField("Score");
        for(int i = 0; i < nCount; ++i)
        {
            ptrInsert->BindInt64(nOid, i + 1);
            ptrInsert->BindText(nName, "Name_" + std::to_string(i + 1), true);
            ptrInsert->BindDouble(nScore, i * 0.5);
            ptrInsert->Next();
        }
        if(bCommit)
            ptrTransaction->Commit();
        else
            ptrTransaction->Rollback();
    }

    int64_t CountRows(IDatabaseWorkspacePtr ptrWorkspace, const std::string& sTable)
    {
        ISelectCursorPtr ptrCursor = ptrWorkspace->GetTable(sTable)->Select("SELECT COUNT(*) FROM " + sTable);
        REQUIRE(ptrCursor->Next());
        return ptrCursor->ReadInt64(0);
    }

    IFieldsPtr CreatePlacesFields()
    {
        IFieldsPtr ptrFields = std::make_shared<CFields>();
        ptrFields->AddField(CreateField("PID", dtInteger64, true));
        ptrFields->AddField(CreateField("Name", dtString));
        ptrFields->AddField(CreateField("Geom", dtGeometry));
        return ptrFields;
    }

    // points (i, i) for i = 1..10, PID = i, record 11 without geometry
    ITablePtr CreatePlaces(IDatabaseWorkspacePtr ptrWorkspace)
    {
        CommonLib::bbox extent;
        extent.type = CommonLib::bbox_type_normal;
        extent.xMin = extent.yMin = 1;
        extent.xMax = extent.yMax = 10;
        Geometry::ISpatialReferencePtr ptrSpatRef = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares")->GetSpatialReference();

        ITablePtr ptrTable = ptrWorkspace->CreateTableWithSpatialIndex("places", "places", "", "Geom", "PID", CreatePlacesFields(),
                                                                      CommonLib::shape_type_point, std::make_shared<Geometry::CEnvelope>(extent, ptrSpatRef), ptrSpatRef);

        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        int32_t nPid = ptrTable->GetFields()->FindField("PID");
        int32_t nName = ptrTable->GetFields()->FindField("Name");
        int32_t nGeom = ptrTable->GetFields()->FindField("Geom");
        for(int i = 1; i <= 11; ++i)
        {
            ptrInsert->BindInt64(nPid, i);
            ptrInsert->BindText(nName, "Place_" + std::to_string(i), true);
            ptrInsert->BindShape(nGeom, i <= 10 ? CreatePoint(i, i) : CommonLib::IGeoShapePtr(), true);
            ptrInsert->Next();
        }
        ptrTransaction->Commit();
        return ptrTable;
    }
}


TEST_CASE("SQLite: create table, insert, select", "[geodatabase][sqlite]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("people.sqlite", sPath);
    REQUIRE(ptrWorkspace->GetWorkspaceType() == wtSqlLite);

    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());
    REQUIRE(ptrTable->GetDatasetType() == dtTypeTable);
    REQUIRE(ptrTable->GetFields()->GetField("OID")->GetType() == dtInteger64);
    REQUIRE(ptrTable->GetFields()->GetField("OID")->GetIsPrimaryKey());
    REQUIRE(ptrTable->GetFields()->GetField("Name")->GetType() == dtString);
    REQUIRE(ptrTable->GetFields()->GetField("Score")->GetType() == dtDouble);

    InsertPeople(ptrWorkspace, ptrTable, 5, true);
    REQUIRE(CountRows(ptrWorkspace, "people") == 5);

    ISelectCursorPtr ptrCursor = ptrTable->Select("SELECT OID, Name, Score FROM people ORDER BY OID");
    int n = 0;
    while(ptrCursor->Next())
    {
        ++n;
        REQUIRE(ptrCursor->ReadInt64(0) == n);
        REQUIRE(ptrCursor->ReadText(1) == "Name_" + std::to_string(n));
        REQUIRE(ptrCursor->ReadDouble(2) == (n - 1) * 0.5);
    }
    REQUIRE(n == 5);
}

TEST_CASE("SQLite: rolled back transaction leaves no rows", "[geodatabase][sqlite]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("rollback.sqlite", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());

    InsertPeople(ptrWorkspace, ptrTable, 3, false);
    REQUIRE(CountRows(ptrWorkspace, "people") == 0);

    InsertPeople(ptrWorkspace, ptrTable, 3, true);
    REQUIRE(CountRows(ptrWorkspace, "people") == 3);
}

TEST_CASE("SQLite: search with where clause and field set", "[geodatabase][sqlite]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("search.sqlite", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());
    InsertPeople(ptrWorkspace, ptrTable, 6, true);

    std::shared_ptr<CQueryFilter> ptrFilter = std::make_shared<CQueryFilter>("Score >= 1.5");
    ptrFilter->GetFieldSet()->Add("OID");
    ptrFilter->GetFieldSet()->Add("Name");

    ISelectCursorPtr ptrCursor = ptrTable->Search(ptrFilter);
    REQUIRE(ptrCursor->ColumnCount() == 2);                  // columns are known before Next
    REQUIRE(ptrCursor->FindFieldByName("Score") == -1);
    REQUIRE(ptrCursor->GetColumnType(ptrCursor->FindFieldByName("Name")) == dtString);

    std::vector<int64_t> oids = ReadOids(ptrCursor, "OID");
    REQUIRE(oids == std::vector<int64_t>{4, 5, 6});
}

TEST_CASE("SQLite: spatial table, R-tree search, null geometry", "[geodatabase][sqlite]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("places.sqlite", sPath);
    ITablePtr ptrTable = CreatePlaces(ptrWorkspace);

    REQUIRE(ptrTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(ptrWorkspace->GetTable("places") == ptrTable);
    REQUIRE(ptrTable->GetShapeFieldName() == "Geom");
    REQUIRE(ptrTable->GetOIDFieldName() == "PID");
    REQUIRE(ptrTable->GetSpatialIndexName() == "places_sidx");
    REQUIRE(ptrTable->GetFields()->GetField("Geom")->GetType() == dtGeometry);
    REQUIRE(CountRows(ptrWorkspace, "places") == 11);
    REQUIRE(CountRows(ptrWorkspace, "places_sidx") == 10); // the record without geometry isn't indexed

    Geometry::ISpatialReferencePtr ptrSpatRef = ptrTable->GetSpatialReference();
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(2.5, 2.5, 5.5, 5.5, ptrSpatRef)), "PID") == std::vector<int64_t>{3, 4, 5});
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(1.0000001, 0, 1.9999999, 100, ptrSpatRef)), "PID").empty()); // full precision
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(20, 20, 30, 30, ptrSpatRef)), "PID").empty());

    // without spatial restriction - all rows, including the one without geometry
    ISelectCursorPtr ptrAll = ptrTable->Search(std::make_shared<CQueryFilter>());
    int32_t nGeom = ptrAll->FindFieldByName("Geom");
    int32_t nPid = ptrAll->FindFieldByName("PID");
    REQUIRE(ptrAll->GetColumnType(nGeom) == dtGeometry);
    int nRows = 0;
    IRowPtr ptrRow = ptrAll->CreateRow();
    while(ptrAll->Next())
    {
        ++nRows;
        ptrAll->FillRow(ptrRow);
        int64_t pid = ptrAll->ReadInt64(nPid);
        if(pid == 11)
        {
            REQUIRE(ptrAll->ColumnIsNull(nGeom));
            REQUIRE(ptrRow->ColumnIsNull(nGeom));
        }
        else
        {
            CommonLib::IGeoShapePtr ptrShape = ptrAll->ReadShape(nGeom);
            REQUIRE(ptrShape->GetPoints()[0].x == (double)pid);
        }
    }
    REQUIRE(nRows == 11);
}

TEST_CASE("SQLite: spatial table is restored when the database is reopened", "[geodatabase][sqlite]")
{
    std::string sPath;
    {
        IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("reopen.sqlite", sPath);
        CreatePlaces(ptrWorkspace);
        ptrWorkspace->CreateTable("people", "people", CreatePeopleFields()); // not spatial
    }

    IDatabaseWorkspacePtr ptrWorkspace = CSQLiteWorkspace::Open("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    REQUIRE(dynamic_cast<CSQLiteWorkspace*>(ptrWorkspace.get())->GetSpatialTableNames() == std::vector<std::string>{"places"});
    REQUIRE(ptrWorkspace->GetDatasetCount() == 1); // spatial tables are loaded on open

    ITablePtr ptrTable = ptrWorkspace->GetTable("places");
    REQUIRE(ptrTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(ptrTable->GetShapeFieldName() == "Geom");
    REQUIRE(ptrTable->GetOIDFieldName() == "PID");
    REQUIRE(ptrTable->GetGeometryType() == CommonLib::shape_type_point);
    REQUIRE(ptrTable->GetExtent()->GetBoundingBox().xMax == 10.);
    REQUIRE(ptrTable->GetSpatialReference()->IsValid());
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(6.5, 6.5, 7.5, 7.5, ptrTable->GetSpatialReference())), "PID") == std::vector<int64_t>{7});

    REQUIRE(ptrWorkspace->GetTable("people")->GetDatasetType() == dtTypeTable);
}

TEST_CASE("SQLite: workspace save / load, table loader", "[geodatabase][sqlite]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("saveload.sqlite", sPath);
    ITablePtr ptrTable = CreatePlaces(ptrWorkspace);
    REQUIRE(ptrTable->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrWorkspace->Save(ptrRoot->CreateChildNode("Workspace"));
    ptrTable->Save(ptrRoot->CreateChildNode("Table"));

    IWorkspacePtr ptrLoaded = CDatasetLoader::LoadWorkspace(ptrRoot->GetChild("Workspace"));
    REQUIRE(ptrLoaded->GetWorkspaceType() == wtSqlLite);
    REQUIRE(ptrLoaded->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());
    REQUIRE(ptrLoaded->GetDatasetCount() == 1);

    // the table is found through the registered workspace
    REQUIRE_THROWS(CDatasetLoader::LoadTable(ptrRoot->GetChild("Table")));
    CWorkspaceHolder::AddWorkspace(ptrLoaded);
    ITablePtr ptrLoadedTable = CDatasetLoader::LoadTable(ptrRoot->GetChild("Table"));
    REQUIRE(ptrLoadedTable->GetDatasetName() == "places");
    REQUIRE(ptrLoadedTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(CountRows(std::dynamic_pointer_cast<IDatabaseWorkspace>(ptrLoaded), "places") == 11);
    CWorkspaceHolder::Clear();
}
