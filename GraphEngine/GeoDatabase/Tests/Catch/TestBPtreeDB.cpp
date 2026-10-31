#include "TestCommon.h"
#include "../../GeoDatabaseBPTreeDB/CBPtreeDBWorkspace.h"
#include "../../../GisGeometry/Envelope.h"
#include "../../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

namespace
{
    void RemoveBPtreeDB(const std::string& sPath)
    {
        for(const char* ext : {"", "-redo", "-tran"})
            std::filesystem::remove(sPath + ext);
    }

    IDatabaseWorkspacePtr CreateDatabase(const std::string& sFileName, std::string& sPath)
    {
        sPath = (TestDir() / sFileName).string();
        RemoveBPtreeDB(sPath);
        return CBPtreeDBWorkspace::Create("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    }

    CBPtreeDBWorkspace* BPtreeDB(IDatabaseWorkspacePtr ptrWorkspace)
    {
        CBPtreeDBWorkspace* pWorkspace = dynamic_cast<CBPtreeDBWorkspace*>(ptrWorkspace.get());
        REQUIRE(pWorkspace != nullptr);
        return pWorkspace;
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
        int64_t nCount = ptrCursor->ReadInt64(0);
        while(ptrCursor->Next()) {}   // the read transaction ends
        return nCount;
    }

    IFieldsPtr CreatePlacesFields()
    {
        IFieldsPtr ptrFields = std::make_shared<CFields>();
        ptrFields->AddField(CreateField("PID", dtInteger64, true));
        ptrFields->AddField(CreateField("Name", dtString));
        ptrFields->AddField(CreateField("Geom", dtGeometry));
        return ptrFields;
    }

    Geometry::ISpatialReferencePtr TestSpatialReference()
    {
        return OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares")->GetSpatialReference();
    }

    CommonLib::bbox MakeBox(double xMin, double yMin, double xMax, double yMax)
    {
        CommonLib::bbox bb;
        bb.type = CommonLib::bbox_type_normal;
        bb.xMin = xMin; bb.yMin = yMin; bb.xMax = xMax; bb.yMax = yMax;
        return bb;
    }

    // points (i, i) for i = 1..10, PID = i, record 11 without geometry
    ITablePtr CreatePlaces(IDatabaseWorkspacePtr ptrWorkspace, const SBPtreeDBSpatialIndexParams& spatialIndex = SBPtreeDBSpatialIndexParams(),
                           bool bWithExtent = true, const std::string& sName = "places")
    {
        Geometry::ISpatialReferencePtr ptrSpatRef = TestSpatialReference();
        Geometry::IEnvelopePtr ptrExtent;
        if(bWithExtent)
            ptrExtent = std::make_shared<Geometry::CEnvelope>(MakeBox(1, 1, 10, 10), ptrSpatRef);

        ITablePtr ptrTable = BPtreeDB(ptrWorkspace)->CreateTableWithSpatialIndex(sName, sName, "", "Geom", "PID", CreatePlacesFields(),
                                                                                CommonLib::shape_type_point, ptrExtent, ptrSpatRef, spatialIndex);

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
        ptrInsert->Finish();
        ptrTransaction->Commit();
        return ptrTable;
    }

    SBPtreeDBSpatialIndexParams ZOrder()
    {
        SBPtreeDBSpatialIndexParams params;
        params.type = bsitZOrder;
        params.nGridCells = 1024;
        return params;
    }

    void CheckPlacesSearch(ITablePtr ptrTable)
    {
        Geometry::ISpatialReferencePtr ptrSpatRef = ptrTable->GetSpatialReference();
        REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(2.5, 2.5, 5.5, 5.5, ptrSpatRef)), "PID") == std::vector<int64_t>{3, 4, 5});
        REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(20, 20, 30, 30, ptrSpatRef)), "PID").empty());
        REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(0, 0, 100, 100, ptrSpatRef)), "PID").size() == 10);
    }
}


TEST_CASE("BPtreeDB: create table, insert, select", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("people.btdb", sPath);
    REQUIRE(ptrWorkspace->GetWorkspaceType() == wtBPtreeDB);

    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());
    REQUIRE(ptrTable->GetDatasetType() == dtTypeTable);
    REQUIRE(ptrTable->GetFields()->GetField("OID")->GetType() == dtInteger64);
    REQUIRE_FALSE(ptrTable->GetFields()->GetField("OID")->GetIsPrimaryKey());   // an int64 key: an ordinary field
    REQUIRE(ptrTable->GetFields()->GetField("Name")->GetType() == dtString);
    REQUIRE(ptrTable->GetFields()->GetField("Score")->GetType() == dtDouble);
    REQUIRE(ptrTable->GetOIDFieldName() == "ROWID");       // the OID is the row id

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

