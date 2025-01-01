#include "SimpleMarketSymbol.h"
#include "../DisplayUtils.h"

namespace GraphEngine {
    namespace Display {

        CSimpleMarketSymbol::CSimpleMarketSymbol(): m_style(SimpleMarkerStyleCircle),
                                                    m_bIsOutline(true),
                                                    m_OutlineColor(Color::RndLineColor()),
                                                    m_dOutlineSize(0.1)
        {
           m_nSymbolID = SimpleMarketSymbolID;
           m_ptrPen = std::make_shared<CPen>();
           m_ptrOutlinePen = std::make_shared<CPen>();
           m_ptrBrush = std::make_shared<CBrush>();
        }

        CSimpleMarketSymbol::~CSimpleMarketSymbol()
        {

        }

        void  CSimpleMarketSymbol::SetStyle(eSimpleMarkerStyle style)
        {
            m_style = style;
        }
        void  CSimpleMarketSymbol::SetOutline(bool bOutline)
        {
            m_bIsOutline = bOutline;
        }
        bool  CSimpleMarketSymbol::IsOutline() const
        {
            return m_bIsOutline;
        }
        Color CSimpleMarketSymbol:: GetOutlineColor() const
        {
            return m_OutlineColor;
        }
        void   CSimpleMarketSymbol::SetOutlineColor( const Color &color)
        {
            m_OutlineColor = color;
        }
        double  CSimpleMarketSymbol::GetOutlineSize() const
        {
            return m_dOutlineSize;
        }
        void   CSimpleMarketSymbol::SetOutlineSize(double size)
        {
            m_dOutlineSize = size;
        }
        bool CSimpleMarketSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(ptrShape->GetPointCnt() > 0)
            {
                if(GetSize() > 0)
                {
                    if(GetColor().GetA() != Color::Transparent)
                        return true;
                    else
                    if(m_bIsOutline)
                    {
                        if(m_OutlineColor.GetA() != Color::Transparent)
                        {
                            if(m_dOutlineSize > 0)
                                return true;
                        }
                    }
                }
            }
            return false;
        }

