#pragma once
#include "SymbolFillBase.h"

namespace GraphEngine {
    namespace Display {

        // polygon filled with a tiled bitmap (ported from UniGIS PictureFillSymbol);
        // the tiles are aligned to the map origin, mono bitmaps are drawn with the fill color and the background color
        class CPictureFillSymbol : public CSymbolFillBase<IPictureFillSymbol>
        {
        public:
            typedef CSymbolFillBase<IPictureFillSymbol> TBase;

            CPictureFillSymbol();
            explicit CPictureFillSymbol(BitmapPtr ptrBitmap);
            virtual ~CPictureFillSymbol();

            // IFillSymbol
            virtual void  SetColor(const Color &color);
            virtual void  FillRect(IDisplayPtr ptrDisplay, const GRect& rect);

            // IPictureFillSymbol
            virtual Color     GetBitmapTransparencyColor() const;   // alpha 0 - no transparency color
            virtual void      SetBitmapTransparencyColor(const Color &color);
            virtual Color     GetBackgroundColor() const;
            virtual void      SetBackgroundColor(const Color &color);
            virtual BitmapPtr GetBitmap() const;
            virtual void      SetBitmap(BitmapPtr ptrBitmap);

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

            BitmapPtr GetDrawBitmap() const { return m_ptrDrawBitmap; }  // converted bitmap, after Prepare

        private:
            void SetBitmapDirty();

        private:
            BitmapPtr m_ptrBitmap;       // source bitmap
            BitmapPtr m_ptrDrawBitmap;   // 32 bpp texture made by Prepare
            Color     m_bgColor;
            Color     m_transparencyColor;
            PenPtr    m_ptrNullPen;
            bool      m_bBitmapDirty;
        };

    }
}
