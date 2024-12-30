#include "Brush.h"

namespace GraphEngine {
    namespace Display {

        CBrush::CBrush(bool bIsNull)
        {
            m_type = bIsNull ? BrushTypeNull : BrushTypeSolid;
            m_color = Color::White;
            m_bgColor = Color::Transparent;
        }

        CBrush::CBrush(eBrushType type, const Color& color, const Color& bgColor, BitmapPtr pTexture) :
                m_type(type), m_color(color), m_bgColor(bgColor), m_pTexture(pTexture)
        {

        }

        CBrush::~CBrush()
        {

        }

        void CBrush::SetType(eBrushType type)
        {
            m_type = type;
        }

        void CBrush::SetColor(const Color& color)
        {
            m_color = color;
        }

        void CBrush::SetBgColor(const Color& color)
        {
            m_bgColor = color;
        }

        void CBrush::SetTexture(BitmapPtr pTexture)
        {
            m_pTexture = pTexture;
        }

        eBrushType CBrush::GetType() const
        {
            return m_type;
        }

        const Color& CBrush::GetColor() const
        {
            return m_color;
        }

        const Color& CBrush::GetBgColor() const
        {
            return m_bgColor;
        }

        BitmapPtr CBrush::GetTexture() const
        {
            return m_pTexture;
        }

        void CBrush::Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const
        {
            try
            {
                CommonLib::ISerializeObjPtr pBrushNode = pObj->CreateChildNode(name);

                pBrushNode->AddPropertyInt16U("Type", m_type);
                m_color.Save(pBrushNode, "Color");
                m_bgColor.Save(pBrushNode, "BgColor");

                if(m_pTexture.get() != nullptr)
                    return m_pTexture->Save(pBrushNode, "Texture");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save brush", exc);
            }
        }

        void CBrush::Load(CommonLib::ISerializeObjPtr pObj, const std::string& name)
        {
            try
            {
                CommonLib::ISerializeObjPtr pBrushNode = pObj->GetChild(name);
                m_color.Load(pBrushNode, "Color");
                m_bgColor.Load(pBrushNode, "BgColor");
                m_type = (eBrushType)pBrushNode->GetPropertyInt16("Type", BrushTypeNull);

                if(pBrushNode->IsChildExists("Texture"))
                {
                    m_pTexture = std::make_shared<CBitmap>();
                    m_pTexture->Load(pBrushNode, "Texture");
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load brush", exc);
            }
        }

    }

}