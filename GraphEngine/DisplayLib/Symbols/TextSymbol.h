#pragma once
#include "SymbolBase.h"


namespace GraphEngine {
    namespace Display {

        class CTextSymbol : public CSymbolBase<ITextSymbol>
        {
        public:

            typedef CSymbolBase<ITextSymbol> TBase;
            CTextSymbol();
            virtual ~CTextSymbol();

            virtual GUnits GetAngle() const;
            virtual void   SetAngle( GUnits dAngle );
            virtual Color  GetColor() const;
            virtual void   SetColor(const Color &color );
            virtual  FontPtr GetFont() const;
            virtual void   SetFont( FontPtr ptrFont );
            virtual void GetTextSize(IDisplayPtr ptrDisplay, const std::wstring& szText, GUnits *pxSize , GUnits *pySize, GUnits* baseLine) const;
            virtual GUnits GetSize() const;
            virtual void SetSize(GUnits size);
            virtual  const std::wstring& GetText() const;
            virtual void SetText(const std::wstring& szText );
            virtual ITextBackgroundPtr GetTextBackground() const;
            virtual void SetTextBackground( ITextBackgroundPtr ptrBg );
            virtual int   GetTextDrawFlags() const;
            virtual void  SetTextDrawFlags(int flags);

            virtual bool CanDraw(const CommonLib::IGeoShapePtr ptrGeom) const;
            virtual void  DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, size_t polyCount);
            virtual void  QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, size_t polyCount,   GRect &rect) const;
            virtual void  Prepare(IDisplayPtr ptrDisplay);
            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount );


            //ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            void PolygonCenterPoint(const CommonLib::IGeoShapePtr ptrGeom, CommonLib::GisXYPoint* pt);
            void QueryBoundaryRectEx1(IDisplayPtr ptrDisplay, const GPoint& point, GRect& rect) const;
            void draw_background(IDisplayPtr ptrDisplay, const GPoint& pt);
        private:
            FontPtr m_ptrFont;
            std::wstring m_sText;
            ITextBackgroundPtr m_pTextBg;
            int   m_nTextDrawFlags;
        };


    }
    }
