#pragma once
#include "SymbolFillBase.h"

namespace GraphEngine {
    namespace Display {

        // polygon filled with markers (ported from UniGIS MarkerFillSymbol): a grid aligned to the map origin,
        // or the same grid with every marker moved by a stable pseudo random shift (random style)
        class CMarkerFillSymbol : public CSymbolFillBase<IMarkerFillSymbol>
        {
        public:
            typedef CSymbolFillBase<IMarkerFillSymbol> TBase;

            CMarkerFillSymbol();
            CMarkerFillSymbol(IMarkerSymbolPtr ptrMarker, double dXSeparation, double dYSeparation);
            virtual ~CMarkerFillSymbol();

            // IFillSymbol
            virtual Color GetColor() const;           // marker color
            virtual void  SetColor(const Color &color);
            virtual void  FillRect(IDisplayPtr ptrDisplay, const GRect& rect);

            // IMarkerFillSymbol
            virtual IMarkerSymbolPtr GetMarkerSymbol() const;
            virtual void             SetMarkerSymbol(IMarkerSymbolPtr ptrMarker);
            virtual eMarkerFillStyle GetStyle() const;
            virtual void             SetStyle(eMarkerFillStyle style);
            virtual double GetXSeparation() const;
            virtual void   SetXSeparation(double sep);
            virtual double GetYSeparation() const;
            virtual void   SetYSeparation(double sep);
            virtual double GetXOffset() const;
            virtual void   SetXOffset(double offset);
            virtual double GetYOffset() const;
            virtual void   SetYOffset(double offset);

            // ISymbol
            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const;
            virtual void Prepare(IDisplayPtr ptrDisplay);
            virtual void Reset();
            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const;
            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            void DrawMarkers(IDisplayPtr ptrDisplay, const GRect& bounds);

        private:
            IMarkerSymbolPtr m_ptrMarker;
            eMarkerFillStyle m_style;
            double m_dXSeparation;
            double m_dYSeparation;
            double m_dXOffset;
            double m_dYOffset;
            double m_dDeviceXSeparation;
            double m_dDeviceYSeparation;
            double m_dDeviceXOffset;
            double m_dDeviceYOffset;
            double m_dDeviceMarkerSize;
        };

    }
}
