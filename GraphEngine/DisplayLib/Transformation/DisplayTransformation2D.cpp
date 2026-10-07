#include "DisplayTransformation2D.h"
#include "Matrix4.h"
#include "../GraphTypes/Rect.h"

#include <cmath>
#include <cstring>
#include <algorithm>

namespace GraphEngine
{
    namespace Display
    {
        namespace
        {
            // device coordinates are limited, the rasterizer works with int (or 24.8 fixed point) coordinates
            const double MAXCLIENT = 500000.;
            const double MINCLIENT = -MAXCLIENT;
#ifdef _FLOAT_GUNITS_
            const double MinPointDistance = 0.25; // pixels, GUnits are double (subpixel drawing)
#else
            const double MinPointDistance = 0.5;  // the same pixel
#endif

            void AddToBox(CommonLib::bbox& box, double x, double y, bool bFirst)
            {
                if (bFirst)
                {
                    box.xMin = box.xMax = x;
                    box.yMin = box.yMax = y;
                    return;
                }
                box.xMin = (std::min)(box.xMin, x);
                box.xMax = (std::max)(box.xMax, x);
                box.yMin = (std::min)(box.yMin, y);
                box.yMax = (std::max)(box.yMax, y);
            }
        }

        CDisplayTransformation2D::CDisplayTransformation2D(double resolution, CommonLib::Units map_units, const GRect &dev_rect , double scale) :
                m_bClipExists(false),
                m_dRefScale(0.0),
                m_dCurScale(scale > 0. ? scale : 1.),
                m_dScaleRatio(1.0),
                m_dResolution(resolution > 0. ? resolution : 96.),
                m_mapUnits(map_units),
                m_dAngle(0),
                m_bVerticalFlip(false),
                m_bHorizontalFlip(false)
        {
            memset(&m_AnchorDev, 0, sizeof(m_AnchorDev));
            memset(&m_AnchorMap, 0, sizeof(m_AnchorMap));
            memset(&m_MatrixDev2Map, 0, sizeof(m_MatrixDev2Map));
            memset(&m_MatrixMap2Dev, 0, sizeof(m_MatrixMap2Dev));
            m_mapCurFittedExtent.type = CommonLib::bbox_type_null;

            SetClientRect(dev_rect);
            UpdateScaleRatio();
        }

        CDisplayTransformation2D::~CDisplayTransformation2D()
        {
        }

        double CDisplayTransformation2D::DeviceToMapMeasure( double deviceLen )
        {
            return deviceLen * m_dScaleRatio;
        }

        double CDisplayTransformation2D::MapToDeviceMeasure( double mapLen )
        {
            return mapLen / m_dScaleRatio;
        }

