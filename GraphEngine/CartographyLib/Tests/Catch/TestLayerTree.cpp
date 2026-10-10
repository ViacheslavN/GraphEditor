// tests of the layer tree / layer properties part of TestMapWTLDraw: group layers in the project,
// moving layers, reading the settings of a layer back and applying them
#include "TestCommon.h"
#include "../TestMapWTLDraw/SymbologyModel.h"
#include "../TestMapWTLDraw/MapProject.h"
#include "../../layers/GroupLayer.h"
#include "../../legend/Legend.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../DisplayLib/Symbols/SymbolPreview.h"
#include "../../../DisplayLib/Symbols/MultiLayerSymbol.h"
#include "../../../DisplayLib/Display/Display.h"
#include "../../../DisplayLib/Transformation/DisplayTransformation2D.h"
#include "../../../ThirdParty/ShapeLib/shapefil.h"
#include <cmath>
#include <filesystem>

using namespace GraphEngine;
using namespace TestMapDraw;

namespace
{
    // two squares: NAME A, B; VALUE 1, 2
    std::string CreateTreeShapefile()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineLayerTreeTests";
        std::filesystem::create_directories(dir);
        std::string base = (dir / "squares").string();
        SHPHandle hShp = SHPCreate(base.c_str(), SHPT_POLYGON);
        DBFHandle hDbf = DBFCreate(base.c_str());
        DBFAddField(hDbf, "NAME", FTString, 10, 0);
        DBFAddField(hDbf, "VALUE", FTDouble, 10, 2);
        const char* names[2] = {"A", "B"};
        for(int i = 0; i < 2; ++i)
        {
            double x0 = i * 20.;
            double x[5] = {x0, x0, x0 + 10., x0 + 10., x0};
            double y[5] = {0., 10., 10., 0., 0.};
            SHPObject* pObj = SHPCreateSimpleObject(SHPT_POLYGON, 5, x, y, nullptr);
            SHPWriteObject(hShp, -1, pObj);
            SHPDestroyObject(pObj);
            DBFWriteStringAttribute(hDbf, i, 0, names[i]);
            DBFWriteDoubleAttribute(hDbf, i, 1, i + 1.);
        }
        SHPClose(hShp);
        DBFClose(hDbf);
        return base + ".shp";
    }

    std::vector<std::string> Names(Cartography::ILayersPtr ptrLayers)
    {
        std::vector<std::string> names;
        for(int i = 0; i < ptrLayers->GetLayerCount(); ++i)
            names.push_back(ptrLayers->GetLayer(i)->GetName());
        return names;
    }
}

TEST_CASE("Project: group layers and moving layers", "[project][layertree]")
{
    CMapProject project;
    Cartography::ILayersPtr ptrMapLayers = project.GetMap()->GetLayers();
    REQUIRE_FALSE(project.HasDataLayers());

    Cartography::IGroupLayerPtr ptrGroup = project.AddGroupLayer("group");
    REQUIRE_FALSE(project.HasDataLayers());   // an empty group has no data

    std::string sShp = CreateTreeShapefile();
    Cartography::ILayerPtr ptrA = project.AddShapefile(sShp);
    ptrA->SetName("a");
    Cartography::ILayerPtr ptrB = project.AddShapefile(sShp);
    ptrB->SetName("b");
    REQUIRE(project.HasDataLayers());
    REQUIRE(Names(ptrMapLayers) == std::vector<std::string>({"group", "a", "b"}));

    // into the group, on its top
    REQUIRE(project.MoveLayer(ptrA, ptrGroup->GetChildren(), Cartography::ILayerPtr(), true));
    REQUIRE(project.FindParentList(ptrA) == ptrGroup->GetChildren());
    REQUIRE(Names(ptrMapLayers) == std::vector<std::string>({"group", "b"}));

    // below / above a neighbour in the same list
    REQUIRE(project.MoveLayer(ptrB, ptrMapLayers, ptrGroup, false));
    REQUIRE(Names(ptrMapLayers) == std::vector<std::string>({"b", "group"}));
    REQUIRE(project.MoveLayer(ptrB, ptrMapLayers, ptrGroup, true));
    REQUIRE(Names(ptrMapLayers) == std::vector<std::string>({"group", "b"}));

    // a group can't go into itself or into its child group
    Cartography::IGroupLayerPtr ptrSub = project.AddGroupLayer("sub", ptrGroup);
    REQUIRE_FALSE(project.MoveLayer(ptrGroup, ptrGroup->GetChildren(), Cartography::ILayerPtr(), true));
    REQUIRE_FALSE(project.MoveLayer(ptrGroup, ptrSub->GetChildren(), Cartography::ILayerPtr(), true));
    REQUIRE(CMapProject::IsInGroup(ptrSub, ptrGroup));
    REQUIRE_FALSE(CMapProject::IsInGroup(ptrB, ptrGroup));

    // the group extent is the extent of its layers
    CommonLib::bbox bb;
    REQUIRE(project.GetLayerExtent(ptrGroup, bb));
    REQUIRE(bb.xMin == 0.);
    REQUIRE(bb.xMax == 30.);

    project.RemoveLayer(ptrA);
    REQUIRE(project.FindParentList(ptrA) == nullptr);
    REQUIRE(Names(ptrGroup->GetChildren()) == std::vector<std::string>({"sub"}));
}

