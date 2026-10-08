#include "TestCommon.h"

using namespace display_test;
using namespace GraphEngine::Display;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    const GRect Window(0, 0, 800, 600);

    std::shared_ptr<CDisplayTransformation2D> CreateTrans()
    {
        auto ptrTrans = std::make_shared<CDisplayTransformation2D>(96., CommonLib::UnitsMeters, Window, ScaleOneMeterPerPixel());
        ptrTrans->SetDeviceClipRect(Window);
        CommonLib::GisXYPoint center = {1000., 2000.};
        ptrTrans->SetMapPos(center, ptrTrans->GetScale());
        return ptrTrans;
    }

    GPoint ToDev(IDisplayTransformation& trans, double x, double y)
    {
        CommonLib::GisXYPoint pt = {x, y};
        GPoint dev;
        trans.MapToDevice(&pt, &dev, 1);
        return dev;
    }

    CommonLib::GisXYPoint ToMap(IDisplayTransformation& trans, GUnits x, GUnits y)
    {
        GPoint dev(x, y);
        CommonLib::GisXYPoint pt;
        trans.DeviceToMap(&dev, &pt, 1);
        return pt;
    }
}

TEST_CASE("Transformation2D: scale and anchor", "[transformation]")
{
    auto ptrTrans = CreateTrans();
    // 1 inch is 0.0254000508 m in CommonLib (US survey inch), so not exactly 1 m per pixel
    REQUIRE_THAT(ptrTrans->DeviceToMapMeasure(1.), WithinRel(1., 1e-5));
    REQUIRE_THAT(ptrTrans->MapToDeviceMeasure(10.), WithinRel(10., 1e-5));

    // the map position is in the window center, north is up
    REQUIRE(Near(ToDev(*ptrTrans, 1000., 2000.), 400, 300));
    REQUIRE(Near(ToDev(*ptrTrans, 1000., 2100.), 400, 200));
    REQUIRE(Near(ToDev(*ptrTrans, 1050., 2000.), 450, 300));

    const CommonLib::bbox& fitted = ptrTrans->GetFittedBounds();
    REQUIRE(fitted.type == CommonLib::bbox_type_normal);
    REQUIRE_THAT(fitted.xMin, WithinAbs(600., 0.01));
    REQUIRE_THAT(fitted.xMax, WithinAbs(1400., 0.01));
    REQUIRE_THAT(fitted.yMin, WithinAbs(1700., 0.01));
    REQUIRE_THAT(fitted.yMax, WithinAbs(2300., 0.01));
}

TEST_CASE("Transformation2D: round trip with rotation and flips", "[transformation]")
{
    const double angles[] = {0., 30., 90., 135., -60.};
    for (double angle : angles)
    {
        for (int flips = 0; flips < 4; ++flips)
        {
            auto ptrTrans = CreateTrans();
            ptrTrans->SetRotation(angle);
            ptrTrans->SetHorizontalFlip((flips & 1) != 0);
            ptrTrans->SetVerticalFlip((flips & 2) != 0);

            for (GUnits x = 0; x <= 800; x += 200)
            {
                for (GUnits y = 0; y <= 600; y += 150)
                {
                    CommonLib::GisXYPoint mp = ToMap(*ptrTrans, x, y);
                    GPoint back = ToDev(*ptrTrans, mp.x, mp.y);
                    INFO("angle " << angle << " flips " << flips << " point " << x << "," << y);
                    REQUIRE(Near(back, x, y, 1e-6));
                }
            }

            // the rotation keeps distances
            GPoint p0 = ToDev(*ptrTrans, 1000., 2000.);
            GPoint p1 = ToDev(*ptrTrans, 1100., 2000.);
            REQUIRE_THAT(GPoint::CalcDistance(p0, p1), WithinAbs(100., 1.5));
        }
    }
}

TEST_CASE("Transformation2D: vertical flip puts north down", "[transformation]")
{
    auto ptrTrans = CreateTrans();
    ptrTrans->SetVerticalFlip(true);
    REQUIRE(Near(ToDev(*ptrTrans, 1000., 2100.), 400, 400));
}

TEST_CASE("Transformation2D: SetMapVisibleRect fits the box", "[transformation]")
{
    auto ptrTrans = CreateTrans();

    CommonLib::bbox box;
    box.type = CommonLib::bbox_type_normal;
    box.xMin = 0; box.xMax = 4000;      // wide box: the width defines the scale
    box.yMin = 0; box.yMax = 1000;
    ptrTrans->SetMapVisibleRect(box);

    REQUIRE_THAT(ptrTrans->DeviceToMapMeasure(1.), WithinRel(4000. / 800., 1e-9));
    REQUIRE_THAT(ptrTrans->GetMapPos().x, WithinAbs(2000., 1e-9));
    REQUIRE_THAT(ptrTrans->GetMapPos().y, WithinAbs(500., 1e-9));

    GRect rect;
    ptrTrans->MapToDevice(box, rect);
    REQUIRE_THAT(rect.xMin, WithinAbs(0., 1e-6));
    REQUIRE_THAT(rect.xMax, WithinAbs(800., 1e-6));
    REQUIRE(rect.yMin >= 0);
    REQUIRE(rect.yMax <= 600);

    SECTION("degenerate box is ignored")
    {
        double scale = ptrTrans->GetScale();
        CommonLib::bbox flat = box;
        flat.xMax = flat.xMin;
        REQUIRE_NOTHROW(ptrTrans->SetMapVisibleRect(flat));
        CommonLib::bbox invalid = box;
        invalid.type = CommonLib::bbox_type_invalid;
        REQUIRE_NOTHROW(ptrTrans->SetMapVisibleRect(invalid));
        REQUIRE(ptrTrans->GetScale() == scale);
    }

    SECTION("rotated box fits too")
    {
        ptrTrans->SetRotation(45.);
        ptrTrans->SetMapVisibleRect(box);
        ptrTrans->MapToDevice(box, rect);
        REQUIRE(rect.xMin >= -1);
        REQUIRE(rect.xMax <= 801);
        REQUIRE(rect.yMin >= -1);
        REQUIRE(rect.yMax <= 601);
        // touches the window on one of the axes
        REQUIRE(((rect.xMin <= 1 && rect.xMax >= 799) || (rect.yMin <= 1 && rect.yMax >= 599)));
    }
}