TEST_CASE("BPtreeDB: integer and float types are kept", "[geodatabase][bptreedb]")
{
    std::string sPath;
    {
        IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("types.btdb", sPath);
        IFieldsPtr ptrFields = std::make_shared<CFields>();
        ptrFields->AddField(CreateField("I8", dtInteger8));
        ptrFields->AddField(CreateField("U8", dtUInteger8));
        ptrFields->AddField(CreateField("I16", dtInteger16));
        ptrFields->AddField(CreateField("U16", dtUInteger16));
        ptrFields->AddField(CreateField("I32", dtInteger32));
        ptrFields->AddField(CreateField("U32", dtUInteger32));
        ptrFields->AddField(CreateField("I64", dtInteger64));
        ptrFields->AddField(CreateField("U64", dtUInteger64));
        ptrFields->AddField(CreateField("F", dtFloat));
        ptrFields->AddField(CreateField("D", dtDouble));
        ITablePtr ptrTable = ptrWorkspace->CreateTable("types", "types", ptrFields);

        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        ptrInsert->BindInt8(0, -100);
        ptrInsert->BindUInt8(1, 200);
        ptrInsert->BindInt16(2, -30000);
        ptrInsert->BindUInt16(3, 60000);
        ptrInsert->BindInt32(4, -2000000000);
        ptrInsert->BindUInt32(5, 4000000000u);
        ptrInsert->BindInt64(6, -9000000000000000000ll);
        ptrInsert->BindUInt64(7, 18000000000000000000ull);
        ptrInsert->BindFloat(8, 1.5f);
        ptrInsert->BindDouble(9, 0.1);
        ptrInsert->Next();
        ptrTransaction->Commit();

        // out of the range of the field: bptreedb checks it
        ITransactionPtr ptrBad = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrBadInsert = ptrBad->CreateInsertCusor(ptrTable);
        ptrBadInsert->BindInt32(0, 1000);
        REQUIRE_THROWS(ptrBadInsert->Next());
        ptrBad->Rollback();
    }

    IDatabaseWorkspacePtr ptrWorkspace = CBPtreeDBWorkspace::Open("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    ITablePtr ptrTable = ptrWorkspace->GetTable("types");
    IFieldsPtr ptrFields = ptrTable->GetFields();
    REQUIRE(ptrFields->GetField("I8")->GetType() == dtInteger8);
    REQUIRE(ptrFields->GetField("U8")->GetType() == dtUInteger8);
    REQUIRE(ptrFields->GetField("I16")->GetType() == dtInteger16);
    REQUIRE(ptrFields->GetField("U16")->GetType() == dtUInteger16);
    REQUIRE(ptrFields->GetField("I32")->GetType() == dtInteger32);
    REQUIRE(ptrFields->GetField("U32")->GetType() == dtUInteger32);
    REQUIRE(ptrFields->GetField("I64")->GetType() == dtInteger64);
    REQUIRE(ptrFields->GetField("U64")->GetType() == dtUInteger64);
    REQUIRE(ptrFields->GetField("F")->GetType() == dtFloat);
    REQUIRE(ptrFields->GetField("D")->GetType() == dtDouble);

    ISelectCursorPtr ptrCursor = ptrTable->Search(std::make_shared<CQueryFilter>());
    REQUIRE(ptrCursor->GetColumnType(ptrCursor->FindFieldByName("U16")) == dtUInteger16);
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadInt8(ptrCursor->FindFieldByName("I8")) == -100);
    REQUIRE(ptrCursor->ReadUInt8(ptrCursor->FindFieldByName("U8")) == 200);
    REQUIRE(ptrCursor->ReadInt16(ptrCursor->FindFieldByName("I16")) == -30000);
    REQUIRE(ptrCursor->ReadUInt16(ptrCursor->FindFieldByName("U16")) == 60000);
    REQUIRE(ptrCursor->ReadInt32(ptrCursor->FindFieldByName("I32")) == -2000000000);
    REQUIRE(ptrCursor->ReadUInt32(ptrCursor->FindFieldByName("U32")) == 4000000000u);
    REQUIRE(ptrCursor->ReadInt64(ptrCursor->FindFieldByName("I64")) == -9000000000000000000ll);
    REQUIRE(ptrCursor->ReadUInt64(ptrCursor->FindFieldByName("U64")) == 18000000000000000000ull);
    REQUIRE(ptrCursor->ReadFloat(ptrCursor->FindFieldByName("F")) == 1.5f);
    REQUIRE(ptrCursor->ReadDouble(ptrCursor->FindFieldByName("D")) == 0.1);
    REQUIRE_FALSE(ptrCursor->Next());

    // the row of FillRow keeps the types
    ISelectCursorPtr ptrRows = ptrTable->Search(std::make_shared<CQueryFilter>());
    IRowPtr ptrRow = ptrRows->CreateRow();
    REQUIRE(ptrRows->Next());
    ptrRows->FillRow(ptrRow);
    REQUIRE(ptrRow->GetColumnType(ptrRows->FindFieldByName("F")) == dtFloat);
    while(ptrRows->Next()) {}
}