TEST_CASE("Project: settings of a layer are read back and applied", "[project][layertree]")
{
    std::string sShp = CreateTreeShapefile();

    SLayerParams params;
    params.ptrSymbology = std::make_shared<SSymbology>();
    params.ptrSymbology->selector = SelectorRanges;
    params.ptrSymbology->sField = "VALUE";
    params.ptrSymbology->vecRanges = CSymbologyBuilder::MakeRanges(1., 2., 2, CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(200, 0, 0)));
    params.ptrSymbology->otherSymbol = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(10, 10, 10));
    params.ptrSymbology->bDrawOther = false;
    params.annotation.sField = "NAME";
    params.annotation.dMinimumScale = 50000.;
    params.labels.sField = "NAME";
    params.labels.dFontSize = 4.;
    params.labels.color = Display::Color(0, 0, 255);
    params.labels.options.m_nPriority = 3;

    CMapProject project;
    Cartography::IFeatureLayerPtr ptrLayer = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(project.AddShapefile(sShp, params));
    REQUIRE(ptrLayer != nullptr);

    SLayerParams read;
    bool bExact = false;
    CMapProject::GetLayerParams(ptrLayer, read, &bExact);
    REQUIRE(bExact);
    REQUIRE(read.annotation.sField == "NAME");
    REQUIRE(read.annotation.dMinimumScale == 50000.);
    REQUIRE(read.labels.sField == "NAME");
    REQUIRE(read.labels.dFontSize == 4.);
    REQUIRE(read.labels.color.GetB() == 255);
    REQUIRE(read.labels.options.m_nPriority == 3);
    REQUIRE(read.ptrSymbology != nullptr);
    REQUIRE(read.ptrSymbology->selector == SelectorRanges);
    REQUIRE(read.ptrSymbology->sField == "VALUE");
    REQUIRE(read.ptrSymbology->vecRanges.size() == 2);
    REQUIRE(read.ptrSymbology->vecRanges[0].symbol.color.GetR() == params.ptrSymbology->vecRanges[0].symbol.color.GetR());
    REQUIRE_FALSE(read.ptrSymbology->bDrawOther);

    // the legend of the layer: two ranges
    std::vector<Cartography::ILegendGroupPtr> vecLegend = Cartography::CLegendUtils::GetLayerLegend(ptrLayer);
    REQUIRE(vecLegend.size() == 1);
    REQUIRE(vecLegend[0]->GetClassCount() == 2);

    // off the annotation and the labels, the symbology isn't set - it is kept
    SLayerParams changed;
    CMapProject::ApplyLayerParams(ptrLayer, changed);
    REQUIRE_FALSE(ptrLayer->HasAnnoField());
    REQUIRE(ptrLayer->GetAnnotationRenderer() == nullptr);
    REQUIRE_FALSE(ptrLayer->HasLabelField());
    REQUIRE(ptrLayer->GetRenderer(0)->GetSymbolSelector()->GetSymbolSelectorID() == Cartography::RangeSymbolSelectorID);

    // a new symbology
    changed.ptrSymbology = std::make_shared<SSymbology>();
    changed.ptrSymbology->simpleSymbol = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(1, 2, 3));
    CMapProject::ApplyLayerParams(ptrLayer, changed);
    REQUIRE(ptrLayer->GetRenderer(0)->GetSymbolSelector()->GetSymbolSelectorID() == Cartography::SimpleSymbolSelectorID);
}

