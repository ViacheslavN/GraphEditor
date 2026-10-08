#include "MarkerLineSymbol.h"
#include "SimpleMarketSymbol.h"
#include "SymbolsLoader.h"

namespace GraphEngine {
    namespace Display {

        CMarkerLineSymbol::CMarkerLineSymbol() : m_dDeviceMarkerSize(0.)
        {
            m_nSymbolID = MarkerLineSymbolID;
            std::shared_ptr<CSimpleMarketSymbol> ptrMarker = std::make_shared<CSimpleMarketSymbol>();
            ptrMarker->SetColor(Color(0, 0, 0));
            ptrMarker->SetSize(1.5);
            ptrMarker->SetOutline(false);
            m_ptrMarker = ptrMarker;
        }

        CMarkerLineSymbol::CMarkerLineSymbol(IMarkerSymbolPtr ptrMarker, double dInterval) : m_ptrMarker(ptrMarker), m_dDeviceMarkerSize(0.)
        {
            m_nSymbolID = MarkerLineSymbolID;
            m_ptrTemplate->SetInterval(dInterval);
        }

        CMarkerLineSymbol::~CMarkerLineSymbol()
        {

        }

        Color CMarkerLineSymbol::GetColor() const
        {
            return m_ptrMarker.get() ? m_ptrMarker->GetColor() : Color();
        }

        void CMarkerLineSymbol::SetColor(const Color &color)
        {
            if(m_ptrMarker.get())
                m_ptrMarker->SetColor(color);
            m_bDirty = true;
        }

        double CMarkerLineSymbol::GetWidth() const
        {
            return m_ptrMarker.get() ? m_ptrMarker->GetSize() : 0.;
        }

        void CMarkerLineSymbol::SetWidth(double dWidth)
        {
            if(m_ptrMarker.get())
                m_ptrMarker->SetSize(dWidth);
            m_bDirty = true;
        }

        IMarkerSymbolPtr CMarkerLineSymbol::GetMarkerSymbol() const
        {
            return m_ptrMarker;
        }

        void CMarkerLineSymbol::SetMarkerSymbol(IMarkerSymbolPtr ptrSymbol)
        {
            m_ptrMarker = ptrSymbol;
            m_bDirty = true;
        }

        bool CMarkerLineSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!m_ptrMarker.get())
                return false;
            return TBase::CanDraw(ptrShape);
        }

        void CMarkerLineSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            if(m_ptrMarker.get())
            {
                m_ptrMarker->Prepare(ptrDisplay);
                m_dDeviceMarkerSize = CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_ptrMarker->GetSize(), GetScaleDependent());
            }
        }

        void CMarkerLineSymbol::Reset()
        {
            if(m_ptrMarker.get())
                m_ptrMarker->Reset();
        }

        void CMarkerLineSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            if(!m_ptrMarker.get() || !m_ptrTemplate.get())
                return;

            // the marker angle is changed for every point and restored after
            IMarkerSymbolPtr ptrMarker = m_ptrMarker;
            double dMarkerAngle = ptrMarker->GetAngle();
            try
            {
                m_ptrTemplate->ForEachMark(points, polyCounts, polyCount, m_dDeviceOffset,
                    [&](const GPoint& pt, double dLineAngle)
                    {
                        ptrMarker->SetAngle(dMarkerAngle + dLineAngle);
                        ptrMarker->Prepare(ptrDisplay);

                        int nCount = 1;
                        ptrMarker->DrawGeometryEx(ptrDisplay, &pt, &nCount, 1);
                    });
            }
            catch (...)
            {
                ptrMarker->SetAngle(dMarkerAngle);
                throw;
            }

            ptrMarker->SetAngle(dMarkerAngle);
            ptrMarker->Prepare(ptrDisplay);
        }

        void CMarkerLineSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            int nTotal = 0;
            for(int part = 0; part < polyCount; ++part)
                nTotal += polyCounts[part];

            for(int i = 0; i < nTotal; ++i)
                rect.ExpandRect(points[i]);

            GUnits dExpand = m_dDeviceMarkerSize / 2 + (m_dDeviceOffset < 0 ? -m_dDeviceOffset : m_dDeviceOffset);
            rect.xMin -= dExpand;
            rect.yMin -= dExpand;
            rect.xMax += dExpand;
            rect.yMax += dExpand;
        }

        void CMarkerLineSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                if(m_ptrMarker.get())
                    m_ptrMarker->Save(pObj->CreateChildNode("MarkerSymbol"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CMarkerLineSymbol", exc);
            }
        }

        void CMarkerLineSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_ptrMarker.reset();
                if(pObj->IsChildExists("MarkerSymbol"))
                    m_ptrMarker = std::dynamic_pointer_cast<IMarkerSymbol>(CSymbolsLoader::LoadSymbol(pObj->GetChild("MarkerSymbol")));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CMarkerLineSymbol", exc);
            }
        }

    }
}