TEST_CASE("BPtreeDB: rolled back transaction leaves no rows", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("rollback.btdb", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());

    InsertPeople(ptrWorkspace, ptrTable, 3, false);
    REQUIRE(CountRows(ptrWorkspace, "people") == 0);

    InsertPeople(ptrWorkspace, ptrTable, 3, true);
    REQUIRE(CountRows(ptrWorkspace, "people") == 3);

    // an int64 primary key (OID) is an ordinary field: the same value again is a new row
    InsertPeople(ptrWorkspace, ptrTable, 1, true);
    REQUIRE(CountRows(ptrWorkspace, "people") == 4);
}

TEST_CASE("BPtreeDB: one write transaction at a time", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("onewrite.btdb", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());

    ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
    REQUIRE_THROWS(ptrWorkspace->StartTransaction(ttModify));                       // would wait for itself
    REQUIRE_THROWS(ptrWorkspace->CreateTable("t2", "t2", CreatePeopleFields()));  // DDL in a modify transaction
    ptrTransaction->Rollback();

    // a transaction that isn't committed is rolled back by the destructor
    {
        ITransactionPtr ptrTran = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTran->CreateInsertCusor(ptrTable);
        ptrInsert->BindInt64(0, 1);
        ptrInsert->Next();
    }
    REQUIRE(CountRows(ptrWorkspace, "people") == 0);

    // tables can be created in a DDL transaction, the queries of the thread see its changes
    ITransactionPtr ptrDDL = ptrWorkspace->StartTransaction(ttDDL);
    ITablePtr ptrNew = ptrWorkspace->CreateTable("t3", "t3", CreatePeopleFields());
    IInsertCursorPtr ptrInsert = ptrDDL->CreateInsertCusor(ptrNew);
    ptrInsert->BindInt64(0, 7);
    ptrInsert->Next();
    REQUIRE(ReadOids(ptrNew->Search(std::make_shared<CQueryFilter>()), "OID") == std::vector<int64_t>{7});
    ptrDDL->Commit();
    REQUIRE(CountRows(ptrWorkspace, "t3") == 1);
}

