#include "TestCommon.h"
#include "../../drawer/MapDrawer.h"
#include "../../../GeoDatabase/GeoDatabaseShape/ShapefileWorkspace.h"
#include "../../../GeoDatabase/WorkspaceHolder.h"
#include "../../../GeoDatabase/DatasetLoader.h"
#include "../../../GeoDatabase/TableCopier.h"
#include "../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../ThirdParty/ShapeLib/shapefil.h"
#include "../../../CommonLib/xml/XMLDoc.h"
#include <filesystem>
#include <chrono>
#include <thread>

#include "../../../DisplayLib/Transformation/DisplayTransformation3D.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    // two 10x10 squares: "A" at (0,0)-(10,10), "B" at (20,0)-(30,10)
    std::string CreateSquaresShapefile()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineCartographyTests";
        std::filesystem::create_directories(dir);
        std::string base = (dir / "squares").string();

        SHPHandle hShp = SHPCreate(base.c_str(), SHPT_POLYGON);
        DBFHandle hDbf = DBFCreate(base.c_str());
        REQUIRE(hShp != nullptr);
        REQUIRE(hDbf != nullptr);
        DBFAddField(hDbf, "NAME", FTString, 10, 0);
        DBFAddField(hDbf, "CODE", FTInteger, 6, 0);

        const double xOffsets[2] = {0., 20.};
        const char* names[2] = {"A", "B"};
        for(int i = 0; i < 2; ++i)
        {
            double x[5] = {xOffsets[i], xOffsets[i], xOffsets[i] + 10., xOffsets[i] + 10., xOffsets[i]};
            double y[5] = {0., 10., 10., 0., 0.};
            SHPObject* pObj = SHPCreateSimpleObject(SHPT_POLYGON, 5, x, y, nullptr);
            SHPWriteObject(hShp, -1, pObj);
            SHPDestroyObject(pObj);
            DBFWriteStringAttribute(hDbf, i, 0, names[i]);
            DBFWriteIntegerAttribute(hDbf, i, 1, 100 + i);
        }
        SHPClose(hShp);
        DBFClose(hDbf);
        return dir.string();
    }

    GeoDatabase::IDatabaseWorkspacePtr OpenSquaresWorkspace()
    {
        static std::string dir = CreateSquaresShapefile();
        return std::dynamic_pointer_cast<GeoDatabase::IDatabaseWorkspace>(GeoDatabase::CShapfileWorkspace::Open("squares", dir.c_str(), CommonLib::CGuid::CreateNew()));
    }

    Display::ISymbolPtr CreateFill(const Display::Color& color)
    {
        std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
        ptrFill->SetColor(color);
        return ptrFill;
    }

    IMapPtr CreateSquaresMap(GeoDatabase::ITablePtr ptrTable)
    {
        std::shared_ptr<CFeatureRenderer> ptrRenderer = std::make_shared<CFeatureRenderer>();
        ptrRenderer->SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>(CreateFill(Display::Color(200, 0, 0, 255))));

        std::shared_ptr<CFeatureLayer> ptrLayer = CreateFeatureLayer("squares");
        ptrLayer->SetLayerTable(ptrTable);
        ptrLayer->AddRenderer(ptrRenderer);

        IMapPtr ptrMap = std::make_shared<CMap>();
        ptrMap->SetSpatialReference(ptrTable->GetSpatialReference());
        ptrMap->GetLayers()->AddLayer(ptrLayer);
        ptrMap->GetSelection()->SetSymbol(CreateFill(Display::Color(0, 0, 255, 255)));
        return ptrMap;
    }

    // the drawer has no callbacks: poll it until n drawings are finished and nothing is drawn now
    bool WaitDrawn(const CMapDrawer& drawer, uint64_t n)
    {
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while(drawer.GetDrawCounter() < n || drawer.IsDrawing())
        {
            if(std::chrono::steady_clock::now() > end)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    Display::Color PixelAtMap(CMapDrawer& drawer, double x, double y)
    {
        CommonLib::GisXYPoint mapPt = {x, y};
        Display::GPoint devPt;
        drawer.GetTransformation()->MapToDevice(&mapPt, &devPt, 1);
        return drawer.GetOutGraphics()->GetPixel(devPt.x, devPt.y);
    }

    bool SameRGB(const Display::Color& c1, const Display::Color& c2)
    {
        return c1.GetR() == c2.GetR() && c1.GetG() == c2.GetG() && c1.GetB() == c2.GetB();
    }
}

TEST_CASE("Shapefile cursor returns FID and attributes of the right record", "[cartography][drawer][shapefile]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    REQUIRE(ptrTable->GetOIDFieldName() == "FID");
    REQUIRE(ptrTable->GetShapeFieldName() == "Shape");

    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
    GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable->Search(ptrFilter);
    int32_t nFid = ptrCursor->FindFieldByName("FID");
    int32_t nName = ptrCursor->FindFieldByName("NAME");
    int32_t nCode = ptrCursor->FindFieldByName("CODE");

    std::vector<std::pair<int64_t, std::string> > rows;
    GeoDatabase::IRowPtr ptrRow = ptrCursor->CreateRow();
    while(ptrCursor->Next())
    {
        rows.push_back(std::make_pair(ptrCursor->ReadInt64(nFid), ptrCursor->ReadText(nName)));
        REQUIRE(ptrCursor->ReadInt32(nCode) == 100 + (int)ptrCursor->ReadInt64(nFid));
        ptrCursor->FillRow(ptrRow); // all columns, including shape and FID
    }

    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0] == std::make_pair(int64_t(0), std::string("A")));
    REQUIRE(rows[1] == std::make_pair(int64_t(1), std::string("B")));
}

