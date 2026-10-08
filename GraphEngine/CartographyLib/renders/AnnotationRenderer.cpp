#include "AnnotationRenderer.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/variant/VariantVisitor.h"
#include "../selectors/SymbolSelectorsLoader.h"

namespace GraphEngine {
    namespace Cartography {

        CAnnotationRenderer::CAnnotationRenderer() : m_nAnnoFieldIndex(-1)
        {
            m_nFeatureRendererID = AnnotationRendererID;
        }

        CAnnotationRenderer::CAnnotationRenderer(ISymbolSelectorPtr ptrSelector) : m_ptrSymbolSelector(ptrSelector), m_nAnnoFieldIndex(-1)
        {
            m_nFeatureRendererID = AnnotationRendererID;
        }

        CAnnotationRenderer::~CAnnotationRenderer()
        {

        }

        bool CAnnotationRenderer::CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const
        {
            if(!m_ptrSymbolSelector.get())
                return false;

            return m_ptrSymbolSelector->CanAssign(ptrTable);
        }

        void CAnnotationRenderer::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
        {
            try
            {
                if(!ptrFilter.get() || !CanRender(ptrTable, Display::IDisplayPtr()))
                    return;

                PrepareShapeField(ptrTable, ptrFilter);
                m_ptrSymbolSelector->PrepareFilter(ptrTable, ptrFilter); // fields the selector chooses the symbol by
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("AnnotationRenderer: failed to prepare filter", exc);
            }
        }

        void CAnnotationRenderer::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter, const std::string& annotationName) const
        {
            try
            {
                if(!ptrFilter.get() || !CanRender(ptrTable, Display::IDisplayPtr()))
                    return;

                PrepareFilter(ptrTable, ptrFilter);

                m_sAnnoField = annotationName;
                m_nAnnoFieldIndex = -1;
                if(!m_sAnnoField.empty() && ptrFilter->GetFieldSet()->Find(m_sAnnoField) < 0)
                    ptrFilter->GetFieldSet()->Add(m_sAnnoField);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("AnnotationRenderer: failed to prepare filter, field: {0}", annotationName, exc);
            }
        }

        Display::ISymbolPtr CAnnotationRenderer::GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const
        {
            if(!m_ptrSymbolSelector.get())
                return Display::ISymbolPtr();

            return m_ptrSymbolSelector->GetSymbolByFeature(ptrRow);
        }

        ISymbolSelectorPtr CAnnotationRenderer::GetSymbolSelector() const
        {
            return m_ptrSymbolSelector;
        }

        void CAnnotationRenderer::SetSymbolSelector(ISymbolSelectorPtr ptrSelector)
        {
            m_ptrSymbolSelector = ptrSelector;
        }

        bool CAnnotationRenderer::GetAnnotationText(GeoDatabase::IRowPtr ptrRow, std::wstring& text) const
        {
            if(m_sAnnoField.empty())
                return false;

            int32_t nColumns = ptrRow->ColumnCount();
            if(m_nAnnoFieldIndex < 0 || m_nAnnoFieldIndex >= nColumns) // reset by PrepareFilter for every new cursor
            {
                m_nAnnoFieldIndex = -1;
                for(int32_t i = 0; i < nColumns; ++i)
                {
                    if(ptrRow->ColumnName(i) == m_sAnnoField)
                    {
                        m_nAnnoFieldIndex = i;
                        break;
                    }
                }

                if(m_nAnnoFieldIndex < 0)
                    throw CommonLib::CExcBase("AnnotationRenderer: annotation field {0} not found", m_sAnnoField);
            }

            if(ptrRow->ColumnIsNull(m_nAnnoFieldIndex))
                return false;

            CommonLib::CVariantPtr ptrVal = ptrRow->GetValue(m_nAnnoFieldIndex);
            if(!ptrVal.get() || ptrVal->IsNull())
                return false;

            if(ptrVal->IsType<CommonLib::wstr_t>())
                text = ptrVal->Get<CommonLib::wstr_t>();
            else if(ptrVal->IsType<CommonLib::astr_t>())
                text = CommonLib::StringEncoding::str_utf82w_safe(ptrVal->Get<CommonLib::astr_t>());   // text fields are utf8
            else if(ptrVal->IsType<CommonLib::IGeoShapePtr>() || ptrVal->IsType<CommonLib::CBlobPtr_t>())
                return false;
            else
            {
                CommonLib::CStringVisitor visitor;   // numbers, guid ...
                ptrVal->Accept(visitor);
                text = CommonLib::StringEncoding::str_utf82w_safe(visitor.GetString());
            }

            return !text.empty();
        }

        void CAnnotationRenderer::DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol)
        {
            try
            {
                // custom symbol is the selection drawing - no annotations for it
                if(!ptrRow.get() || !m_ptrSymbolSelector.get() || ptrCustomSymbol.get())
                    return;

                std::wstring text;
                if(!GetAnnotationText(ptrRow, text))
                    return;

                Display::ITextSymbolPtr ptrTextSymbol = std::dynamic_pointer_cast<Display::ITextSymbol>(m_ptrSymbolSelector->GetSymbolByFeature(ptrRow));
                if(!ptrTextSymbol.get())
                    return;

                CommonLib::IGeoShapePtr ptrShape = GetShape(ptrRow);
                if(!ptrShape.get() || !ptrTextSymbol->CanDraw(ptrShape))
                    return;

                ptrTextSymbol->SetText(text);
                ptrTextSymbol->Prepare(ptrDisplay);
                ptrTextSymbol->Draw(ptrDisplay, ptrShape);
                ptrTextSymbol->Reset();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("AnnotationRenderer: failed to draw feature", exc);
            }
        }

        void CAnnotationRenderer::Save(CommonLib::ISerializeObjPtr pObj) const
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
                CommonLib::CExcBase::RegenExc("AnnotationRenderer: failed to save", exc);
            }
        }

        void CAnnotationRenderer::Load(CommonLib::ISerializeObjPtr pObj)
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
                CommonLib::CExcBase::RegenExc("AnnotationRenderer: failed to load", exc);
            }
        }

    }
}
