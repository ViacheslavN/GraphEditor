#pragma once
#include "MarkerSymbolBase.h"

namespace GraphEngine {
    namespace Display {

        // marker drawn as one character of a font, e.g. a symbol font (ported from UniGIS CharacterMarkerSymbol);
        // the font gives the face and style, the size and the color are the marker ones, the character is centered at the point
        class CCharacterMarkerSymbol : public CMarkerSymbolBase<ICharacterMarkerSymbol>
        {
        public:
            typedef CMarkerSymbolBase<ICharacterMarkerSymbol> TBase;

            CCharacterMarkerSymbol();
            CCharacterMarkerSymbol(const std::string& sFontFace, int nCharacterIndex, double dSize, const Color& color);
            virtual ~CCharacterMarkerSymbol();

            // ICharacterMarkerSymbol
            virtual FontPtr GetFont() const;
            virtual void    SetFont(FontPtr ptrFont);
            virtual int     GetCharacterIndex() const;   // unicode code of the character
            virtual void    SetCharacterIndex(int nIndex);

            // ISymbol
            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const;
            virtual void Prepare(IDisplayPtr ptrDisplay);
            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const;
            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            FontPtr m_ptrFont;       // face, style, charset
            FontPtr m_ptrDrawFont;   // device font made by Prepare
            int     m_nCharacterIndex;
            double  m_dTextShiftX;   // from the point to the text origin (baseline, left), rotated
            double  m_dTextShiftY;
            double  m_dHalfDiagonal;
        };

    }
}
