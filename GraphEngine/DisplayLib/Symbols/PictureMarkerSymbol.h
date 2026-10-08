#pragma once
#include "MarkerSymbolBase.h"

namespace GraphEngine {
    namespace Display {

        // bitmap marker (ported from UniGIS PictureMarkerSymbol): the bitmap is scaled to the marker size
        // (the larger side), or drawn pixel to pixel with DrawExact; rotated by the marker angle
        class CPictureMarkerSymbol : public CMarkerSymbolBase<IPictureMarkerSymbol>
        {
        public:
            typedef CMarkerSymbolBase<IPictureMarkerSymbol> TBase;

            CPictureMarkerSymbol();
            CPictureMarkerSymbol(BitmapPtr ptrBitmap, double dSize);
            virtual ~CPictureMarkerSymbol();

            // IMarkerSymbol
            virtual void SetColor(const Color &color);   // foreground of mono bitmaps

            // IPictureMarkerSymbol
            virtual Color     GetBitmapTransparencyColor() const;   // alpha 0 - no transparency color
            virtual void      SetBitmapTransparencyColor(const Color &color);
            virtual BitmapPtr GetBitmap() const;
            virtual void      SetBitmap(BitmapPtr ptrBitmap);
            virtual Color     GetBackgroundColor() const;
            virtual void      SetBackgroundColor(const Color &color);
            virtual bool      GetDrawExact() const;
            virtual void      SetDrawExact(bool bDrawExact);

            // ISymbol
            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const;
            virtual void Prepare(IDisplayPtr ptrDisplay);
            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const;
            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            BitmapPtr GetDrawBitmap() const { return m_ptrDrawBitmap; }  // converted bitmap, after Prepare
            double    GetDrawScale() const { return m_dScale; }          // bitmap scale, after Prepare

        private:
            BitmapPtr m_ptrBitmap;
            BitmapPtr m_ptrDrawBitmap;
            Color     m_bgColor;
            Color     m_transparencyColor;
            bool      m_bDrawExact;
            bool      m_bBitmapDirty;
            double    m_dScale;
        };

    }
}