TEST_CASE("BPtreeDB: search with where clause and field set", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("search.btdb", sPath);
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

TEST_CASE("BPtreeDB: delete cursor by OID", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("delete.btdb", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());
    InsertPeople(ptrWorkspace, ptrTable, 5, true);

    ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
    IDeleteCursorPtr ptrDelete = ptrTransaction->CreateDeleteCusor(ptrTable);
    ptrDelete->DeleteRow(2);
    ptrDelete->DeleteRow(4);
    ptrTransaction->Commit();

    REQUIRE(ReadOids(ptrTable->Search(std::make_shared<CQueryFilter>()), "OID") == std::vector<int64_t>{1, 3, 5});
}

TEST_CASE("BPtreeDB: spatial table, R-tree and Z-order search, null geometry", "[geodatabase][bptreedb]")
{
    SBPtreeDBSpatialIndexParams params;
    eBPtreeDBSpatialIndexType expected = bsitRTree;
    SECTION("R-tree")
    {
    }
    SECTION("Z-order")
    {
        params = ZOrder();
        expected = bsitZOrder;
    }

    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("places.btdb", sPath);
    ITablePtr ptrTable = CreatePlaces(ptrWorkspace, params);

    eBPtreeDBSpatialIndexType type;
    REQUIRE(BPtreeDB(ptrWorkspace)->GetSpatialIndexType("places", type));
    REQUIRE(type == expected);

    REQUIRE(ptrTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(ptrWorkspace->GetTable("places") == ptrTable);
    REQUIRE(ptrTable->GetShapeFieldName() == "Geom");
    REQUIRE(ptrTable->GetOIDFieldName() == "PID");
    REQUIRE(ptrTable->GetSpatialIndexName() == "places_sidx");
    REQUIRE(ptrTable->GetFields()->GetField("Geom")->GetType() == dtGeometry);
    REQUIRE(CountRows(ptrWorkspace, "places") == 11);

    CheckPlacesSearch(ptrTable);
    // full precision
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(1.0000001, 0, 1.9999999, 100, ptrTable->GetSpatialReference())), "PID").empty());

    // spatial + where: SQL with ST_Intersects
    std::shared_ptr<CQueryFilter> ptrFilter = CreateBBoxFilter(2.5, 2.5, 8.5, 8.5, ptrTable->GetSpatialReference());
    ptrFilter->SetWhereClause("PID > 5");
    REQUIRE(ReadOids(ptrTable->Search(ptrFilter), "PID") == std::vector<int64_t>{6, 7, 8});

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

TEST_CASE("BPtreeDB: Z-order table without an extent gets an R-tree", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("zorder_noextent.btdb", sPath);
    ITablePtr ptrTable = CreatePlaces(ptrWorkspace, ZOrder(), false);

    eBPtreeDBSpatialIndexType type;
    REQUIRE(BPtreeDB(ptrWorkspace)->GetSpatialIndexType("places", type));
    REQUIRE(type == bsitRTree);
    CheckPlacesSearch(ptrTable);
}

TEST_CASE("BPtreeDB: bulk load builds the indexes at the end", "[geodatabase][bptreedb]")
{
    // as the OSM converter: the bulk mode, the tables are created in the open transaction, no extent
    SBPtreeDBSpatialIndexParams params;
    eBPtreeDBSpatialIndexType expected = bsitRTree;
    SECTION("R-tree")
    {
    }
    SECTION("Z-order: the grid covers the inserted shapes")
    {
        params = ZOrder();
        expected = bsitZOrder;
    }

    std::string sPath;
    {
        IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("bulk.btdb", sPath);
        BPtreeDB(ptrWorkspace)->SetDefaultSpatialIndex(params);
        ptrWorkspace->SetBulkLoadMode(true);
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);

        Geometry::ISpatialReferencePtr ptrSpatRef = TestSpatialReference();
        ITablePtr ptrTable = ptrWorkspace->CreateTableWithSpatialIndex("bulk", "bulk", "bulk_sidx", "Geom", "PID", CreatePlacesFields(),
                                                                      CommonLib::shape_type_point, Geometry::IEnvelopePtr(), ptrSpatRef);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        for(int i = 1; i <= 1000; ++i)
        {
            ptrInsert->BindInt64(0, i);
            ptrInsert->BindText(1, "P" + std::to_string(i), true);
            ptrInsert->BindShape(2, CreatePoint(i, i), true);
            ptrInsert->Next();
        }

        // no index yet: a search of the thread checks the boxes by SQL
        REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(2.5, 2.5, 5.5, 5.5, ptrSpatRef)), "PID") == std::vector<int64_t>{3, 4, 5});

        ptrInsert->Finish();
        ptrTable->SetExtent(std::make_shared<Geometry::CEnvelope>(MakeBox(1, 1, 1000, 1000), ptrSpatRef));
        ptrTransaction->Commit();
        ptrWorkspace->SetBulkLoadMode(false);

        eBPtreeDBSpatialIndexType type;
        REQUIRE(BPtreeDB(ptrWorkspace)->GetSpatialIndexType("bulk", type));
        REQUIRE(type == expected);
    }

    IDatabaseWorkspacePtr ptrWorkspace = CBPtreeDBWorkspace::Open("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    ITablePtr ptrTable = ptrWorkspace->GetTable("bulk");
    REQUIRE_FALSE(ptrTable->GetFields()->GetField("PID")->GetIsPrimaryKey());           // an int64 key: no index
    REQUIRE(ptrTable->GetExtent()->GetBoundingBox().xMax == 1000.);
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(10.5, 10.5, 13.5, 13.5, ptrTable->GetSpatialReference())), "PID") == std::vector<int64_t>{11, 12, 13});
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(0, 0, 2000, 2000, ptrTable->GetSpatialReference())), "PID").size() == 1000);
}

