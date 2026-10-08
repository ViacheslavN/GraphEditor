#include "TestCommon.h"

using namespace display_test;
using namespace GraphEngine::Display;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    const GRect Window(0, 0, 800, 600);

    std::shared_ptr<CDisplayTransformation3D> CreateTrans3D(double tilt)
    {
        auto ptrTrans = std::make_shared<CDisplayTransformation3D>(96., CommonLib::UnitsMeters, Window, ScaleOneMeterPerPixel(), tilt);
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

TEST_CASE("Transformation3D: tilt 0 is the plan view", "[transformation3d]")
{
    auto ptr3D = CreateTrans3D(0.);
    auto ptr2D = std::make_shared<CDisplayTransformation2D>(96., CommonLib::UnitsMeters, Window, ScaleOneMeterPerPixel());
    ptr2D->SetMapPos(ptr3D->GetMapPos(), ptr3D->GetScale());

    for (double x = 500.; x <= 1500.; x += 125.)
    {
        for (double y = 1600.; y <= 2400.; y += 100.)
        {
            GPoint p3 = ToDev(*ptr3D, x, y);
            GPoint p2 = ToDev(*ptr2D, x, y);
            REQUIRE(Near(p3, p2.x, p2.y, 1e-6));
        }
    }

    REQUIRE(ptr3D->GetSkyLine() < 0.);
    REQUIRE_THAT(ptr3D->GetFittedBounds().xMin, WithinAbs(600., 0.01));
    REQUIRE_THAT(ptr3D->GetFittedBounds().yMax, WithinAbs(2300., 0.01));
}

TEST_CASE("Transformation3D: perspective", "[transformation3d]")
{
    auto ptrTrans = CreateTrans3D(55.);
    REQUIRE(ptrTrans->GetTilt() == 55.);

    // the anchor stays in the window center
    REQUIRE(Near(ToDev(*ptrTrans, 1000., 2000.), 400, 300));

    // the same 100 m segment: smaller ahead (north, up), bigger near the viewer (south, down)
    GPoint farL = ToDev(*ptrTrans, 950., 2200.), farR = ToDev(*ptrTrans, 1050., 2200.);
    GPoint midL = ToDev(*ptrTrans, 950., 2000.), midR = ToDev(*ptrTrans, 1050., 2000.);
    GPoint nearL = ToDev(*ptrTrans, 950., 1800.), nearR = ToDev(*ptrTrans, 1050., 1800.);

    double farLen = GPoint::CalcDistance(farL, farR);
    double midLen = GPoint::CalcDistance(midL, midR);
    double nearLen = GPoint::CalcDistance(nearL, nearR);
    REQUIRE_THAT(midLen, WithinAbs(100., 1.));
    REQUIRE(farLen < midLen);
    REQUIRE(nearLen > midLen);

    REQUIRE(farL.y < 300);   // ahead is up
    REQUIRE(nearL.y > 300);  // near is down
    REQUIRE(300 - farL.y < 200);  // and foreshortened
    REQUIRE_THAT(farL.y, WithinAbs(farR.y, 1e-6)); // the horizontal line stays horizontal

    REQUIRE_THAT(ptrTrans->GetPerspectiveScale(GPoint(400, 300)), WithinAbs(1., 1e-9));
    REQUIRE(ptrTrans->GetPerspectiveScale(GPoint(400, 0)) < 1.);
    REQUIRE(ptrTrans->GetPerspectiveScale(GPoint(400, 600)) > 1.);

    // straight lines stay straight: the middle of a segment is on the projected segment
    GPoint a = ToDev(*ptrTrans, 900., 1900.), b = ToDev(*ptrTrans, 1100., 2300.), m = ToDev(*ptrTrans, 1000., 2100.);
    double cross = double(b.x - a.x) * double(m.y - a.y) - double(b.y - a.y) * double(m.x - a.x);
    REQUIRE(std::fabs(cross) / GPoint::CalcDistance(a, b) < 1.5);
}

TEST_CASE("Transformation3D: device to map round trip", "[transformation3d]")
{
    const double tilts[] = {20., 55., 70.};
    for (double tilt : tilts)
    {
        auto ptrTrans = CreateTrans3D(tilt);
        ptrTrans->SetRotation(30.);
        GUnits top = (GUnits)std::ceil((std::max)(0., ptrTrans->GetSkyLine())) + 1;
        for (GUnits y = top; y <= 600; y += 37)
        {
            for (GUnits x = 0; x <= 800; x += 100)
            {
                CommonLib::GisXYPoint mp = ToMap(*ptrTrans, x, y);
                GPoint back = ToDev(*ptrTrans, mp.x, mp.y);
                INFO("tilt " << tilt << " point " << x << "," << y);
                REQUIRE(std::abs(back.x - x) <= 1);
                REQUIRE(std::abs(back.y - y) <= 1);
            }
        }
    }
}

TEST_CASE("Transformation3D: visible extent and the sky", "[transformation3d]")
{
    auto ptr2D = CreateTrans3D(0.);
    auto ptrTrans = CreateTrans3D(55.);

    const CommonLib::bbox& box2D = ptr2D->GetFittedBounds();
    const CommonLib::bbox& box3D = ptrTrans->GetFittedBounds();
    // much more ground ahead is visible
    REQUIRE(box3D.yMax > box2D.yMax + 500.);
    REQUIRE(box3D.xMin < box2D.xMin);
    REQUIRE(box3D.xMax > box2D.xMax);
    REQUIRE(box3D.yMin < 2000.);

    // tilt 55: no sky, tilt 75: the sky above the far edge
    REQUIRE(ptrTrans->GetSkyLine() < 0.);
    ptrTrans->SetTilt(75.);
    REQUIRE(ptrTrans->GetSkyLine() > 0.);
    REQUIRE(ptrTrans->GetSkyLine() < 300.);

    // the farthest ground is not smaller than MinPerspectiveScale
    GPoint sky(400, (GUnits)ptrTrans->GetSkyLine());
    REQUIRE(ptrTrans->GetPerspectiveScale(sky) >= ptrTrans->GetMinPerspectiveScale() * 0.99);

    // points in the sky are mapped to the far edge
    CommonLib::GisXYPoint p0 = ToMap(*ptrTrans, 400, 0);
    CommonLib::GisXYPoint p1 = ToMap(*ptrTrans, 400, sky.y);
    REQUIRE_THAT(p0.y, WithinAbs(p1.y, 1e-6));

    SECTION("tilt is limited")
    {
        ptrTrans->SetTilt(120.);
        REQUIRE(ptrTrans->GetTilt() == CDisplayTransformation3D::MaxTilt);
        ptrTrans->SetTilt(-5.);
        REQUIRE(ptrTrans->GetTilt() == 0.);
    }
}

TEST_CASE("Transformation3D: shapes are clipped by the window and the far edge", "[transformation3d]")
{
    auto ptrTrans = CreateTrans3D(70.);
    double sky = ptrTrans->GetSkyLine();
    REQUIRE(sky > 0.);

    SECTION("huge polygon covers the ground below the sky")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polygon, { Square(1000., 2000., 1e6) }));
        REQUIRE(res.parts.size() == 1);
        double minY = 1e9;
        for (auto& pt : res.parts[0])
        {
            REQUIRE(InRect(pt, Window, 1));
            minY = (std::min)(minY, (double)pt.y);
        }
        REQUIRE(std::fabs(minY - sky) <= 1.);
    }

    SECTION("line going far away ends at the sky line")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polyline, { { {1000., 1990.}, {1000., 1e7} } }));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 2);
        REQUIRE_THAT(res.parts[0][0].x, WithinAbs(400., 1e-6));
        REQUIRE_THAT(res.parts[0][1].x, WithinAbs(400., 1e-6));
        REQUIRE(std::fabs(res.parts[0][1].y - sky) <= 1.);
    }

    SECTION("line behind the viewer is not shown")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateShape(CommonLib::shape_type_polyline, { { {500., -1e6}, {1500., -1e6} } }));
        REQUIRE(res.parts.empty());
    }

    SECTION("points: only visible ones")
    {
        ShapeResult res = ToDevice(*ptrTrans, CreateMultiPoint({ {1000., 2000.}, {1000., 1e8}, {1000., -1e8} }));
        REQUIRE(res.parts.size() == 1);
        REQUIRE(res.parts[0].size() == 1);
        REQUIRE(Near(res.parts[0][0], 400, 300));
    }

    SECTION("bbox to device rect")
    {
        CommonLib::bbox bb;
        bb.type = CommonLib::bbox_type_normal;
        bb.xMin = 990.; bb.xMax = 1010.;
        bb.yMin = 1990.; bb.yMax = 2010.;
        GRect rc;
        ptrTrans->MapToDevice(bb, rc);
        REQUIRE(rc.PointInRect(GPoint(400, 300)));

        // the box behind the viewer gives an empty rect, not garbage
        bb.yMin = -1e8; bb.yMax = -1e8 + 10.;
        ptrTrans->MapToDevice(bb, rc);
        REQUIRE(rc.IsEmpty());

        // the box from the viewer to far away: from the bottom to the sky line
        bb.xMin = 999.; bb.xMax = 1001.;
        bb.yMin = 1000.; bb.yMax = 1e7;
        ptrTrans->MapToDevice(bb, rc);
        REQUIRE(rc.yMax >= 600);
        REQUIRE(std::fabs(rc.yMin - sky) <= 1.);
    }
}

TEST_CASE("Transformation3D: rotation turns the direction of travel up", "[transformation3d]")
{
    auto ptrTrans = CreateTrans3D(55.);
    GPoint north = ToDev(*ptrTrans, 1000., 2100.);
    REQUIRE(north.y < 300);

    ptrTrans->SetRotation(180.);
    north = ToDev(*ptrTrans, 1000., 2100.);
    GPoint south = ToDev(*ptrTrans, 1000., 1900.);
    REQUIRE(north.y > 300); // now behind: near the viewer
    REQUIRE(south.y < 300);
}
