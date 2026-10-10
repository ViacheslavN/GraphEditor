#include "LabelStyle.h"
#include "../../DisplayLib/DisplayUtils.h"

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            CLabelStyle::CLabelStyle(Display::ITextSymbolPtr ptrSymbol, Display::IDisplayPtr ptrDisplay) :
                    m_dHeight(0.), m_dAscent(0.), m_dHalo(0.), m_bValid(false)
            {
                if(!ptrSymbol.get() || !ptrDisplay.get() || !ptrSymbol->GetFont().get())
                    return;

                m_ptrGraphics = ptrDisplay->GetGraphics();
                if(!m_ptrGraphics.get())
                    return;

                // own copy of the font: the symbol isn't changed (the reference engine changed the alignment of the symbol)
                Display::IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
                m_ptrFont = std::make_shared<Display::CFont>(*ptrSymbol->GetFont());
                m_ptrFont->SetSize(Display::CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, ptrSymbol->GetFont()->GetSize(), ptrSymbol->GetScaleDependent()));
                if(m_ptrFont->GetHaloSize() != 0)
                    m_ptrFont->SetHaloSize(Display::CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, ptrSymbol->GetFont()->GetHaloSize(), ptrSymbol->GetScaleDependent()));
                m_ptrFont->SetOrientation(0);
                m_ptrFont->SetTextHAlignment(Display::TextHAlignmentLeft);
                m_ptrFont->SetTextVAlignment(Display::TextVAlignmentBaseline);

                if(m_ptrFont->GetSize() <= 0 || m_ptrFont->GetColor().GetA() == Display::Color::Transparent)
                    return;

                Display::GUnits height = 0, baseLine = 0, lineSpacing = 0;
                m_ptrGraphics->QueryTextMetrics(m_ptrFont, &height, &baseLine, &lineSpacing);
                m_dHeight = height;
                m_dAscent = baseLine;
                m_dHalo = m_ptrFont->GetHaloSize() > 0 ? m_ptrFont->GetHaloSize() : 0.;
                m_bValid = m_dHeight > 0.;
            }

            double CLabelStyle::TextWidth(const std::wstring& text) const
            {
                if(text.empty())
                    return 0.;

                Display::GUnits width = 0, height = 0, baseLine = 0;
                m_ptrGraphics->QueryTextMetrics(m_ptrFont, text.c_str(), (int)text.length(), &width, &height, &baseLine);
                return width;
            }

            double CLabelStyle::CharWidth(wchar_t ch) const
            {
                std::unordered_map<wchar_t, double>::const_iterator it = m_charWidths.find(ch);
                if(it != m_charWidths.end())
                    return it->second;

                Display::GUnits width = 0, height = 0, baseLine = 0;
                m_ptrGraphics->QueryTextMetrics(m_ptrFont, &ch, 1, &width, &height, &baseLine);
                m_charWidths[ch] = width;
                return width;
            }

            void CLabelStyle::DrawRun(Display::IGraphicsPtr ptrGraphics, const wchar_t* text, int len, double x, double y, double angle, int drawFlags) const
            {
                // the label angles are clockwise on the screen (y goes down), the font orientation is counter clockwise
                m_ptrFont->SetOrientation(-angle);
                ptrGraphics->DrawText(m_ptrFont, text, len, Display::GPoint((Display::GUnits)x, (Display::GUnits)y), drawFlags);
                m_ptrFont->SetOrientation(0);
            }

        }
    }
}
