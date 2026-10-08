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

namespace
{
    // text symbol which only records calls, works without a real display
    class CCountingTextSymbol : public Display::ITextSymbol
    {
    public:
        int nPrepare = 0;
        int nDraw = 0;
        int nReset = 0;
        std::wstring sLastDrawnText;
        CommonLib::IGeoShapePtr ptrLastShape;

        // ISymbol
        virtual uint32_t GetSymbolID() const {return Display::UndefineSymbolID;}
        virtual void Init(Display::IDisplayPtr) {}
        virtual void Reset() {++nReset;}
        virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const {return ptrShape.get() != nullptr;}
        virtual void Draw(Display::IDisplayPtr, CommonLib::IGeoShapePtr ptrShape) {++nDraw; ptrLastShape = ptrShape; sLastDrawnText = m_sText;}
        virtual void FlushBuffers(Display::IDisplayPtr, Display::ITrackCancelPtr) {}
        virtual void GetBoundaryRect(CommonLib::IGeoShapePtr, Display::IDisplayPtr, Display::GRect&) const {}
        virtual bool GetScaleDependent() const {return false;}
        virtual void SetScaleDependent(bool) {}
        virtual bool GetDrawToBuffers() const {return false;}
        virtual void SetDrawToBuffers(bool) {}
        virtual void DrawDirectly(Display::IDisplayPtr, const Display::GPoint*, const int*, int) {}
        virtual void DrawGeometryEx(Display::IDisplayPtr, const Display::GPoint*, const int*, int) {}
        virtual void QueryBoundaryRectEx(Display::IDisplayPtr, const Display::GPoint*, const int*, int, Display::GRect&) const {}
        virtual void Prepare(Display::IDisplayPtr) {++nPrepare;}
        virtual void Save(CommonLib::ISerializeObjPtr) const {}
        virtual void Load(CommonLib::ISerializeObjPtr) {}

        // ITextSymbol
        virtual Display::GUnits GetAngle() const {return 0;}
        virtual void SetAngle(Display::GUnits) {}
        virtual Display::Color GetColor() const {return Display::Color();}
        virtual void SetColor(const Display::Color&) {}
        virtual Display::FontPtr GetFont() const {return Display::FontPtr();}
        virtual void SetFont(Display::FontPtr) {}
        virtual void GetTextSize(Display::IDisplayPtr, const std::wstring&, Display::GUnits*, Display::GUnits*, Display::GUnits*) const {}
        virtual Display::GUnits GetSize() const {return 0;}
        virtual void SetSize(Display::GUnits) {}
        virtual const std::wstring& GetText() const {return m_sText;}
        virtual void SetText(const std::wstring& text) {m_sText = text;}
        virtual Display::ITextBackgroundPtr GetTextBackground() const {return Display::ITextBackgroundPtr();}
        virtual void SetTextBackground(Display::ITextBackgroundPtr) {}
        virtual int GetTextDrawFlags() const {return 0;}
        virtual void SetTextDrawFlags(int) {}

    private:
        std::wstring m_sText;
    };
    typedef std::shared_ptr<CCountingTextSymbol> CCountingTextSymbolPtr;
}

TEST_CASE("Annotation renderer id and CanRender", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CAnnotationRenderer renderer;

    REQUIRE(renderer.GetFeatureRendererID() == AnnotationRendererID);
    REQUIRE(renderer.GetSymbolSelector() == nullptr);
    REQUIRE_FALSE(renderer.CanRender(ptrTable, Display::IDisplayPtr()));

    renderer.SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>(std::make_shared<CCountingTextSymbol>()));
    REQUIRE(renderer.CanRender(ptrTable, Display::IDisplayPtr()));
    REQUIRE_FALSE(renderer.CanRender(GeoDatabase::ITablePtr(), Display::IDisplayPtr()));
}

TEST_CASE("Annotation PrepareFilter adds shape and annotation fields", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(std::make_shared<CCountingTextSymbol>()));
    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();

    renderer.PrepareFilter(ptrTable, ptrFilter, "Name");
    renderer.PrepareFilter(ptrTable, ptrFilter, "Name"); // no duplicates

    REQUIRE(ptrFilter->GetFieldSet()->GetCount() == 2);
    REQUIRE(ptrFilter->GetFieldSet()->Find("Shape") >= 0);
    REQUIRE(ptrFilter->GetFieldSet()->Find("Name") >= 0);
}