TEST_CASE("Shapefile spatial search returns only intersecting records", "[cartography][drawer][shapefile]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");

    std::shared_ptr<GeoDatabase::CQueryFilter> ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 24; bb.xMax = 26; bb.yMin = 4; bb.yMax = 6;
    ptrFilter->SetBB(bb);
    ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
    ptrFilter->SetOutputSpatialReference(ptrTable->GetSpatialReference());

    GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable->Search(ptrFilter);
    std::vector<int64_t> fids;
    while(ptrCursor->Next())
        fids.push_back(ptrCursor->ReadInt64(ptrCursor->FindFieldByName("FID")));

    REQUIRE(fids == std::vector<int64_t>{1});
}

TEST_CASE("Map full extent follows the layers", "[cartography][drawer][map]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    CommonLib::bbox full = ptrMap->GetFullExtent(ptrMap->GetSpatialReference())->GetBoundingBox();
    REQUIRE(full.xMin == 0.);
    REQUIRE(full.xMax == 30.);
    REQUIRE(full.yMax == 10.);

    ptrMap->GetLayers()->RemoveAllLayers();
    CommonLib::bbox empty = ptrMap->GetFullExtent(ptrMap->GetSpatialReference())->GetBoundingBox();
    REQUIRE(empty.xMax == 1.); // default extent of an empty map
}

TEST_CASE("Map drawer draws the map in the background thread", "[cartography][drawer]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(300, 100, true);

    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.IsDrawCompleted());
    REQUIRE(drawer.GetLastError().empty());
    REQUIRE_FALSE(drawer.IsDrawing());
    REQUIRE(drawer.GetDrawCounter() == 1);

    REQUIRE(SameRGB(PixelAtMap(drawer, 5, 5), Display::Color(200, 0, 0, 255)));     // square A
    REQUIRE(SameRGB(PixelAtMap(drawer, 25, 5), Display::Color(200, 0, 0, 255)));    // square B
    REQUIRE(SameRGB(PixelAtMap(drawer, 15, 5), Display::Color(255, 255, 255, 255))); // gap between

    // Update copies the picture into the window graphics
    Display::IGraphicsPtr ptrScreen = Display::IGraphics::CreateCGraphicsAgg(300, 100, false);
    ptrScreen->Erase(Display::Color(0, 255, 0, 255));
    drawer.Update(ptrScreen, nullptr, nullptr);
    REQUIRE(SameRGB(ptrScreen->GetPixel(0, 0), drawer.GetOutGraphics()->GetPixel(0, 0)));
}

TEST_CASE("Map drawer draws the selection", "[cartography][drawer][selection]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = bb.xMax = 25;
    bb.yMin = bb.yMax = 5;
    ptrMap->SelectFeatures(bb, true);
    ILayerPtr ptrLayer = ptrMap->GetLayers()->GetLayer(0);
    REQUIRE(ptrMap->GetSelection()->GetFeatures(ptrLayer->GetLayerId()) == std::vector<int64_t>{1});

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(300, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));

    REQUIRE(SameRGB(PixelAtMap(drawer, 5, 5), Display::Color(200, 0, 0, 255)));   // not selected
    REQUIRE(SameRGB(PixelAtMap(drawer, 25, 5), Display::Color(0, 0, 255, 255)));   // selected
}

