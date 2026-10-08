#include "ArrowMarkerSymbol.h"

namespace GraphEngine {
    namespace Display {

        CArrowMarkerSymbol::CArrowMarkerSymbol() : m_style(ArrowMarkerStylePlain), m_dLength(4.), m_dWidth(2.),
            m_dDeviceLength(0.), m_dDeviceWidth(0.)
        {
            m_nSymbolID = ArrowMarkerSymbolID;
            m_Color = Color(0, 0, 0);
            m_dSize = m_dLength;
            m_ptrPen = std::make_shared<CPen>(true);   // filled only, as in UniGIS
            m_ptrBrush = std::make_shared<CBrush>();
        }

        CArrowMarkerSymbol::CArrowMarkerSymbol(double dLength, double dWidth, const Color& color) : CArrowMarkerSymbol()
        {
            m_dLength = dLength;
            m_dWidth = dWidth;
            m_dSize = GetSize();
            m_Color = color;
        }

        CArrowMarkerSymbol::~CArrowMarkerSymbol()
        {

        }

        double CArrowMarkerSymbol::GetSize() const
        {
            return (std::max)(m_dLength, m_dWidth);
        }

        void CArrowMarkerSymbol::SetSize(double dSize)
        {
            double dCurSize = GetSize();
            if(dCurSize <= 0.)
            {
                m_dLength = dSize;
                m_dWidth = dSize / 2.;
            }
            else
            {
                double k = dSize / dCurSize;
                m_dLength *= k;
                m_dWidth *= k;
            }
            m_dSize = GetSize();
        }

        eArrowMarkerStyle CArrowMarkerSymbol::GetStyle() const
        {
            return m_style;
        }

        void CArrowMarkerSymbol::SetStyle(eArrowMarkerStyle style)
        {
            m_style = style;
        }

        double CArrowMarkerSymbol::GetLength() const
        {
            return m_dLength;
        }

        void CArrowMarkerSymbol::SetLength(double dLength)
        {
            m_dLength = dLength;
            m_dSize = GetSize();
        }

        double CArrowMarkerSymbol::GetWidth() const
        {
            return m_dWidth;
        }

        void CArrowMarkerSymbol::SetWidth(double dWidth)
        {
            m_dWidth = dWidth;
            m_dSize = GetSize();
        }

        bool CArrowMarkerSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!ptrShape.get() || ptrShape->GetPointCnt() == 0)
                return false;
            return m_dLength > 0. && m_dWidth > 0. && m_Color.GetA() != Color::Transparent;
        }

        void CArrowMarkerSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
            m_dDeviceLength = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dLength, GetScaleDependent());
            m_dDeviceWidth = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dWidth, GetScaleDependent());
            m_ptrBrush->SetColor(m_Color);
        }

        void CArrowMarkerSymbol::GetArrowPoints(const GPoint& point, GPoint* pTriangle) const
        {
            GPoint center((GUnits)(point.x + m_dDeviceOffsetX), (GUnits)(point.y + m_dDeviceOffsetY));
            double dHalfLength = m_dDeviceLength / 2.;
            double dHalfWidth = m_dDeviceWidth / 2.;

            pTriangle[0] = GPoint((GUnits)(center.x + dHalfLength), center.y);                       // tip
            pTriangle[1] = GPoint((GUnits)(center.x - dHalfLength), (GUnits)(center.y - dHalfWidth));
            pTriangle[2] = GPoint((GUnits)(center.x - dHalfLength), (GUnits)(center.y + dHalfWidth));
            CDisplayUtils::RotateCoords(center, m_dDisplayAngle, pTriangle, 3);
        }

        void CArrowMarkerSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            if(m_dDeviceLength <= 0. || m_dDeviceWidth <= 0.)
                return;

            IGraphicsPtr ptrGraphics = ptrDisplay->GetGraphics();
            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                for(int i = offset; i < offset + polyCounts[part]; ++i)
                {
                    GPoint triangle[4];
                    GetArrowPoints(points[i], triangle);
                    triangle[3] = triangle[0];
                    ptrGraphics->DrawPolygon(m_ptrPen, m_ptrBrush, triangle, 4);
                }
                offset += polyCounts[part];
            }
        }

        void CArrowMarkerSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void CArrowMarkerSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                for(int i = offset; i < offset + polyCounts[part]; ++i)
                {
                    GPoint triangle[3];
                    GetArrowPoints(points[i], triangle);
                    for(int p = 0; p < 3; ++p)
                        rect.ExpandRect(triangle[p]);
                }
                offset += polyCounts[part];
            }
        }

        void CArrowMarkerSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyInt16("Style", (int16_t)m_style);
                pObj->AddPropertyDouble("Length", m_dLength);
                pObj->AddPropertyDouble("Width", m_dWidth);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CArrowMarkerSymbol", exc);
            }
        }

        void CArrowMarkerSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_style = (eArrowMarkerStyle)pObj->GetPropertyInt16("Style", (int16_t)m_style);
                m_dLength = pObj->GetPropertyDouble("Length", m_dLength);
                m_dWidth = pObj->GetPropertyDouble("Width", m_dWidth);
                m_dSize = GetSize();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CArrowMarkerSymbol", exc);
            }
        }

    }
}
