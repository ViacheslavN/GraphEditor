#include "TestCommon.h"

using namespace display_test;
using namespace GraphEngine::Display;
using Catch::Matchers::WithinAbs;

namespace
{
    bool InsideRect(const DPoint& pt, double xMin, double yMin, double xMax, double yMax)
    {
        const double eps = 1e-9;
        return pt.x >= xMin - eps && pt.x <= xMax + eps && pt.y >= yMin - eps && pt.y <= yMax + eps;
    }

    double Area(const TVecDPoints& ring)
    {
        double a = 0.;
        for (size_t i = 0; i < ring.size(); ++i)
        {
            const DPoint& p0 = ring[i];
            const DPoint& p1 = ring[(i + 1) % ring.size()];
            a += p0.x * p1.y - p1.x * p0.y;
        }
        return std::fabs(a) / 2.;
    }
}

TEST_CASE("ConvexClipper: empty clipper rejects everything", "[clip]")
{
    CConvexClipper clipper;
    REQUIRE(clipper.IsEmpty());
    REQUIRE_FALSE(clipper.IsInside(0, 0));

    DPoint pts[3] = { {0, 0}, {1, 0}, {0, 1} };
    REQUIRE(clipper.Relation(pts, 3) == CConvexClipper::RelationOutside);

    // degenerate polygon - nothing is visible
    DPoint line[3] = { {0, 0}, {1, 1}, {2, 2} };
    clipper.SetClipPolygon(line, 3);
    REQUIRE(clipper.IsEmpty());
}

TEST_CASE("ConvexClipper: relation of polygons to a rect", "[clip]")
{
    CConvexClipper clipper;
    clipper.SetClipRect(0, 0, 100, 100);

    DPoint inside[4] = { {10, 10}, {20, 10}, {20, 20}, {10, 20} };
    DPoint outside[4] = { {110, 10}, {120, 10}, {120, 20}, {110, 20} };
    DPoint crossing[4] = { {90, 10}, {120, 10}, {120, 20}, {90, 20} };
    DPoint covering[4] = { {-50, -50}, {150, -50}, {150, 150}, {-50, 150} };
    // outside but not separated by one edge: the diagonal band near the corner
    DPoint corner[3] = { {120, 90}, {130, 130}, {90, 120} };

    REQUIRE(clipper.Relation(inside, 4) == CConvexClipper::RelationInside);
    REQUIRE(clipper.Relation(outside, 4) == CConvexClipper::RelationOutside);
    REQUIRE(clipper.Relation(crossing, 4) == CConvexClipper::RelationIntersects);
    REQUIRE(clipper.Relation(covering, 4) == CConvexClipper::RelationIntersects);
    REQUIRE(clipper.Relation(corner, 3) == CConvexClipper::RelationOutside);
}

TEST_CASE("ConvexClipper: polygon clipping keeps the visible area", "[clip]")
{
    CConvexClipper clipper;
    clipper.SetClipRect(0, 0, 100, 100);

    SECTION("half outside")
    {
        DPoint square[4] = { {50, 20}, {150, 20}, {150, 80}, {50, 80} };
        TVecDPoints out;
        clipper.ClipPolygon(square, 4, out);
        REQUIRE(out.size() >= 4);
        for (auto& pt : out)
            REQUIRE(InsideRect(pt, 0, 0, 100, 100));
        REQUIRE_THAT(Area(out), WithinAbs(50. * 60., 1e-6));
    }

    SECTION("covering polygon becomes the clip rect")
    {
        DPoint big[4] = { {-1e7, -1e7}, {1e7, -1e7}, {1e7, 1e7}, {-1e7, 1e7} };
        TVecDPoints out;
        clipper.ClipPolygon(big, 4, out);
        REQUIRE_THAT(Area(out), WithinAbs(100. * 100., 1e-3));
    }

    SECTION("orientation of the clip polygon does not matter")
    {
        DPoint cw[4] = { {0, 0}, {0, 100}, {100, 100}, {100, 0} };
        clipper.SetClipPolygon(cw, 4);
        DPoint tri[3] = { {50, 50}, {200, 50}, {50, 200} };
        TVecDPoints out;
        clipper.ClipPolygon(tri, 3, out);
        REQUIRE_THAT(Area(out), WithinAbs(50. * 50., 1e-6));
        for (auto& pt : out)
            REQUIRE(InsideRect(pt, 0, 0, 100, 100));
    }

    SECTION("outside polygon gives nothing")
    {
        DPoint square[4] = { {150, 20}, {250, 20}, {250, 80}, {150, 80} };
        TVecDPoints out;
        clipper.ClipPolygon(square, 4, out);
        REQUIRE(out.empty());
    }
}

TEST_CASE("ConvexClipper: polyline clipping", "[clip]")
{
    CConvexClipper clipper;
    clipper.SetClipRect(0, 0, 100, 100);

    SECTION("leaves and comes back: two parts")
    {
        DPoint line[4] = { {10, 50}, {200, 50}, {200, 60}, {10, 60} };
        TVecDPoints out;
        std::vector<int> parts;
        clipper.ClipPolyline(line, 4, out, parts);
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0] == 2);
        REQUIRE(parts[1] == 2);
        REQUIRE_THAT(out[1].x, WithinAbs(100., 1e-9));
        REQUIRE_THAT(out[2].x, WithinAbs(100., 1e-9));
        REQUIRE_THAT(out[3].x, WithinAbs(10., 1e-9));
    }

    SECTION("segment through the rect with both ends outside")
    {
        DPoint line[2] = { {-100, -100}, {200, 200} };
        TVecDPoints out;
        std::vector<int> parts;
        clipper.ClipPolyline(line, 2, out, parts);
        REQUIRE(parts.size() == 1);
        REQUIRE(out.size() == 2);
        REQUIRE_THAT(out[0].x, WithinAbs(0., 1e-9));
        REQUIRE_THAT(out[0].y, WithinAbs(0., 1e-9));
        REQUIRE_THAT(out[1].x, WithinAbs(100., 1e-9));
        REQUIRE_THAT(out[1].y, WithinAbs(100., 1e-9));
    }

    SECTION("segment passing outside near the corner")
    {
        DPoint line[2] = { {90, 130}, {130, 90} };
        TVecDPoints out;
        std::vector<int> parts;
        clipper.ClipPolyline(line, 2, out, parts);
        REQUIRE(parts.empty());
        REQUIRE(out.empty());
    }

    SECTION("inside line is one part with all vertices")
    {
        DPoint line[4] = { {10, 10}, {20, 30}, {40, 10}, {60, 30} };
        TVecDPoints out;
        std::vector<int> parts;
        clipper.ClipPolyline(line, 4, out, parts);
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0] == 4);
    }

    SECTION("very far vertices keep the line direction")
    {
        // y = 50 + x * 0.001, the ends are 1e9 away
        DPoint line[2] = { {-1e9, 50 - 1e6}, {1e9, 50 + 1e6} };
        TVecDPoints out;
        std::vector<int> parts;
        clipper.ClipPolyline(line, 2, out, parts);
        REQUIRE(parts.size() == 1);
        REQUIRE_THAT(out[0].x, WithinAbs(0., 1e-6));
        REQUIRE_THAT(out[0].y, WithinAbs(50., 1e-6));
        REQUIRE_THAT(out[1].x, WithinAbs(100., 1e-6));
        REQUIRE_THAT(out[1].y, WithinAbs(50.1, 1e-6));
    }
}
