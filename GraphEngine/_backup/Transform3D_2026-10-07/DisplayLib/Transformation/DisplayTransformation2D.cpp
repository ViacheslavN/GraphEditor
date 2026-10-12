#include "DisplayTransformation2D.h"
#include "Matrix4.h"
#include "../GraphTypes/Rect.h"

namespace GraphEngine
{
    namespace Display
    {
        const int MAXCLIENT = 500000;
        const int MINCLIENT = -MAXCLIENT;

        CDisplayTransformation2D::CDisplayTransformation2D(double resolution, CommonLib::Units map_units, const GRect &dev_rect , double scale) :
                m_dRefScale(0.0),
                m_dScaleRatio(1.0),
                m_dCurScale(scale),
                m_dResolution(resolution),
                m_mapUnits(map_units),
                m_bVerticalFlip(false),
                m_bHorizontalFlip(false),
                m_dAngle(0),
                m_bPseudo3D(false),
                m_bClipExists(false)
        {
            memset(&m_AnchorDev, 0, sizeof(m_AnchorDev));
            memset(&m_AnchorMap, 0, sizeof(m_AnchorMap));
            memset(&m_MatrixDev2Map, 0, sizeof(m_MatrixDev2Map));
            memset(&m_MatrixMap2Dev, 0, sizeof(m_MatrixMap2Dev));

            UpdateScaleRatio();
            SetClientRect(dev_rect);

            m_clipPolygon.SetPointDst(&m_vecPoints);
            m_ClipLine.SetPointDst(&m_vecPoints, &m_vecParts);

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
            if (!(bound.type & CommonLib::bbox_type_normal) || (bound.xMin == bound.xMax) || (bound.yMin == bound.yMax))
            {
                return;
            }

            if (m_ClientRect.IsEmpty())
            {
                return;
            }

            class local
            {
            public:
                local(const matrix4 &mat, double centerx, double centery) :
                        m_mat(mat),  m_dCenterX(centerx), m_dCenterY(centery)
                {
                    m_box.type = CommonLib::bbox_type_null;
                }
                void operator()(double x, double y)
                {
                    double xm = x - m_dCenterX;
                    double ym = y - m_dCenterY;
                    double xn = xm * m_mat(0, 0) + ym * m_mat(1, 0);
                    double yn = xm * m_mat(0, 1) + ym * m_mat(1, 1);
                    if (!(m_box.type & CommonLib::bbox_type_normal))
                    {
                        m_box.xMin = m_box.xMax = xn;
                        m_box.yMin = m_box.yMax = yn;
                        m_box.type = CommonLib::bbox_type_normal;
                    }
                    else
                    {
                        if (m_box.xMin > xn)
                            m_box.xMin = xn;
                        if (m_box.xMax < xn)
                            m_box.xMax = xn;
                        if (m_box.yMin > yn)
                            m_box.yMin = yn;
                        if (m_box.yMax < yn)
                            m_box.yMax = yn;
                    }
                }
                const CommonLib::bbox &result() const {return m_box;}
            private:
                matrix4 m_mat;
                double m_dCenterX, m_dCenterY;
                CommonLib::bbox m_box;
            };
            matrix4 mat;
            mat.setRotationDegrees(vector3df(0, 0, m_dAngle));

            matrix4 smat;
            smat.setScale(vector3df(GetHorizontalFlip() ? -1 : 1, GetVerticalFlip() ? 1 : -1, 1));

            mat *= smat;

            double centerx = (bound.xMin + bound.xMax) / 2;
            double centery = (bound.yMin + bound.yMax) / 2;

            local wrk(mat, centerx, centery);

            wrk(bound.xMin, bound.yMin);
            wrk(bound.xMin, bound.yMax);
            wrk(bound.xMax, bound.yMax);
            wrk(bound.xMax, bound.yMin);

            if(!((wrk.result().xMin < 0) && (wrk.result().xMax > 0) && (wrk.result().yMin < 0) && (wrk.result().yMax > 0)))
                throw CommonLib::CExcBase("DisplayTransformation2D: Wrong bound box size");

            int w = int(m_ClientRect.xMin - m_AnchorDev[0]);
            double sr = fabs(wrk.result().xMin / (w ? w : 1));
            w = int(m_ClientRect.xMax - m_AnchorDev[0]);

            sr = std::max<double>(sr, fabs(wrk.result().xMax / (w ? w : 1)));
            w = int(m_ClientRect.yMin - m_AnchorDev[1]);

            sr = std::max<double>(sr, fabs(wrk.result().yMin / (w ? w : 1)));
            w = int(m_ClientRect.yMax - m_AnchorDev[1]);

            sr = std::max<double>(sr, fabs(wrk.result().yMax / (w ? w : 1)));

            m_dCurScale = sr * m_dResolution / CalcMapUnitPerInch();
            m_dScaleRatio = sr;
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
            m_clipPolygon.SetClipBox(m_devClipRect);
            m_ClipLine.SetClipBox(m_devClipRect);
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
                    GUnits bound_size_min = min(bound.Width(), bound.Height());
                    GUnits client_size_min = min(m_ClientRect.Width(), m_ClientRect.Height());
                    if(bound_size_min <= 0)
                        throw CommonLib::CExcBase("DisplayTransformation2D: Wrong bound size");


                    if (!bound_size_min || !client_size_min || (bound_size_min == client_size_min))
                    {
                        if (!client_size_min && bound_size_min && (m_mapCurFittedExtent.type & CommonLib::bbox_type_normal))
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
            OnResolutionChangedEvent((IDisplayTransformation*)this);
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
                OnUnitsChangedEvent((IDisplayTransformation*)this);
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

        int CDisplayTransformation2D::MapToDeviceOpt(const CommonLib::GisXYPoint *pIn, GPoint *pOutOrig, int nPts, CommonLib::eShapeType type)
        {
            GPoint *pOut = pOutOrig;
            int lag;
            if(type == CommonLib::shape_type_general_point || type == CommonLib::shape_type_general_multipoint)
                lag = std::min<int>(nPts, 0);
            else if(type == CommonLib::shape_type_general_polyline)
                lag = std::min<int>(nPts, 2);
            else
                lag = std::min<int>(nPts, 4);

            bool first = true;
            GUnits xd_prev = 0, yd_prev = 0;
            for (; nPts > 0; ++pIn, --nPts)
            {

                double xm = pIn->x - m_AnchorMap[0];
                double ym = pIn->y - m_AnchorMap[1];
#ifdef _FLOAT_GUNITS_
                GUnits xd = static_cast<GUnits>((xm * m_MatrixMap2Dev[0][0] + ym * m_MatrixMap2Dev[0][1]));
                GUnits yd = static_cast<GUnits>((xm * m_MatrixMap2Dev[1][0] + ym * m_MatrixMap2Dev[1][1]));
#else
                GUnits xd = static_cast<GUnits>(floor(xm * m_MatrixMap2Dev[0][0] + ym * m_MatrixMap2Dev[0][1] + 0.5));
				GUnits yd = static_cast<GUnits>(floor(xm * m_MatrixMap2Dev[1][0] + ym * m_MatrixMap2Dev[1][1] + 0.5));
#endif


                xd += m_AnchorDev[0];
                yd += m_AnchorDev[1];

                if (first || (xd != xd_prev) || (yd != yd_prev))
                {
                    pOut->x = xd;
                    pOut->y = yd;
                    ++pOut;
                    --lag;
                    first = false;
                    xd_prev = xd;
                    yd_prev = yd;
                }
            }
            for (;lag > 0; --lag, ++pOut)
            {
                pOut->x = xd_prev;
                pOut->y = yd_prev;
            }

            return static_cast<int>(pOut - pOutOrig);

        }
        void CDisplayTransformation2D::MapToDevicePoint(const CommonLib::GisXYPoint& ptIn, GPoint& ptOut)
        {

            double xm = ptIn.x - m_AnchorMap[0];
            double ym = ptIn.y - m_AnchorMap[1];
#ifdef _FLOAT_GUNITS_
            GUnits xd = static_cast<GUnits>((xm * m_MatrixMap2Dev[0][0] + ym * m_MatrixMap2Dev[0][1]));
            GUnits yd = static_cast<GUnits>((xm * m_MatrixMap2Dev[1][0] + ym * m_MatrixMap2Dev[1][1]));
#else
            GUnits xd = static_cast<GUnits>(floor(xm * m_MatrixMap2Dev[0][0] + ym * m_MatrixMap2Dev[0][1] + 0.5));
			GUnits yd = static_cast<GUnits>(floor(xm * m_MatrixMap2Dev[1][0] + ym * m_MatrixMap2Dev[1][1] + 0.5));
#endif

            xd += m_AnchorDev[0];
            yd += m_AnchorDev[1];

            ptOut.x = xd;
            ptOut.y = yd;

        }


        void CDisplayTransformation2D::MapToDevice(const CommonLib::IGeoShapePtr geom, GPoint **pOut, int** partCounts, int* count)
        {

            CommonLib::bbox bb = geom->GetBB();
            GRect gBB;
            MapToDevice(bb, gBB);
            bool bAllPointInBox = m_devClipRect.IsInRect(gBB);
            if (!bAllPointInBox)
            {
                if (!m_devClipRect.IsIntersect(gBB))
                {
                    *partCounts = nullptr;
                    *pOut = nullptr;
                    *count = 0;

                    return;
                }
            }

            uint32_t nInPartSize = (int)geom->GetPartCount();
            uint32_t nInPointCnt = (int)geom->GetPartCount();

            nInPartSize = (nInPartSize > 0 ? nInPartSize : 1);

            //TO DO alloc
            m_vecPoints.clear();
            m_vecParts.clear();

            CommonLib::GisXYPoint pt;
            GPoint ptPoint;
            GPoint ptPrev;

            if (geom->GeneralType() == CommonLib::shape_type_general_polygon)
            {

                for (size_t part = 0, offset = 0; part < nInPartSize; part++)
                {
                    uint32_t nPartPoints = geom->NextPart((uint32_t)part);
                    uint32_t nNewCount = (uint32_t)m_vecPoints.size();
                    int nClipPoint = 0;
                    //	bool bPointInRect = false;

                    m_clipPolygon.BeginPolygon();
                    for (uint32_t i = 0; i < nPartPoints; ++i)
                    {
                        geom->NextPoint(i + (uint32_t)offset, pt);
                        MapToDevicePoint(pt, ptPoint);

                        if (i != 0 && ptPrev == ptPoint)
                            continue;


                        nClipPoint += 1;
                        m_clipPolygon.AddVertex(ptPoint, bAllPointInBox);
                        ptPrev = ptPoint;
                    }

                    if (nClipPoint < 4)
                    {
                        for (int i = 0; i < nClipPoint; ++i)
                        {
                            m_clipPolygon.AddVertex(ptPrev, bAllPointInBox);
                        }

                    }
                    m_clipPolygon.EndPolygon(bAllPointInBox);
                    nNewCount = (uint32_t)m_vecPoints.size() - nNewCount;
                    offset += nPartPoints;
                    if (nNewCount != 0)
                    {
                        m_vecParts.push_back((int)nNewCount);
                    }
                }

                if (!m_vecPoints.empty())
                    *pOut = &m_vecPoints[0];
                else
                    *pOut = nullptr;

                if (!m_vecPoints.empty())
                    *partCounts = &m_vecParts[0];
                else
                    *partCounts = nullptr;

                *count = (int)m_vecParts.size();
                return;
            }

            if (geom->GeneralType() == CommonLib::shape_type_general_polyline)
            {

                for (size_t part = 0, offset = 0; part < nInPartSize; part++)
                {
                    int nPartPoints = (int)geom->NextPart((uint32_t)part);
                    int nClipPoint = 0;
                    m_ClipLine.BeginLine(bAllPointInBox);
                    for (uint32_t i = 0; i < (uint32_t)nPartPoints; ++i)
                    {
                        geom->NextPoint(i + (uint32_t)offset, pt);
                        MapToDevicePoint(pt, ptPoint);

                        if (i != 0 && ptPrev == ptPoint)
                            continue;

                        nClipPoint += 1;
                        m_ClipLine.AddVertex(ptPoint, bAllPointInBox);
                        ptPrev = ptPoint;
                    }

                    if (nClipPoint < 2)
                    {
                        for (int i = 0; i < nClipPoint; ++i)
                        {
                            m_ClipLine.AddVertex(ptPrev, bAllPointInBox);
                        }

                    }
                    m_ClipLine.EndLine(bAllPointInBox);

                }

                if (!m_vecPoints.empty())
                    *pOut = &m_vecPoints[0];
                else
                    *pOut = nullptr;

                if (!m_vecPoints.empty())
                    *partCounts = &m_vecParts[0];
                else
                    *partCounts = nullptr;

                *count = (int)m_vecParts.size();
                return;
            }

            if (geom->GeneralType() == CommonLib::shape_type_general_point || geom->GeneralType() == CommonLib::shape_type_general_multipoint)
            {
                for (uint32_t i = 0; i < nInPointCnt; ++i)
                {
                    for (uint32_t i = 0; i < nInPointCnt; ++i)
                    {
                        geom->NextPoint(i, pt);
                        MapToDevicePoint(pt, ptPoint);
                        if (m_devClipRect.PointInRect(ptPoint))
                        {
                            m_vecPoints.push_back(ptPoint);
                        }
                    }

                }
                m_vecParts[0] = (int)m_vecPoints.size();

                *pOut = &m_vecPoints[0];
                *partCounts = &m_vecParts[0];
                *count = 1;

            }
        }

        /*	void CDisplayTransformation2D::MapToDevice(const CommonLib::IGeoShape *geom, GPoint **pOut, int** partCounts, int* count)
            {
                uint32_t nInPartSize = (int)geom->GetPartCount();
                uint32_t nInPointCnt = (int)geom->GetPointCnt();

                nInPartSize = (nInPartSize > 0 ? nInPartSize : 1);

                //TO DO alloc
                m_vecPoints.clear();
                m_vecParts.clear();
                int nOutPartCount = 0;

                auto pPoints =geom->GetPoints();
                auto pParts = geom->GetParts();


                if (m_vecPoints.size() < nInPointCnt)
                    m_vecPoints.resize(nInPointCnt);

                if (m_vecParts.size() < nInPartSize)
                    m_vecParts.resize(nInPointCnt);

                int *parts = &m_vecParts[0];
                GPoint *buffer = &m_vecPoints[0];

                    const CommonLib::GisXYPoint* points = geom->GetPoints();
                    if (geom->GeneralType() == CommonLib::shape_type_general_point || geom->GeneralType() == CommonLib::shape_type_general_multipoint)
                    {
                        int newCount = MapToDeviceOpt(geom->GetPoints(), buffer, (int)geom->GetPointCnt(), geom->GeneralType());
                        *pOut = buffer;
                        parts[0] = newCount;
                        *count = 1;
                        *partCounts = parts;
                        return;
                    }

                    int nCount = 0;
                    for (size_t part = 0, offset = 0, buf_offset = 0, partCount = geom->GetPartCount(); part < partCount; part++)
                    {
                        int newCount = MapToDeviceOpt(points + offset, buffer + buf_offset, (int)geom->GetPart(part), geom->GeneralType());
                        nCount += newCount;
                        offset += geom->GetPart(part);
                        parts[part] = newCount;
                        buf_offset += newCount;
                    }

                    int partCount = (int)geom->GetPartCount();


                    if (m_pClipper.get())
                    {
                        GRect rect = m_devClipRect;
    #ifdef _FLOAT_GUNITS_
                        rect.inflate(ceill(GUnits(rect.width() * 0.1)), ceill(GUnits(rect.height() * 0.1)));
    #else
                        rect.Inflate(GUnits(rect.width() * 0.1), GUnits(rect.height() * 0.1));
    #endif
                        if (geom.generalType() == CommonLib::shape_type_general_polyline)
                            m_pClipper->clipLine(rect, &buffer, &parts, &partCount);
                        else if (geom.generalType() == CommonLib::shape_type_general_polygon)
                            m_pClipper->clipPolygon(rect, &buffer, &parts, &partCount);
                    }

                    *pOut = buffer;
                    *partCounts = parts;
                    *count = partCount;
        }*/

        void CDisplayTransformation2D::DeviceToMap(const GPoint *pIn, CommonLib::GisXYPoint *pOut, int nPoints )
        {

            double shift_map_x = m_AnchorMap[0];
            double shift_map_y = m_AnchorMap[1];
            for (; nPoints > 0; --nPoints, ++pIn, ++pOut)
            {
                GUnits xd = pIn->x - m_AnchorDev[0];
                GUnits yd = pIn->y - m_AnchorDev[1];

                pOut->x = xd * m_MatrixDev2Map[0][0] + yd * m_MatrixDev2Map[0][1] + shift_map_x;
                pOut->y = xd * m_MatrixDev2Map[1][0] + yd * m_MatrixDev2Map[1][1] + shift_map_y;

            }
        }

        void CDisplayTransformation2D::MapToDevice(const CommonLib::GisXYPoint *pIn, GPoint *pOut, int nPts )
        {

            double shift_map_x = m_AnchorMap[0];
            double shift_map_y = m_AnchorMap[1];
            for (; nPts > 0; ++pIn, ++pOut, --nPts)
            {
                double xm = pIn->x - shift_map_x;
                double ym = pIn->y - shift_map_y;
                GUnits xd = static_cast<GUnits>(floor(xm * m_MatrixMap2Dev[0][0] + ym * m_MatrixMap2Dev[0][1] + 0.5));
                GUnits yd = static_cast<GUnits>(floor(xm * m_MatrixMap2Dev[1][0] + ym * m_MatrixMap2Dev[1][1] + 0.5));

                xd += m_AnchorDev[0];
                yd += m_AnchorDev[1];

                pOut->x = (xd <= MINCLIENT)	? MINCLIENT	: ((xd >= MAXCLIENT) ? MAXCLIENT : xd);
                pOut->y = (yd <= MINCLIENT)	? MINCLIENT	: ((yd >= MAXCLIENT) ? MAXCLIENT : yd);
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
                rect.xMin = min( rect.xMin, pts[i].x );
                rect.xMax = max( rect.xMax, pts[i].x );
                rect.yMin = min( rect.yMin, pts[i].y );
                rect.yMax = max( rect.yMax, pts[i].y );
            }
        }

        void CDisplayTransformation2D::DeviceToMap(const GRect &rect, CommonLib::bbox &mapBox )
        {
            GPoint pts[4] =
                    {
                            GPoint(rect.xMin, rect.yMin),
                            GPoint(rect.xMax, rect.yMin),
                            GPoint(rect.xMax, rect.yMax),
                            GPoint(rect.xMin, rect.yMax)
                    };
            CommonLib::GisXYPoint mapXY[4];
            DeviceToMap(pts, mapXY, 4);
            mapBox.xMin = mapBox.xMax = mapXY[0].x;
            mapBox.yMin = mapBox.yMax = mapXY[0].y;
            for (int i = 1; i < 4; ++i)
            {
                mapBox.xMin = min(mapBox.xMin, mapXY[i].x);
                mapBox.yMin = min(mapBox.yMin, mapXY[i].y);
                mapBox.xMax = max(mapBox.xMax, mapXY[i].x);
                mapBox.yMax = max(mapBox.yMax, mapXY[i].y);
            }
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
            m_MatrixDev2Map[1][0] = -d10 / grDet;
            m_MatrixDev2Map[0][1] = -d01 / grDet;


            UpdateFittedBounds();
        }

        void CDisplayTransformation2D::UpdateFittedBounds()
        {
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

        /*void CDisplayTransformation2D::SetClipper(IClip *pClip)
        {
            m_pClipper = pClip;
        }
        IClipPtr CDisplayTransformation2D::GetClipper() const
        {
            return m_pClipper;
        }*/

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