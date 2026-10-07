#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

TEST_CASE("Shapefile table: fields, geometry type, extent", "[geodatabase][shapefile]")
{
    ITablePtr ptrTable = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");

    REQUIRE(ptrTable->GetDatasetType() == dtSpatialTable);
    REQUIRE(ptrTable->GetGeometryType() == CommonLib::shape_type_polygon);
    REQUIRE(ptrTable->GetShapeFieldName() == "Shape");
    REQUIRE(ptrTable->GetOIDFieldName() == "FID");

    IFieldsPtr ptrFields = ptrTable->GetFields();
    REQUIRE(ptrFields->GetField("NAME")->GetType() == dtString);
    REQUIRE(ptrFields->GetField("CODE")->GetType() == dtInteger32);
    REQUIRE(ptrFields->GetField("VALUE")->GetType() == dtDouble);
    REQUIRE(ptrFields->GetField("Shape")->GetType() == dtGeometry);
    REQUIRE(ptrFields->GetField("FID")->GetType() == dtInteger64);

    const CommonLib::bbox& bb = ptrTable->GetExtent()->GetBoundingBox();
    REQUIRE(bb.xMin == 0.);
    REQUIRE(bb.xMax == 90.);
    REQUIRE(bb.yMax == 10.);

    REQUIRE(ptrTable->GetSpatialReference() != nullptr);
    REQUIRE(ptrTable->GetSpatialReference()->IsValid());
}

TEST_CASE("Shapefile without .prj opens", "[geodatabase][shapefile]")
{
    ITablePtr ptrTable = OpenShapeWorkspace(CreatePointsShapefile())->GetTable("points");
    REQUIRE(ptrTable->GetGeometryType() == CommonLib::shape_type_point);
    REQUIRE(ptrTable->GetSpatialReference() != nullptr); // guessed from the bounds

    ISelectCursorPtr ptrCursor = ptrTable->Search(std::make_shared<CQueryFilter>());
    int32_t nShape = ptrCursor->FindFieldByName("Shape");
    int32_t nId = ptrCursor->FindFieldByName("ID");
    int n = 0;
    while(ptrCursor->Next())
    {
        CommonLib::IGeoShapePtr ptrShape = ptrCursor->ReadShape(nShape);
        REQUIRE(ptrShape->GetPointCnt() == 1);
        REQUIRE(ptrShape->GetPoints()[0].x == ptrCursor->ReadInt32(nId));
        ++n;
    }
    REQUIRE(n == 3);
}

TEST_CASE("Shapefile cursor reads attributes of the current record", "[geodatabase][shapefile]")
{
    ITablePtr ptrTable = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    ISelectCursorPtr ptrCursor = ptrTable->Search(std::make_shared<CQueryFilter>());

    int32_t nFid = ptrCursor->FindFieldByName("FID");
    int32_t nName = ptrCursor->FindFieldByName("NAME");
    int32_t nCode = ptrCursor->FindFieldByName("CODE");
    int32_t nValue = ptrCursor->FindFieldByName("VALUE");
    int32_t nShape = ptrCursor->FindFieldByName("Shape");

    int64_t nExpected = 0;
    while(ptrCursor->Next())
    {
        REQUIRE(ptrCursor->ReadInt64(nFid) == nExpected);
        REQUIRE(ptrCursor->ReadText(nName) == "P" + std::to_string(nExpected));
        REQUIRE(ptrCursor->ReadDouble(nValue) == nExpected * 1.5);
        if(nExpected == 3)
            REQUIRE(ptrCursor->ColumnIsNull(nCode));
        else
        {
            REQUIRE_FALSE(ptrCursor->ColumnIsNull(nCode));
            REQUIRE(ptrCursor->ReadInt32(nCode) == 100 + nExpected);
        }

        CommonLib::bbox bb = ptrCursor->ReadShape(nShape)->GetBB();
        REQUIRE(bb.xMin == 20. * nExpected);
        ++nExpected;
    }
    REQUIRE(nExpected == 5);

    // wrong type / column
    REQUIRE_THROWS(ptrCursor->ReadText(nCode));
    byte_t* pBuf = nullptr;
    int32_t nSize = 0;
    REQUIRE_THROWS(ptrCursor->ReadBlob(nName, &pBuf, nSize));
}

TEST_CASE("Shapefile cursor fills rows", "[geodatabase][shapefile]")
{
    ITablePtr ptrTable = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    ISelectCursorPtr ptrCursor = ptrTable->Search(std::make_shared<CQueryFilter>());
    IRowPtr ptrRow = ptrCursor->CreateRow();

    int nRows = 0;
    while(ptrCursor->Next())
    {
        ptrCursor->FillRow(ptrRow);
        int32_t nCode = ptrRow->ColumnCount() - 1; // last column is FID
        REQUIRE(ptrRow->ColumnName(nCode) == "FID");
        REQUIRE(ptrRow->GetValue(nCode)->Get<int64_t>() == nRows);
        REQUIRE(ptrRow->ColumnIsNull(1) == (nRows == 3)); // CODE
        ++nRows;
    }
    REQUIRE(nRows == 5);
}

TEST_CASE("Shapefile spatial search", "[geodatabase][shapefile]")
{
    ITablePtr ptrTable = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    Geometry::ISpatialReferencePtr ptrSpatRef = ptrTable->GetSpatialReference();

    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(25, 2, 26, 3, ptrSpatRef)), "FID") == std::vector<int64_t>{1});
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(5, 2, 45, 3, ptrSpatRef)), "FID") == std::vector<int64_t>{0, 1, 2});
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(11, 2, 19, 3, ptrSpatRef)), "FID").empty());       // between squares
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(-50, 50, -40, 60, ptrSpatRef)), "FID").empty());   // outside
    REQUIRE(ReadOids(ptrTable->Search(CreateBBoxFilter(-100, -100, 1000, 1000, ptrSpatRef)), "FID").size() == 5);
}

TEST_CASE("Shapefile workspace: table cache, save / load", "[geodatabase][shapefile]")
{
    std::string sDir = CreatePolygonsShapefile();
    IDatabaseWorkspacePtr ptrWorkspace = OpenShapeWorkspace(sDir);
    REQUIRE(ptrWorkspace->GetWorkspaceType() == wtShapeFile);

    ITablePtr ptrTable = ptrWorkspace->GetTable("squares");
    REQUIRE(ptrWorkspace->GetTable("squares") == ptrTable); // cached
    REQUIRE(ptrWorkspace->GetDatasetCount() == 1);
    REQUIRE(ptrTable->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());
    REQUIRE_THROWS(ptrWorkspace->GetTable("missing"));

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrWorkspace->Save(ptrRoot);
    IWorkspacePtr ptrLoaded = CDatasetLoader::LoadWorkspace(ptrRoot);
    REQUIRE(ptrLoaded->GetWorkspaceType() == wtShapeFile);
    REQUIRE(ptrLoaded->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());

    ITablePtr ptrLoadedTable = std::dynamic_pointer_cast<IDatabaseWorkspace>(ptrLoaded)->GetTable("squares");
    REQUIRE(ReadOids(ptrLoadedTable->Search(std::make_shared<CQueryFilter>()), "FID").size() == 5);
}