TEST_CASE("Project: data fields of a layer", "[project][layertree]")
{
    CMapProject project;
    Cartography::IFeatureLayerPtr ptrLayer = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(project.AddShapefile(CreateTreeShapefile()));
    REQUIRE(ptrLayer != nullptr);

    SLayerDataInfo info = CMapProject::GetLayerDataInfo(ptrLayer);
    REQUIRE(info.sTable == "squares");
    REQUIRE(info.sWorkspace.find("Shape files") == 0);
    REQUIRE(info.sGeometryType == "Polygon");
    REQUIRE(info.vecFields.size() >= 3);   // NAME, VALUE, the OID and the shape
    REQUIRE(info.vecShapeFields.size() == 1);
    REQUIRE(info.vecShapeFields[0] == info.sTableShapeField);
    REQUIRE_FALSE(info.vecOIDFields.empty());
    REQUIRE(info.sOIDField.empty());     // the table defaults
    REQUIRE(info.sShapeField.empty());
    REQUIRE((info.extent.type & CommonLib::bbox_type_normal));

    // wrong fields
    REQUIRE_THROWS(CMapProject::SetLayerDataFields(ptrLayer, "NAME", ""));     // text, not an integer
    REQUIRE_THROWS(CMapProject::SetLayerDataFields(ptrLayer, "", "NAME"));     // not a geometry
    REQUIRE_THROWS(CMapProject::SetLayerDataFields(ptrLayer, "NO_FIELD", ""));

    // the shape field goes to the renderers of the layer
    CMapProject::SetLayerDataFields(ptrLayer, info.vecOIDFields[0], info.sTableShapeField);
    REQUIRE(ptrLayer->GetOIDField() == info.vecOIDFields[0]);
    REQUIRE(ptrLayer->GetRenderer(0)->GetShapeField() == info.sTableShapeField);

    CMapProject::SetLayerDataFields(ptrLayer, "", "");
    REQUIRE(ptrLayer->GetOIDField().empty());
    REQUIRE(ptrLayer->GetRenderer(0)->GetShapeField().empty());
}

TEST_CASE("Project: map properties and the coordinate system", "[project][layertree]")
{
    REQUIRE(CMapProject::CoordinateSystemFromEpsg(3857) == CMapProject::WebMercatorProj4());
    REQUIRE(CMapProject::CoordinateSystemFromEpsg(4326).find("+proj=longlat") == 0);
    REQUIRE_THROWS(CMapProject::CoordinateSystemFromEpsg(999999));

    CommonLib::Units units = CommonLib::UnitsUnknown;
    REQUIRE(CMapProject::DescribeCoordinateSystem(CMapProject::WebMercatorProj4(), &units) == "Projected, Meters");
    REQUIRE(units == CommonLib::UnitsMeters);
    CMapProject::DescribeCoordinateSystem("+proj=longlat +datum=WGS84 +no_defs", &units);
    REQUIRE(units == CommonLib::UnitsDecimalDegrees);
    REQUIRE_THROWS(CMapProject::DescribeCoordinateSystem("garbage"));

    CMapProject project;
    project.AddShapefile(CreateTreeShapefile());   // no .prj - the shape file workspace takes WGS 84 longitude / latitude

    SMapParams params = project.GetMapParams();
    params.sName = "Test map";
    params.sSpatialReference = CMapProject::WebMercatorProj4();
    params.units = CommonLib::UnitsMeters;
    params.bReferenceScale = true;
    params.dReferenceScale = 10000.;
    params.background = Display::Color(10, 20, 30, 255);
    project.ApplyMapParams(params);

    SMapParams read = project.GetMapParams();
    REQUIRE(read.sName == "Test map");
    REQUIRE(read.sSpatialReference == CMapProject::WebMercatorProj4());
    REQUIRE(read.units == CommonLib::UnitsMeters);
    REQUIRE(read.bReferenceScale);
    REQUIRE(read.dReferenceScale == 10000.);
    REQUIRE(read.background.GetB() == 30);

    // the squares are 0..30 degrees of longitude, the center is 15E: UTM zone 33N is offered
    bool bUtm = false;
    for(const SCoordinateSystemPreset& preset : project.GetCoordinateSystemPresets())
        bUtm = bUtm || preset.sProj4.find("+zone=33 ") != std::string::npos;
    REQUIRE(bUtm);

    // a wrong system doesn't change the map
    params.sSpatialReference = "garbage";
    params.sName = "Changed";
    REQUIRE_THROWS(project.ApplyMapParams(params));
    REQUIRE(project.GetMapParams().sName == "Test map");
}