        void CDisplayTransformation2D::SetMapVisibleRect( const CommonLib::bbox& bound )
        {
            if (bound.type != CommonLib::bbox_type_normal || !(bound.xMin < bound.xMax) || !(bound.yMin < bound.yMax))
                return;

            if (m_ClientRect.IsEmpty())
                return;

            // the box corners relative to the box center, rotated and flipped as on the screen (without the scale)
            matrix4 mat;
            mat.setRotationDegrees(vector3df(0, 0, m_dAngle));

            matrix4 smat;
            smat.setScale(vector3df(GetHorizontalFlip() ? -1 : 1, GetVerticalFlip() ? 1 : -1, 1));

            mat *= smat;

            double centerx = (bound.xMin + bound.xMax) / 2;
            double centery = (bound.yMin + bound.yMax) / 2;

            const double corners[4][2] = { {bound.xMin, bound.yMin}, {bound.xMin, bound.yMax}, {bound.xMax, bound.yMax}, {bound.xMax, bound.yMin} };
            CommonLib::bbox box;
            for (int i = 0; i < 4; ++i)
            {
                double xm = corners[i][0] - centerx;
                double ym = corners[i][1] - centery;
                double xn = xm * mat(0, 0) + ym * mat(1, 0);
                double yn = xm * mat(0, 1) + ym * mat(1, 1);
                AddToBox(box, xn, yn, i == 0);
            }

            // map units per pixel so that every side of the box fits between the window center and the window edge
            const double edges[4][2] = {
                    {box.xMin, double(m_ClientRect.xMin) - m_AnchorDev[0]},
                    {box.xMax, double(m_ClientRect.xMax) - m_AnchorDev[0]},
                    {box.yMin, double(m_ClientRect.yMin) - m_AnchorDev[1]},
                    {box.yMax, double(m_ClientRect.yMax) - m_AnchorDev[1]}
            };

            double sr = 0.;
            for (int i = 0; i < 4; ++i)
            {
                double pixels = std::fabs(edges[i][1]);
                sr = (std::max)(sr, std::fabs(edges[i][0]) / (pixels > 0. ? pixels : 1.));
            }

            if (!(sr > 0.))
                return;

            m_dScaleRatio = sr;
            m_dCurScale = sr * m_dResolution / CalcMapUnitPerInch();
            m_AnchorMap[0] = centerx;
            m_AnchorMap[1] = centery;
            SetMatrix();

            OnVisibleBoundsChangedEvent.fire((IDisplayTransformation*)this);
        }

        void CDisplayTransformation2D::SetMapPos(const CommonLib::GisXYPoint &map_pos, double new_scale)
        {
            m_AnchorMap[0] = map_pos.x;
            m_AnchorMap[1] = map_pos.y;
            if ((new_scale > 0) && (new_scale != m_dCurScale))
            {
                m_dCurScale = new_scale;
                UpdateScaleRatio();
            }
            else
            {
                UpdateFittedBounds();
            }
            OnVisibleBoundsChangedEvent.fire((IDisplayTransformation*)this);
        }

        CommonLib::GisXYPoint CDisplayTransformation2D::GetMapPos() const
        {
            CommonLib::GisXYPoint mp = {m_AnchorMap[0], m_AnchorMap[1]};
            return mp;
        }

        const CommonLib::bbox& CDisplayTransformation2D::GetFittedBounds() const
        {
            return m_mapCurFittedExtent;
        }

        void CDisplayTransformation2D::SetDeviceClipRect(const GRect& devRect)
        {
            m_devClipRect = devRect;
            UpdateFlatClip();
        }

        const GRect& CDisplayTransformation2D::GetDeviceClipRect() const
        {
            return m_devClipRect;
        }

        bool CDisplayTransformation2D::UseReferenceScale() const
        {
            if(GetUnits() == CommonLib::UnitsUnknown || GetReferenceScale() == 0.0)
                return false;

            return true;
        }

        double CDisplayTransformation2D::GetScale() const
        {
            return m_dCurScale;
        }

        void CDisplayTransformation2D::SetDeviceRect( const GRect& bound, eDisplayTransformationPreserve preserve_type )
        {
            switch (preserve_type)
            {
                case DisplayTransformationPreserveScale:
                    SetClientRect(bound);
                    UpdateFittedBounds();
                    break;

                case DisplayTransformationPreserveCenterExtent:
                {
                    GUnits bound_size_min = (std::min)(bound.Width(), bound.Height());
                    GUnits client_size_min = (std::min)(m_ClientRect.Width(), m_ClientRect.Height());
                    if(bound_size_min <= 0)
                        throw CommonLib::CExcBase("DisplayTransformation2D: Wrong bound size");

                    if (client_size_min <= 0 || bound_size_min == client_size_min)
                    {
                        // the first real window size: show the extent that was set for the empty window
                        if (client_size_min <= 0 && m_mapCurFittedExtent.type == CommonLib::bbox_type_normal)
                        {
                            CommonLib::bbox extent = m_mapCurFittedExtent;
                            SetClientRect(bound);
                            SetMapVisibleRect(extent);
                            break;
                        }
                        return SetDeviceRect(bound, DisplayTransformationPreserveScale);
                    }
                    SetClientRect(bound);
                    m_dCurScale *= double(client_size_min) / double(bound_size_min);
                    UpdateScaleRatio();
                }
                    break;
                default:
                    throw CommonLib::CExcBase("DisplayTransformation2D: Wrong TransformationPreserveType");
                    break;
            }

            OnDeviceFrameChangedEvent.fire((IDisplayTransformation*)this);
        }

