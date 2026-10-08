#pragma once
#include "TemplateLineSymbolBase.h"

namespace GraphEngine {
    namespace Display {

        // hashes across the line (ported from UniGIS HashLineSymbol): at the middle of every template mark
        // a segment of the line width length is drawn by the hash symbol, rotated by the angle from the line direction
        class CHashLineSymbol : public CTemplateLineSymbolBase<IHashLineSymbol>
        {
        public:
            typedef CTemplateLineSymbolBase<IHashLineSymbol> TBase;

            CHashLineSymbol();
            CHashLineSymbol(ILineSymbolPtr ptrHashSymbol, double dWidth, double dInterval);
            virtual ~CHashLineSymbol();

            // ILineSymbol
            virtual Color  GetColor() const;        // color of the hash symbol
            virtual void   SetColor(const Color &color);
            virtual double GetWidth() const;        // hash length, symbol units
            virtual void   SetWidth(double dWidth);

            // IHashLineSymbol
            virtual double         GetAngle() const;
            virtual void           SetAngle(double dAngle);
            virtual ILineSymbolPtr GetHashSymbol() const;
            virtual void           SetHashSymbol(ILineSymbolPtr ptrSymbol);

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
            ILineSymbolPtr m_ptrHashSymbol;
            double m_dAngle;
            double m_dWidth;
            GUnits m_dDeviceWidth;
        };

    }
}
