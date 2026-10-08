#include "MultiLayerSymbol.h"

namespace GraphEngine {
    namespace Display {

        // ---------------- marker

        CMultiLayerMarkerSymbol::CMultiLayerMarkerSymbol()
        {
            m_nSymbolID = MultiLayerMarkerSymbolID;
        }

        CMultiLayerMarkerSymbol::~CMultiLayerMarkerSymbol()
        {

        }

        double CMultiLayerMarkerSymbol::GetAngle() const
        {
            return m_vecLayers.empty() ? 0. : LayerAs<IMarkerSymbol>(0)->GetAngle();
        }

        void CMultiLayerMarkerSymbol::SetAngle(double dAngle)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<IMarkerSymbol>(i)->SetAngle(dAngle);
        }

        Color CMultiLayerMarkerSymbol::GetColor() const
        {
            return m_vecLayers.empty() ? Color() : LayerAs<IMarkerSymbol>(0)->GetColor();
        }

        void CMultiLayerMarkerSymbol::SetColor(const Color &color)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<IMarkerSymbol>(i)->SetColor(color);
        }

        double CMultiLayerMarkerSymbol::GetSize() const
        {
            return m_vecLayers.empty() ? 0. : LayerAs<IMarkerSymbol>(0)->GetSize();
        }

        void CMultiLayerMarkerSymbol::SetSize(double dSize)
        {
            double dOldSize = GetSize();
            double k = dOldSize > 0. ? dSize / dOldSize : 1.;
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
            {
                IMarkerSymbolPtr ptrMarker = LayerAs<IMarkerSymbol>(i);
                ptrMarker->SetSize(i == 0 || dOldSize <= 0. ? dSize : ptrMarker->GetSize() * k);
            }
        }

        double CMultiLayerMarkerSymbol::GetXOffset() const
        {
            return m_vecLayers.empty() ? 0. : LayerAs<IMarkerSymbol>(0)->GetXOffset();
        }

        void CMultiLayerMarkerSymbol::SetXOffset(double dOffset)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<IMarkerSymbol>(i)->SetXOffset(dOffset);
        }

        double CMultiLayerMarkerSymbol::GetYOffset() const
        {
            return m_vecLayers.empty() ? 0. : LayerAs<IMarkerSymbol>(0)->GetYOffset();
        }

        void CMultiLayerMarkerSymbol::SetYOffset(double dOffset)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<IMarkerSymbol>(i)->SetYOffset(dOffset);
        }

        bool CMultiLayerMarkerSymbol::GetIgnoreRotation() const
        {
            return m_vecLayers.empty() ? false : LayerAs<IMarkerSymbol>(0)->GetIgnoreRotation();
        }

        void CMultiLayerMarkerSymbol::SetIgnoreRotation(bool bIgnore)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<IMarkerSymbol>(i)->SetIgnoreRotation(bIgnore);
        }

        void CMultiLayerMarkerSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CMultiLayerMarkerSymbol", exc);
            }
        }

        void CMultiLayerMarkerSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CMultiLayerMarkerSymbol", exc);
            }
        }

        // ---------------- line

        CMultiLayerLineSymbol::CMultiLayerLineSymbol()
        {
            m_nSymbolID = MultiLayerLineSymbolID;
        }

        CMultiLayerLineSymbol::~CMultiLayerLineSymbol()
        {

        }

        Color CMultiLayerLineSymbol::GetColor() const
        {
            return m_vecLayers.empty() ? Color() : LayerAs<ILineSymbol>(0)->GetColor();
        }

        void CMultiLayerLineSymbol::SetColor(const Color &color)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<ILineSymbol>(i)->SetColor(color);
        }

        double CMultiLayerLineSymbol::GetWidth() const
        {
            return m_vecLayers.empty() ? 0. : LayerAs<ILineSymbol>(0)->GetWidth();
        }

        void CMultiLayerLineSymbol::SetWidth(double dWidth)
        {
            double dOldWidth = GetWidth();
            double k = dOldWidth > 0. ? dWidth / dOldWidth : 1.;
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
            {
                ILineSymbolPtr ptrLine = LayerAs<ILineSymbol>(i);
                ptrLine->SetWidth(i == 0 || dOldWidth <= 0. ? dWidth : ptrLine->GetWidth() * k);
            }
        }

        void CMultiLayerLineSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CMultiLayerLineSymbol", exc);
            }
        }

        void CMultiLayerLineSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CMultiLayerLineSymbol", exc);
            }
        }

        // ---------------- fill

        CMultiLayerFillSymbol::CMultiLayerFillSymbol()
        {
            m_nSymbolID = MultiLayerFillSymbolID;
        }

        CMultiLayerFillSymbol::~CMultiLayerFillSymbol()
        {

        }

        ILineSymbolPtr CMultiLayerFillSymbol::GetOutlineSymbol() const
        {
            return m_ptrOutline;
        }

        void CMultiLayerFillSymbol::SetOutlineSymbol(ILineSymbolPtr ptrLine)
        {
            m_ptrOutline = ptrLine;
        }

        Color CMultiLayerFillSymbol::GetColor() const
        {
            return m_vecLayers.empty() ? Color() : LayerAs<IFillSymbol>(0)->GetColor();
        }

        void CMultiLayerFillSymbol::SetColor(const Color &color)
        {
            if(!m_vecLayers.empty())
                LayerAs<IFillSymbol>(0)->SetColor(color);
        }

        void CMultiLayerFillSymbol::FillRect(IDisplayPtr ptrDisplay, const GRect& rect)
        {
            for(size_t i = 0; i < m_vecLayers.size(); ++i)
                LayerAs<IFillSymbol>(i)->FillRect(ptrDisplay, rect);
        }

        void CMultiLayerFillSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);
            if(m_ptrOutline.get())
                m_ptrOutline->Prepare(ptrDisplay);
        }

        void CMultiLayerFillSymbol::Reset()
        {
            TBase::Reset();
            if(m_ptrOutline.get())
                m_ptrOutline->Reset();
        }

        void CMultiLayerFillSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            TBase::DrawGeometryEx(ptrDisplay, points, polyCounts, polyCount);
            if(!m_bUseCache && m_ptrOutline.get())
                m_ptrOutline->DrawGeometryEx(ptrDisplay, points, polyCounts, polyCount);
        }

        void CMultiLayerFillSymbol::FlushBuffers(IDisplayPtr ptrDisplay, ITrackCancelPtr ptrTrackCancel)
        {
            if(m_bUseCache && m_ptrOutline.get() && !m_vecCache.empty())
            {
                // the outlines over all the fills: copy, TBase::FlushBuffers clears the cache
                std::vector<SCachedGeometry> vecCache = m_vecCache;
                TBase::FlushBuffers(ptrDisplay, ptrTrackCancel);

                m_ptrOutline->Prepare(ptrDisplay);
                for(size_t g = 0; g < vecCache.size(); ++g)
                    m_ptrOutline->DrawGeometryEx(ptrDisplay, vecCache[g].vecPoints.data(), vecCache[g].vecParts.data(), (int)vecCache[g].vecParts.size());
                m_ptrOutline->Reset();
                return;
            }

            TBase::FlushBuffers(ptrDisplay, ptrTrackCancel);
        }

        void CMultiLayerFillSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                if(m_ptrOutline.get())
                    m_ptrOutline->Save(pObj->CreateChildNode("Outline"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CMultiLayerFillSymbol", exc);
            }
        }

        void CMultiLayerFillSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_ptrOutline.reset();
                if(pObj->IsChildExists("Outline"))
                    m_ptrOutline = std::dynamic_pointer_cast<ILineSymbol>(CSymbolsLoader::LoadSymbol(pObj->GetChild("Outline")));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CMultiLayerFillSymbol", exc);
            }
        }

    }
}
