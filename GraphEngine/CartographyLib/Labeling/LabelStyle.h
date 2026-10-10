#pragma once
#include "../Cartography.h"
#include <unordered_map>

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            // Device font of a text symbol prepared for the current display (size in pixels, the reference scale is applied)
            // and the text metrics. The font is drawn with left / baseline alignment, the label positions are
            // calculated for the baseline start of every text run.
            class CLabelStyle
            {
            public:
                CLabelStyle(Display::ITextSymbolPtr ptrSymbol, Display::IDisplayPtr ptrDisplay);

                bool   IsValid() const { return m_bValid; }
                double Height() const { return m_dHeight; }
                double Ascent() const { return m_dAscent; }
                double Descent() const { return m_dHeight - m_dAscent; }
                double HaloSize() const { return m_dHalo; }

                double TextWidth(const std::wstring& text) const;
                double CharWidth(wchar_t ch) const;

                // draws the run with the start of its baseline at (x, y), angle in degrees (clockwise on the screen),
                // drawFlags - Display::eTextDraw (halo and text can be drawn by separate calls)
                void DrawRun(Display::IGraphicsPtr ptrGraphics, const wchar_t* text, int len, double x, double y, double angle, int drawFlags) const;

            private:
                Display::IGraphicsPtr       m_ptrGraphics;
                Display::FontPtr            m_ptrFont;
                double                      m_dHeight;
                double                      m_dAscent;
                double                      m_dHalo;
                bool                        m_bValid;
                mutable std::unordered_map<wchar_t, double> m_charWidths;
            };

            typedef std::shared_ptr<CLabelStyle> CLabelStylePtr;
        }
    }
}