TEST_CASE("BPtreeDB: spatial table is restored when the database is reopened", "[geodatabase][bptreedb]")
{
    std::string sPath;
    {
        IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("reopen.btdb", sPath);
        CreatePlaces(ptrWorkspace, ZOrder());
        ptrWorkspace->CreateTable("people", "people", CreatePeopleFields()); // not spatial
    }

    IDatabaseWorkspacePtr ptrWorkspace = CBPtreeDBWorkspace::Open("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    REQUIRE(BPtreeDB(ptrWorkspace)->GetSpatialTableNames() == std::vector<std::string>{"places"});
    REQUIRE(ptrWorkspace->GetDatasetCount() == 1); // spatial tables are loaded on open

    ITablePtr ptrTable = ptrWorkspace->GetTable("places");
    REQUIRE(ptrTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(ptrTable->GetShapeFieldName() == "Geom");
    REQUIRE(ptrTable->GetOIDFieldName() == "PID");
    REQUIRE(ptrTable->GetGeometryType() == CommonLib::shape_type_point);
    REQUIRE(ptrTable->GetExtent()->GetBoundingBox().xMax == 10.);
    REQUIRE(ptrTable->GetSpatialReference()->IsValid());
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(6.5, 6.5, 7.5, 7.5, ptrTable->GetSpatialReference())), "PID") == std::vector<int64_t>{7});

    eBPtreeDBSpatialIndexType type;
    REQUIRE(BPtreeDB(ptrWorkspace)->GetSpatialIndexType("places", type));
    REQUIRE(type == bsitZOrder);

    REQUIRE(ptrWorkspace->GetTable("people")->GetDatasetType() == dtTypeTable);
    REQUIRE_THROWS(ptrWorkspace->GetTable("missing"));
}

TEST_CASE("BPtreeDB: shapes are given in the output coordinate system of the filter", "[geodatabase][bptreedb]")
{
    Geometry::ISpatialReferencePtr ptrUtm = std::make_shared<Geometry::CSpatialReferenceProj4>(std::string("+proj=utm +zone=44 +datum=WGS84 +units=m +no_defs"));
    Geometry::ISpatialReferencePtr ptrMerc = std::make_shared<Geometry::CSpatialReferenceProj4>(
            std::string("+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +no_defs"));

    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("project_output.btdb", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTableWithSpatialIndex("utm", "utm", "", "Geom", "PID", CreatePlacesFields(),
                                                                  CommonLib::shape_type_point,
                                                                  std::make_shared<Geometry::CEnvelope>(MakeBox(599000, 6079000, 601000, 6081000), ptrUtm), ptrUtm);
    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        ptrInsert->BindInt64(ptrTable->GetFields()->FindField("PID"), 1);
        ptrInsert->BindText(ptrTable->GetFields()->FindField("Name"), "p", true);
        ptrInsert->BindShape(ptrTable->GetFields()->FindField("Geom"), CreatePoint(600000, 6080000), true);
        ptrInsert->Next();
        ptrTransaction->Commit();
    }

    CommonLib::IGeoShapePtr ptrExpected = CreatePoint(600000, 6080000);
    REQUIRE(ptrUtm->Project(ptrMerc, ptrExpected));
    double x = ptrExpected->GetPoints()[0].x, y = ptrExpected->GetPoints()[0].y;
    REQUIRE(std::fabs(x - 600000) > 1000.);

    ISelectCursorPtr ptrCursor = ptrTable->Search(CreateBBoxFilter(x - 100, y - 100, x + 100, y + 100, ptrMerc));
    int32_t nGeom = ptrCursor->FindFieldByName("Geom");
    REQUIRE(ptrCursor->Next());
    CommonLib::IGeoShapePtr ptrShape = ptrCursor->ReadShape(nGeom);
    REQUIRE(std::fabs(ptrShape->GetPoints()[0].x - x) < 0.01);
    REQUIRE(std::fabs(ptrShape->GetPoints()[0].y - y) < 0.01);
    REQUIRE_FALSE(ptrCursor->Next());

    // the same output system - no projection, the stored shape isn't changed by the projection above
    ISelectCursorPtr ptrSame = ptrTable->Search(CreateBBoxFilter(599900, 6079900, 600100, 6080100, ptrUtm));
    REQUIRE(ptrSame->Next());
    REQUIRE(ptrSame->ReadShape(nGeom)->GetPoints()[0].x == 600000.);
    REQUIRE_FALSE(ptrSame->Next());
}

