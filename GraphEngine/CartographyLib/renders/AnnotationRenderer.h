#pragma once
#include "FeatureRendererBase.h"

namespace GraphEngine {
    namespace Cartography {

        // Simple annotation renderer: draws the value of the annotation field right after the feature is drawn.
        // The text symbol is chosen per row by the symbol selector (rows whose symbol isn't a text symbol are skipped).
        // No label cache / conflict detection yet.
        class CAnnotationRenderer : public CFeatureRendererBase<IAnnotationRender>
        {
        public:
            typedef CFeatureRendererBase<IAnnotationRender> TBase;

            CAnnotationRenderer();
            explicit CAnnotationRenderer(ISymbolSelectorPtr ptrSelector);
            virtual ~CAnnotationRenderer();

        private:
            CAnnotationRenderer(const CAnnotationRenderer&);
            CAnnotationRenderer& operator=(const CAnnotationRenderer&);

        public:
            // IFeatureRenderer
            virtual bool                   CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const;
            virtual ISymbolSelectorPtr     GetSymbolSelector() const;
            virtual void                   SetSymbolSelector(ISymbolSelectorPtr ptrSelector);
            virtual void                   DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol = Display::ISymbolPtr());

            // IAnnotationRender
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter, const std::string& annotationName) const;

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            bool GetAnnotationText(GeoDatabase::IRowPtr ptrRow, std::wstring& text) const;

        private:
            ISymbolSelectorPtr      m_ptrSymbolSelector;
            mutable std::string     m_sAnnoField;       // set by PrepareFilter
            mutable int32_t         m_nAnnoFieldIndex;  // resolved on the cursor row
        };

    }
}
