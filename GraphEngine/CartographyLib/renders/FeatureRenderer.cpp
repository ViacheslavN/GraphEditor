#include "FeatureRenderer.h"
#include "../selectors/SymbolSelectorsLoader.h"

namespace GraphEngine {
    namespace Cartography {

        CFeatureRenderer::CFeatureRenderer()
        {
            m_nFeatureRendererID = SimpleFeatureRendererID;
        }

        CFeatureRenderer::~CFeatureRenderer()
        {

        }

        bool CFeatureRenderer::CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const
        {
            if(!m_ptrSymbolSelector.get())
                return false;

            return m_ptrSymbolSelector->CanAssign(ptrTable);
        }

        void CFeatureRenderer::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
        {
            try
            {
                if(!ptrFilter.get() || !CanRender(ptrTable, Display::IDisplayPtr()))
                    return;

                PrepareShapeField(ptrTable, ptrFilter);

                m_ptrSymbolSelector->PrepareFilter(ptrTable, ptrFilter);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("FeatureRenderer: failed to prepare filter", exc);
            }
        }

        Display::ISymbolPtr CFeatureRenderer::GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const
        {
            if(!m_ptrSymbolSelector.get())
                return Display::ISymbolPtr();

            return m_ptrSymbolSelector->GetSymbolByFeature(ptrRow);
        }

        ISymbolSelectorPtr CFeatureRenderer::GetSymbolSelector() const
        {
            return m_ptrSymbolSelector;
        }

        void CFeatureRenderer::SetSymbolSelector(ISymbolSelectorPtr ptrSelector)
        {
            m_ptrSymbolSelector = ptrSelector;
        }

        void CFeatureRenderer::DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol)
        {
            try
            {
                if(!ptrRow.get() || (!m_ptrSymbolSelector.get() && !ptrCustomSymbol.get()))
                    return;

                CommonLib::IGeoShapePtr ptrShape = GetShape(ptrRow);
                if(!ptrShape.get())
                    return;

                Display::ISymbolPtr ptrSymbol = ptrCustomSymbol.get() ? ptrCustomSymbol : m_ptrSymbolSelector->GetSymbolByFeature(ptrRow);
                if(!ptrSymbol.get() || !ptrSymbol->CanDraw(ptrShape))
                    return;

                ptrSymbol->Prepare(ptrDisplay);
                ptrSymbol->Draw(ptrDisplay, ptrShape);
                ptrSymbol->Reset();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("FeatureRenderer: failed to draw feature", exc);
            }
        }

        void CFeatureRenderer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                if(m_ptrSymbolSelector.get())
                {
                    CommonLib::ISerializeObjPtr ptrSelectorNode = pObj->CreateChildNode("SymbolSelector");
                    m_ptrSymbolSelector->Save(ptrSelectorNode);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("FeatureRenderer: failed to save", exc);
            }
        }

        void CFeatureRenderer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);

                m_ptrSymbolSelector.reset();
                if(pObj->IsChildExists("SymbolSelector"))
                    m_ptrSymbolSelector = CSymbolSelectorsLoader::LoadSymbolSelector(pObj->GetChild("SymbolSelector"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("FeatureRenderer: failed to load", exc);
            }
        }

    }
}