TEST_CASE("BPtreeDB: empty spatial table has no extent after the database is reopened", "[geodatabase][bptreedb]")
{
    std::string sPath;
    {
        IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("empty_extent.btdb", sPath);
        Geometry::ISpatialReferencePtr ptrSpatRef = TestSpatialReference();
        ptrWorkspace->CreateTableWithSpatialIndex("empty", "empty", "", "Geom", "PID", CreatePlacesFields(),
                                                  CommonLib::shape_type_point, std::make_shared<Geometry::CEnvelope>(CommonLib::bbox(), ptrSpatRef), ptrSpatRef);
    }

    IDatabaseWorkspacePtr ptrWorkspace = CBPtreeDBWorkspace::Open("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    Geometry::IEnvelopePtr ptrExtent = ptrWorkspace->GetTable("empty")->GetExtent();
    REQUIRE((!ptrExtent.get() || !(ptrExtent->GetBoundingBox().type & CommonLib::bbox_type_normal)));
}

TEST_CASE("BPtreeDB: workspace save / load, table loader", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("saveload.btdb", sPath);
    ITablePtr ptrTable = CreatePlaces(ptrWorkspace);
    REQUIRE(ptrTable->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrWorkspace->Save(ptrRoot->CreateChildNode("Workspace"));
    ptrTable->Save(ptrRoot->CreateChildNode("Table"));
    ptrTable.reset();
    ptrWorkspace.reset();   // closes the database: one open of a file at a time

    IWorkspacePtr ptrLoaded = CDatasetLoader::LoadWorkspace(ptrRoot->GetChild("Workspace"));
    REQUIRE(ptrLoaded->GetWorkspaceType() == wtBPtreeDB);
    REQUIRE(ptrLoaded->GetDatasetCount() == 1);

    REQUIRE_THROWS(CDatasetLoader::LoadTable(ptrRoot->GetChild("Table")));
    CWorkspaceHolder::AddWorkspace(ptrLoaded);
    ITablePtr ptrLoadedTable = CDatasetLoader::LoadTable(ptrRoot->GetChild("Table"));
    REQUIRE(ptrLoadedTable->GetDatasetName() == "places");
    REQUIRE(ptrLoadedTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(CountRows(std::dynamic_pointer_cast<IDatabaseWorkspace>(ptrLoaded), "places") == 11);
    CWorkspaceHolder::Clear();
}

TEST_CASE("BPtreeDB: space statistics (SQL system tables)", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("space.btdb", sPath);
    CreatePlaces(ptrWorkspace);

    bptreedb::IDatabasePtr ptrDb = BPtreeDB(ptrWorkspace)->GetDatabase();
    bptreedb::ITransactionPtr ptrTran = ptrDb->StartTransaction(bptreedb::eTT_SELECT, bptreedb::eTTDirectRead);

    // the API: a tree per rows / values / index
    bptreedb::SDatabaseSpace space;
    ptrTran->GetSpaceStatistics(space, "places", true);
    REQUIRE(space.m_nPageSize == 8192);
    REQUIRE(space.m_nFilePages > 0);
    int nSpatial = 0, nValues = 0;
    uint64_t nPages = 0;
    for(const bptreedb::SSpaceInfo& info : space.m_trees)
    {
        REQUIRE(info.m_sTable == "places");
        REQUIRE(info.m_nLeafPages >= 1);
        nPages += info.Pages();
        nSpatial += info.m_sKind == "spatial index R-tree" ? 1 : 0;
        nValues += info.m_sKind == "values" ? 1 : 0;
    }
    REQUIRE(nSpatial == 1);
    REQUIRE(nValues == 3);
    REQUIRE(nPages <= space.m_nFilePages);

    // SQL
    bptreedb::ISelectCursorPtr ptrCursor = ptrTran->executeQuery(
        "SELECT object, kind, pages FROM bptreedb_space WHERE table_name = 'places' AND kind = 'values' ORDER BY object");
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadText(0) == "Geom");
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadText(0) == "Name");
    REQUIRE(ptrCursor->ReadInt64(2) >= 2);
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadText(0) == "PID");
    REQUIRE_FALSE(ptrCursor->Next());

    ptrCursor = ptrTran->executeQuery("SELECT SUM(pages) FROM bptreedb_space_full WHERE kind = 'file'");
    REQUIRE(ptrCursor->Next());
    REQUIRE((uint64_t)ptrCursor->ReadInt64(0) == space.m_nFilePages);
    while(ptrCursor->Next()) {}
    ptrCursor.reset();
    ptrTran->Commit();
}

