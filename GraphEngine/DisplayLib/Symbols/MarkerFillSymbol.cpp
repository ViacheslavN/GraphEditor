#include "MarkerFillSymbol.h"
#include "SimpleMarketSymbol.h"
#include "SymbolBitmapUtils.h"
#include "../DisplayUtils.h"
#include <cmath>
#include <cstdint>

namespace GraphEngine {
    namespace Display {

        namespace
        {
            const double MaxFillMarkers = 200000.;

            // stable pseudo random value 0..1 of the grid cell, the markers don't jump when the map is redrawn
            double CellRandom(int64_t i, int64_t j, uint32_t salt)
            {
                uint64_t h = (uint64_t)i * 0x9E3779B97F4A7C15ULL ^ ((uint64_t)j + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL ^ salt;
                h ^= h >> 33;
                h *= 0xFF51AFD7ED558CCDULL;
                h ^= h >> 33;
                h *= 0xC4CEB9FE1A85EC53ULL;
                h ^= h >> 33;
                return (double)(h & 0xFFFFFF) / (double)0x1000000;
            }
        }

        CMarkerFillSymbol::CMarkerFillSymbol() : m_style(MarkerFillStyleGrid), m_dXSeparation(4.), m_dYSeparation(4.), m_dXOffset(0.), m_dYOffset(0.),
            m_dDeviceXSeparation(0.), m_dDeviceYSeparation(0.), m_dDeviceXOffset(0.), m_dDeviceYOffset(0.), m_dDeviceMarkerSize(0.)
        {
            m_nSymbolID = MarkerFillSymbolID;
            std::shared_ptr<CSimpleMarketSymbol> ptrMarker = std::make_shared<CSimpleMarketSymbol>();
            ptrMarker->SetColor(Color(0, 0, 0));
            ptrMarker->SetSize(1.);
            ptrMarker->SetOutline(false);
            m_ptrMarker = ptrMarker;
        }

        CMarkerFillSymbol::CMarkerFillSymbol(IMarkerSymbolPtr ptrMarker, double dXSeparation, double dYSeparation) :
            m_ptrMarker(ptrMarker), m_style(MarkerFillStyleGrid), m_dXSeparation(dXSeparation), m_dYSeparation(dYSeparation), m_dXOffset(0.), m_dYOffset(0.),
            m_dDeviceXSeparation(0.), m_dDeviceYSeparation(0.), m_dDeviceXOffset(0.), m_dDeviceYOffset(0.), m_dDeviceMarkerSize(0.)
        {
            m_nSymbolID = MarkerFillSymbolID;
        }

        CMarkerFillSymbol::~CMarkerFillSymbol()
        {

        }

        Color CMarkerFillSymbol::GetColor() const
        {
            return m_ptrMarker.get() ? m_ptrMarker->GetColor() : Color();
        }

        void CMarkerFillSymbol::SetColor(const Color &color)
        {
            if(m_ptrMarker.get())
                m_ptrMarker->SetColor(color);
            m_bDirty = true;
        }

        IMarkerSymbolPtr CMarkerFillSymbol::GetMarkerSymbol() const
        {
            return m_ptrMarker;
        }

        void CMarkerFillSymbol::SetMarkerSymbol(IMarkerSymbolPtr ptrMarker)
        {
            m_ptrMarker = ptrMarker;
            m_bDirty = true;
        }

        eMarkerFillStyle CMarkerFillSymbol::GetStyle() const
        {
            return m_style;
        }

        void CMarkerFillSymbol::SetStyle(eMarkerFillStyle style)
        {
            m_style = style;
            m_bDirty = true;
        }

        double CMarkerFillSymbol::GetXSeparation() const { return m_dXSeparation; }
        void   CMarkerFillSymbol::SetXSeparation(double sep) { m_dXSeparation = sep; m_bDirty = true; }
        double CMarkerFillSymbol::GetYSeparation() const { return m_dYSeparation; }
        void   CMarkerFillSymbol::SetYSeparation(double sep) { m_dYSeparation = sep; m_bDirty = true; }
        double CMarkerFillSymbol::GetXOffset() const { return m_dXOffset; }
        void   CMarkerFillSymbol::SetXOffset(double offset) { m_dXOffset = offset; m_bDirty = true; }
        double CMarkerFillSymbol::GetYOffset() const { return m_dYOffset; }
        void   CMarkerFillSymbol::SetYOffset(double offset) { m_dYOffset = offset; m_bDirty = true; }

        bool CMarkerFillSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!ptrShape.get() || ptrShape->GetPointCnt() < 3)
                return false;
            return m_ptrMarker.get() != nullptr || m_ptrBorderSymbol.get() != nullptr;
        }

