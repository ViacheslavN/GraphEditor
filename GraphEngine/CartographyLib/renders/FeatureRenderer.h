#pragma once
#include "FeatureRendererBase.h"

namespace GraphEngine {
    namespace Cartography {

        class CFeatureRenderer : public CFeatureRendererBase<IFeatureRenderer>
        {
        public:
            typedef CFeatureRendererBase<IFeatureRenderer> TBase;

            CFeatureRenderer();
            virtual ~CFeatureRenderer();

        private:
            CFeatureRenderer(const CFeatureRenderer&);
            CFeatureRenderer& operator=(const CFeatureRenderer&);

        public:
            // IFeatureRenderer
            virtual bool                   CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const;
            virtual ISymbolSelectorPtr     GetSymbolSelector() const;
            virtual void                   SetSymbolSelector(ISymbolSelectorPtr ptrSelector);
            virtual void                   DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol = Display::ISymbolPtr());

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            CommonLib::IGeoShapePtr GetShape(GeoDatabase::IRowPtr ptrRow) const;

        private:
            ISymbolSelectorPtr m_ptrSymbolSelector;
        };

    }
}