TEST_CASE("BPtreeDB / SQLite: the table copy compresses the geometry", "[geodatabase][bptreedb][copy]")
{
    bool bBPtreeDB = true;
    SECTION("BPtreeDB")
    {
    }
    SECTION("SQLite")
    {
        bBPtreeDB = false;
    }

    // squares 10x10 at x = 0, 20, 40, 60, 80 in degrees (WGS 84): 1e-7 degree
    ITablePtr ptrShape = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    std::string sPath;
    IDatabaseWorkspacePtr ptrDb;
    if(bBPtreeDB)
        ptrDb = CreateDatabase("copy_compressed.btdb", sPath);
    else
    {
        sPath = (TestDir() / "copy_compressed.sqlite").string();
        RemoveDatabase(sPath);
        ptrDb = CSQLiteWorkspace::Create("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    }

    CTableCopier::SCopyOptions options;
    options.bCompressGeometry = true;
    ITablePtr ptrCopy = CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares", CTableCopier::TProgress(), 1000, options);

    // the blobs are compressed, the shapes and the spatial index are the same
    ISelectCursorPtr ptrCursor = ptrCopy->Search(CreateBBoxFilter(15, -5, 45, 15, ptrCopy->GetSpatialReference()));
    int32_t nShape = ptrCursor->FindFieldByName("Shape");
    int nRows = 0;
    while(ptrCursor->Next())
    {
        ++nRows;
        byte_t* pBuf = nullptr;
        int32_t nSize = 0;
        ptrCursor->ReadBlob(nShape, &pBuf, nSize);
        REQUIRE(CommonLib::CGeoShape::IsCompressed(pBuf, (uint32_t)nSize));
        CommonLib::bbox bb = ptrCursor->ReadShape(nShape)->GetBB();
        REQUIRE((bb.xMin == 20. || bb.xMin == 40.));
        REQUIRE(bb.xMax - bb.xMin == 10.);
    }
    REQUIRE(nRows == 2);
}

TEST_CASE("BPtreeDB: an int64 primary key (OID) is an ordinary field", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("oidfield.btdb", sPath);
    ITablePtr ptrTable = ptrWorkspace->CreateTable("people", "people", CreatePeopleFields());
    int32_t nOid = ptrTable->GetFields()->FindField("OID");
    int32_t nName = ptrTable->GetFields()->FindField("Name");

    // the values of the source in any order: the row ids are 1, 2, 3, ...
    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        for(int64_t oid : {10, 5, 7})
        {
            ptrInsert->BindInt64(nOid, oid);
            ptrInsert->BindText(nName, "Name_" + std::to_string(oid), true);
            ptrInsert->Next();
        }
        ptrInsert->Finish();
        ptrTransaction->Commit();
    }

    // a filter by it: a scan of the rows
    ISelectCursorPtr ptrCursor = ptrTable->Search(std::make_shared<CQueryFilter>("OID = 7"));
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadText(ptrCursor->FindFieldByName("Name")) == "Name_7");
    REQUIRE_FALSE(ptrCursor->Next());
    ptrCursor.reset();

    ptrCursor = ptrTable->Select("SELECT ROWID FROM people WHERE OID = 7");
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadInt64(0) == 3);
    REQUIRE_FALSE(ptrCursor->Next());
    ptrCursor.reset();

    // the row id in SQL as in SQLite: ROWID, _ROWID_, OID (here OID is the field of the table)
    for(const char* sql : {"SELECT Name FROM people WHERE ROWID = 2", "SELECT Name FROM people WHERE _rowid_ = 2", "SELECT Name FROM people WHERE OID = 5"})
    {
        ptrCursor = ptrTable->Select(sql);
        REQUIRE(ptrCursor->Next());
        REQUIRE(ptrCursor->ReadText(0) == "Name_5");
        REQUIRE_FALSE(ptrCursor->Next());
        ptrCursor.reset();
    }

    // the OID of the table: the row id as the column ROWID - by the API of bptreedb and by SQL (a where clause)
    REQUIRE(ptrTable->GetOIDFieldName() == "ROWID");
    for(const char* sWhere : {"", "Name <> 'x'"})
    {
        std::shared_ptr<CQueryFilter> ptrFilter = std::make_shared<CQueryFilter>(sWhere);
        ptrFilter->GetFieldSet()->Add("Name");
        ptrFilter->GetFieldSet()->Add(ptrTable->GetOIDFieldName());
        REQUIRE(ReadOids(ptrTable->Search(ptrFilter), "ROWID") == std::vector<int64_t>{1, 2, 3});
    }

    // the delete cursor deletes by the row id
    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IDeleteCursorPtr ptrDelete = ptrTransaction->CreateDeleteCusor(ptrTable);
        ptrDelete->DeleteRow(1);
        ptrTransaction->Commit();
    }
    REQUIRE(ReadOids(ptrTable->Search(std::make_shared<CQueryFilter>()), "OID") == std::vector<int64_t>{5, 7});

    // no index of the field
    bptreedb::ITransactionPtr ptrTran = BPtreeDB(ptrWorkspace)->GetDatabase()->StartTransaction(bptreedb::eTT_SELECT, bptreedb::eTTDirectRead);
    bptreedb::SDatabaseSpace space;
    ptrTran->GetSpaceStatistics(space, "people", false);
    for(const bptreedb::SSpaceInfo& info : space.m_trees)
        REQUIRE(info.m_sKind.find("index") == std::string::npos);
    ptrTran->Commit();
}