        const GRect& CDisplayTransformation2D::GetDeviceRect() const
        {
            return m_ClientRect;
        }

        void CDisplayTransformation2D::SetReferenceScale( double lScale )
        {
            m_dRefScale = lScale;
        }

        double CDisplayTransformation2D::GetReferenceScale() const
        {
            return m_dRefScale;
        }

        void CDisplayTransformation2D::SetRotation( double degrees )
        {
            if ( m_dAngle != degrees )
            {
                m_dAngle = degrees;
                SetMatrix();
                OnRotationChangedEvent.fire((IDisplayTransformation*)this);
            }
        }

        double CDisplayTransformation2D::GetRotation()
        {
            return m_dAngle;
        }

        void CDisplayTransformation2D::SetResolution( double pDpi )
        {
            if(pDpi <= 0)
                throw CommonLib::CExcBase("DisplayTransformation2D: Wrong dpi: {0}", pDpi);

            // number of pixel in device inch width
            m_dResolution = pDpi;
            UpdateScaleRatio();
            OnResolutionChangedEvent.fire((IDisplayTransformation*)this);
        }

        double CDisplayTransformation2D::GetResolution()
        {
            return m_dResolution;
        }

        void CDisplayTransformation2D::SetUnits( CommonLib::Units units)
        {
            if ( m_mapUnits != units )
            {
                m_mapUnits = units;
                UpdateScaleRatio(); // the same scale is another number of map units per pixel
                OnUnitsChangedEvent.fire((IDisplayTransformation*)this);
            }
        }

        CommonLib::Units CDisplayTransformation2D::GetUnits()const
        {
            return m_mapUnits ;
        }

        void CDisplayTransformation2D::SetSpatialReference( Geometry::ISpatialReferencePtr ptrSp )
        {
            m_pSpatialRef = ptrSp;
        }

        Geometry::ISpatialReferencePtr CDisplayTransformation2D::GetSpatialReference() const
        {
            return m_pSpatialRef;
        }

        // ---------------------------------------------------------------------------------------------
        // flat coordinates and the projection

        void CDisplayTransformation2D::MapToFlat(double mapX, double mapY, double& flatX, double& flatY) const
        {
            double xm = mapX - m_AnchorMap[0];
            double ym = mapY - m_AnchorMap[1];
            flatX = xm * m_MatrixMap2Dev[0][0] + ym * m_MatrixMap2Dev[0][1];
            flatY = xm * m_MatrixMap2Dev[1][0] + ym * m_MatrixMap2Dev[1][1];
        }

        void CDisplayTransformation2D::FlatToMap(double flatX, double flatY, double& mapX, double& mapY) const
        {
            mapX = flatX * m_MatrixDev2Map[0][0] + flatY * m_MatrixDev2Map[0][1] + m_AnchorMap[0];
            mapY = flatX * m_MatrixDev2Map[1][0] + flatY * m_MatrixDev2Map[1][1] + m_AnchorMap[1];
        }

        void CDisplayTransformation2D::FlatToDevice(double flatX, double flatY, double& devX, double& devY) const
        {
            devX = flatX + m_AnchorDev[0];
            devY = flatY + m_AnchorDev[1];
        }

        void CDisplayTransformation2D::DeviceToFlat(double devX, double devY, double& flatX, double& flatY) const
        {
            flatX = devX - m_AnchorDev[0];
            flatY = devY - m_AnchorDev[1];
        }

