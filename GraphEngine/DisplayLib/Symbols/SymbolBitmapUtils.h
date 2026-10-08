#pragma once
#include "../DisplayLib.h"
#include "../GraphTypes/Bitmap.h"

namespace GraphEngine {
    namespace Display {

        class CSymbolBitmapUtils
        {
        public:
            // converts a symbol bitmap to 32 bpp BGRA (the format the graphics draws and uses as a texture):
            //  1 bpp - set bits get bgColor, others fgColor (as UniGIS mono bitmaps);
            //  other formats - pixels equal to transparencyColor (compared by RGB, used when its alpha isn't 0)
            //  get bgColor, or become transparent when bgColor is transparent
            static BitmapPtr ToARGB(BitmapPtr ptrSource, const Color& fgColor, const Color& bgColor, const Color& transparencyColor);

            // bitmap filled with one color, for tests and defaults
            static BitmapPtr CreateSolid(size_t width, size_t height, const Color& color);
        };

        // origin of the map coordinates on the device: fill patterns are aligned to it so they don't move when the map is panned
        GPoint GetPatternOrigin(IDisplayPtr ptrDisplay);

        // bounding box of the device points, false - no points
        bool GetPointsBounds(const GPoint* points, const int* polyCounts, int polyCount, GRect& rect);

        // clips the graphics by the polygon while it exists, restores the previous clip rect after
        class CPolygonClipGuard
        {
        public:
            CPolygonClipGuard(IGraphicsPtr ptrGraphics, const GPoint* points, const int* polyCounts, int polyCount);
            ~CPolygonClipGuard();
        private:
            CPolygonClipGuard(const CPolygonClipGuard&);
            CPolygonClipGuard& operator=(const CPolygonClipGuard&);

            IGraphicsPtr m_ptrGraphics;
            GRect m_clipRect;
        };

    }
}
