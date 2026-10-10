#include "OSMGeometry.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"
#include "../../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const double EarthHalfCircumference = 20037508.342789244;   // Web Mercator, m
            const double MaxMercatorLatitude = 85.05112877980659;
            const double Pi = 3.14159265358979323846;

            const char* WebMercatorProj4 = "+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +no_defs";
            const char* WGS84Proj4 = "+proj=longlat +datum=WGS84 +no_defs";

            bool SamePoint(const CommonLib::GisXYPoint& a, const CommonLib::GisXYPoint& b)
            {
                return a.x == b.x && a.y == b.y;
            }

            struct SRing
            {
                TOSMPoints points;
                double     dArea = 0.;     // absolute
                double     xMin = 0., yMin = 0., xMax = 0., yMax = 0.;

                void Calc()
                {
                    dArea = std::fabs(COSMGeometry::SignedArea(points));
                    xMin = xMax = points[0].x;
                    yMin = yMax = points[0].y;
                    for(size_t i = 1; i < points.size(); ++i)
                    {
                        xMin = (std::min)(xMin, points[i].x);
                        xMax = (std::max)(xMax, points[i].x);
                        yMin = (std::min)(yMin, points[i].y);
                        yMax = (std::max)(yMax, points[i].y);
                    }
                }

                bool BoundsContain(const SRing& other) const
                {
                    return other.xMin >= xMin && other.xMax <= xMax && other.yMin >= yMin && other.yMax <= yMax;
                }
            };

            // the inner ring lies inside the outer one: most of the tested vertices are inside
            // (rings of a multipolygon can touch each other at some vertices)
            bool RingInside(const SRing& inner, const SRing& outer)
            {
                if(!outer.BoundsContain(inner))
                    return false;

                const size_t nSamples = 5;
                size_t nStep = (std::max)((size_t)1, (inner.points.size() - 1) / nSamples);
                int nInside = 0, nOutside = 0;
                for(size_t i = 0; i + 1 < inner.points.size(); i += nStep)
                {
                    if(COSMGeometry::PointInRing(outer.points, inner.points[i].x, inner.points[i].y))
                        ++nInside;
                    else
                        ++nOutside;
                }
                return nInside > nOutside;
            }
        }

        void COSMProjection::Project(double dLon, double dLat, double& x, double& y) const
        {
            if(!m_bWebMercator)
            {
                x = dLon;
                y = dLat;
                return;
            }

            double lat = (std::max)(-MaxMercatorLatitude, (std::min)(MaxMercatorLatitude, dLat));
            x = dLon * EarthHalfCircumference / 180.;
            y = std::log(std::tan((90. + lat) * Pi / 360.)) * EarthHalfCircumference / Pi;
        }

        Geometry::ISpatialReferencePtr COSMProjection::CreateSpatialReference() const
        {
            return std::make_shared<Geometry::CSpatialReferenceProj4>(std::string(m_bWebMercator ? WebMercatorProj4 : WGS84Proj4));
        }

        CommonLib::Units COSMProjection::GetUnits() const
        {
            return m_bWebMercator ? CommonLib::UnitsMeters : CommonLib::UnitsDecimalDegrees;
        }

        CommonLib::IGeoShapePtr COSMGeometry::CreatePoint(double x, double y)
        {
            std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
            ptrShape->Create(CommonLib::shape_type_point, 1);
            ptrShape->GetPoints()[0].x = x;
            ptrShape->GetPoints()[0].y = y;
            ptrShape->CalcBB();
            return ptrShape;
        }

        CommonLib::IGeoShapePtr COSMGeometry::CreatePolyline(const std::vector<TOSMPoints>& parts)
        {
            uint32_t nPoints = 0, nParts = 0;
            for(size_t i = 0; i < parts.size(); ++i)
            {
                if(parts[i].size() < 2)
                    continue;
                nPoints += (uint32_t)parts[i].size();
                ++nParts;
            }
            if(nParts == 0)
                return CommonLib::IGeoShapePtr();

            std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
            ptrShape->Create(CommonLib::shape_type_polyline, nPoints, nParts);
            CommonLib::GisXYPoint* pPoints = ptrShape->GetPoints();
            uint32_t nPos = 0, nPart = 0;
            for(size_t i = 0; i < parts.size(); ++i)
            {
                if(parts[i].size() < 2)
                    continue;
                if(nParts > 1)
                    ptrShape->GetParts()[nPart] = nPos;
                ++nPart;
                for(size_t p = 0; p < parts[i].size(); ++p)
                    pPoints[nPos++] = parts[i][p];
            }
            ptrShape->CalcBB();
            return ptrShape;
        }

        CommonLib::IGeoShapePtr COSMGeometry::CreatePolygon(const std::vector<TOSMPoints>& rings, const std::vector<bool>& outer)
        {
            uint32_t nPoints = 0, nParts = 0;
            for(size_t i = 0; i < rings.size(); ++i)
            {
                if(rings[i].size() < 4)
                    continue;
                nPoints += (uint32_t)rings[i].size();
                ++nParts;
            }
            if(nParts == 0)
                return CommonLib::IGeoShapePtr();

            std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
            ptrShape->Create(CommonLib::shape_type_polygon, nPoints, nParts);
            CommonLib::GisXYPoint* pPoints = ptrShape->GetPoints();
            uint32_t nPos = 0, nPart = 0;
            for(size_t i = 0; i < rings.size(); ++i)
            {
                const TOSMPoints& ring = rings[i];
                if(ring.size() < 4)
                    continue;
                if(nParts > 1)
                    ptrShape->GetParts()[nPart] = nPos;
                ++nPart;

                // outer: clockwise (negative area), hole: counter clockwise
                bool bOuter = i < outer.size() ? outer[i] : true;
                bool bReverse = (SignedArea(ring) > 0.) == bOuter;
                for(size_t p = 0; p < ring.size(); ++p)
                    pPoints[nPos++] = bReverse ? ring[ring.size() - 1 - p] : ring[p];
            }
            ptrShape->CalcBB();
            return ptrShape;
        }

        double COSMGeometry::SignedArea(const TOSMPoints& ring)
        {
            double dArea = 0.;
            for(size_t i = 0, sz = ring.size(); i + 1 < sz; ++i)
                dArea += ring[i].x * ring[i + 1].y - ring[i + 1].x * ring[i].y;
            return dArea / 2.;
        }

        bool COSMGeometry::PointInRing(const TOSMPoints& ring, double x, double y)
        {
            bool bInside = false;
            for(size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
            {
                const CommonLib::GisXYPoint& a = ring[i];
                const CommonLib::GisXYPoint& b = ring[j];
                if((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
                    bInside = !bInside;
            }
            return bInside;
        }

        bool COSMGeometry::BuildMultipolygon(const std::vector<TOSMPoints>& ways, std::vector<TOSMPoints>& rings, std::vector<bool>& outer)
        {
            rings.clear();
            outer.clear();

            std::vector<SRing> built;
            std::vector<size_t> open;
            for(size_t i = 0; i < ways.size(); ++i)
            {
                const TOSMPoints& way = ways[i];
                if(way.size() < 2)
                    continue;
                if(way.size() >= 4 && SamePoint(way.front(), way.back()))
                {
                    SRing ring;
                    ring.points = way;
                    built.push_back(ring);
                }
                else
                    open.push_back(i);
            }

            // the open ways are joined by their end points
            typedef std::pair<double, double> TKey;
            std::multimap<TKey, size_t> ends;
            for(size_t k = 0; k < open.size(); ++k)
            {
                const TOSMPoints& way = ways[open[k]];
                ends.insert(std::make_pair(TKey(way.front().x, way.front().y), k));
                ends.insert(std::make_pair(TKey(way.back().x, way.back().y), k));
            }

            std::vector<bool> used(open.size(), false);
            for(size_t k = 0; k < open.size(); ++k)
            {
                if(used[k])
                    continue;
                used[k] = true;
                TOSMPoints ring = ways[open[k]];

                bool bClosed = false;
                while(!bClosed)
                {
                    const CommonLib::GisXYPoint end = ring.back();
                    auto range = ends.equal_range(TKey(end.x, end.y));
                    size_t nNext = open.size();
                    for(auto it = range.first; it != range.second; ++it)
                    {
                        if(!used[it->second])
                        {
                            nNext = it->second;
                            break;
                        }
                    }
                    if(nNext == open.size())
                        break;   // can't be closed

                    used[nNext] = true;
                    const TOSMPoints& next = ways[open[nNext]];
                    if(SamePoint(next.front(), end))
                        ring.insert(ring.end(), next.begin() + 1, next.end());
                    else
                        ring.insert(ring.end(), next.rbegin() + 1, next.rend());

                    bClosed = ring.size() >= 4 && SamePoint(ring.front(), ring.back());
                }

                if(bClosed)
                {
                    SRing closed;
                    closed.points.swap(ring);
                    built.push_back(closed);
                }
            }

            for(size_t i = 0; i < built.size(); ++i)
                built[i].Calc();
            built.erase(std::remove_if(built.begin(), built.end(), [](const SRing& ring) { return ring.dArea <= 0.; }), built.end());
            if(built.empty())
                return false;

            // nesting: a ring in an outer ring is a hole, a ring in a hole is an outer ring (an island) ...
            std::stable_sort(built.begin(), built.end(), [](const SRing& a, const SRing& b) { return a.dArea > b.dArea; });
            std::vector<int> depth(built.size(), 0);
            for(size_t i = 0; i < built.size(); ++i)
            {
                for(size_t j = i; j-- > 0;)
                {
                    // the smallest containing ring (the rings are sorted by area)
                    if(RingInside(built[i], built[j]))
                    {
                        depth[i] = depth[j] + 1;
                        break;
                    }
                }
            }

            for(size_t i = 0; i < built.size(); ++i)
            {
                rings.push_back(std::move(built[i].points));
                outer.push_back(depth[i] % 2 == 0);
            }
            return true;
        }
    }
}
