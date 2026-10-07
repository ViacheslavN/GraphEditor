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

                if(m_sShapeField.empty())
                {
                    m_sShapeField = ptrTable->GetShapeFieldName();
                }
                else if(isdigit((unsigned char)m_sShapeField[0]))
                {
                    // shape field is set as index among geometry fields ("0" - first geometry field, ...)
                    int nShapeIndex = atoi(m_sShapeField.c_str());
                    GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
                    int nShape = -1;
                    for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
                    {
                        GeoDatabase::IFieldPtr ptrField = ptrFields->GetField(i);
                        if(ptrField->GetType() == GeoDatabase::dtGeometry)
                            ++nShape;

                        if(nShape == nShapeIndex)
                        {
                            m_sShapeField = ptrField->GetName();
                            break;
                        }
                    }
                }

                if(ptrFilter->GetFieldSet()->Find(m_sShapeField) < 0)
                    ptrFilter->GetFieldSet()->Add(m_sShapeField);

                m_nShapeFieldIndex = -1; // column index is resolved on the cursor row, see GetShape

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

        CommonLib::IGeoShapePtr CFeatureRenderer::GetShape(GeoDatabase::IRowPtr ptrRow) const
        {
            int32_t nColumns = ptrRow->ColumnCount();
            if(m_nShapeFieldIndex < 0 || m_nShapeFieldIndex >= nColumns || ptrRow->GetColumnType(m_nShapeFieldIndex) != GeoDatabase::dtGeometry)
            {
                m_nShapeFieldIndex = -1;
                for(int32_t i = 0; i < nColumns; ++i)
                {
                    if(ptrRow->GetColumnType(i) != GeoDatabase::dtGeometry)
                        continue;

                    if(m_sShapeField.empty() || ptrRow->ColumnName(i) == m_sShapeField)
                    {
                        m_nShapeFieldIndex = i;
                        break;
                    }

                    if(m_nShapeFieldIndex < 0)
                        m_nShapeFieldIndex = i; // fallback: first geometry column
                }

                if(m_nShapeFieldIndex < 0)
                    throw CommonLib::CExcBase("FeatureRenderer: shape field {0} not found", m_sShapeField);
            }

            if(ptrRow->ColumnIsNull(m_nShapeFieldIndex))
                return CommonLib::IGeoShapePtr();

            CommonLib::CVariantPtr ptrVal = ptrRow->GetValue(m_nShapeFieldIndex);
            if(!ptrVal.get() || !ptrVal->IsType<CommonLib::IGeoShapePtr>())
                return CommonLib::IGeoShapePtr();

            return ptrVal->Get<CommonLib::IGeoShapePtr>();
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