TEST_CASE("BPtreeDB: a spatial table without an OID field: the row id", "[geodatabase][bptreedb]")
{
    std::string sPath;
    IDatabaseWorkspacePtr ptrWorkspace = CreateDatabase("nooid.btdb", sPath);
    IFieldsPtr ptrFields = std::make_shared<CFields>();
    ptrFields->AddField(CreateField("Name", dtString));
    ptrFields->AddField(CreateField("Geom", dtGeometry));
    Geometry::ISpatialReferencePtr ptrSpatRef = TestSpatialReference();
    ITablePtr ptrTable = ptrWorkspace->CreateTableWithSpatialIndex("pts", "pts", "", "Geom", "", ptrFields, CommonLib::shape_type_point,
                                                                  Geometry::IEnvelopePtr(), ptrSpatRef);
    REQUIRE(ptrTable->GetOIDFieldName() == "ROWID");
    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTable);
        std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
        for(int i = 0; i < 5; ++i)
        {
            ptrShape->Create(CommonLib::shape_type_point, 1);
            ptrShape->GetPoints()[0].x = 10 + i;
            ptrShape->GetPoints()[0].y = 10 + i;
            ptrShape->CalcBB();
            ptrInsert->BindText(0, "p" + std::to_string(i + 1), true);
            ptrInsert->BindShape(1, ptrShape, true);
            ptrInsert->Next();
        }
        ptrTransaction->Commit();
    }

    // a spatial search with the OID field (as the map layers ask for it)
    auto ptrFilter = CreateBBoxFilter(10.5, 10.5, 13.5, 13.5, ptrSpatRef);
    ptrFilter->GetFieldSet()->Add(ptrTable->GetOIDFieldName());
    REQUIRE(ReadOids(ptrTable->Search(ptrFilter), "ROWID") == std::vector<int64_t>{2, 3, 4});

    {
        ITransactionPtr ptrTransaction = ptrWorkspace->StartTransaction(ttModify);
        IDeleteCursorPtr ptrDelete = ptrTransaction->CreateDeleteCusor(ptrTable);
        ptrDelete->DeleteRow(3);
        ptrTransaction->Commit();
    }
    ptrFilter = CreateBBoxFilter(0, 0, 100, 100, ptrSpatRef);
    ptrFilter->SetWhereClause("ROWID IN (1, 3, 5)");
    ptrFilter->GetFieldSet()->Add("ROWID");
    REQUIRE(ReadOids(ptrTable->Search(ptrFilter), "ROWID") == std::vector<int64_t>{1, 5});
}