TEST_CASE("Annotation DrawFeature draws the field value at the feature shape", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingTextSymbolPtr ptrSymbol = std::make_shared<CCountingTextSymbol>();
    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(ptrSymbol));
    renderer.PrepareFilter(ptrTable, std::make_shared<GeoDatabase::CQueryFilter>(), "Name");

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetInt64(0, 1);
    ptrRow->SetText(1, "Praha");
    ptrRow->SetShape(2, CreatePoint(10., 20.));

    renderer.DrawFeature(Display::IDisplayPtr(), ptrRow);

    REQUIRE(ptrSymbol->nPrepare == 1);
    REQUIRE(ptrSymbol->nDraw == 1);
    REQUIRE(ptrSymbol->nReset == 1);
    REQUIRE(ptrSymbol->sLastDrawnText == L"Praha");
    REQUIRE(ptrSymbol->ptrLastShape->GetPoints()[0].x == 10.);
}

TEST_CASE("Annotation DrawFeature converts non text values", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingTextSymbolPtr ptrSymbol = std::make_shared<CCountingTextSymbol>();
    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(ptrSymbol));
    renderer.PrepareFilter(ptrTable, std::make_shared<GeoDatabase::CQueryFilter>(), "OID");

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetInt64(0, 42);
    ptrRow->SetShape(2, CreatePoint(1., 1.));

    renderer.DrawFeature(Display::IDisplayPtr(), ptrRow);

    REQUIRE(ptrSymbol->nDraw == 1);
    REQUIRE(ptrSymbol->sLastDrawnText == L"42");
}

TEST_CASE("Annotation DrawFeature skips empty values, rows without shape and selection drawing", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingTextSymbolPtr ptrSymbol = std::make_shared<CCountingTextSymbol>();
    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(ptrSymbol));
    renderer.PrepareFilter(ptrTable, std::make_shared<GeoDatabase::CQueryFilter>(), "Name");

    GeoDatabase::IRowPtr ptrNoText = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrNoText->SetShape(2, CreatePoint(1., 1.));
    renderer.DrawFeature(Display::IDisplayPtr(), ptrNoText);

    GeoDatabase::IRowPtr ptrNoShape = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrNoShape->SetText(1, "text");
    renderer.DrawFeature(Display::IDisplayPtr(), ptrNoShape);

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetText(1, "text");
    ptrRow->SetShape(2, CreatePoint(1., 1.));
    renderer.DrawFeature(Display::IDisplayPtr(), ptrRow, std::make_shared<CCountingSymbol>());

    REQUIRE(ptrSymbol->nDraw == 0);
}

TEST_CASE("Annotation DrawFeature skips rows whose selector symbol isn't a text symbol", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(ptrSymbol));
    renderer.PrepareFilter(ptrTable, std::make_shared<GeoDatabase::CQueryFilter>(), "Name");

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetText(1, "text");
    ptrRow->SetShape(2, CreatePoint(1., 1.));
    renderer.DrawFeature(Display::IDisplayPtr(), ptrRow);

    REQUIRE(ptrSymbol->nDraw == 0);
}

TEST_CASE("Annotation DrawFeature throws when the annotation field is missing", "[cartography][annotation]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(std::make_shared<CCountingTextSymbol>()));
    renderer.PrepareFilter(ptrTable, std::make_shared<GeoDatabase::CQueryFilter>(), "NoSuchField");

    GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
    ptrRow->SetShape(2, CreatePoint(1., 1.));

    REQUIRE_THROWS(renderer.DrawFeature(Display::IDisplayPtr(), ptrRow));
}

TEST_CASE("Annotation renderer save/load with a text symbol selector", "[cartography][annotation][serialize]")
{
    std::shared_ptr<Display::CTextSymbol> ptrSymbol = std::make_shared<Display::CTextSymbol>();
    ptrSymbol->SetSize(4);
    ptrSymbol->SetColor(Display::Color(255, 0, 0));

    CAnnotationRenderer renderer(std::make_shared<CSimpleSymbolSelector>(ptrSymbol));
    renderer.SetMaximumScale(1000.);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    renderer.Save(ptrRoot);

    IFeatureRendererPtr ptrLoaded = CLoaderRenderers::LoadRenderer(ptrRoot);
    IAnnotationRenderPtr ptrAnno = std::dynamic_pointer_cast<IAnnotationRender>(ptrLoaded);
    REQUIRE(ptrAnno != nullptr);
    REQUIRE(ptrAnno->GetMaximumScale() == 1000.);
    ISimpleSymbolSelectorPtr ptrSelector = std::dynamic_pointer_cast<ISimpleSymbolSelector>(ptrAnno->GetSymbolSelector());
    REQUIRE(ptrSelector != nullptr);
    Display::ITextSymbolPtr ptrLoadedSymbol = std::dynamic_pointer_cast<Display::ITextSymbol>(ptrSelector->GetSymbol());
    REQUIRE(ptrLoadedSymbol != nullptr);
    REQUIRE(ptrLoadedSymbol->GetSize() == 4);
}
