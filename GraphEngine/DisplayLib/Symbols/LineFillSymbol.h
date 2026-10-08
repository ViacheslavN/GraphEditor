#pragma once
#include "SymbolFillBase.h"

namespace GraphEngine {
    namespace Display {

        // polygon filled with parallel lines (ported from UniGIS LineFillSymbol);
        // lines are drawn by the line symbol, clipped by the polygon and aligned to the map origin
        class CLineFillSymbol : public CSymbolFillBase<ILineFillSymbol>
        {
        public:
            typedef CSymbolFillBase<ILineFillSymbol> TBase;

            CLineFillSymbol();
            CLineFillSymbol(ILineSymbolPtr ptrLine, double dAngle, double dSeparation);
            virtual ~CLineFillSymbol();

            // IFillSymbol
            virtual Color GetColor() const;          // color of the line symbol
            virtual void  SetColor(const Color &color);
            virtual void  FillRect(IDisplayPtr ptrDisplay, const GRect& rect);

            // ILineFillSymbol
            virtual ILineSymbolPtr GetLineSymbol() const;
            virtual void           SetLineSymbol(ILineSymbolPtr ptrLine);
            virtual double         GetAngle() const;       // degrees, counter clockwise from the x axis
            virtual void           SetAngle(double dAngle);
            virtual double         GetOffset() const;      // symbol units, shift of the lines across their direction
            virtual void           SetOffset(double dOffset);
            virtual double         GetSeparation() const;  // symbol units, distance between the lines
            virtual void           SetSeparation(double dSeparation);

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
            void DrawLines(IDisplayPtr ptrDisplay, const GRect& bounds);

        private:
            ILineSymbolPtr m_ptrLine;
            double m_dAngle;
            double m_dOffset;
            double m_dSeparation;
            double m_dDeviceSeparation;
            double m_dDeviceOffset;
        };

    }
}
