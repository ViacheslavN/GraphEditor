#pragma once
#include "FeatureRendererBase.h"

namespace GraphEngine {
    namespace Cartography {

        // Label renderer (ported from UniGIS LabelRenderer): the text of the label field, the text symbol
        // from the symbol selector (it can depend on the row attributes). AddLabel gives the label to the label drawer
        // of the map which places it without conflicts; DrawFeature draws it at once (no conflict check).
        class CLabelRenderer : public CFeatureRendererBase<ILabelRenderer>
        {
        public:
            typedef CFeatureRendererBase<ILabelRenderer> TBase;

            CLabelRenderer();
            explicit CLabelRenderer(ISymbolSelectorPtr ptrSelector);
            virtual ~CLabelRenderer();

        private:
            CLabelRenderer(const CLabelRenderer&);
            CLabelRenderer& operator=(const CLabelRenderer&);

        public:
            // IFeatureRenderer
            virtual bool                   CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const;
            virtual ISymbolSelectorPtr     GetSymbolSelector() const;
            virtual void                   SetSymbolSelector(ISymbolSelectorPtr ptrSelector);
            virtual void                   DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol = Display::ISymbolPtr());

            // ILabelRenderer
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter, const std::string& labelFieldName) const;
            virtual void                   AddLabel(ILabelDrawerPtr ptrLabelDrawer, GeoDatabase::IRowPtr ptrRow, const SLabelingOptions& options);
            virtual int                    GetClassIndex() const;
            virtual void                   SetClassIndex(int nIndex);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            bool GetLabelText(GeoDatabase::IRowPtr ptrRow, std::wstring& text) const;

        private:
            ISymbolSelectorPtr      m_ptrSymbolSelector;
            int                     m_nClassIndex;
            mutable std::string     m_sLabelField;       // set by PrepareFilter
            mutable int32_t         m_nLabelFieldIndex;  // resolved on the cursor row
        };

    }
}