        void CDisplayTransformation2D::UpdateFlatClip()
        {
            if (m_devClipRect.IsEmpty())
            {
                m_flatClip.Clear();
                return;
            }

            m_flatClip.SetClipRect(double(m_devClipRect.xMin) - m_AnchorDev[0], double(m_devClipRect.yMin) - m_AnchorDev[1],
                                   double(m_devClipRect.xMax) - m_AnchorDev[0], double(m_devClipRect.yMax) - m_AnchorDev[1]);
        }

        GUnits CDisplayTransformation2D::ToDevice(double v)
        {
            if (!(v > MINCLIENT)) // NaN too
                v = MINCLIENT;
            else if (v > MAXCLIENT)
                v = MAXCLIENT;
#ifdef _FLOAT_GUNITS_
            return static_cast<GUnits>(v);
#else
            return static_cast<GUnits>(std::floor(v + 0.5));
#endif
        }

        // ---------------------------------------------------------------------------------------------
        // conversions

        void CDisplayTransformation2D::MapToDevicePoint(const CommonLib::GisXYPoint& ptIn, GPoint& ptOut)
        {
            double fx, fy, dx, dy;
            MapToFlat(ptIn.x, ptIn.y, fx, fy);
            FlatToDevice(fx, fy, dx, dy);
            ptOut.x = ToDevice(dx);
            ptOut.y = ToDevice(dy);
        }

        void CDisplayTransformation2D::MapToDevice(const CommonLib::GisXYPoint *pIn, GPoint *pOut, int nPts )
        {
            for (; nPts > 0; ++pIn, ++pOut, --nPts)
                MapToDevicePoint(*pIn, *pOut);
        }

        int CDisplayTransformation2D::MapToDeviceOpt(const CommonLib::GisXYPoint *pIn, GPoint *pOutOrig, int nPts, CommonLib::eShapeType type)
        {
            // converts and removes the repeated device points, keeps the minimum number of points for the shape type
            GPoint *pOut = pOutOrig;
            int lag;
            if(type == CommonLib::shape_type_general_point || type == CommonLib::shape_type_general_multipoint)
                lag = 0;
            else if(type == CommonLib::shape_type_general_polyline)
                lag = (std::min)(nPts, 2);
            else
                lag = (std::min)(nPts, 4);

            bool first = true;
            GPoint prev;
            for (; nPts > 0; ++pIn, --nPts)
            {
                GPoint pt;
                MapToDevicePoint(*pIn, pt);
                if (first || pt != prev)
                {
                    *pOut++ = pt;
                    --lag;
                    first = false;
                    prev = pt;
                }
            }
            for (;lag > 0; --lag, ++pOut)
                *pOut = prev;

            return static_cast<int>(pOut - pOutOrig);
        }

        void CDisplayTransformation2D::AddDevicePart(const DPoint* pFlat, size_t nCount, size_t nMinPoints)
        {
            size_t nBegin = m_vecPoints.size();
            for (size_t i = 0; i < nCount; ++i)
            {
                double dx, dy;
                FlatToDevice(pFlat[i].x, pFlat[i].y, dx, dy);
                GPoint pt(ToDevice(dx), ToDevice(dy));
                if (m_vecPoints.size() > nBegin)
                {
                    // skip the points that are too close to the previous one, they do not change the picture
                    const GPoint& prev = m_vecPoints.back();
                    if (std::fabs(double(pt.x - prev.x)) < MinPointDistance && std::fabs(double(pt.y - prev.y)) < MinPointDistance)
                        continue;
                }

                m_vecPoints.push_back(pt);
            }

            size_t nNew = m_vecPoints.size() - nBegin;
            if (nNew == 0)
                return;

            // a tiny shape is still drawn (as a dot)
            GPoint last = m_vecPoints.back();
            for (; nNew < nMinPoints; ++nNew)
                m_vecPoints.push_back(last);

            m_vecParts.push_back((int)nNew);
        }

