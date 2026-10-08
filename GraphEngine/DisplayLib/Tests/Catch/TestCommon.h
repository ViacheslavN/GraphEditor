#pragma once

#ifndef NOMINMAX
#define NOMINMAX // windows.h min/max macros break std::min/max
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "../../Transformation/DisplayTransformation2D.h"
#include "../../Transformation/DisplayTransformation3D.h"
#include "../../Clip/ConvexClipper.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"

namespace display_test
{
    using namespace GraphEngine;

    inline CommonLib::IGeoShapePtr CreatePoint(double x, double y)
    {
        std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
        ptrShape->Create(CommonLib::shape_type_point, 1);
        ptrShape->GetPoints()[0].x = x;
        ptrShape->GetPoints()[0].y = y;
        ptrShape->CalcBB();
        return ptrShape;
    }

    inline CommonLib::IGeoShapePtr CreateMultiPoint(const std::vector<CommonLib::GisXYPoint>& points)
    {
        std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
        ptrShape->Create(CommonLib::shape_type_multipoint, (uint32_t)points.size());
        for (size_t i = 0; i < points.size(); ++i)
            ptrShape->GetPoints()[i] = points[i];
        ptrShape->CalcBB();
        return ptrShape;
    }

    // parts - point lists, polygon rings are closed by the caller if needed
    inline CommonLib::IGeoShapePtr CreateShape(CommonLib::eShapeType type, const std::vector<std::vector<CommonLib::GisXYPoint> >& parts)
    {
        uint32_t nPoints = 0;
        for (auto& part : parts)
            nPoints += (uint32_t)part.size();

        std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
        ptrShape->Create(type, nPoints, (uint32_t)parts.size());
        uint32_t pos = 0;
        for (size_t p = 0; p < parts.size(); ++p)
        {
            if (parts.size() > 1) // a single part shape has no part offsets
                ptrShape->GetParts()[p] = pos;
            for (auto& pt : parts[p])
                ptrShape->GetPoints()[pos++] = pt;
        }
        ptrShape->CalcBB();
        return ptrShape;
    }

    inline std::vector<CommonLib::GisXYPoint> Square(double cx, double cy, double half)
    {
        return { {cx - half, cy - half}, {cx - half, cy + half}, {cx + half, cy + half}, {cx + half, cy - half}, {cx - half, cy - half} };
    }

    struct ShapeResult
    {
        std::vector<std::vector<Display::GPoint> > parts;
        size_t PointCount() const
        {
            size_t n = 0;
            for (auto& p : parts)
                n += p.size();
            return n;
        }
    };

    inline ShapeResult ToDevice(Display::IDisplayTransformation& trans, CommonLib::IGeoShapePtr ptrShape)
    {
        Display::GPoint* pPoints = nullptr;
        int* pParts = nullptr;
        int nCount = -1;
        trans.MapToDevice(ptrShape, &pPoints, &pParts, &nCount);

        ShapeResult res;
        REQUIRE(nCount >= 0);
        if (nCount == 0)
        {
            REQUIRE(pPoints == nullptr);
            REQUIRE(pParts == nullptr);
            return res;
        }
        REQUIRE(pPoints != nullptr);
        REQUIRE(pParts != nullptr);
        for (int i = 0, pos = 0; i < nCount; ++i)
        {
            REQUIRE(pParts[i] > 0);
            res.parts.emplace_back(pPoints + pos, pPoints + pos + pParts[i]);
            pos += pParts[i];
        }
        return res;
    }

    // GUnits are double: compare with a tolerance
    inline bool Near(const Display::GPoint& pt, double x, double y, double tolerance = 0.01)
    {
        return std::fabs(pt.x - x) <= tolerance && std::fabs(pt.y - y) <= tolerance;
    }

    inline bool InRect(const Display::GPoint& pt, const Display::GRect& rc, Display::GUnits tolerance = 1)
    {
        return pt.x >= rc.xMin - tolerance && pt.x <= rc.xMax + tolerance && pt.y >= rc.yMin - tolerance && pt.y <= rc.yMax + tolerance;
    }

    // 800 x 600 window, meters, 1 pixel = 1 meter at 96 dpi
    inline double ScaleOneMeterPerPixel()
    {
        return 96. / 0.0254;
    }
}
