#pragma once

#include "GraphTypes.h"
#include "../../CommonLib/Serialize/SerializeObj.h"
#include "Color.h"
#include "Bitmap.h"


namespace GraphEngine {
    namespace Display {

        class CBrush
        {
        public:
            CBrush(bool bIsNull = false);
            CBrush(eBrushType type, const Color& color, const Color& bgColor, BitmapPtr pTexture);
            ~CBrush();

            void SetType(eBrushType type);
            void SetColor(const Color& color);
            void SetBgColor(const Color& color);
            void SetTexture(BitmapPtr pTexture);


            eBrushType GetType() const;
            const Color& GetColor() const;
            const Color& GetBgColor() const;
            BitmapPtr GetTexture() const;

            void Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const;
            void Load(CommonLib::ISerializeObjPtr pObj, const std::string& name);


        private:
            eBrushType  m_type;
            Color      m_color;
            Color      m_bgColor;
            BitmapPtr   m_pTexture;

        };


    }

}