        void CDisplayTransformation2D::MapToDevice(const CommonLib::IGeoShapePtr geom, GPoint **pOut, int** partCounts, int* count)
        {
            *pOut = nullptr;
            *partCounts = nullptr;
            *count = 0;

            m_vecPoints.clear();
            m_vecParts.clear();

            if (!geom.get() || m_flatClip.IsEmpty())
                return;

            // quick test of the shape bounding box against the visible area
            CommonLib::bbox bb = geom->GetBB();
            DPoint corners[4];
            const double cornersMap[4][2] = { {bb.xMin, bb.yMin}, {bb.xMax, bb.yMin}, {bb.xMax, bb.yMax}, {bb.xMin, bb.yMax} };
            for (int i = 0; i < 4; ++i)
                MapToFlat(cornersMap[i][0], cornersMap[i][1], corners[i].x, corners[i].y);

            CConvexClipper::eRelation relation = m_flatClip.Relation(corners, 4);
            if (relation == CConvexClipper::RelationOutside)
                return;

            bool bAllInside = relation == CConvexClipper::RelationInside;

            CommonLib::eShapeType generalType = geom->GeneralType();
            uint32_t nPointCnt = geom->GetPointCnt();
            uint32_t nPartCnt = geom->GetPartCount();
            CommonLib::GisXYPoint pt;

            if (generalType == CommonLib::shape_type_general_point || generalType == CommonLib::shape_type_general_multipoint)
            {
                for (uint32_t i = 0; i < nPointCnt; ++i)
                {
                    geom->NextPoint(i, pt);
                    DPoint fp;
                    MapToFlat(pt.x, pt.y, fp.x, fp.y);
                    if (bAllInside || m_flatClip.IsInside(fp.x, fp.y))
                    {
                        double dx, dy;
                        FlatToDevice(fp.x, fp.y, dx, dy);
                        m_vecPoints.push_back(GPoint(ToDevice(dx), ToDevice(dy)));
                    }
                }

                if (!m_vecPoints.empty())
                    m_vecParts.push_back((int)m_vecPoints.size());
            }
            else if (generalType == CommonLib::shape_type_general_polygon || generalType == CommonLib::shape_type_general_polyline)
            {
                bool bPolygon = generalType == CommonLib::shape_type_general_polygon;
                uint32_t nParts = nPartCnt > 0 ? nPartCnt : 1;
                for (uint32_t part = 0, offset = 0; part < nParts && offset < nPointCnt; ++part)
                {
                    uint32_t nPartPoints = nPartCnt > 0 ? geom->NextPart(part) : nPointCnt;
                    if (offset + nPartPoints > nPointCnt)
                        nPartPoints = nPointCnt - offset;

                    m_vecFlat.resize(nPartPoints);
                    for (uint32_t i = 0; i < nPartPoints; ++i)
                    {
                        geom->NextPoint(offset + i, pt);
                        MapToFlat(pt.x, pt.y, m_vecFlat[i].x, m_vecFlat[i].y);
                    }
                    offset += nPartPoints;

                    if (m_vecFlat.empty())
                        continue;

                    if (bAllInside)
                    {
                        AddDevicePart(m_vecFlat.data(), m_vecFlat.size(), bPolygon ? 4 : 2);
                    }
                    else if (bPolygon)
                    {
                        m_flatClip.ClipPolygon(m_vecFlat.data(), (int)m_vecFlat.size(), m_vecClipped);
                        if (!m_vecClipped.empty())
                            AddDevicePart(m_vecClipped.data(), m_vecClipped.size(), 4);
                    }
                    else
                    {
                        m_vecClipped.clear();
                        m_vecClippedParts.clear();
                        m_flatClip.ClipPolyline(m_vecFlat.data(), (int)m_vecFlat.size(), m_vecClipped, m_vecClippedParts);
                        for (size_t i = 0, pos = 0; i < m_vecClippedParts.size(); ++i)
                        {
                            AddDevicePart(m_vecClipped.data() + pos, (size_t)m_vecClippedParts[i], 2);
                            pos += (size_t)m_vecClippedParts[i];
                        }
                    }
                }
            }

            if (m_vecParts.empty())
                return;

            *pOut = m_vecPoints.data();
            *partCounts = m_vecParts.data();
            *count = (int)m_vecParts.size();
        }

