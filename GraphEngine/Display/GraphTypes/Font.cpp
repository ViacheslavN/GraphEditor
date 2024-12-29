
#include "Font.h"

namespace GraphEngine
{
    namespace  Display
    {
        CFont::CFont()
        {
            m_sFace = "Arial";
            m_nCharSet = 0;
            m_color = Color::Black;
            m_nStyle = FontStyleRegular;
            m_dOrientation = 0;
            m_bgColor = Color::Transparent;
            m_vAlignment = TextVAlignmentTop;
            m_hAlignment = TextHAlignmentLeft;
            m_bIsTransparent = true;
            m_nSize = 8;
            m_nHaloSize = 0;
        }
        CFont::CFont(const std::string& sFace, GUnits nSize, short nCharSet, Color color, int  nStyle, double dOrientation,
                     Color    bgColor, bool  bIsTransparent, eTextVAlignment vAlignmen, eTextHAlignment hAlignment, GUnits nHaloSize) :
                m_sFace(sFace), m_nSize(nSize), m_nCharSet(nCharSet), m_nStyle(nStyle), m_color(color), m_dOrientation(dOrientation),
                m_bgColor(bgColor), m_vAlignment(vAlignmen), m_hAlignment(hAlignment), m_bIsTransparent(bIsTransparent),
                m_nHaloSize(nHaloSize)
        {

        }


        void CFont::SetFace(const std::string& sFace)
        {
            m_sFace = sFace;
        }
        void CFont::SetSize(GUnits nSize)
        {
            m_nSize = nSize;
        }
        void CFont::SetColor(const Color& color)
        {
            m_color = color;
        }
        void  CFont::SetCharSet(short nCharSet)
        {
            m_nCharSet = nCharSet;
        }
        void CFont::SetStyle(int nStyle)
        {
            m_nStyle = nStyle;
        }
        void CFont::SetOrientation(double dOrientation)
        {
            m_dOrientation = dOrientation;
        }
        void CFont::SetBgColor(const Color& color)
        {
            m_bgColor = color;
        }
        void CFont::SetTransparent(bool bIsTransparent)
        {
            m_bIsTransparent = bIsTransparent;
        }
        void CFont::SetTextVAlignment(eTextVAlignment vAlignmen)
        {
            m_vAlignment = vAlignmen;
        }
        void CFont::SetTextHAlignment(eTextHAlignment hAlignment)
        {
            m_hAlignment = hAlignment;
        }
        void CFont::SetHaloSize(GUnits nHaloSize)
        {
            m_nHaloSize = nHaloSize;
        }

        const std::string &  CFont::GetFace() const
        {
            return m_sFace;
        }
        GUnits CFont::GetSize() const
        {
            return m_nSize;
        }
        const Color&  CFont::GetColor() const
        {
            return m_color;
        }
        int CFont::GetStyle() const
        {
            return m_nStyle;
        }
        short CFont::GetCharSet() const
        {
            return m_nCharSet;
        }
        GUnits CFont::GetOrientation() const
        {
            return m_dOrientation;
        }
        const Color& CFont::GetBgColor() const
        {
            return m_bgColor;
        }
        bool CFont::GetTransparent() const
        {
            return m_bIsTransparent;
        }
        eTextVAlignment CFont::GetTextVAlignment() const
        {
            return m_vAlignment;
        }
        eTextHAlignment CFont::GetTextHAlignment() const
        {
            return m_hAlignment;
        }
        GUnits CFont::GetHaloSize() const
        {
            return m_nHaloSize;
        }

        void CFont::Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const
        {
            try
            {
                CommonLib::ISerializeObjPtr pFontNode = pObj->CreateChildNode(name);

                pFontNode->AddPropertyString("Face", m_sFace);
                pFontNode->AddPropertyDouble("Size", m_nSize);
                pFontNode->AddPropertyInt16("CharSet", m_nCharSet);
                pFontNode->AddPropertyInt32U("Color", m_color.GetRGBA());
                pFontNode->AddPropertyDouble("Orientation", m_dOrientation);
                pFontNode->AddPropertyInt32U("BgColor", m_bgColor.GetRGBA());
                pFontNode->AddPropertyBool("Transparent", m_bIsTransparent);
                pFontNode->AddPropertyInt16("VAlignment", m_vAlignment);
                pFontNode->AddPropertyInt16("HAlignment", m_hAlignment);
                pFontNode->AddPropertyDouble("HaloSize", m_nHaloSize);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save font", exc);
            }
        }

        void CFont::Load(CommonLib::ISerializeObjPtr pObj, const std::string& name)
        {
            try
            {
                CommonLib::ISerializeObjPtr pFontNode = pObj->GetChild(name);
                m_sFace = pFontNode->GetPropertyString("Face", m_sFace);
                m_nSize = pFontNode->GetPropertyDouble("Size", m_nSize);
                m_nCharSet = pFontNode->GetPropertyInt16("CharSet", m_nCharSet);
                m_color = pFontNode->GetPropertyInt32U("Color", m_color.GetRGBA());
                m_dOrientation = pFontNode->GetPropertyDouble("Orientation", m_dOrientation);
                m_bgColor = pFontNode->GetPropertyInt32U("BgColor", m_bgColor.GetRGBA());
                m_bIsTransparent = pFontNode->GetPropertyBool("Transparent", m_bIsTransparent);
                m_vAlignment = (eTextVAlignment)pFontNode->GetPropertyInt16("VAlignment", m_vAlignment);
                m_hAlignment = (eTextHAlignment)pFontNode->GetPropertyInt16("HAlignment", m_hAlignment);
                m_nHaloSize = pFontNode->GetPropertyDouble("HaloSize", m_nHaloSize);

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load font", exc);
            }
        }
    }
}