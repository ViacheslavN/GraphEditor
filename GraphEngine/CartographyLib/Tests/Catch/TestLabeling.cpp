#include "TestCommon.h"
#include "../../renders/LabelRenderer.h"
#include "../../Labeling/LabelDrawer.h"
#include "../../Labeling/LabelStrategy.h"
#include "../../Labeling/LabelCollisionGrid.h"
#include "../../../DisplayLib/Display/Display.h"
#include "../../../DisplayLib/Transformation/DisplayTransformation2D.h"
#include <catch2/catch_approx.hpp>

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace GraphEngine::Cartography::Labeling;
using namespace cartography_test;

namespace
{
    TLabelPoints Ring(std::initializer_list<std::pair<double, double> > points)
    {
        TLabelPoints ring;
        for(const auto& pt : points)
            ring.push_back(SLabelPoint(pt.first, pt.second));
        return ring;
    }

    // display 1000 x 700 px, map units = device pixels (y goes up in the map)
    Display::IDisplayPtr CreateTestDisplay()
    {
        Display::GRect rect(0, 0, 1000, 700);
        Display::IDisplayTransformationPtr ptrTrans = std::make_shared<Display::CDisplayTransformation2D>(96., CommonLib::UnitsMeters, rect);
        ptrTrans->SetDeviceClipRect(rect);
        CommonLib::bbox bb;
        bb.xMin = 0; bb.yMin = 0; bb.xMax = 1000; bb.yMax = 700;
        bb.type = CommonLib::bbox_type_normal;
        ptrTrans->SetMapVisibleRect(bb);

        Display::IDisplayPtr ptrDisplay = std::make_shared<Display::CDisplay>(ptrTrans);
        ptrDisplay->StartDrawing(Display::IGraphics::CreateCGraphicsAgg(1000, 700, false));
        return ptrDisplay;
    }

    Display::ITextSymbolPtr CreateLabelSymbol()
    {
        std::shared_ptr<Display::CTextSymbol> ptrSymbol = std::make_shared<Display::CTextSymbol>();
        ptrSymbol->SetSize(3.);
        ptrSymbol->SetColor(Display::Color(0, 0, 0, 255));
        return ptrSymbol;
    }

    class CTestTrackCancel : public Display::ITrackCancel
    {
    public:
        virtual void Cancel() {}
        virtual bool Continue() {return true;}
        virtual void Reset() {}
    };

    // the text metrics need the font file (Arial), without it the font tests are skipped
    bool HasFont(Display::IDisplayPtr ptrDisplay)
    {
        CLabelStyle style(CreateLabelSymbol(), ptrDisplay);
        return style.IsValid() && style.TextWidth(L"Test") > 0.;
    }
}

TEST_CASE("Label angles", "[cartography][labeling]")
{
    REQUIRE(NormalizeReadableAngle(0.) == Catch::Approx(0.));
    REQUIRE(NormalizeReadableAngle(170.) == Catch::Approx(-10.));
    REQUIRE(NormalizeReadableAngle(-100.) == Catch::Approx(80.));
    REQUIRE(NormalizeReadableAngle(90.) == Catch::Approx(90.));
    REQUIRE(NormalizeReadableAngle(-90.) == Catch::Approx(90.));
    REQUIRE(NormalizeReadableAngle(450.) == Catch::Approx(90.));
    REQUIRE(AngleDifference(350., 10.) == Catch::Approx(-20.));
}

TEST_CASE("Label box intersection", "[cartography][labeling]")
{
    SLabelBox box = SLabelBox::FromCenter(0., 0., 100., 20., 0.);
    REQUIRE(box.Intersects(SLabelBox::FromCenter(90., 0., 100., 20., 0.)));
    REQUIRE_FALSE(box.Intersects(SLabelBox::FromCenter(101., 0., 100., 20., 0.)));
    REQUIRE_FALSE(box.Intersects(SLabelBox::FromCenter(100., 0., 100., 20., 0.)));   // touching

    // the bounding boxes intersect, the rotated boxes don't
    SLabelBox rotated = SLabelBox::FromCenter(0., 0., 100., 4., 45.);
    SLabelBox other = SLabelBox::FromCenter(30., -30., 20., 20., 0.);
    REQUIRE(rotated.Bounds().IsIntersect(other.Bounds()));
    REQUIRE_FALSE(rotated.Intersects(other));
    REQUIRE(rotated.Intersects(SLabelBox::FromCenter(20., 20., 10., 10., 0.)));

    REQUIRE(box.IntersectsSegment(SLabelPoint(-100., 0.), SLabelPoint(100., 0.)));
    REQUIRE(box.IntersectsSegment(SLabelPoint(0., 0.), SLabelPoint(1., 1.)));          // inside
    REQUIRE_FALSE(box.IntersectsSegment(SLabelPoint(-100., 30.), SLabelPoint(100., 30.)));
    REQUIRE_FALSE(box.IntersectsSegment(SLabelPoint(-100., 10.), SLabelPoint(100., 10.))); // along the border
}

