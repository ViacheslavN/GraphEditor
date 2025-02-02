#include "CommonLib.h"
#include "Color.h"
#include "../../CommonLib/exception/exc_base.h"

namespace GraphEngine
{
	namespace Display
	{
		Color::Color(const std::string& rgb, AlfaChannel a)
		{
			int r, g, b;
			scanf_s(rgb.c_str(), "%2X%2X%2X", &r, &g, &b);
			m_rgba = (static_cast<ColorType>(ColorComponent(r))
				| (static_cast<ColorType>(ColorComponent(g)) << 8)
				| (static_cast<ColorType>(ColorComponent(b)) << 16)
				| (static_cast<ColorType>(a) << 24));
		}

		Color::Color(ColorType rgb, AlfaChannel a) :
			m_rgba((rgb & 0xffffff) | (static_cast<ColorType>(a) << 24))
		{
		}

		Color::Color(ColorComponent r, ColorComponent g, ColorComponent b, AlfaChannel a) :
			m_rgba(static_cast<ColorType>(r)
				| (static_cast<ColorType>(g) << 8)
				| (static_cast<ColorType>(b) << 16)
				| (static_cast<ColorType>(a) << 24))
		{
		}

		Color::ColorType Color::GetRGB() const
		{
			return m_rgba & 0x00ffffff;
		}

		Color::ColorType Color::GetRGBA() const
		{
			return m_rgba;
		}

		Color::ColorComponent Color::GetR() const
		{
			return static_cast<Color::ColorComponent>(m_rgba);
		}

		Color::ColorComponent Color::GetG() const
		{
			return static_cast<Color::ColorComponent>(m_rgba >> 8);
		}

		Color::ColorComponent Color::GetB() const
		{
			return static_cast<Color::ColorComponent>(m_rgba >> 16);
		}

		Color::ColorComponent Color::GetA() const
		{
			return static_cast<Color::ColorComponent>(m_rgba >> 24);
		}

		Color Color::RndLineColor()
		{
			Color::ColorComponent rgb[3];
			rgb[0] = 128;
			rgb[1] = rgb[0] - Color::ColorComponent(rand() % 128);
			rgb[2] = 128 - rgb[1];
			return RndColor(rgb);
		}
		Color Color::RndFillColor()
		{
			Color::ColorComponent rgb[3];
			rgb[0] = 255;
			rgb[1] = rgb[0] - Color::ColorComponent(rand() % 80);
			rgb[2] = 255 - (rgb[1] - 175);
			return RndColor(rgb);
		}

		Color Color::RndColor(Color::ColorComponent *rgb)
		{
			int idx[3];
			idx[0] = rand() % 3;
			while ((idx[1] = rand() % 3) == idx[0]);
			idx[2] = 3 - (idx[0] + idx[1]);

			Color::ColorComponent r = rgb[idx[0]];
			Color::ColorComponent g = rgb[idx[1]];
			Color::ColorComponent b = rgb[idx[2]];
			return Color(r, g, b);
		}

		bool Color::operator==(const Color& c) const
		{
			return m_rgba == c.m_rgba;
		}

		bool Color::operator!=(const Color& c) const
		{
			return !(this->operator ==(c));
		}
		Color& Color::operator=(const Color& a)
		{
			m_rgba = a.m_rgba;
			return *this;
		}

		Color& Color::operator=(ColorType rgba)
		{
			m_rgba = rgba;
			return *this;
		}

		void Color::SetRGBA(ColorType rgba)
		{
			m_rgba = rgba;
		}

        void Color::Load(CommonLib::IReadStream *pStream)
        {
            try
            {
                pStream->Read(m_rgba);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load color", exc);
            }

        }


        void Color::Save(CommonLib::IWriteStream *pStream) const
        {
            try
            {
                pStream->Write(m_rgba);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save color", exc);
            }
        }

		void Color::Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const
		{
			try
			{
                CommonLib::ISerializeObjPtr pColorNode = pObj->CreateChildNode(name);
                pColorNode->AddPropertyInt32U("RGBA", m_rgba);
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExc("Failed to save color", exc);
			} 
		}

		void Color::Load(CommonLib::ISerializeObjPtr pObj, const std::string& name)
		{
			try
			{
                CommonLib::ISerializeObjPtr pColorNode = pObj->GetChild(name);
				m_rgba = pColorNode->GetPropertyInt32U("RGBA", m_rgba);
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExc("Failed to load color", exc);
			}
		}
	}
}