        void CSimpleMarketSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {

                TSymbolBase::Save(pObj);
                pObj->AddPropertyInt16("Style", m_style);
                pObj->AddPropertyBool("IsOutline", m_bIsOutline);
                pObj->AddPropertyDouble("OutlineSize", m_dOutlineSize);
                m_OutlineColor.Save(pObj, "Color");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CSimpleMarketSymbol", exc);
            }
        }

        void CSimpleMarketSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TSymbolBase::Load(pObj);
                m_style = (eSimpleMarkerStyle)pObj->GetPropertyInt16("Style", m_style);
                m_bIsOutline = pObj->GetPropertyBool("IsOutline", m_bIsOutline);
                m_dOutlineSize = pObj->GetPropertyDouble("OutlineSize", m_dOutlineSize);
                m_OutlineColor.Load(pObj, "Color");

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CSimpleMarketSymbol", exc);
            }
        }

        void  CSimpleMarketSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount )
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }
        void  CSimpleMarketSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            for(size_t part = 0, offset = 0; part < polyCount; part++)
            {
                for(int point = (int)offset; point < (int)offset + polyCounts[part]; point++)
                {
                    switch(m_style)
                    {
                        case SimpleMarkerStyleCircle:
                            DrawCircle(points[point], ptrDisplay->GetGraphics());
                            break;
                        case SimpleMarkerStyleSquare:
                            DrawSquare(points[point], ptrDisplay->GetGraphics());
                            break;
                        case SimpleMarkerStyleCross:
                            DrawCross(points[point], ptrDisplay->GetGraphics());
                            break;
                        case SimpleMarkerStyleX:
                            DrawX(points[point], ptrDisplay->GetGraphics());
                            break;
                        case SimpleMarkerStyleDiamond:
                            DrawDiamond(points[point], ptrDisplay->GetGraphics());
                            break;
                    }
                }
                offset += polyCounts[part];
            }
        }
        void  CSimpleMarketSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount,   GRect &rect) const
        {

            for(size_t part = 0, offset = 0; part < polyCount; part++)
            {
                for(size_t p = offset; p < offset + (size_t)polyCounts[part]; p ++)
                {
                    GRect r;
                    r.xMin = GUnits(points[p].x - m_dDeviceSize + m_dDeviceOffsetX);
                    r.xMax = GUnits(points[p].x + m_dDeviceSize + m_dDeviceOffsetX);
                    r.yMin = GUnits(points[p].y - m_dDeviceSize + m_dDeviceOffsetY);
                    r.yMax = GUnits(points[p].y + m_dDeviceSize + m_dDeviceOffsetY);

                    r.xMax -= m_ptrOutlinePen->GetWidth() / 2;
                    r.yMin -= m_ptrOutlinePen->GetWidth() / 2;
                    r.xMax += m_ptrOutlinePen->GetWidth() / 2;
                    r.yMax += m_ptrOutlinePen->GetWidth() / 2;

                    rect.ExpandRect(r);
                }
            }
        }
        void  CSimpleMarketSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TSymbolBase::Prepare(ptrDisplay);
            GUnits devicesizeOutline = CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dOutlineSize, GetScaleDependent());

            if(m_style == SimpleMarkerStyleX || m_style == SimpleMarkerStyleCross)
            {
                m_ptrPen->SetColor(GetColor());
                if(m_bIsOutline)
                {
                    m_ptrOutlinePen->SetWidth(m_ptrPen->GetWidth() + 2 * devicesizeOutline);
                    m_ptrOutlinePen->SetColor(m_OutlineColor);
                }
            }
            else
            {
                m_ptrBrush->SetColor(GetColor());
                if(m_bIsOutline)
                {
                    m_ptrOutlinePen->SetWidth(devicesizeOutline);
                    m_ptrOutlinePen->SetColor(m_OutlineColor);
                }
                else
                    m_ptrOutlinePen->SetPenType(PenTypeNull);
            }
        }

        void CSimpleMarketSymbol::DrawCircle(const GPoint& point, IGraphicsPtr pGraphics)
        {
            pGraphics->DrawEllipse(m_ptrOutlinePen, m_ptrBrush,
                                   point.x - m_dDeviceSize + m_dDeviceOffsetX,
                                   point.y - m_dDeviceSize + m_dDeviceOffsetY,
                                   point.x + m_dDeviceSize + m_dDeviceOffsetX,
                                   point.y + m_dDeviceSize + m_dDeviceOffsetY);
        }

        void CSimpleMarketSymbol::DrawSquare(const GPoint& point, IGraphicsPtr pGraphics)
        {
            GPoint coord[5];
            coord[0].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[0].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;
            coord[1].x = point.x + m_dDeviceSize + m_dDeviceOffsetX;
            coord[1].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;
            coord[2].x = point.x + m_dDeviceSize + m_dDeviceOffsetX;
            coord[2].y = point.y + m_dDeviceSize + m_dDeviceOffsetY;
            coord[3].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[3].y = point.y + m_dDeviceSize + m_dDeviceOffsetY;
            coord[4].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[4].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;

            CDisplayUtils::RotateCoords(point, m_dDisplayAngle, coord, 5);
            pGraphics->DrawPolygon(m_ptrOutlinePen, m_ptrBrush, coord, 5);
        }

        void CSimpleMarketSymbol::DrawDiamond(const GPoint& point, IGraphicsPtr pGraphics)
        {
            GUnits centerX = point.x + m_dDeviceOffsetX;
            GUnits centerY = point.y + m_dDeviceOffsetY;

            GPoint coord[5];
            coord[0].x = centerX;
            coord[0].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;

            coord[1].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[1].y = centerY;

            coord[2].x = centerX;
            coord[2].y = point.y + m_dDeviceSize + m_dDeviceOffsetY;

            coord[3].x = point.x + m_dDeviceSize + m_dDeviceOffsetX;
            coord[3].y = centerY;

            coord[4].x = centerX;
            coord[4].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;

            CDisplayUtils::RotateCoords(point, m_dDisplayAngle, coord, 5);

            pGraphics->DrawPolygon(m_ptrOutlinePen, m_ptrBrush, coord, 5);
        }

        void CSimpleMarketSymbol::DrawX(const GPoint& point, IGraphicsPtr pGraphics)
        {
            GPoint coord[4];
            coord[0].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[0].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;

            coord[1].x = point.x + m_dDeviceSize + 1 + m_dDeviceOffsetX;
            coord[1].y = point.y + m_dDeviceSize + 1 + m_dDeviceOffsetY;

            coord[2].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[2].y = point.y + m_dDeviceSize + m_dDeviceOffsetY;

            coord[3].x = point.x + m_dDeviceSize + m_dDeviceOffsetX;
            coord[3].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;

            GUnits centerX = point.x + m_dDeviceOffsetX;
            GUnits centerY = point.y + m_dDeviceOffsetY;

            CDisplayUtils::RotateCoords(point, m_dDisplayAngle, coord, 4);

            if (m_bIsOutline)
            {
                pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[0], coord[1]);
                pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[2].x, coord[2].y,centerX, centerY);
                pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[3].x, coord[3].y,centerX, centerY);
            }

            pGraphics->DrawLineSeg(m_ptrPen, coord[0], coord[1]);
            pGraphics->DrawLineSeg(m_ptrPen, coord[2].x, coord[2].y,centerX, centerY);
            pGraphics->DrawLineSeg(m_ptrPen, coord[3].x, coord[3].y,centerX, centerY);
        }
        void CSimpleMarketSymbol::DrawCross(const GPoint& point, IGraphicsPtr pGraphics)
        {
            GPoint coord[4];
            coord[0].x = point.x + m_dDeviceOffsetX;
            coord[0].y = point.y - m_dDeviceSize + m_dDeviceOffsetY;

            coord[1].x = point.x + m_dDeviceOffsetX;
            coord[1].y = point.y + m_dDeviceSize + 1 + m_dDeviceOffsetY;

            coord[2].x = point.x - m_dDeviceSize + m_dDeviceOffsetX;
            coord[2].y = point.y + m_dDeviceOffsetY;

            coord[3].x = point.x + m_dDeviceSize + m_dDeviceOffsetX;
            coord[3].y = point.y + m_dDeviceOffsetY;

            GUnits centerX = point.x + m_dDeviceOffsetX;
            GUnits centerY = point.y + m_dDeviceOffsetY;

            CDisplayUtils::RotateCoords(point, m_dDisplayAngle, coord, 4);

            if (m_bIsOutline)
            {
                pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[0], coord[1]);
                pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[2].x, coord[2].y,centerX,centerY);
                pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[3].x, coord[3].y,centerX,centerY);
            }

            pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[0], coord[1]);
            pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[2].x, coord[2].y,centerX,centerY);
            pGraphics->DrawLineSeg(m_ptrOutlinePen, coord[3].x, coord[3].y,centerX,centerY);
        }


    }
}