TEST_CASE("Map drawer pan and zoom", "[cartography][drawer]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(300, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));

    // the map point under (150 - 40, 50 - 10) must be in the center after the pan by (40, 10)
    Display::GPoint before(110, 40);
    CommonLib::GisXYPoint expected;
    drawer.GetCalcTransformation()->DeviceToMap(&before, &expected, 1);

    drawer.StartPan(Display::GPoint(100, 50));
    drawer.MovePan(Display::GPoint(120, 55));
    drawer.StopPan(Display::GPoint(140, 60));
    REQUIRE(WaitDrawn(drawer, 2));

    Display::GPoint center(150, 50);
    CommonLib::GisXYPoint actual;
    drawer.GetTransformation()->DeviceToMap(&center, &actual, 1);
    REQUIRE(fabs(actual.x - expected.x) < 1e-6);
    REQUIRE(fabs(actual.y - expected.y) < 1e-6);

    // zoom to square A
    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 0; bb.xMax = 10; bb.yMin = 0; bb.yMax = 10;
    drawer.ZoomIn(bb);
    REQUIRE(WaitDrawn(drawer, 3));
    const CommonLib::bbox& fitted = drawer.GetTransformation()->GetFittedBounds();
    REQUIRE(fitted.xMin <= 0.);
    REQUIRE(fitted.xMax >= 10.);
    REQUIRE(fitted.xMax - fitted.xMin < 31.);
    REQUIRE(SameRGB(PixelAtMap(drawer, 5, 5), Display::Color(200, 0, 0, 255)));

    drawer.ZoomToFullExtent();
    REQUIRE(WaitDrawn(drawer, 4));
    REQUIRE(drawer.GetTransformation()->GetFittedBounds().xMax >= 30.);
}

TEST_CASE("Map drawer 3D view", "[cartography][drawer][3d]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(300, 200, true);
    REQUIRE(WaitDrawn(drawer, 1));

    CommonLib::GisXYPoint pos = drawer.GetCalcTransformation()->GetMapPos();
    double scale = drawer.GetCalcTransformation()->GetScale();

    // the tilt is kept in the plan view and used when the 3D mode is on
    drawer.SetTilt(60.);
    REQUIRE(drawer.GetTilt() == 60.);
    REQUIRE_FALSE(drawer.Is3DMode());
    REQUIRE(std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(drawer.GetTransformation()).get() == nullptr);

    drawer.Set3DMode(true);
    REQUIRE(WaitDrawn(drawer, 2));
    REQUIRE(drawer.Is3DMode());
    REQUIRE(drawer.GetLastError().empty());

    Display::CDisplayTransformation3DPtr ptr3D = std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(drawer.GetTransformation());
    REQUIRE(ptr3D.get() != nullptr);
    REQUIRE(ptr3D->GetTilt() == 60.);
    REQUIRE(fabs(ptr3D->GetMapPos().x - pos.x) < 1e-9);
    REQUIRE(fabs(ptr3D->GetMapPos().y - pos.y) < 1e-9);
    REQUIRE(ptr3D->GetScale() == scale);

    REQUIRE(SameRGB(PixelAtMap(drawer, 5, 5), Display::Color(200, 0, 0, 255)));
    REQUIRE(SameRGB(PixelAtMap(drawer, 25, 5), Display::Color(200, 0, 0, 255)));
    REQUIRE(SameRGB(PixelAtMap(drawer, 15, 5), Display::Color(255, 255, 255, 255)));

    // a big tilt shows the sky above the far edge of the ground
    drawer.SetTilt(78.);
    REQUIRE(WaitDrawn(drawer, 3));
    ptr3D = std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(drawer.GetTransformation());
    REQUIRE(ptr3D->GetTilt() == 78.);
    REQUIRE(ptr3D->GetSkyLine() > 2.);
    Display::Color sky = drawer.GetOutGraphics()->GetPixel(150, 0);
    REQUIRE(sky.GetB() > sky.GetR());

    // pan: the map point under the start of the pan is under the cursor after it
    Display::GPoint start(150, 170), end(150, 140);
    CommonLib::GisXYPoint expected;
    drawer.GetCalcTransformation()->DeviceToMap(&start, &expected, 1);
    drawer.StartPan(start);
    drawer.StopPan(end);
    REQUIRE(WaitDrawn(drawer, 4));
    CommonLib::GisXYPoint actual;
    drawer.GetTransformation()->DeviceToMap(&end, &actual, 1);
    REQUIRE(fabs(actual.x - expected.x) < 1e-6);
    REQUIRE(fabs(actual.y - expected.y) < 1e-6);

    drawer.SetRotation(-90.);
    REQUIRE(WaitDrawn(drawer, 5));
    REQUIRE(drawer.GetRotation() == 270.);
    REQUIRE(drawer.GetTransformation()->GetRotation() == 270.);

    // back to the plan view: the same position, scale and rotation
    CommonLib::GisXYPoint pos3D = drawer.GetCalcTransformation()->GetMapPos();
    drawer.Set3DMode(false);
    REQUIRE(WaitDrawn(drawer, 6));
    REQUIRE_FALSE(drawer.Is3DMode());
    REQUIRE(std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(drawer.GetTransformation()).get() == nullptr);
    REQUIRE(fabs(drawer.GetTransformation()->GetMapPos().x - pos3D.x) < 1e-9);
    REQUIRE(drawer.GetTransformation()->GetRotation() == 270.);
    REQUIRE(drawer.GetTilt() == 78.);
}