TEST_CASE("Symbol parameters are read back from the symbols", "[symbology][layertree]")
{
    const eSymbolKind kinds[] = {SymbolSimpleMarker, SymbolArrowMarker, SymbolCharacterMarker, SymbolSimpleLine, SymbolHashLine,
                                 SymbolMarkerLine, SymbolSimpleFill, SymbolLineFill, SymbolMarkerFill};
    for(eSymbolKind kind : kinds)
    {
        SSymbolParams params = CSymbolFactory::CreateDefault(kind, Display::Color(12, 34, 56));
        params.nStyle = (kind == SymbolSimpleFill || kind == SymbolSimpleLine || kind == SymbolSimpleMarker) ? 2 : params.nStyle;
        SSymbolParams read;
        INFO("kind " << CSymbolFactory::GetSymbolKindName(kind));
        REQUIRE(CSymbolFactory::FromSymbol(CSymbolFactory::CreateSymbol(params), read));
        REQUIRE(read.kind == kind);
        REQUIRE(read.color.GetR() == 12);
        REQUIRE(read.color.GetB() == 56);
        REQUIRE(read.dSize == params.dSize);
        if(kind == SymbolSimpleFill || kind == SymbolSimpleLine || kind == SymbolSimpleMarker || kind == SymbolMarkerLine)
            REQUIRE(read.nStyle == params.nStyle);
        if(kind == SymbolHashLine || kind == SymbolMarkerLine || kind == SymbolLineFill || kind == SymbolMarkerFill)
            REQUIRE(read.dSeparation == params.dSeparation);
    }

    // a hollow fill can't be shown by the page
    std::shared_ptr<Display::CSimpleFillSymbol> ptrHollow = std::make_shared<Display::CSimpleFillSymbol>();
    ptrHollow->SetStyle(Display::SimpleFillStyleNull);
    SSymbolParams read;
    REQUIRE_FALSE(CSymbolFactory::FromSymbol(ptrHollow, read));
}

TEST_CASE("Symbol preview draws into the rect", "[symbology][layertree]")
{
    Display::ISymbolPtr ptrSymbol = CSymbolFactory::CreateSymbol(CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(255, 0, 0)));
    REQUIRE(Display::CSymbolPreview::GetShape(ptrSymbol) == Display::SymbolPreviewPolygon);

    Display::IGraphicsPtr ptrGraphics = Display::CSymbolPreview::CreatePreview(ptrSymbol, 16, 16, Display::Color(255, 255, 255));
    REQUIRE(ptrGraphics != nullptr);
    Display::Color center = ptrGraphics->GetPixel(8, 8);
    REQUIRE(center.GetR() == 255);
    REQUIRE(center.GetG() == 0);

    // a line is drawn too (DrawDirectly of the simple line)
    Display::ISymbolPtr ptrLine = CSymbolFactory::CreateSymbol(CSymbolFactory::CreateDefault(SymbolSimpleLine, Display::Color(0, 0, 255)));
    REQUIRE(Display::CSymbolPreview::GetShape(ptrLine) == Display::SymbolPreviewLine);
    Display::IGraphicsPtr ptrLineGraphics = Display::CSymbolPreview::CreatePreview(ptrLine, 16, 16, Display::Color(255, 255, 255));
    bool bBlue = false;
    for(int x = 0; x < 16 && !bBlue; ++x)
        for(int y = 0; y < 16 && !bBlue; ++y)
            bBlue = ptrLineGraphics->GetPixel(x, y).GetB() > 200 && ptrLineGraphics->GetPixel(x, y).GetR() < 100;
    REQUIRE(bBlue);
}

