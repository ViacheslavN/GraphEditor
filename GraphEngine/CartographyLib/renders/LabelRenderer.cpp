#include "LabelRenderer.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/variant/VariantVisitor.h"
#include "../selectors/SymbolSelectorsLoader.h"

namespace GraphEngine {
    namespace Cartography {

        CLabelRenderer::CLabelRenderer() : m_nClassIndex(0), m_nLabelFieldIndex(-1)
        {
            m_nFeatureRendererID = LabelRendererID;
        }

        CLabelRenderer::CLabelRenderer(ISymbolSelectorPtr ptrSelector) : m_ptrSymbolSelector(ptrSelector), m_nClassIndex(0), m_nLabelFieldIndex(-1)
        {
            m_nFeatureRendererID = LabelRendererID;
        }

        CLabelRenderer::~CLabelRenderer()
        {

        }

        bool CLabelRenderer::CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const
        {
            if(!m_ptrSymbolSelector.get())
                return false;

            return m_ptrSymbolSelector->CanAssign(ptrTable);
        }

        void CLabelRenderer::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
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
                CommonLib::CExcBase::RegenExc("LabelRenderer: failed to prepare filter", exc);
            }
        }

        void CLabelRenderer::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter, const std::string& labelFieldName) const
        {
            try
            {
                if(!ptrFilter.get() || !CanRender(ptrTable, Display::IDisplayPtr()))
                    return;

                PrepareFilter(ptrTable, ptrFilter);

                m_sLabelField = labelFieldName;
                m_nLabelFieldIndex = -1;
                if(!m_sLabelField.empty() && ptrFilter->GetFieldSet()->Find(m_sLabelField) < 0)
                    ptrFilter->GetFieldSet()->Add(m_sLabelField);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("LabelRenderer: failed to prepare filter, field: {0}", labelFieldName, exc);
            }
        }

        Display::ISymbolPtr CLabelRenderer::GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const
        {
            if(!m_ptrSymbolSelector.get())
                return Display::ISymbolPtr();

            return m_ptrSymbolSelector->GetSymbolByFeature(ptrRow);
        }

        ISymbolSelectorPtr CLabelRenderer::GetSymbolSelector() const
        {
            return m_ptrSymbolSelector;
        }

        void CLabelRenderer::SetSymbolSelector(ISymbolSelectorPtr ptrSelector)
        {
            m_ptrSymbolSelector = ptrSelector;
        }

        int CLabelRenderer::GetClassIndex() const
        {
            return m_nClassIndex;
        }

        void CLabelRenderer::SetClassIndex(int nIndex)
        {
            m_nClassIndex = nIndex;
        }

        bool CLabelRenderer::GetLabelText(GeoDatabase::IRowPtr ptrRow, std::wstring& text) const
        {
            if(m_sLabelField.empty())
                return false;

            int32_t nColumns = ptrRow->ColumnCount();
            if(m_nLabelFieldIndex < 0 || m_nLabelFieldIndex >= nColumns) // reset by PrepareFilter for every new cursor
            {
                m_nLabelFieldIndex = -1;
                for(int32_t i = 0; i < nColumns; ++i)
                {
                    if(ptrRow->ColumnName(i) == m_sLabelField)
                    {
                        m_nLabelFieldIndex = i;
                        break;
                    }
                }

                if(m_nLabelFieldIndex < 0)
                    throw CommonLib::CExcBase("LabelRenderer: label field {0} not found", m_sLabelField);
            }

            if(ptrRow->ColumnIsNull(m_nLabelFieldIndex))
                return false;

            CommonLib::CVariantPtr ptrVal = ptrRow->GetValue(m_nLabelFieldIndex);
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

        void CLabelRenderer::AddLabel(ILabelDrawerPtr ptrLabelDrawer, GeoDatabase::IRowPtr ptrRow, const SLabelingOptions& options)
        {
            try
            {
                if(!ptrLabelDrawer.get() || !ptrRow.get() || !m_ptrSymbolSelector.get())
                    return;

                std::wstring text;
                if(!GetLabelText(ptrRow, text))
                    return;

                Display::ITextSymbolPtr ptrTextSymbol = std::dynamic_pointer_cast<Display::ITextSymbol>(m_ptrSymbolSelector->GetSymbolByFeature(ptrRow));
                if(!ptrTextSymbol.get())
                    return;

                CommonLib::IGeoShapePtr ptrShape = GetShape(ptrRow);
                if(!ptrShape.get())
                    return;

                ptrLabelDrawer->AddLabel(text, ptrShape, ptrTextSymbol, m_nClassIndex, options);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("LabelRenderer: failed to add label", exc);
            }
        }

        void CLabelRenderer::DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol)
        {
            try
            {
                // custom symbol is the selection drawing - no labels for it
                if(!ptrRow.get() || !m_ptrSymbolSelector.get() || ptrCustomSymbol.get())
                    return;

                std::wstring text;
                if(!GetLabelText(ptrRow, text))
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
                CommonLib::CExcBase::RegenExc("LabelRenderer: failed to draw feature", exc);
            }
        }

        void CLabelRenderer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyInt32("ClassIndex", m_nClassIndex);
                if(m_ptrSymbolSelector.get())
                {
                    CommonLib::ISerializeObjPtr ptrSelectorNode = pObj->CreateChildNode("SymbolSelector");
                    m_ptrSymbolSelector->Save(ptrSelectorNode);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("LabelRenderer: failed to save", exc);
            }
        }

        void CLabelRenderer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_nClassIndex = pObj->GetPropertyInt32("ClassIndex", m_nClassIndex);

                m_ptrSymbolSelector.reset();
                if(pObj->IsChildExists("SymbolSelector"))
                    m_ptrSymbolSelector = CSymbolSelectorsLoader::LoadSymbolSelector(pObj->GetChild("SymbolSelector"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("LabelRenderer: failed to load", exc);
            }
        }

    }
}