TEST_CASE("Map drawer can be stopped and destroyed while drawing", "[cartography][drawer]")
{
    GeoDatabase::ITablePtr ptrTable = OpenSquaresWorkspace()->GetTable("squares");
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    for(int i = 0; i < 20; ++i)
    {
        CMapDrawer drawer;
        drawer.SetMap(ptrMap);
        drawer.SetSize(300, 100, true);
        if(i % 2)
            drawer.StopDraw(true);
        drawer.Redraw();
        // destructor stops the draw thread
    }
    SUCCEED();
}

TEST_CASE("Map project save / load with workspaces", "[cartography][drawer][serialize]")
{
    GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace = OpenSquaresWorkspace();
    GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
    GeoDatabase::ITablePtr ptrTable = ptrWorkspace->GetTable("squares");
    REQUIRE(ptrTable->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrWorkspace->Save(ptrRoot->CreateChildNode("Workspace"));
    ptrMap->Save(ptrRoot);

    GeoDatabase::CWorkspaceHolder::Clear();

    // reopen
    GeoDatabase::IWorkspacePtr ptrLoadedWks = GeoDatabase::CDatasetLoader::LoadWorkspace(ptrRoot->GetChild("Workspace"));
    REQUIRE(ptrLoadedWks->GetWorkspaceId() == ptrWorkspace->GetWorkspaceId());
    GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrLoadedWks);

    std::shared_ptr<CMap> ptrLoaded = std::make_shared<CMap>();
    ptrLoaded->Load(ptrRoot);
    REQUIRE(ptrLoaded->GetLayers()->GetLayerCount() == 1);
    IFeatureLayerPtr ptrLayer = std::dynamic_pointer_cast<IFeatureLayer>(ptrLoaded->GetLayers()->GetLayer(0));
    REQUIRE(ptrLayer->GetLayerTable() != nullptr);
    REQUIRE(ptrLayer->IsValid());

    CMapDrawer drawer;
    drawer.SetMap(ptrLoaded);
    drawer.SetSize(300, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.GetLastError().empty());
    REQUIRE(SameRGB(PixelAtMap(drawer, 25, 5), Display::Color(200, 0, 0, 255)));

    GeoDatabase::CWorkspaceHolder::Clear();
}

TEST_CASE("Map project save / load through an xml file", "[cartography][drawer][serialize]")
{
    GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace = OpenSquaresWorkspace();
    GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
    IMapPtr ptrMap = CreateSquaresMap(ptrWorkspace->GetTable("squares"));

    std::string sFile = (std::filesystem::temp_directory_path() / "GraphEngineCartographyTests" / "project.xml").string();
    {
        CommonLib::xml::CXMLDoc xmlDoc;
        CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(xmlDoc.GetNodes());
        CommonLib::ISerializeObjPtr ptrProject = ptrRoot->CreateChildNode("Project");
        ptrWorkspace->Save(ptrProject->CreateChildNode("Workspace"));
        ptrMap->Save(ptrProject);
        xmlDoc.Save(sFile);
    }
    GeoDatabase::CWorkspaceHolder::Clear();

    CommonLib::xml::CXMLDoc xmlDoc;
    xmlDoc.Open(sFile);
    CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(xmlDoc.GetNodes());
    REQUIRE(ptrRoot->IsChildExists("Project"));
    CommonLib::ISerializeObjPtr ptrProject = ptrRoot->GetChild("Project");
    GeoDatabase::CWorkspaceHolder::AddWorkspace(GeoDatabase::CDatasetLoader::LoadWorkspace(ptrProject->GetChild("Workspace")));

    std::shared_ptr<CMap> ptrLoaded = std::make_shared<CMap>();
    ptrLoaded->Load(ptrProject);
    REQUIRE(ptrLoaded->GetLayers()->GetLayerCount() == 1);
    REQUIRE(ptrLoaded->GetLayers()->GetLayer(0)->GetName() == "squares");
    REQUIRE(ptrLoaded->GetLayers()->GetLayer(0)->IsValid());

    GeoDatabase::CWorkspaceHolder::Clear();
}

