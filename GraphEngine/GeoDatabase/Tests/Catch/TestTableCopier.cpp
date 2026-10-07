#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

namespace
{
    IDatabaseWorkspacePtr CreateDatabase(const std::string& sFileName)
    {
        std::string sPath = (TestDir() / sFileName).string();
        RemoveDatabase(sPath);
        return CSQLiteWorkspace::Create("test", sPath.c_str(), CommonLib::CGuid::CreateNew());
    }
}

TEST_CASE("Table name for SQL", "[geodatabase][copy]")
{
    REQUIRE(CTableCopier::MakeValidTableName("roads") == "roads");
    REQUIRE(CTableCopier::MakeValidTableName("my roads-2024") == "my_roads_2024");
    REQUIRE(CTableCopier::MakeValidTableName("2024") == "t_2024");
    REQUIRE(CTableCopier::MakeValidTableName("") == "t_");
}

TEST_CASE("Shapefile is copied into SQLite", "[geodatabase][copy]")
{
    ITablePtr ptrShape = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    IDatabaseWorkspacePtr ptrDb = CreateDatabase("copy.sqlite");

    std::vector<int64_t> progress;
    ITablePtr ptrCopy = CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares", [&](int64_t n){ progress.push_back(n); return true; }, 2);

    REQUIRE(progress == std::vector<int64_t>{2, 4, 5});
    REQUIRE(ptrCopy->GetDatasetType() == dtSpatialTable);
    REQUIRE(ptrCopy->GetOIDFieldName() == "FID");
    REQUIRE(ptrCopy->GetShapeFieldName() == "Shape");
    REQUIRE(ptrCopy->GetGeometryType() == ptrShape->GetGeometryType());
    REQUIRE(ptrCopy->GetExtent()->GetBoundingBox().xMax == ptrShape->GetExtent()->GetBoundingBox().xMax);

    // same attributes and shapes
    ISelectCursorPtr ptrSrc = ptrShape->Search(std::make_shared<CQueryFilter>());
    std::shared_ptr<CQueryFilter> ptrOrdered = std::make_shared<CQueryFilter>();
    ISelectCursorPtr ptrDst = ptrCopy->Search(ptrOrdered);
    std::map<int64_t, std::tuple<std::string, bool, int64_t, double, double> > src, dst;

    auto readAll = [](ISelectCursorPtr ptrCursor, std::map<int64_t, std::tuple<std::string, bool, int64_t, double, double> >& rows)
    {
        int32_t nFid = ptrCursor->FindFieldByName("FID");
        int32_t nName = ptrCursor->FindFieldByName("NAME");
        int32_t nCode = ptrCursor->FindFieldByName("CODE");
        int32_t nValue = ptrCursor->FindFieldByName("VALUE");
        int32_t nShape = ptrCursor->FindFieldByName("Shape");
        while(ptrCursor->Next())
        {
            bool bCodeNull = ptrCursor->ColumnIsNull(nCode);
            rows[ptrCursor->ReadInt64(nFid)] = std::make_tuple(ptrCursor->ReadText(nName), bCodeNull, bCodeNull ? 0 : ptrCursor->ReadInt64(nCode),
                                                               ptrCursor->ReadDouble(nValue), ptrCursor->ReadShape(nShape)->GetBB().xMin);
        }
    };
    readAll(ptrSrc, src);
    readAll(ptrDst, dst);
    REQUIRE(src.size() == 5);
    REQUIRE(dst == src);
    REQUIRE(std::get<1>(dst[3])); // NULL stays NULL

    // the spatial index works on the copy
    REQUIRE(ReadOids(ptrCopy->Search(CreateBBoxFilter(41, 1, 42, 2, ptrCopy->GetSpatialReference())), "FID") == std::vector<int64_t>{2});
}

TEST_CASE("Copy into an existing table name fails, a new name works", "[geodatabase][copy]")
{
    ITablePtr ptrShape = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    IDatabaseWorkspacePtr ptrDb = CreateDatabase("copy_twice.sqlite");

    CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares");
    REQUIRE_THROWS(CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares"));
    CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares_1");
    REQUIRE(dynamic_cast<CSQLiteWorkspace*>(ptrDb.get())->GetSpatialTableNames() == std::vector<std::string>{"squares", "squares_1"});
}

TEST_CASE("Copy can be canceled", "[geodatabase][copy]")
{
    ITablePtr ptrShape = OpenShapeWorkspace(CreatePolygonsShapefile())->GetTable("squares");
    IDatabaseWorkspacePtr ptrDb = CreateDatabase("copy_cancel.sqlite");

    REQUIRE_THROWS(CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares", [](int64_t n){ return n < 2; }, 1));

    // the rows are rolled back
    ISelectCursorPtr ptrCursor = ptrDb->GetTable("squares")->Select("SELECT COUNT(*) FROM squares");
    REQUIRE(ptrCursor->Next());
    REQUIRE(ptrCursor->ReadInt64(0) == 0);
}

TEST_CASE("Points without OID get one", "[geodatabase][copy]")
{
    ITablePtr ptrShape = OpenShapeWorkspace(CreatePointsShapefile())->GetTable("points");
    IDatabaseWorkspacePtr ptrDb = CreateDatabase("copy_points.sqlite");
    ITablePtr ptrCopy = CTableCopier::CopySpatialTable(ptrShape, ptrDb, "points");
    REQUIRE(ReadOids(ptrCopy->Search(std::make_shared<CQueryFilter>()), ptrCopy->GetOIDFieldName()).size() == 3);
    REQUIRE(ReadOids(ptrCopy->Search(CreateBBoxFilter(1.5, 1.5, 2.5, 2.5, ptrCopy->GetSpatialReference())), ptrCopy->GetOIDFieldName()) == std::vector<int64_t>{1});
}

TEST_CASE("Workspace holder", "[geodatabase][holder]")
{
    CWorkspaceHolder::Clear();
    IWorkspacePtr ptrShape = OpenShapeWorkspace(CreatePolygonsShapefile());
    IWorkspacePtr ptrDb = CreateDatabase("holder.sqlite");

    CWorkspaceHolder::AddWorkspace(ptrShape);
    CWorkspaceHolder::AddWorkspace(ptrDb);
    CWorkspaceHolder::AddWorkspace(ptrDb); // same id - replaced
    REQUIRE(CWorkspaceHolder::GetWorkspaces().size() == 2);
    REQUIRE(CWorkspaceHolder::GetWorkspace(ptrShape->GetWorkspaceId()) == ptrShape);
    REQUIRE(CWorkspaceHolder::GetWorkspace(CommonLib::CGuid::CreateNew()) == nullptr);

    CWorkspaceHolder::RemoveWorkspace(ptrShape->GetWorkspaceId());
    REQUIRE(CWorkspaceHolder::GetWorkspace(ptrShape->GetWorkspaceId()) == nullptr);
    REQUIRE_THROWS(CWorkspaceHolder::AddWorkspace(IWorkspacePtr()));

    CWorkspaceHolder::Clear();
    REQUIRE(CWorkspaceHolder::GetWorkspaces().empty());
}
