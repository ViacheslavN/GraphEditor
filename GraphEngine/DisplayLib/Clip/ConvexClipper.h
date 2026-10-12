#pragma once

#include <vector>
#include <cstddef>

namespace GraphEngine
{
    namespace Display
    {
        struct DPoint
        {
            double x;
            double y;
        };
        typedef std::vector<DPoint> TVecDPoints;

        // Clips polygons, polylines and points against a convex polygon in double precision.
        // Used by the display transformations: geometry is clipped before it is rounded to device units,
        // so there is no integer overflow or precision loss for far away vertices.
        // Not thread safe (internal buffers), one clipper per transformation.
        class CConvexClipper
        {
        public:
            enum eRelation
            {
                RelationOutside,
                RelationIntersects,
                RelationInside
            };

            CConvexClipper();

            void SetClipRect(double xMin, double yMin, double xMax, double yMax);
            // convex polygon, any orientation, without the closing point
            void SetClipPolygon(const DPoint* pPoints, int nCount);
            void Clear();

            bool IsEmpty() const;
            const TVecDPoints& GetClipPolygon() const;

            bool IsInside(double x, double y) const;
            // relation of a convex polygon (f.e. a transformed bounding box) to the clip area
            eRelation Relation(const DPoint* pPoints, int nCount) const;

            // Sutherland-Hodgman, the result is one ring (empty if the polygon is outside)
            void ClipPolygon(const DPoint* pPoints, int nCount, TVecDPoints& out) const;
            // Cyrus-Beck for each segment, the visible pieces are appended to out, their sizes to parts
            void ClipPolyline(const DPoint* pPoints, int nCount, TVecDPoints& out, std::vector<int>& parts) const;

        private:
            double EdgeValue(std::size_t edge, double x, double y) const
            {
                return m_vecA[edge] * x + m_vecB[edge] * y + m_vecC[edge];
            }

        private:
            TVecDPoints m_vecPolygon;   // counter clockwise
            std::vector<double> m_vecA; // edge lines, a*x + b*y + c >= 0 is inside
            std::vector<double> m_vecB;
            std::vector<double> m_vecC;
            mutable TVecDPoints m_vecTmp;
        };
    }
}