        void CMarkerFillSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
            m_dDeviceXSeparation = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dXSeparation, GetScaleDependent());
            m_dDeviceYSeparation = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dYSeparation, GetScaleDependent());
            m_dDeviceXOffset = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dXOffset, GetScaleDependent());
            m_dDeviceYOffset = -CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dYOffset, GetScaleDependent());  // device y goes down

            if(m_ptrMarker.get())
            {
                m_ptrMarker->Prepare(ptrDisplay);
                m_dDeviceMarkerSize = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_ptrMarker->GetSize(), GetScaleDependent());
            }
            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->Prepare(ptrDisplay);
        }

        void CMarkerFillSymbol::Reset()
        {
            if(m_ptrMarker.get())
                m_ptrMarker->Reset();
            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->Reset();
        }

        void CMarkerFillSymbol::DrawMarkers(IDisplayPtr ptrDisplay, const GRect& bounds)
        {
            if(!m_ptrMarker.get() || m_dDeviceXSeparation < 1. || m_dDeviceYSeparation < 1.)
                return;

            GPoint origin = GetPatternOrigin(ptrDisplay);
            double x0 = origin.x + m_dDeviceXOffset;
            double y0 = origin.y + m_dDeviceYOffset;

            // markers which centers are outside the polygon can still be partly visible
            double dMargin = m_dDeviceMarkerSize + (m_style == MarkerFillStyleRandom ? (std::max)(m_dDeviceXSeparation, m_dDeviceYSeparation) : 0.);
            double iStart = floor((bounds.xMin - dMargin - x0) / m_dDeviceXSeparation);
            double iEnd = ceil((bounds.xMax + dMargin - x0) / m_dDeviceXSeparation);
            double jStart = floor((bounds.yMin - dMargin - y0) / m_dDeviceYSeparation);
            double jEnd = ceil((bounds.yMax + dMargin - y0) / m_dDeviceYSeparation);
            if((iEnd - iStart + 1.) * (jEnd - jStart + 1.) > MaxFillMarkers)
                return;

            for(double i = iStart; i <= iEnd; i += 1.)
            {
                for(double j = jStart; j <= jEnd; j += 1.)
                {
                    GPoint pt((GUnits)(x0 + i * m_dDeviceXSeparation), (GUnits)(y0 + j * m_dDeviceYSeparation));
                    if(m_style == MarkerFillStyleRandom)
                    {
                        pt.x += (GUnits)((CellRandom((int64_t)i, (int64_t)j, 1) - 0.5) * m_dDeviceXSeparation);
                        pt.y += (GUnits)((CellRandom((int64_t)i, (int64_t)j, 2) - 0.5) * m_dDeviceYSeparation);
                    }

                    int nCount = 1;
                    m_ptrMarker->DrawGeometryEx(ptrDisplay, &pt, &nCount, 1);
                }
            }
        }

        void CMarkerFillSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            GRect bounds;
            if(!GetPointsBounds(points, polyCounts, polyCount, bounds))
                return;

            GRect deviceRect = ptrDisplay->GetTransformation()->GetDeviceRect();
            if(bounds.xMin < deviceRect.xMin) bounds.xMin = deviceRect.xMin;
            if(bounds.yMin < deviceRect.yMin) bounds.yMin = deviceRect.yMin;
            if(bounds.xMax > deviceRect.xMax) bounds.xMax = deviceRect.xMax;
            if(bounds.yMax > deviceRect.yMax) bounds.yMax = deviceRect.yMax;

            if(bounds.xMin < bounds.xMax && bounds.yMin < bounds.yMax)
            {
                CPolygonClipGuard clip(ptrDisplay->GetGraphics(), points, polyCounts, polyCount);
                DrawMarkers(ptrDisplay, bounds);
            }

            DrawOutline(ptrDisplay, points, polyCounts, polyCount);
        }

        void CMarkerFillSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void CMarkerFillSymbol::FillRect(IDisplayPtr ptrDisplay, const GRect& rect)
        {
            GPoint points[5] = {GPoint(rect.xMin, rect.yMin), GPoint(rect.xMax, rect.yMin), GPoint(rect.xMax, rect.yMax),
                                GPoint(rect.xMin, rect.yMax), GPoint(rect.xMin, rect.yMin)};
            int nCount = 5;
            DrawGeometryEx(ptrDisplay, points, &nCount, 1);
        }

        void CMarkerFillSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            GRect bounds;
            if(GetPointsBounds(points, polyCounts, polyCount, bounds))
                rect.ExpandRect(bounds);

            if(m_ptrBorderSymbol.get())
                m_ptrBorderSymbol->QueryBoundaryRectEx(ptrDisplay, points, polyCounts, polyCount, rect);
        }

        void CMarkerFillSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyInt16("Style", (int16_t)m_style);
                pObj->AddPropertyDouble("XSeparation", m_dXSeparation);
                pObj->AddPropertyDouble("YSeparation", m_dYSeparation);
                pObj->AddPropertyDouble("XOffset", m_dXOffset);
                pObj->AddPropertyDouble("YOffset", m_dYOffset);
                if(m_ptrMarker.get())
                    m_ptrMarker->Save(pObj->CreateChildNode("MarkerSymbol"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CMarkerFillSymbol", exc);
            }
        }

        void CMarkerFillSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_style = (eMarkerFillStyle)pObj->GetPropertyInt16("Style", (int16_t)m_style);
                m_dXSeparation = pObj->GetPropertyDouble("XSeparation", m_dXSeparation);
                m_dYSeparation = pObj->GetPropertyDouble("YSeparation", m_dYSeparation);
                m_dXOffset = pObj->GetPropertyDouble("XOffset", m_dXOffset);
                m_dYOffset = pObj->GetPropertyDouble("YOffset", m_dYOffset);

                m_ptrMarker.reset();
                if(pObj->IsChildExists("MarkerSymbol"))
                    m_ptrMarker = std::dynamic_pointer_cast<IMarkerSymbol>(CSymbolsLoader::LoadSymbol(pObj->GetChild("MarkerSymbol")));
                m_bDirty = true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CMarkerFillSymbol", exc);
            }
        }

    }
}