TEST_CASE("Shapefile converted to SQLite: attributes, spatial search, drawing", "[cartography][drawer][sqlite]")
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineCartographyTests";
    std::string sDbPath = (dir / "squares.sqlite").string();
    for(const char* ext : {"", "-wal", "-shm"})
        std::filesystem::remove(sDbPath + ext);

    REQUIRE(GeoDatabase::CTableCopier::MakeValidTableName("1 my-table") == "t_1_my_table");

    // convert
    {
        GeoDatabase::ITablePtr ptrShape = OpenSquaresWorkspace()->GetTable("squares");
        GeoDatabase::IDatabaseWorkspacePtr ptrDb = GeoDatabase::CSQLiteWorkspace::Create("squares", sDbPath.c_str(), CommonLib::CGuid::CreateNew());
        int64_t nCopied = 0;
        GeoDatabase::CTableCopier::CopySpatialTable(ptrShape, ptrDb, "squares", [&](int64_t n){ nCopied = n; return true; });
        REQUIRE(nCopied == 2);
    }

    // reopen: the spatial table is described by the metadata
    GeoDatabase::IDatabaseWorkspacePtr ptrDb = GeoDatabase::CSQLiteWorkspace::Open("squares", sDbPath.c_str(), CommonLib::CGuid::CreateNew());
    REQUIRE(dynamic_cast<GeoDatabase::CSQLiteWorkspace*>(ptrDb.get())->GetSpatialTableNames() == std::vector<std::string>{"squares"});
    REQUIRE(ptrDb->GetDatasetCount() == 1);

    GeoDatabase::ITablePtr ptrTable = ptrDb->GetTable("squares");
    REQUIRE(ptrTable->GetDatasetType() == GeoDatabase::dtSpatialTable);
    REQUIRE(ptrTable->GetOIDFieldName() == "FID");
    REQUIRE(ptrTable->GetShapeFieldName() == "Shape");
    REQUIRE(ptrTable->GetGeometryType() == CommonLib::shape_type_polygon);
    REQUIRE(ptrTable->GetExtent()->GetBoundingBox().xMax == 30.);
    REQUIRE(ptrTable->GetFields()->GetField("Shape")->GetType() == GeoDatabase::dtGeometry);

    // spatial search through the R-tree
    std::shared_ptr<GeoDatabase::CQueryFilter> ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 24; bb.xMax = 26; bb.yMin = 4; bb.yMax = 6;
    ptrFilter->SetBB(bb);
    ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
    ptrFilter->SetOutputSpatialReference(ptrTable->GetSpatialReference());
    GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable->Search(ptrFilter);
    int32_t nFid = ptrCursor->FindFieldByName("FID");    // known before Next
    int32_t nName = ptrCursor->FindFieldByName("NAME");
    REQUIRE(nFid >= 0);
    REQUIRE(nName >= 0);
    std::vector<std::pair<int64_t, std::string> > rows;
    while(ptrCursor->Next())
        rows.push_back(std::make_pair(ptrCursor->ReadInt64(nFid), ptrCursor->ReadText(nName)));
    REQUIRE(rows.size() == 1);
    REQUIRE(rows[0] == std::make_pair(int64_t(1), std::string("B")));

    // draw + select
    IMapPtr ptrMap = CreateSquaresMap(ptrTable);
    bb.xMin = bb.xMax = 25;
    bb.yMin = bb.yMax = 5;
    ptrMap->SelectFeatures(bb, true);
    REQUIRE(ptrMap->GetSelection()->GetFeatures(ptrMap->GetLayers()->GetLayer(0)->GetLayerId()) == std::vector<int64_t>{1});

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(300, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.GetLastError().empty());
    REQUIRE(SameRGB(PixelAtMap(drawer, 5, 5), Display::Color(200, 0, 0, 255)));
    REQUIRE(SameRGB(PixelAtMap(drawer, 25, 5), Display::Color(0, 0, 255, 255)));   // selected

    // the workspace is restored from a project
    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrDb->Save(ptrRoot);
    GeoDatabase::IWorkspacePtr ptrLoaded = GeoDatabase::CDatasetLoader::LoadWorkspace(ptrRoot);
    REQUIRE(ptrLoaded->GetDatasetCount() == 1);
    REQUIRE(std::dynamic_pointer_cast<GeoDatabase::ITable>(ptrLoaded->GetDataset(0))->GetShapeFieldName() == "Shape");
}