TEST_CASE("Label path", "[cartography][labeling]")
{
    CLabelPath path(Ring({{0., 0.}, {100., 0.}, {100., 100.}, {100., 100.}}));
    REQUIRE(path.IsValid());
    REQUIRE(path.Points().size() == 3);   // the duplicate point is removed
    REQUIRE(path.Length() == Catch::Approx(200.));

    double angle = 0.;
    SLabelPoint pt = path.PointAt(150., &angle);
    REQUIRE(pt.x == Catch::Approx(100.));
    REQUIRE(pt.y == Catch::Approx(50.));
    REQUIRE(angle == Catch::Approx(90.));

    CLabelPath reversed = path.Reversed();
    pt = reversed.PointAt(50.);
    REQUIRE(pt.x == Catch::Approx(100.));
    REQUIRE(pt.y == Catch::Approx(50.));

    REQUIRE(path.Deviation(50., 150.) == Catch::Approx(50. / sqrt(2.)).epsilon(0.01));
}

TEST_CASE("Label polygon with a hole", "[cartography][labeling]")
{
    CLabelPolygon polygon;
    polygon.AddRing(Ring({{0., 0.}, {300., 0.}, {300., 300.}, {0., 300.}}));
    polygon.AddRing(Ring({{50., 50.}, {250., 50.}, {250., 250.}, {50., 250.}}));   // the orientation doesn't matter

    REQUIRE(polygon.Contains(25., 150.));
    REQUIRE_FALSE(polygon.Contains(150., 150.));   // in the hole
    REQUIRE_FALSE(polygon.Contains(350., 150.));

    REQUIRE(polygon.ContainsBox(SLabelBox::FromCenter(150., 25., 200., 20., 0.)));
    REQUIRE_FALSE(polygon.ContainsBox(SLabelBox::FromCenter(150., 45., 200., 20., 0.)));    // crosses the hole
    REQUIRE_FALSE(polygon.ContainsBox(SLabelBox::FromCenter(150., 150., 20., 20., 0.)));    // in the hole

    SLabelPoint pole = polygon.PoleOfInaccessibility(1.);
    REQUIRE(polygon.Contains(pole.x, pole.y));
    REQUIRE(polygon.SignedDistance(pole.x, pole.y) > 20.);

    std::vector<std::pair<double, double> > intervals;
    polygon.ScanLine(150., intervals);
    REQUIRE(intervals.size() == 2);
    REQUIRE(intervals[0].first == Catch::Approx(0.));
    REQUIRE(intervals[0].second == Catch::Approx(50.));
}

TEST_CASE("Label polygon main axis", "[cartography][labeling]")
{
    // long rectangle along 30 degrees
    double c = cos(LabelDegToRad(30.)), s = sin(LabelDegToRad(30.));
    TLabelPoints ring;
    const double corners[4][2] = {{-200., -10.}, {200., -10.}, {200., 10.}, {-200., 10.}};
    for(int i = 0; i < 4; ++i)
        ring.push_back(SLabelPoint(corners[i][0] * c - corners[i][1] * s, corners[i][0] * s + corners[i][1] * c));

    CLabelPolygon polygon;
    polygon.AddRing(ring);
    double angle = 0., elongation = 0.;
    polygon.MainAxis(&angle, &elongation);
    REQUIRE(NormalizeReadableAngle(angle) == Catch::Approx(30.).margin(0.5));
    REQUIRE(elongation > 0.9);

    CLabelPolygon square;
    square.AddRing(Ring({{0., 0.}, {100., 0.}, {100., 100.}, {0., 100.}}));
    square.MainAxis(&angle, &elongation);
    REQUIRE(elongation < 0.1);
}

