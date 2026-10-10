#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <utility>
#include "../../../DisplayLib/GraphTypes/Rect.h"

// Device space geometry used to place labels (y axis goes down, angles are in degrees, clockwise on the screen,
// the same as the orientation of Display::CFont).

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            const double LabelPi = 3.14159265358979323846;

            inline double LabelDegToRad(double deg) { return deg * LabelPi / 180.0; }
            inline double LabelRadToDeg(double rad) { return rad * 180.0 / LabelPi; }

            // angle of the text to be read from left to right: (-90, 90]
            inline double NormalizeReadableAngle(double angle)
            {
                while(angle > 180.0)
                    angle -= 360.0;
                while(angle <= -180.0)
                    angle += 360.0;
                if(angle > 90.0)
                    angle -= 180.0;
                else if(angle <= -90.0)
                    angle += 180.0;
                return angle;
            }

            // difference of two angles in (-180, 180]
            inline double AngleDifference(double a1, double a2)
            {
                double d = a1 - a2;
                while(d > 180.0)
                    d -= 360.0;
                while(d <= -180.0)
                    d += 360.0;
                return d;
            }

            struct SLabelPoint
            {
                double x;
                double y;

                SLabelPoint() : x(0.), y(0.) {}
                SLabelPoint(double _x, double _y) : x(_x), y(_y) {}
            };

            typedef std::vector<SLabelPoint> TLabelPoints;

            // oriented rectangle: center, half sizes along the text direction (u) and across it (v)
            struct SLabelBox
            {
                double cx;
                double cy;
                double hw;
                double hh;
                double cosA;
                double sinA;

                SLabelBox() : cx(0.), cy(0.), hw(0.), hh(0.), cosA(1.), sinA(0.) {}

                static SLabelBox FromCenter(double cx, double cy, double width, double height, double angleDeg)
                {
                    SLabelBox box;
                    box.cx = cx;
                    box.cy = cy;
                    box.hw = width / 2.0;
                    box.hh = height / 2.0;
                    double rad = LabelDegToRad(angleDeg);
                    box.cosA = cos(rad);
                    box.sinA = sin(rad);
                    return box;
                }

                SLabelBox Inflated(double d) const
                {
                    SLabelBox box = *this;
                    box.hw += d;
                    box.hh += d;
                    return box;
                }

                // half sizes of the axis aligned bounding box
                double ExtentX() const { return hw * fabs(cosA) + hh * fabs(sinA); }
                double ExtentY() const { return hw * fabs(sinA) + hh * fabs(cosA); }

                Display::GRect Bounds() const
                {
                    double ex = ExtentX();
                    double ey = ExtentY();
                    return Display::GRect(cx - ex, cy - ey, cx + ex, cy + ey);
                }

                void GetCorners(SLabelPoint corners[4]) const
                {
                    const double su[4] = {-1., 1., 1., -1.};
                    const double sv[4] = {-1., -1., 1., 1.};
                    for(int i = 0; i < 4; ++i)
                    {
                        double u = su[i] * hw;
                        double v = sv[i] * hh;
                        corners[i].x = cx + u * cosA - v * sinA;
                        corners[i].y = cy + u * sinA + v * cosA;
                    }
                }

                // separating axis test, touching boxes don't intersect
                bool Intersects(const SLabelBox& other) const
                {
                    const double axes[4][2] = { {cosA, sinA}, {-sinA, cosA}, {other.cosA, other.sinA}, {-other.sinA, other.cosA} };
                    double dx = other.cx - cx;
                    double dy = other.cy - cy;
                    for(int i = 0; i < 4; ++i)
                    {
                        double ax = axes[i][0];
                        double ay = axes[i][1];
                        double dist = fabs(dx * ax + dy * ay);
                        double r1 = hw * fabs(cosA * ax + sinA * ay) + hh * fabs(-sinA * ax + cosA * ay);
                        double r2 = other.hw * fabs(other.cosA * ax + other.sinA * ay) + other.hh * fabs(-other.sinA * ax + other.cosA * ay);
                        if(dist >= r1 + r2)
                            return false;
                    }
                    return true;
                }

                // segment crosses the inside of the box (Liang-Barsky in the box frame)
                bool IntersectsSegment(const SLabelPoint& p1, const SLabelPoint& p2) const
                {
                    double x1 = (p1.x - cx) * cosA + (p1.y - cy) * sinA;
                    double y1 = -(p1.x - cx) * sinA + (p1.y - cy) * cosA;
                    double x2 = (p2.x - cx) * cosA + (p2.y - cy) * sinA;
                    double y2 = -(p2.x - cx) * sinA + (p2.y - cy) * cosA;

                    double t0 = 0.0, t1 = 1.0;
                    double dx = x2 - x1;
                    double dy = y2 - y1;
                    const double p[4] = {-dx, dx, -dy, dy};
                    const double q[4] = {x1 + hw, hw - x1, y1 + hh, hh - y1};
                    for(int i = 0; i < 4; ++i)
                    {
                        if(p[i] == 0.0)
                        {
                            if(q[i] <= 0.0)
                                return false;
                            continue;
                        }
                        double t = q[i] / p[i];
                        if(p[i] < 0.0)
                        {
                            if(t > t1)
                                return false;
                            if(t > t0)
                                t0 = t;
                        }
                        else
                        {
                            if(t < t0)
                                return false;
                            if(t < t1)
                                t1 = t;
                        }
                    }
                    return t0 < t1;
                }
            };

            typedef std::vector<SLabelBox> TLabelBoxes;

            // polyline with the distances of its vertices from the start
            class CLabelPath
            {
            public:
                CLabelPath() {}

                explicit CLabelPath(const TLabelPoints& points)
                {
                    Set(points);
                }

                void Set(const TLabelPoints& points)
                {
                    m_points.clear();
                    m_dist.clear();
                    for(size_t i = 0; i < points.size(); ++i)
                    {
                        if(!m_points.empty() && m_points.back().x == points[i].x && m_points.back().y == points[i].y)
                            continue;
                        double d = 0.0;
                        if(!m_points.empty())
                            d = m_dist.back() + hypot(points[i].x - m_points.back().x, points[i].y - m_points.back().y);
                        m_points.push_back(points[i]);
                        m_dist.push_back(d);
                    }
                }

                CLabelPath Reversed() const
                {
                    TLabelPoints points(m_points.rbegin(), m_points.rend());
                    return CLabelPath(points);
                }

                bool IsValid() const { return m_points.size() > 1 && Length() > 0.0; }
                double Length() const { return m_dist.empty() ? 0.0 : m_dist.back(); }
                const TLabelPoints& Points() const { return m_points; }
                const std::vector<double>& Distances() const { return m_dist; }

                // point at the distance from the start and the direction (degrees) of its segment
                SLabelPoint PointAt(double d, double* pAngle = nullptr) const
                {
                    if(m_points.size() < 2)
                    {
                        if(pAngle)
                            *pAngle = 0.0;
                        return m_points.empty() ? SLabelPoint() : m_points[0];
                    }

                    size_t seg = SegmentAt(d);
                    const SLabelPoint& a = m_points[seg];
                    const SLabelPoint& b = m_points[seg + 1];
                    double len = m_dist[seg + 1] - m_dist[seg];
                    double t = len > 0.0 ? (d - m_dist[seg]) / len : 0.0;
                    t = (std::max)(0.0, (std::min)(1.0, t));
                    if(pAngle)
                        *pAngle = LabelRadToDeg(atan2(b.y - a.y, b.x - a.x));
                    return SLabelPoint(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
                }

                // max distance of the path between d1 and d2 from the straight line through the points at d1 and d2
                double Deviation(double d1, double d2) const
                {
                    SLabelPoint a = PointAt(d1);
                    SLabelPoint b = PointAt(d2);
                    double len = hypot(b.x - a.x, b.y - a.y);
                    if(len <= 0.0)
                        return 0.0;

                    double maxDev = 0.0;
                    for(size_t i = SegmentAt(d1) + 1; i < m_points.size() && m_dist[i] < d2; ++i)
                    {
                        double dev = fabs((b.x - a.x) * (a.y - m_points[i].y) - (a.x - m_points[i].x) * (b.y - a.y)) / len;
                        maxDev = (std::max)(maxDev, dev);
                    }
                    return maxDev;
                }

            private:
                size_t SegmentAt(double d) const
                {
                    std::vector<double>::const_iterator it = std::upper_bound(m_dist.begin(), m_dist.end(), d);
                    size_t idx = it == m_dist.begin() ? 0 : (size_t)(it - m_dist.begin()) - 1;
                    if(idx > m_points.size() - 2)
                        idx = m_points.size() - 2;
                    return idx;
                }

            private:
                TLabelPoints m_points;
                std::vector<double> m_dist;
            };

            // polygon (outer ring with holes) in device coordinates, the even-odd rule is used
            class CLabelPolygon
            {
            public:
                CLabelPolygon() : m_xMin(0.), m_yMin(0.), m_xMax(0.), m_yMax(0.) {}

                void AddRing(const TLabelPoints& ring)
                {
                    if(ring.size() < 3)
                        return;

                    if(m_rings.empty())
                    {
                        m_xMin = m_xMax = ring[0].x;
                        m_yMin = m_yMax = ring[0].y;
                    }
                    for(size_t i = 0; i < ring.size(); ++i)
                    {
                        m_xMin = (std::min)(m_xMin, ring[i].x);
                        m_xMax = (std::max)(m_xMax, ring[i].x);
                        m_yMin = (std::min)(m_yMin, ring[i].y);
                        m_yMax = (std::max)(m_yMax, ring[i].y);
                    }
                    m_rings.push_back(ring);
                }

                bool IsEmpty() const { return m_rings.empty(); }
                const std::vector<TLabelPoints>& Rings() const { return m_rings; }
                double XMin() const { return m_xMin; }
                double YMin() const { return m_yMin; }
                double XMax() const { return m_xMax; }
                double YMax() const { return m_yMax; }

                static double SignedArea(const TLabelPoints& ring)
                {
                    double area = 0.0;
                    for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                        area += (ring[j].x - ring[i].x) * (ring[j].y + ring[i].y);
                    return area / 2.0;
                }

                bool Contains(double x, double y) const
                {
                    bool inside = false;
                    for(size_t r = 0; r < m_rings.size(); ++r)
                    {
                        const TLabelPoints& ring = m_rings[r];
                        for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                        {
                            const SLabelPoint& a = ring[i];
                            const SLabelPoint& b = ring[j];
                            if((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
                                inside = !inside;
                        }
                    }
                    return inside;
                }

                // distance to the border, positive inside
                double SignedDistance(double x, double y) const
                {
                    double minDist2 = -1.0;
                    for(size_t r = 0; r < m_rings.size(); ++r)
                    {
                        const TLabelPoints& ring = m_rings[r];
                        for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                        {
                            double d2 = SegmentDistance2(x, y, ring[j], ring[i]);
                            if(minDist2 < 0.0 || d2 < minDist2)
                                minDist2 = d2;
                        }
                    }
                    double dist = minDist2 < 0.0 ? 0.0 : sqrt(minDist2);
                    return Contains(x, y) ? dist : -dist;
                }

                // the box is inside the polygon: its center is inside and no edge crosses the box
                // (a border touching the box is allowed, the box is reduced a bit against the rounding errors)
                bool ContainsBox(const SLabelBox& testBox) const
                {
                    SLabelBox box = testBox.Inflated(-0.01);
                    if(!Contains(box.cx, box.cy))
                        return false;

                    Display::GRect bounds = box.Bounds();
                    for(size_t r = 0; r < m_rings.size(); ++r)
                    {
                        const TLabelPoints& ring = m_rings[r];
                        for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                        {
                            const SLabelPoint& a = ring[j];
                            const SLabelPoint& b = ring[i];
                            if((std::max)(a.x, b.x) < bounds.xMin || (std::min)(a.x, b.x) > bounds.xMax ||
                               (std::max)(a.y, b.y) < bounds.yMin || (std::min)(a.y, b.y) > bounds.yMax)
                                continue;
                            if(box.IntersectsSegment(a, b))
                                return false;
                        }
                    }
                    return true;
                }

                // inside parts of the horizontal line y, sorted
                void ScanLine(double y, std::vector<std::pair<double, double> >& intervals) const
                {
                    intervals.clear();
                    std::vector<double> xs;
                    for(size_t r = 0; r < m_rings.size(); ++r)
                    {
                        const TLabelPoints& ring = m_rings[r];
                        for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                        {
                            const SLabelPoint& a = ring[i];
                            const SLabelPoint& b = ring[j];
                            if((a.y > y) != (b.y > y))
                                xs.push_back(a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y));
                        }
                    }
                    std::sort(xs.begin(), xs.end());
                    for(size_t i = 0; i + 1 < xs.size(); i += 2)
                        intervals.push_back(std::make_pair(xs[i], xs[i + 1]));
                }

                // pole of inaccessibility: the inside point most distant from the border
                // (V. Agafonkin, "polylabel"), it is a better label point than the centroid for concave polygons
                SLabelPoint PoleOfInaccessibility(double precision, int maxCells = 2000) const;

                // direction of the main axis (degrees) and how elongated the polygon is (0 - round, 1 - a line)
                void MainAxis(double* pAngle, double* pElongation) const;

                CLabelPolygon Rotated(double cx, double cy, double angleDeg) const
                {
                    double rad = LabelDegToRad(angleDeg);
                    double c = cos(rad), s = sin(rad);
                    CLabelPolygon polygon;
                    TLabelPoints rotated;
                    for(size_t r = 0; r < m_rings.size(); ++r)
                    {
                        rotated.resize(m_rings[r].size());
                        for(size_t i = 0; i < m_rings[r].size(); ++i)
                        {
                            double dx = m_rings[r][i].x - cx;
                            double dy = m_rings[r][i].y - cy;
                            rotated[i].x = cx + dx * c - dy * s;
                            rotated[i].y = cy + dx * s + dy * c;
                        }
                        polygon.AddRing(rotated);
                    }
                    return polygon;
                }

            private:
                static double SegmentDistance2(double x, double y, const SLabelPoint& a, const SLabelPoint& b)
                {
                    double dx = b.x - a.x;
                    double dy = b.y - a.y;
                    double px = a.x, py = a.y;
                    if(dx != 0.0 || dy != 0.0)
                    {
                        double t = ((x - a.x) * dx + (y - a.y) * dy) / (dx * dx + dy * dy);
                        if(t > 1.0)
                        {
                            px = b.x;
                            py = b.y;
                        }
                        else if(t > 0.0)
                        {
                            px += dx * t;
                            py += dy * t;
                        }
                    }
                    dx = x - px;
                    dy = y - py;
                    return dx * dx + dy * dy;
                }

            private:
                std::vector<TLabelPoints> m_rings;
                double m_xMin, m_yMin, m_xMax, m_yMax;
            };

        }
    }
}