TEST_CASE("Transformation2D: units change the map units per pixel", "[transformation]")
{
    auto ptrTrans = CreateTrans();
    double scale = ptrTrans->GetScale();
    ptrTrans->SetUnits(CommonLib::UnitsKilometers);
    REQUIRE(ptrTrans->GetScale() == scale);
    REQUIRE_THAT(ptrTrans->DeviceToMapMeasure(1.), WithinRel(0.001, 1e-5));
}

TEST_CASE("Transformation2D: device rect keeps the extent or the scale", "[transformation]")
{
    auto ptrTrans = CreateTrans();
    ptrTrans->SetDeviceRect(GRect(0, 0, 1600, 1200));
    // the same extent in a twice bigger window
    REQUIRE_THAT(ptrTrans->DeviceToMapMeasure(1.), WithinRel(0.5, 1e-5));
    REQUIRE_THAT(ptrTrans->GetFittedBounds().xMin, WithinAbs(600., 0.01));

    ptrTrans->SetDeviceRect(GRect(0, 0, 800, 600), DisplayTransformationPreserveScale);
    REQUIRE_THAT(ptrTrans->DeviceToMapMeasure(1.), WithinRel(0.5, 1e-5));
    REQUIRE_THAT(ptrTrans->GetFittedBounds().xMin, WithinAbs(800., 0.01));
}

TEST_CASE("Transformation2D: point shapes", "[transformation]")
{
    auto ptrTrans = CreateTrans();

    SECTION("point inside")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreatePoint(1010., 2020.));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 1);
        REQUIRE(Near(res.parts[0][0], 410, 280));
    }

    SECTION("point outside")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreatePoint(5000., 2000.));
        REQUIRE(res.parts.empty());
    }

    SECTION("multipoint: only visible points")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateMultiPoint({ {1000., 2000.}, {9000., 2000.}, {1100., 2100.} }));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 2);
        REQUIRE(Near(res.parts[0][0], 400, 300));
        REQUIRE(Near(res.parts[0][1], 500, 200));
    }
}

TEST_CASE("Transformation2D: polygons and lines are clipped by the device clip rect", "[transformation]")
{
    auto ptrTrans = CreateTrans();

    SECTION("inside polygon is not changed")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polygon, { Square(1000., 2000., 100.) }));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 5);
        REQUIRE(Near(res.parts[0][0], 300, 400));
        REQUIRE(Near(res.parts[0][2], 500, 200));
    }

    SECTION("polygon bigger than the window")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polygon, { Square(1000., 2000., 1e7) }));
        REQUIRE(res.parts.size() == 1);
        for (auto& pt : res.parts[0])
            REQUIRE(InRect(pt, Window, 1e-6));
    }

    SECTION("polygon with a hole, the hole outside")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polygon, { Square(1000., 2000., 1000.), Square(3000., 2000., 10.) }));
        REQUIRE(res.parts.size() == 1);
    }

    SECTION("tiny polygon is a dot of 4 points")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polygon, { Square(1000., 2000., 0.01) }));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 4);
    }

    SECTION("long line at a big zoom keeps the direction")
    {
        // 1 pixel = 1 mm, the line ends are 1e6 m away (1e9 pixels)
        ptrTrans->SetMapPos(ptrTrans->GetMapPos(), ptrTrans->GetScale() / 1000.);
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polyline, { { {1000. - 1e6, 2000. - 1e3}, {1000. + 1e6, 2000. + 1e3} } }));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 2);
        // y = 300 - (x - 400) * 0.001
        for (auto& pt : res.parts[0])
        {
            REQUIRE(InRect(pt, Window, 0));
            REQUIRE(std::abs(pt.y - (300 - (pt.x - 400) * 0.001)) <= 1.);
        }
        REQUIRE_THAT(res.parts[0][0].x, WithinAbs(0., 1e-6));
        REQUIRE_THAT(res.parts[0][1].x, WithinAbs(800., 1e-6));
    }

    SECTION("line leaving and entering the window")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polyline,
                { { {1000., 2000.}, {2000., 2000.}, {2000., 2050.}, {1000., 2050.} } }));
        REQUIRE(res.parts.size() == 2);
        REQUIRE_THAT(res.parts[0].back().x, WithinAbs(800., 1e-6));
        REQUIRE_THAT(res.parts[1].front().x, WithinAbs(800., 1e-6));
    }

    SECTION("shape outside")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polyline, { { {5000., 5000.}, {6000., 6000.} } }));
        REQUIRE(res.parts.empty());
    }

    SECTION("no clip rect - nothing is visible")
    {
        ptrTrans->SetDeviceClipRect(GRect());
        ShapeResult res = ToDevice(*ptrTrans, CreatePoint(1000., 2000.));
        REQUIRE(res.parts.empty());
    }
}