TEST_CASE("Scale dependent symbols: parameters, layer and the simple line width", "[symbology][layertree]")
{
    // the flag goes through the symbol and its parts (outline) and back
    SSymbolParams params = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(1, 2, 3));
    params.bScaleDependent = true;
    Display::ISymbolPtr ptrFill = CSymbolFactory::CreateSymbol(params);
    REQUIRE(CSymbolFactory::GetScaleDependent(ptrFill) == CSymbolFactory::ScaleDependentYes);
    SSymbolParams read;
    REQUIRE(CSymbolFactory::FromSymbol(ptrFill, read));
    REQUIRE(read.bScaleDependent);
    REQUIRE(CSymbolFactory::GetPropertyText(read, PropScaleDependent) == "1");
    CSymbolFactory::SetPropertyText(read, PropScaleDependent, "0");
    REQUIRE_FALSE(read.bScaleDependent);

    // the outline only: mixed, can't be shown by one flag
    std::dynamic_pointer_cast<Display::IFillSymbol>(ptrFill)->GetOutlineSymbol()->SetScaleDependent(false);
    REQUIRE(CSymbolFactory::GetScaleDependent(ptrFill) == CSymbolFactory::ScaleDependentMixed);
    REQUIRE_FALSE(CSymbolFactory::FromSymbol(ptrFill, read));

    // multi layer symbols: all the layers
    std::shared_ptr<Display::CMultiLayerLineSymbol> ptrMulti = std::make_shared<Display::CMultiLayerLineSymbol>();
    ptrMulti->AddLayer(std::make_shared<Display::CSimpleLineSymbol>(Display::Color(0, 0, 0), 3.));
    ptrMulti->AddLayer(std::make_shared<Display::CSimpleLineSymbol>(Display::Color(255, 255, 255), 1.));
    CSymbolFactory::SetScaleDependent(ptrMulti, true);
    REQUIRE(ptrMulti->GetLayer(1)->GetScaleDependent());

    // all the symbols of a layer
    CMapProject project;
    Cartography::IFeatureLayerPtr ptrLayer = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(project.AddShapefile(CreateTreeShapefile()));
    REQUIRE(ptrLayer != nullptr);
    REQUIRE(CMapProject::GetLayerScaleDependent(ptrLayer) == CSymbolFactory::ScaleDependentNo);
    CMapProject::SetLayerScaleDependent(ptrLayer, true);
    REQUIRE(CMapProject::GetLayerScaleDependent(ptrLayer) == CSymbolFactory::ScaleDependentYes);
    SLayerParams layerParams;
    bool bExact = false;
    CMapProject::GetLayerParams(ptrLayer, layerParams, &bExact);
    REQUIRE(layerParams.ptrSymbology != nullptr);
    REQUIRE(layerParams.ptrSymbology->simpleSymbol.bScaleDependent);

    // the width of a simple line: pixels at the reference scale, twice wider when zoomed in twice
    Display::GRect rect(0, 0, 200, 100);
    std::shared_ptr<Display::CDisplayTransformation2D> ptrTrans = std::make_shared<Display::CDisplayTransformation2D>(96., CommonLib::UnitsMeters, rect);
    ptrTrans->SetDeviceClipRect(rect);
    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 0; bb.yMin = 0; bb.xMax = 2000; bb.yMax = 1000;
    ptrTrans->SetMapVisibleRect(bb);
    ptrTrans->SetReferenceScale(ptrTrans->GetScale());

    auto LineWidth = [&](Display::ISymbolPtr ptrLine)
    {
        Display::IGraphicsPtr ptrGraphics = Display::IGraphics::CreateCGraphicsAgg(200, 100, false);
        ptrGraphics->Erase(Display::Color(255, 255, 255, 255));
        Display::IDisplayPtr ptrDisplay = std::make_shared<Display::CDisplay>(ptrTrans);
        ptrDisplay->StartDrawing(ptrGraphics);
        Display::GPoint points[2] = {Display::GPoint(10, 50), Display::GPoint(190, 50)};
        int nCount = 2;
        ptrLine->Init(ptrDisplay);
        ptrLine->DrawDirectly(ptrDisplay, points, &nCount, 1);
        ptrDisplay->FinishDrawing();
        // coverage of a column across the line (antialiased edges count partly)
        double dWidth = 0.;
        for(int y = 0; y < 100; ++y)
            dWidth += (255. - ptrGraphics->GetPixel(100, y).GetR()) / 255.;
        return (int)std::lround(dWidth);
    };

    std::shared_ptr<Display::CSimpleLineSymbol> ptrFixed = std::make_shared<Display::CSimpleLineSymbol>(Display::Color(0, 0, 0), 4.);
    std::shared_ptr<Display::CSimpleLineSymbol> ptrScaled = std::make_shared<Display::CSimpleLineSymbol>(Display::Color(0, 0, 0), 4.);
    ptrScaled->SetScaleDependent(true);
    REQUIRE(LineWidth(ptrFixed) == 4);
    REQUIRE(LineWidth(ptrScaled) == 4);

    bb.xMin = 500; bb.yMin = 250; bb.xMax = 1500; bb.yMax = 750;   // zoomed in twice
    ptrTrans->SetMapVisibleRect(bb);
    REQUIRE(LineWidth(ptrFixed) == 4);
    REQUIRE(LineWidth(ptrScaled) == 8);
    REQUIRE(ptrScaled->GetWidth() == 4.);   // the symbol keeps its width
}
