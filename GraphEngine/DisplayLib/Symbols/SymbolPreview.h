#pragma once
#include "../Symbols.h"

namespace GraphEngine {
    namespace Display {

        // shape a symbol is drawn on in a preview (legend patch, layer tree, symbol list)
        enum eSymbolPreviewShape
        {
            SymbolPreviewAuto = 0,     // by the symbol type: marker - point, line - line, fill - polygon, text - text
            SymbolPreviewPoint,        // the center of the rect
            SymbolPreviewLine,         // a zig-zag line across the rect
            SymbolPreviewPolygon,      // the rect without a 1 pixel border
            SymbolPreviewText          // "AaBb" in the center
        };

        // Draws a symbol as a legend patch into a rect of a graphics, platform independent
        // (Windows image lists, Android bitmaps through the jni libraries).
        // Sizes of the symbol (mm) are converted by the given resolution; a part out of the rect is clipped.
        class CSymbolPreview
        {
        public:
            static eSymbolPreviewShape GetShape(ISymbolPtr ptrSymbol);

            // copy of the symbol by save / load: a symbol keeps the drawing state of its last Prepare,
            // the symbols of a map are used by the draw thread, the copy can be drawn in another thread
            static ISymbolPtr CloneSymbol(ISymbolPtr ptrSymbol);

            // bClone - draw a copy (CloneSymbol), the symbol isn't changed; without a copy the symbol is prepared for this drawing
            static void Draw(ISymbolPtr ptrSymbol, IGraphicsPtr ptrGraphics, const GRect& rect, double dpi = 96.,
                             eSymbolPreviewShape shape = SymbolPreviewAuto, bool bClone = true);

            // new graphics (Agg) width x height filled with the background color, with the symbol over the whole area
            static IGraphicsPtr CreatePreview(ISymbolPtr ptrSymbol, int nWidth, int nHeight, const Color& background, double dpi = 96.,
                                              eSymbolPreviewShape shape = SymbolPreviewAuto);
        };
    }
}
