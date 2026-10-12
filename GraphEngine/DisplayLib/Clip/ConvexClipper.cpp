#include "ConvexClipper.h"
#include <cmath>

namespace GraphEngine
{
    namespace Display
    {
        CConvexClipper::CConvexClipper()
        {

        }

        void CConvexClipper::Clear()
        {
            m_vecPolygon.clear();
            m_vecA.clear();
            m_vecB.clear();
            m_vecC.clear();
        }

        void CConvexClipper::SetClipRect(double xMin, double yMin, double xMax, double yMax)
        {
            DPoint pts[4] = { {xMin, yMin}, {xMax, yMin}, {xMax, yMax}, {xMin, yMax} };
            SetClipPolygon(pts, 4);
        }

        void CConvexClipper::SetClipPolygon(const DPoint* pPoints, int nCount)
        {
            Clear();
            if(nCount < 3)
                return;

            double area = 0.;
            for (int i = 0; i < nCount; ++i)
            {
                const DPoint& p0 = pPoints[i];
                const DPoint& p1 = pPoints[(i + 1) % nCount];
                area += p0.x * p1.y - p1.x * p0.y;
            }

            if(!(std::fabs(area) > 0.))
                return; // degenerate, nothing is visible

            m_vecPolygon.assign(pPoints, pPoints + nCount);
            if(area < 0.)
                m_vecPolygon = TVecDPoints(m_vecPolygon.rbegin(), m_vecPolygon.rend());

            for (size_t i = 0; i < m_vecPolygon.size(); ++i)
            {
                const DPoint& p0 = m_vecPolygon[i];
                const DPoint& p1 = m_vecPolygon[(i + 1) % m_vecPolygon.size()];
                // left side of p0->p1 is inside for a counter clockwise polygon
                double a = -(p1.y - p0.y);
                double b = p1.x - p0.x;
                double len = std::sqrt(a * a + b * b);
                if(len == 0.)
                    continue;

                a /= len;
                b /= len;
                m_vecA.push_back(a);
                m_vecB.push_back(b);
                m_vecC.push_back(-(a * p0.x + b * p0.y));
            }

            if(m_vecA.size() < 3)
                Clear();
        }

        bool CConvexClipper::IsEmpty() const
        {
            return m_vecA.empty();
        }

        const TVecDPoints& CConvexClipper::GetClipPolygon() const
        {
            return m_vecPolygon;
        }

        bool CConvexClipper::IsInside(double x, double y) const
        {
            if(IsEmpty())
                return false;

            for (size_t e = 0; e < m_vecA.size(); ++e)
            {
                if(EdgeValue(e, x, y) < 0.)
                    return false;
            }
            return true;
        }

        CConvexClipper::eRelation CConvexClipper::Relation(const DPoint* pPoints, int nCount) const
        {
            if(IsEmpty() || nCount <= 0)
                return RelationOutside;

            bool bAllInside = true;
            for (int i = 0; i < nCount && bAllInside; ++i)
                bAllInside = IsInside(pPoints[i].x, pPoints[i].y);

            if(bAllInside)
                return RelationInside;

            // all points outside of one edge - outside
            for (size_t e = 0; e < m_vecA.size(); ++e)
            {
                bool bAllOut = true;
                for (int i = 0; i < nCount && bAllOut; ++i)
                    bAllOut = EdgeValue(e, pPoints[i].x, pPoints[i].y) < 0.;

                if(bAllOut)
                    return RelationOutside;
            }

            if(nCount >= 3)
            {
                TVecDPoints out;
                ClipPolygon(pPoints, nCount, out);
                return out.empty() ? RelationOutside : RelationIntersects;
            }

            return RelationIntersects;
        }

        void CConvexClipper::ClipPolygon(const DPoint* pPoints, int nCount, TVecDPoints& out) const
        {
            out.clear();
            if(IsEmpty() || nCount <= 0)
                return;

            out.assign(pPoints, pPoints + nCount);
            for (size_t e = 0; e < m_vecA.size() && !out.empty(); ++e)
            {
                m_vecTmp.swap(out);
                out.clear();

                size_t n = m_vecTmp.size();
                DPoint prev = m_vecTmp[n - 1];
                double prevVal = EdgeValue(e, prev.x, prev.y);
                for (size_t i = 0; i < n; ++i)
                {
                    const DPoint cur = m_vecTmp[i];
                    double curVal = EdgeValue(e, cur.x, cur.y);
                    bool bCurIn = curVal >= 0.;
                    bool bPrevIn = prevVal >= 0.;

                    if(bCurIn != bPrevIn)
                    {
                        double t = prevVal / (prevVal - curVal);
                        DPoint ip = { prev.x + (cur.x - prev.x) * t, prev.y + (cur.y - prev.y) * t };
                        out.push_back(ip);
                    }

                    if(bCurIn)
                        out.push_back(cur);

                    prev = cur;
                    prevVal = curVal;
                }
            }
        }

        void CConvexClipper::ClipPolyline(const DPoint* pPoints, int nCount, TVecDPoints& out, std::vector<int>& parts) const
        {
            if(IsEmpty() || nCount <= 0)
                return;

            size_t nPartBegin = out.size();
            bool bOpen = false; // the last visible piece ended at the end of its segment

            auto closePart = [&]()
            {
                if(out.size() > nPartBegin)
                    parts.push_back((int)(out.size() - nPartBegin));
                nPartBegin = out.size();
                bOpen = false;
            };

            if(nCount == 1)
            {
                if(IsInside(pPoints[0].x, pPoints[0].y))
                {
                    out.push_back(pPoints[0]);
                    closePart();
                }
                return;
            }

            for (int i = 0; i + 1 < nCount; ++i)
            {
                const DPoint& p0 = pPoints[i];
                const DPoint& p1 = pPoints[i + 1];
                double dx = p1.x - p0.x;
                double dy = p1.y - p0.y;

                double t0 = 0., t1 = 1.;
                bool bVisible = true;
                for (size_t e = 0; e < m_vecA.size() && bVisible; ++e)
                {
                    double f0 = EdgeValue(e, p0.x, p0.y);
                    double df = m_vecA[e] * dx + m_vecB[e] * dy;
                    if(df == 0.)
                    {
                        if(f0 < 0.)
                            bVisible = false;
                        continue;
                    }

                    double t = -f0 / df;
                    if(df > 0.)
                    {
                        if(t > t0)
                            t0 = t; // entering
                    }
                    else if(t < t1)
                        t1 = t;     // leaving

                    if(t0 > t1)
                        bVisible = false;
                }

                if(!bVisible)
                {
                    closePart();
                    continue;
                }

                DPoint q0 = { p0.x + dx * t0, p0.y + dy * t0 };
                DPoint q1 = { p0.x + dx * t1, p0.y + dy * t1 };

                if(!(bOpen && t0 == 0.))
                {
                    closePart();
                    out.push_back(q0);
                }
                out.push_back(q1);
                bOpen = true;

                if(t1 < 1.)
                    closePart(); // left the clip area
            }
            closePart();
        }
    }
}
