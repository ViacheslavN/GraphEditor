#pragma once
#include "TemplateLineSymbolBase.h"

namespace GraphEngine {
    namespace Display {

        // markers along the line (ported from UniGIS MarkerLineSymbol): a marker at the middle of every template mark,
        // rotated by the line direction (plus the marker own angle)
        class CMarkerLineSymbol : public CTemplateLineSymbolBase<IMarkerLineSymbol>
        {
        public:
            typedef CTemplateLineSymbolBase<IMarkerLineSymbol> TBase;

            CMarkerLineSymbol();
            CMarkerLineSymbol(IMarkerSymbolPtr ptrMarker, double dInterval);
            virtual ~CMarkerLineSymbol();

            // ILineSymbol
            virtual Color  GetColor() const;        // marker color
            virtual void   SetColor(const Color &color);
            virtual double GetWidth() const;        // marker size
            virtual void   SetWidth(double dWidth);

            // IMarkerLineSymbol
            virtual IMarkerSymbolPtr GetMarkerSymbol() const;
            virtual void             SetMarkerSymbol(IMarkerSymbolPtr ptrSymbol);

            // ISymbol
            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const;
            virtual void Prepare(IDisplayPtr ptrDisplay);
            virtual void Reset();
            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const;

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            IMarkerSymbolPtr m_ptrMarker;
            GUnits m_dDeviceMarkerSize;
        };

    }
}
