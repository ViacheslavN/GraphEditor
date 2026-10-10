#include "LabelGeometry.h"
#include <queue>

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            namespace
            {
                struct SCell
                {
                    double x;     // center
                    double y;
                    double h;     // half size
                    double d;     // distance from the center to the polygon border (positive inside)
                    double max;   // max possible distance for a point in the cell

                    SCell(double _x, double _y, double _h, const CLabelPolygon& polygon) : x(_x), y(_y), h(_h)
                    {
                        d = polygon.SignedDistance(x, y);
                        max = d + h * 1.4142135623730951;
                    }

                    bool operator<(const SCell& other) const
                    {
                        return max < other.max;
                    }
                };

                SCell CentroidCell(const CLabelPolygon& polygon)
                {
                    const TLabelPoints& ring = polygon.Rings()[0];
                    double area = 0.0, x = 0.0, y = 0.0;
                    for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                    {
                        const SLabelPoint& a = ring[i];
                        const SLabelPoint& b = ring[j];
                        double f = a.x * b.y - b.x * a.y;
                        x += (a.x + b.x) * f;
                        y += (a.y + b.y) * f;
                        area += f * 3.0;
                    }
                    if(area == 0.0)
                        return SCell(ring[0].x, ring[0].y, 0.0, polygon);
                    return SCell(x / area, y / area, 0.0, polygon);
                }
            }

            SLabelPoint CLabelPolygon::PoleOfInaccessibility(double precision, int maxCells) const
            {
                if(m_rings.empty())
                    return SLabelPoint();

                double width = m_xMax - m_xMin;
                double height = m_yMax - m_yMin;
                double cellSize = (std::min)(width, height);
                if(cellSize <= 0.0)
                    return SLabelPoint(m_xMin, m_yMin);

                // for a long thin polygon the cells are too many, they are made bigger
                const double maxInitialCells = 256.0;
                double initialCells = (width / cellSize) * (height / cellSize);
                if(initialCells > maxInitialCells)
                    cellSize *= sqrt(initialCells / maxInitialCells);

                double h = cellSize / 2.0;
                std::priority_queue<SCell> queue;
                for(double x = m_xMin; x < m_xMax; x += cellSize)
                {
                    for(double y = m_yMin; y < m_yMax; y += cellSize)
                        queue.push(SCell(x + h, y + h, h, *this));
                }

                SCell best = CentroidCell(*this);
                SCell bboxCell(m_xMin + width / 2.0, m_yMin + height / 2.0, 0.0, *this);
                if(bboxCell.d > best.d)
                    best = bboxCell;

                int nCells = (int)queue.size();
                while(!queue.empty())
                {
                    SCell cell = queue.top();
                    queue.pop();

                    if(cell.d > best.d)
                        best = cell;

                    if(cell.max - best.d <= precision || nCells >= maxCells)
                        continue;

                    h = cell.h / 2.0;
                    queue.push(SCell(cell.x - h, cell.y - h, h, *this));
                    queue.push(SCell(cell.x + h, cell.y - h, h, *this));
                    queue.push(SCell(cell.x - h, cell.y + h, h, *this));
                    queue.push(SCell(cell.x + h, cell.y + h, h, *this));
                    nCells += 4;
                }

                return SLabelPoint(best.x, best.y);
            }

            void CLabelPolygon::MainAxis(double* pAngle, double* pElongation) const
            {
                // second moments of the border (every edge is a uniform segment), it doesn't depend on the vertex density
                double sumLen = 0.0, mx = 0.0, my = 0.0, sxx = 0.0, syy = 0.0, sxy = 0.0;
                for(size_t r = 0; r < m_rings.size(); ++r)
                {
                    const TLabelPoints& ring = m_rings[r];
                    for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                    {
                        const SLabelPoint& a = ring[j];
                        const SLabelPoint& b = ring[i];
                        double len = hypot(b.x - a.x, b.y - a.y);
                        if(len <= 0.0)
                            continue;
                        sumLen += len;
                        mx += len * (a.x + b.x) / 2.0;
                        my += len * (a.y + b.y) / 2.0;
                        sxx += len * (a.x * a.x + a.x * b.x + b.x * b.x) / 3.0;
                        syy += len * (a.y * a.y + a.y * b.y + b.y * b.y) / 3.0;
                        sxy += len * (2.0 * a.x * a.y + a.x * b.y + b.x * a.y + 2.0 * b.x * b.y) / 6.0;
                    }
                }

                *pAngle = 0.0;
                *pElongation = 0.0;
                if(sumLen <= 0.0)
                    return;

                mx /= sumLen;
                my /= sumLen;
                double cxx = sxx / sumLen - mx * mx;
                double cyy = syy / sumLen - my * my;
                double cxy = sxy / sumLen - mx * my;

                double tr = (cxx + cyy) / 2.0;
                double det = sqrt((std::max)(0.0, (cxx - cyy) * (cxx - cyy) / 4.0 + cxy * cxy));
                double lmax = tr + det;
                double lmin = tr - det;
                if(lmax <= 0.0)
                    return;

                *pAngle = LabelRadToDeg(0.5 * atan2(2.0 * cxy, cxx - cyy));
                *pElongation = 1.0 - (std::max)(0.0, lmin) / lmax;
            }

        }
    }
}
