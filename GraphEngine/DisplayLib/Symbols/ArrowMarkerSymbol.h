#pragma once
#include "MarkerSymbolBase.h"

namespace GraphEngine {
    namespace Display {

        // filled triangle arrow (ported from UniGIS ArrowMarkerSymbol): the tip is half of the length ahead of the point,
        // the base of the width is half of the length behind it; rotated by the marker angle as the other markers.
        // Size is the larger of the length and the width, setting it keeps the proportions.
        // ArrowMarkerStylePosition is drawn as the plain arrow (it wasn't implemented in UniGIS either)
        class CArrowMarkerSymbol : public CMarkerSymbolBase<IArrowMarkerSymbol>
        {
        public:
            typedef CMarkerSymbolBase<IArrowMarkerSymbol> TBase;

            CArrowMarkerSymbol();
            CArrowMarkerSymbol(double dLength, double dWidth, const Color& color);
            virtual ~CArrowMarkerSymbol();

            // IMarkerSymbol
            virtual double GetSize() const;
            virtual void   SetSize(double dSize);

            // IArrowMarkerSymbol
            virtual eArrowMarkerStyle GetStyle() const;
            virtual void              SetStyle(eArrowMarkerStyle style);
            virtual double            GetLength() const;
            virtual void              SetLength(double dLength);
            virtual double            GetWidth() const;
            virtual void              SetWidth(double dWidth);

            // ISymbol
            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const;
            virtual void Prepare(IDisplayPtr ptrDisplay);
            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const;
            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            // triangle of the arrow at the device point: tip, base left, base right (after Prepare)
            void GetArrowPoints(const GPoint& point, GPoint* pTriangle) const;

        private:
            eArrowMarkerStyle m_style;
            double   m_dLength;
            double   m_dWidth;
            double   m_dDeviceLength;
            double   m_dDeviceWidth;
            PenPtr   m_ptrPen;
            BrushPtr m_ptrBrush;
        };

    }
}
