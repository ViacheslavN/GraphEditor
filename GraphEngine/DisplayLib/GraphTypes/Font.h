#pragma once

#include "GraphTypes.h"
#include "../../CommonLib/Serialize/SerializeObj.h"
#include "Color.h"

namespace GraphEngine
{
    namespace  Display
    {
        typedef std::shared_ptr<class CFont> FontPtr;

        class CFont
        {
        public:
            CFont();
            CFont(const std::string& sFace, GUnits nSize, short nCharSet, Color color, int  nStyle, double dOrientation,
                  Color    bgColor, bool  bIsTransparent, eTextVAlignment vAlignmen, eTextHAlignment hAlignment, GUnits nHaloSize);


            void SetFace(const std::string& sFace);
            void SetSize(GUnits nSize);
            void SetColor(const Color& color);
            void SetCharSet(short nCharSet);
            void SetStyle(int nStyle);
            void SetOrientation(double dOrientation);
            void SetBgColor(const Color& color);
            void SetTransparent(bool bIsTransparent);
            void SetTextVAlignment(eTextVAlignment vAlignmen);
            void SetTextHAlignment(eTextHAlignment hAlignment);
            void SetHaloSize(GUnits nHaloSize);


            const std::string&  GetFace() const;
            GUnits GetSize() const;
            const Color&  GetColor() const;
            short GetCharSet() const;
            int GetStyle() const;
            GUnits GetOrientation() const;
            const Color& GetBgColor() const;
            bool GetTransparent() const;
            eTextVAlignment GetTextVAlignment() const;
            eTextHAlignment GetTextHAlignment() const;
            GUnits GetHaloSize() const;

            void Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const;
            void Load(CommonLib::ISerializeObjPtr pObj, const std::string& name);

        private:
            std::string    m_sFace;
            GUnits         m_nSize;
            short          m_nCharSet;
            Color          m_color;
            int            m_nStyle;
            double         m_dOrientation;
            Color          m_bgColor;
            bool           m_bIsTransparent;
            eTextVAlignment m_vAlignment;
            eTextHAlignment m_hAlignment;
            GUnits         m_nHaloSize;

        };
    }
}