TEST_CASE("Label collision grid", "[cartography][labeling]")
{
    CLabelCollisionGrid grid;
    grid.Setup(Display::GRect(0, 0, 500, 500), 32.);
    REQUIRE_FALSE(grid.Intersects(SLabelBox::FromCenter(100., 100., 50., 10., 0.)));

    grid.Insert(SLabelBox::FromCenter(100., 100., 200., 10., 30.));
    REQUIRE(grid.GetBoxCount() == 1);
    REQUIRE(grid.Intersects(SLabelBox::FromCenter(100., 100., 10., 10., 0.)));
    REQUIRE_FALSE(grid.Intersects(SLabelBox::FromCenter(400., 400., 10., 10., 0.)));
    REQUIRE_FALSE(grid.Intersects(SLabelBox::FromCenter(150., 50., 10., 10., 0.)));   // in the bounding box only

    // a box outside the grid area is kept in the border cells
    grid.Insert(SLabelBox::FromCenter(-50., -50., 20., 20., 0.));
    REQUIRE(grid.Intersects(SLabelBox::FromCenter(-45., -45., 10., 10., 0.)));
}

TEST_CASE("Labeling options save / load", "[cartography][labeling][serialize]")
{
    SLabelingOptions options;
    options.m_strategy = LabelStrategySimple;
    options.m_lineOrientation = LineLabelOrientationCurved;
    options.m_linePosition = LineLabelPositionAboveBelow;
    options.m_polygonPlacement = PolygonLabelPlacementMixed;
    options.m_duplicateStrategy = DuplicateStrategyDistance;
    options.m_pointPriorities[PointLabelCenterCenter] = 5;
    options.m_bPolygonAllowOutside = true;
    options.m_nPriority = 3;
    options.m_dOffset = 2.5;
    options.m_dDuplicateDistance = 70.;
    options.m_dMaxCurvedCharAngle = 20.;

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    options.Save(ptrRoot);

    SLabelingOptions loaded;
    loaded.Load(ptrRoot);
    REQUIRE(loaded.m_strategy == LabelStrategySimple);
    REQUIRE(loaded.m_lineOrientation == LineLabelOrientationCurved);
    REQUIRE(loaded.m_linePosition == LineLabelPositionAboveBelow);
    REQUIRE(loaded.m_polygonPlacement == PolygonLabelPlacementMixed);
    REQUIRE(loaded.m_duplicateStrategy == DuplicateStrategyDistance);
    REQUIRE(loaded.m_pointPriorities[PointLabelCenterCenter] == 5);
    REQUIRE(loaded.m_pointPriorities[PointLabelRightTop] == 1);
    REQUIRE(loaded.m_bPolygonAllowOutside);
    REQUIRE(loaded.m_nPriority == 3);
    REQUIRE(loaded.m_dOffset == Catch::Approx(2.5));
    REQUIRE(loaded.m_dDuplicateDistance == Catch::Approx(70.));
    REQUIRE(loaded.m_dMaxCurvedCharAngle == Catch::Approx(20.));
}

TEST_CASE("Label renderer save / load", "[cartography][labeling][serialize]")
{
    CLabelRenderer renderer(std::make_shared<CSimpleSymbolSelector>(CreateLabelSymbol()));
    renderer.SetClassIndex(7);
    renderer.SetMinimumScale(50000.);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    renderer.Save(ptrRoot);

    ILabelRendererPtr ptrLoaded = std::dynamic_pointer_cast<ILabelRenderer>(CLoaderRenderers::LoadRenderer(ptrRoot));
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetFeatureRendererID() == LabelRendererID);
    REQUIRE(ptrLoaded->GetClassIndex() == 7);
    REQUIRE(ptrLoaded->GetMinimumScale() == Catch::Approx(50000.));
    REQUIRE(ptrLoaded->GetSymbolSelector() != nullptr);
}

