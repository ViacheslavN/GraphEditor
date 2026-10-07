#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    // OID, Name, Shape, Shape2
    GeoDatabase::IFieldsPtr CreateFields()
    {
        GeoDatabase::IFieldsPtr ptrFields = std::make_shared<GeoDatabase::CFields>();
        ptrFields->AddField(CreateField("OID", GeoDatabase::dtInteger64));
        ptrFields->AddField(CreateField("Name", GeoDatabase::dtString));
        ptrFields->AddField(CreateField("Shape", GeoDatabase::dtGeometry));
        ptrFields->AddField(CreateField("Shape2", GeoDatabase::dtGeometry));
        return ptrFields;
    }

    std::shared_ptr<CFeatureRenderer> CreateRenderer(Display::ISymbolPtr ptrSymbol)
    {
        std::shared_ptr<CFeatureRenderer> ptrRenderer = std::make_shared<CFeatureRenderer>();
        ptrRenderer->SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>(ptrSymbol));
        return ptrRenderer;
    }
}

TEST_CASE("Renderer id and defaults", "[cartography][renderer]")
{
    CFeatureRenderer renderer;

    REQUIRE(renderer.GetFeatureRendererID() == SimpleFeatureRendererID);
    REQUIRE(renderer.GetMinimumScale() == 0.);
    REQUIRE(renderer.GetMaximumScale() == 0.);
    REQUIRE(renderer.GetShapeField().empty());
    REQUIRE(renderer.GetSymbolSelector() == nullptr);
}

TEST_CASE("Renderer can render only with a symbol selector and a table", "[cartography][renderer]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CFeatureRenderer renderer;

    REQUIRE_FALSE(renderer.CanRender(ptrTable, Display::IDisplayPtr()));

    renderer.SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>());
    REQUIRE(renderer.CanRender(ptrTable, Display::IDisplayPtr()));
    REQUIRE_FALSE(renderer.CanRender(GeoDatabase::ITablePtr(), Display::IDisplayPtr()));
}

TEST_CASE("PrepareFilter adds the table shape field by default", "[cartography][renderer]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(std::make_shared<CCountingSymbol>());
    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();

    ptrRenderer->PrepareFilter(ptrTable, ptrFilter);
    ptrRenderer->PrepareFilter(ptrTable, ptrFilter); // second call doesn't duplicate the field

    REQUIRE(ptrRenderer->GetShapeField() == "Shape");
    REQUIRE(ptrFilter->GetFieldSet()->GetCount() == 1);
    REQUIRE(ptrFilter->GetFieldSet()->Find("Shape") >= 0);
}

TEST_CASE("PrepareFilter resolves shape field given as geometry index", "[cartography][renderer]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(std::make_shared<CCountingSymbol>());
    ptrRenderer->SetShapeField("1"); // second geometry field
    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();

    ptrRenderer->PrepareFilter(ptrTable, ptrFilter);

    REQUIRE(ptrRenderer->GetShapeField() == "Shape2");
    REQUIRE(ptrFilter->GetFieldSet()->Find("Shape2") >= 0);
}

TEST_CASE("DrawFeature draws the row shape with the selector symbol", "[cartography][renderer]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(ptrSymbol);

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetInt64(0, 1);
    ptrRow->SetShape(2, CreatePoint(10., 20.));

    ptrRenderer->DrawFeature(Display::IDisplayPtr(), ptrRow);

    REQUIRE(ptrSymbol->nPrepare == 1);
    REQUIRE(ptrSymbol->nDraw == 1);
    REQUIRE(ptrSymbol->nReset == 1);
    REQUIRE(ptrSymbol->ptrLastShape != nullptr);
    REQUIRE(ptrSymbol->ptrLastShape->GetPoints()[0].x == 10.);
    REQUIRE(ptrSymbol->ptrLastShape->GetPoints()[0].y == 20.);
}

TEST_CASE("DrawFeature uses the named shape column", "[cartography][renderer]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(ptrSymbol);
    ptrRenderer->SetShapeField("Shape2");

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetShape(2, CreatePoint(1., 1.));
    ptrRow->SetShape(3, CreatePoint(2., 2.));

    ptrRenderer->DrawFeature(Display::IDisplayPtr(), ptrRow);

    REQUIRE(ptrSymbol->nDraw == 1);
    REQUIRE(ptrSymbol->ptrLastShape->GetPoints()[0].x == 2.);
}

TEST_CASE("DrawFeature with a custom symbol ignores the selector", "[cartography][renderer]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrCustom = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(ptrSymbol);

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetShape(2, CreatePoint(1., 1.));

    ptrRenderer->DrawFeature(Display::IDisplayPtr(), ptrRow, ptrCustom);

    REQUIRE(ptrCustom->nDraw == 1);
    REQUIRE(ptrSymbol->nDraw == 0);
}

TEST_CASE("DrawFeature skips rows without shape", "[cartography][renderer]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(ptrSymbol);

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRenderer->DrawFeature(Display::IDisplayPtr(), ptrRow);
    ptrRenderer->DrawFeature(Display::IDisplayPtr(), GeoDatabase::IRowPtr());

    REQUIRE(ptrSymbol->nDraw == 0);
}

TEST_CASE("DrawFeature throws when the row has no geometry column", "[cartography][renderer]")
{
    std::shared_ptr<CFeatureRenderer> ptrRenderer = CreateRenderer(std::make_shared<CCountingSymbol>());

    GeoDatabase::IFieldsPtr ptrFields = std::make_shared<GeoDatabase::CFields>();
    ptrFields->AddField(CreateField("OID", GeoDatabase::dtInteger64));
    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(ptrFields);

    REQUIRE_THROWS(ptrRenderer->DrawFeature(Display::IDisplayPtr(), ptrRow));
}

TEST_CASE("Simple symbol selector", "[cartography][selector]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    CSimpleSymbolSelector selector;

    REQUIRE(selector.GetSymbolSelectorID() == SimpleSymbolSelectorID);
    REQUIRE(selector.GetSymbolCount() == 1);
    REQUIRE(selector.GetSymbolByIndex(0) == nullptr);

    selector.SetSymbolByIndex(0, ptrSymbol);
    REQUIRE(selector.GetSymbol() == ptrSymbol);
    REQUIRE(selector.GetSymbolByFeature(GeoDatabase::IRowPtr()) == ptrSymbol);
    REQUIRE(selector.GetSymbolByIndex(1) == nullptr);

    selector.SetupSymbols(Display::IDisplayPtr()); // no display -> nothing
    REQUIRE(ptrSymbol->nPrepare == 0);
    selector.ResetSymbols();
    REQUIRE(ptrSymbol->nReset == 1);
    selector.FlushBuffers(Display::IDisplayPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrSymbol->nFlush == 1);
}
