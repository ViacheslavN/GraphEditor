#include "CharacterMarkerSymbol.h"
#include <cmath>

namespace GraphEngine {
    namespace Display {

        CCharacterMarkerSymbol::CCharacterMarkerSymbol() : m_nCharacterIndex(L'*'), m_dTextShiftX(0.), m_dTextShiftY(0.), m_dHalfDiagonal(0.)
        {
            m_nSymbolID = CharacterMarkerSymbolID;
            m_ptrFont = std::make_shared<CFont>();
            m_ptrDrawFont = std::make_shared<CFont>();
            m_Color = Color(0, 0, 0);
            m_dSize = 4.;
        }

        CCharacterMarkerSymbol::CCharacterMarkerSymbol(const std::string& sFontFace, int nCharacterIndex, double dSize, const Color& color) : CCharacterMarkerSymbol()
        {
            m_ptrFont->SetFace(sFontFace);
            m_nCharacterIndex = nCharacterIndex;
            m_dSize = dSize;
            m_Color = color;
        }

        CCharacterMarkerSymbol::~CCharacterMarkerSymbol()
        {

        }

        FontPtr CCharacterMarkerSymbol::GetFont() const
        {
            return m_ptrFont;
        }

        void CCharacterMarkerSymbol::SetFont(FontPtr ptrFont)
        {
            m_ptrFont = ptrFont.get() ? ptrFont : std::make_shared<CFont>();
            m_bDirty = true;
        }

        int CCharacterMarkerSymbol::GetCharacterIndex() const
        {
            return m_nCharacterIndex;
        }

        void CCharacterMarkerSymbol::SetCharacterIndex(int nIndex)
        {
            m_nCharacterIndex = nIndex;
            m_bDirty = true;
        }

        bool CCharacterMarkerSymbol::CanDraw(CommonLib::IGeoShapePtr ptrShape) const
        {
            if(!ptrShape.get() || ptrShape->GetPointCnt() == 0)
                return false;
            return m_dSize > 0. && m_nCharacterIndex > 0 && m_Color.GetA() != Color::Transparent;
        }

        void CCharacterMarkerSymbol::Prepare(IDisplayPtr ptrDisplay)
        {
            TBase::Prepare(ptrDisplay);

            *m_ptrDrawFont = *m_ptrFont;
            m_ptrDrawFont->SetSize(CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dSize, GetScaleDependent()));
            m_ptrDrawFont->SetColor(m_Color);
            // the graphics rotates glyphs by -orientation (counter clockwise on the device), markers turn clockwise (RotateCoords)
            m_ptrDrawFont->SetOrientation(-m_dDisplayAngle);
            // left / baseline: the graphics doesn't move the text, the character is centered here
            m_ptrDrawFont->SetTextHAlignment(TextHAlignmentLeft);
            m_ptrDrawFont->SetTextVAlignment(TextVAlignmentBaseline);

            GUnits width = 0, height = 0, baseLine = 0;
            IGraphicsPtr ptrGraphics = ptrDisplay->GetGraphics();
            wchar_t szText[2] = {(wchar_t)m_nCharacterIndex, 0};
            if(ptrGraphics.get())
                ptrGraphics->QueryTextMetrics(m_ptrDrawFont, szText, 1, &width, &height, &baseLine);

            // from the center of the character box to its baseline origin: left by the half width,
            // down by the baseline (ascent) less the half height; then rotated with the glyph
            double dx = -(double)width / 2.;
            double dy = (double)baseLine - (double)height / 2.;
            double dRad = DEG2RAD(m_dDisplayAngle);
            m_dTextShiftX = dx * cos(dRad) - dy * sin(dRad);
            m_dTextShiftY = dx * sin(dRad) + dy * cos(dRad);
            m_dHalfDiagonal = sqrt((double)width * width + (double)height * height) / 2.;
        }

        void CCharacterMarkerSymbol::DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
        {
            if(m_nCharacterIndex <= 0)
                return;

            IGraphicsPtr ptrGraphics = ptrDisplay->GetGraphics();
            wchar_t szText[2] = {(wchar_t)m_nCharacterIndex, 0};
            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                for(int i = offset; i < offset + polyCounts[part]; ++i)
                {
                    GPoint pt((GUnits)(points[i].x + m_dDeviceOffsetX + m_dTextShiftX), (GUnits)(points[i].y + m_dDeviceOffsetY + m_dTextShiftY));
                    ptrGraphics->DrawText(m_ptrDrawFont, szText, 1, pt);
                }
                offset += polyCounts[part];
            }
        }

        void CCharacterMarkerSymbol::DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
        }

        void CCharacterMarkerSymbol::QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
        {
            for(int part = 0, offset = 0; part < polyCount; ++part)
            {
                for(int i = offset; i < offset + polyCounts[part]; ++i)
                {
                    double x = points[i].x + m_dDeviceOffsetX;
                    double y = points[i].y + m_dDeviceOffsetY;
                    rect.ExpandRect(GRect((GUnits)(x - m_dHalfDiagonal), (GUnits)(y - m_dHalfDiagonal), (GUnits)(x + m_dHalfDiagonal), (GUnits)(y + m_dHalfDiagonal)));
                }
                offset += polyCounts[part];
            }
        }

        void CCharacterMarkerSymbol::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);
                pObj->AddPropertyInt32("CharacterIndex", m_nCharacterIndex);
                m_ptrFont->Save(pObj, "Font");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CCharacterMarkerSymbol", exc);
            }
        }

        void CCharacterMarkerSymbol::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_nCharacterIndex = pObj->GetPropertyInt32("CharacterIndex", m_nCharacterIndex);
                m_ptrFont = std::make_shared<CFont>();
                m_ptrFont->Load(pObj, "Font");
                m_bDirty = true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CCharacterMarkerSymbol", exc);
            }
        }

    }
}