TEST_CASE("Feature layer label settings save / load", "[cartography][labeling][serialize]")
{
    std::shared_ptr<CFeatureLayer> ptrLayer = CreateFeatureLayer("Cities");
    REQUIRE((ptrLayer->GetSupportedDrawPhases() & DrawPhaseLabeling) != 0);
    REQUIRE_FALSE(ptrLayer->HasLabelField());

    SLabelingOptions options;
    options.m_lineOrientation = LineLabelOrientationHorizontal;
    options.m_nPriority = 2;
    ptrLayer->SetLabelFieldName("NAME");
    ptrLayer->SetLabelRenderer(std::make_shared<CLabelRenderer>(std::make_shared<CSimpleSymbolSelector>(CreateLabelSymbol())));
    ptrLayer->SetLabelingOptions(options);
    REQUIRE(ptrLayer->HasLabelField());

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrLayer->Save(ptrRoot);

    std::shared_ptr<CFeatureLayer> ptrLoaded = std::make_shared<CFeatureLayer>();
    ptrLoaded->Load(ptrRoot);
    REQUIRE(ptrLoaded->GetLabelFieldName() == "NAME");
    REQUIRE(ptrLoaded->GetLabelRenderer() != nullptr);
    REQUIRE(ptrLoaded->GetLabelingOptions().m_lineOrientation == LineLabelOrientationHorizontal);
    REQUIRE(ptrLoaded->GetLabelingOptions().m_nPriority == 2);
}

TEST_CASE("Map has a label drawer, it is saved", "[cartography][labeling][serialize]")
{
    CMap map;
    std::shared_ptr<CLabelDrawer> ptrDrawer = std::dynamic_pointer_cast<CLabelDrawer>(map.GetLabelDrawer());
    REQUIRE(ptrDrawer != nullptr);
    ptrDrawer->SetLabelBuffer(1.5);
    ptrDrawer->SetMaxCandidates(50);
    ptrDrawer->SetKeepInsideView(false);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    map.Save(ptrRoot);

    CMap loaded;
    loaded.Load(ptrRoot);
    std::shared_ptr<CLabelDrawer> ptrLoaded = std::dynamic_pointer_cast<CLabelDrawer>(loaded.GetLabelDrawer());
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetLabelBuffer() == Catch::Approx(1.5));
    REQUIRE(ptrLoaded->GetMaxCandidates() == 50);
    REQUIRE_FALSE(ptrLoaded->GetKeepInsideView());
}

TEST_CASE("Label drawer places labels without overlapping", "[cartography][labeling]")
{
    Display::IDisplayPtr ptrDisplay = CreateTestDisplay();
    if(!HasFont(ptrDisplay))
        SKIP("Arial font isn't found");

    Display::ITextSymbolPtr ptrSymbol = CreateLabelSymbol();
    Display::ITrackCancelPtr ptrTrackCancel = std::make_shared<CTestTrackCancel>();
    CLabelDrawer drawer;

    SECTION("Points far from each other are all placed")
    {
        drawer.BeginLabeling(ptrDisplay);
        SLabelingOptions options;
        for(int i = 0; i < 5; ++i)
            drawer.AddLabel(L"City", CreatePoint(100. + i * 180., 350.), ptrSymbol, 0, options);
        drawer.DrawLabels(ptrTrackCancel);
        drawer.EndLabeling();
        REQUIRE(drawer.GetLabelCount() == 5);
        REQUIRE(drawer.GetPlacedLabelCount() == 5);
    }

    SECTION("The same point: another position is found (complex), only the first one is tried (simple)")
    {
        SLabelingOptions options;
        drawer.BeginLabeling(ptrDisplay);
        drawer.AddLabel(L"First", CreatePoint(500., 350.), ptrSymbol, 0, options);
        drawer.AddLabel(L"Second", CreatePoint(500., 350.), ptrSymbol, 0, options);
        drawer.DrawLabels(ptrTrackCancel);
        drawer.EndLabeling();
        REQUIRE(drawer.GetPlacedLabelCount() == 2);

        options.m_strategy = LabelStrategySimple;
        drawer.BeginLabeling(ptrDisplay);
        drawer.AddLabel(L"First", CreatePoint(500., 350.), ptrSymbol, 0, options);
        drawer.AddLabel(L"Second", CreatePoint(500., 350.), ptrSymbol, 0, options);
        drawer.DrawLabels(ptrTrackCancel);
        drawer.EndLabeling();
        REQUIRE(drawer.GetPlacedLabelCount() == 1);
    }

    SECTION("Duplicates are removed")
    {
        SLabelingOptions options;
        options.m_duplicateStrategy = DuplicateStrategyRemove;
        drawer.BeginLabeling(ptrDisplay);
        for(int i = 0; i < 4; ++i)
            drawer.AddLabel(L"Main street", CreatePoint(100. + i * 200., 200.), ptrSymbol, 0, options);
        drawer.AddLabel(L"Other street", CreatePoint(100., 500.), ptrSymbol, 0, options);
        drawer.DrawLabels(ptrTrackCancel);
        drawer.EndLabeling();
        REQUIRE(drawer.GetPlacedLabelCount() == 2);
    }

    SECTION("Empty text, points outside the view are skipped")
    {
        SLabelingOptions options;
        drawer.BeginLabeling(ptrDisplay);
        drawer.AddLabel(L"  ", CreatePoint(500., 350.), ptrSymbol, 0, options);
        drawer.AddLabel(L"Outside", CreatePoint(-500., 350.), ptrSymbol, 0, options);
        drawer.DrawLabels(ptrTrackCancel);
        drawer.EndLabeling();
        REQUIRE(drawer.GetLabelCount() == 0);
        REQUIRE(drawer.GetPlacedLabelCount() == 0);
    }
}