        void CDisplayTransformation2D::DeviceToMap(const GPoint *pIn, CommonLib::GisXYPoint *pOut, int nPoints )
        {
            for (; nPoints > 0; --nPoints, ++pIn, ++pOut)
            {
                double fx, fy;
                DeviceToFlat(double(pIn->x), double(pIn->y), fx, fy);
                FlatToMap(fx, fy, pOut->x, pOut->y);
            }
        }

        void CDisplayTransformation2D::MapToDevice(const CommonLib::bbox &mapBox, GRect &rect )
        {
            CommonLib::GisXYPoint map_xy[4] =
                    {
                            {mapBox.xMin, mapBox.yMin},
                            {mapBox.xMax, mapBox.yMin},
                            {mapBox.xMax, mapBox.yMax},
                            {mapBox.xMin, mapBox.yMax}
                    };
            GPoint pts[4];
            MapToDevice(map_xy, pts, 4);
            rect.Set(pts[0].x, pts[0].y, pts[0].x, pts[0].y);
            for (int i = 1; i < 4; ++i)
            {
                rect.xMin = (std::min)( rect.xMin, pts[i].x );
                rect.xMax = (std::max)( rect.xMax, pts[i].x );
                rect.yMin = (std::min)( rect.yMin, pts[i].y );
                rect.yMax = (std::max)( rect.yMax, pts[i].y );
            }
        }

        void CDisplayTransformation2D::DeviceToMap(const GRect &rect, CommonLib::bbox &mapBox )
        {
            // the visible area of a rect is convex (also in 3D), its corners give the bounding box
            GPoint pts[4] =
                    {
                            GPoint(rect.xMin, rect.yMin),
                            GPoint(rect.xMax, rect.yMin),
                            GPoint(rect.xMax, rect.yMax),
                            GPoint(rect.xMin, rect.yMax)
                    };
            CommonLib::GisXYPoint mapXY[4];
            DeviceToMap(pts, mapXY, 4);
            for (int i = 0; i < 4; ++i)
                AddToBox(mapBox, mapXY[i].x, mapXY[i].y, i == 0);

            mapBox.type = CommonLib::bbox_type_normal;
        }

        void CDisplayTransformation2D::SetVerticalFlip( bool  flag )
        {
            m_bVerticalFlip = flag;
            SetMatrix();
        }

        bool CDisplayTransformation2D::GetVerticalFlip() const
        {
            return m_bVerticalFlip;
        }

        void CDisplayTransformation2D::SetHorizontalFlip( bool  flag )
        {
            m_bHorizontalFlip = flag;
            SetMatrix();
        }

        bool CDisplayTransformation2D::GetHorizontalFlip() const
        {
            return m_bHorizontalFlip;
        }

        void CDisplayTransformation2D::SetClientRect(const GRect &arg)
        {
            m_ClientRect = arg;
            m_AnchorDev[0] = m_ClientRect.CenterPoint().x;
            m_AnchorDev[1] = m_ClientRect.CenterPoint().y;
            UpdateFlatClip(); // the clip rect is relative to the anchor
        }

        void CDisplayTransformation2D::UpdateScaleRatio()
        {
            double dMapUnitPerInch = CalcMapUnitPerInch();
            m_dScaleRatio = m_dCurScale * dMapUnitPerInch / m_dResolution;
            SetMatrix();
        }