TEST_CASE("Label strategy: text boxes of a curved label follow the line", "[cartography][labeling]")
{
    Display::IDisplayPtr ptrDisplay = CreateTestDisplay();
    if(!HasFont(ptrDisplay))
        SKIP("Arial font isn't found");

    SLabelItem item;
    item.text = L"Curved label";
    item.style = std::make_shared<CLabelStyle>(CreateLabelSymbol(), ptrDisplay);
    item.width = item.style->TextWidth(item.text);
    item.type = LabelGeometryLine;
    item.options.m_lineOrientation = LineLabelOrientationCurved;

    // the line goes from right to left: the text must still be read from left to right
    TLabelPoints line;
    for(int i = 200; i >= 0; --i)
        line.push_back(SLabelPoint(100. + i * 4., 350. + 40. * sin(i / 30.)));
    item.parts.push_back(line);

    SLabelStrategyParams params;
    bool bFound = false;
    CLabelStrategy::GenerateCandidates(item, params, [&](const SLabelPlacement& placement) -> bool
    {
        bFound = true;
        REQUIRE(placement.runs.size() == item.text.length());
        REQUIRE(placement.runs.front().x < placement.runs.back().x);
        for(size_t i = 1; i < placement.runs.size(); ++i)
            REQUIRE(fabs(AngleDifference(placement.runs[i].angle, placement.runs[i - 1].angle)) <= item.options.m_dMaxCurvedCharAngle);
        return true;
    });
    REQUIRE(bFound);
}

TEST_CASE("Label strategy: polygon label is inside the polygon, not in the hole", "[cartography][labeling]")
{
    Display::IDisplayPtr ptrDisplay = CreateTestDisplay();
    if(!HasFont(ptrDisplay))
        SKIP("Arial font isn't found");

    SLabelItem item;
    item.text = L"Lake";
    item.style = std::make_shared<CLabelStyle>(CreateLabelSymbol(), ptrDisplay);
    item.width = item.style->TextWidth(item.text);
    item.type = LabelGeometryPolygon;
    item.parts.push_back(Ring({{100., 100.}, {500., 100.}, {500., 500.}, {100., 500.}}));
    item.parts.push_back(Ring({{150., 150.}, {450., 150.}, {450., 450.}, {150., 450.}}));

    CLabelPolygon polygon;
    polygon.AddRing(item.parts[0]);
    polygon.AddRing(item.parts[1]);

    SLabelStrategyParams params;
    int nCandidates = 0;
    CLabelStrategy::GenerateCandidates(item, params, [&](const SLabelPlacement& placement) -> bool
    {
        REQUIRE(placement.runs.size() == 1);
        REQUIRE(polygon.ContainsBox(placement.runs[0].box));
        return ++nCandidates >= 10;
    });
    REQUIRE(nCandidates > 0);
}