        double CDisplayTransformation2D::CalcMapUnitPerInch()
        {
            return CommonLib::ConvertUnits(1., CommonLib::UnitsInches, 	(CommonLib::UnitsUnknown == m_mapUnits) ? CommonLib::UnitsCentimeters : m_mapUnits);
        }

        void CDisplayTransformation2D::SetMatrix()
        {
            memset( m_MatrixMap2Dev, 0, sizeof(m_MatrixMap2Dev));
            memset(m_MatrixDev2Map, 0, sizeof(m_MatrixDev2Map));

            double S = (m_dScaleRatio != 0)? (1.0 / m_dScaleRatio) : (1.0);

            // device y goes down: without the vertical flip the map y axis is inverted (north is up)
            matrix4 mat;
            mat.setRotationDegrees(vector3df(0, 0, m_dAngle));

            matrix4 smat;
            smat.setScale(vector3df(GetHorizontalFlip() ? -S : S, GetVerticalFlip() ? S : -S, 1));

            mat *= smat;

            double d00 = mat(0,0);
            double d11 = mat(1,1);
            double d10 = mat(1,0);
            double d01 = mat(0,1);
            double grDet = d00 * d11 - d10 * d01;

            m_MatrixMap2Dev[0][0] = d00;
            m_MatrixMap2Dev[1][1] = d11;
            m_MatrixMap2Dev[0][1] = d10;
            m_MatrixMap2Dev[1][0] = d01;

            m_MatrixDev2Map[1][1] = d00 / grDet;
            m_MatrixDev2Map[0][0] = d11 / grDet;
            m_MatrixDev2Map[1][0] = -d01 / grDet;
            m_MatrixDev2Map[0][1] = -d10 / grDet;

            UpdateFittedBounds();
        }

        void CDisplayTransformation2D::UpdateFittedBounds()
        {
            if (m_ClientRect.IsEmpty())
                return;

            DeviceToMap(m_ClientRect, m_mapCurFittedExtent);
        }

        const GRect& CDisplayTransformation2D::GetClipRect() const
        {
            return m_clipRect;
        }

        void CDisplayTransformation2D::SetClipRect(const GRect& rect)
        {
            m_clipRect = rect;
            m_bClipExists = true;
        }

        bool CDisplayTransformation2D::ClipExists()
        {
            return m_bClipExists;
        }

        void CDisplayTransformation2D::RemoveClip()
        {
            m_bClipExists = false;
        }

        void CDisplayTransformation2D::SetOnDeviceFrameChanged(OnDeviceFrameChanged* pFunck, bool bAdd)
        {
            if(bAdd)
                OnDeviceFrameChangedEvent += pFunck;
            else
                OnDeviceFrameChangedEvent -= pFunck;
        }
        void CDisplayTransformation2D::SetOnResolutionChanged(OnResolutionChanged* pFunck, bool bAdd)
        {
            if(bAdd)
                OnResolutionChangedEvent += pFunck;
            else
                OnResolutionChangedEvent -= pFunck;
        }
        void CDisplayTransformation2D::SetOnRotationChanged(OnRotationChanged* pFunck, bool bAdd)
        {
            if(bAdd)
                OnRotationChangedEvent += pFunck;
            else
                OnRotationChangedEvent -= pFunck;
        }
        void CDisplayTransformation2D::SetOnUnitsChanged(OnUnitsChanged* pFunck, bool bAdd)
        {
            if(bAdd)
                OnUnitsChangedEvent += pFunck;
            else
                OnUnitsChangedEvent -= pFunck;
        }
        void CDisplayTransformation2D::SetOnVisibleBoundsChanged(OnVisibleBoundsChanged* pFunck, bool bAdd)
        {
            if(bAdd)
                OnVisibleBoundsChangedEvent += pFunck;
            else
                OnVisibleBoundsChangedEvent -= pFunck;
        }
    }
}
