#pragma once
#include "GraphTypes.h"
#include "DisplayMath.h"
#include "Bitmap.h"

namespace GraphEngine
{
	namespace Display {
        typedef std::vector<std::pair<GUnits, GUnits> > TPenTemplates;
        class CPen
        {
        public:
            CPen(bool bisNull = false);
            CPen(ePenType penType, const Color& color, GUnits nWidth, eCapType capType, eJoinType joinType,
                 GUnits nOffset, BitmapPtr pTexture);
            ~CPen();

            void SetPenType(ePenType penType);
            void SetColor(const Color& color);
            void SetWidth(GUnits nWidth);
            void SetCapType(eCapType capType);
            void SetJoinType(eJoinType joinType);
            void SetTexture(BitmapPtr pTexture);
            void SetOffset(GUnits nOffset);
            void AddTemplate(GUnits par, GUnits tem);
            void ClearTmplates();


            ePenType GetPenType() const;
            const Color& GetColor() const;
            GUnits GetWidth() const;
            eCapType GetCapType() const;
            eJoinType GetJoinType() const;
            BitmapPtr GetTexture() const;
            GUnits GetOffset() const;
            const TPenTemplates& GetTemplates() const;

            void Save(CommonLib::ISerializeObjPtr pObj, const astr& name) const;
            void Load(CommonLib::ISerializeObjPtr pObj, const astr& name);


        private:

            ePenType  m_type;
            Color    m_color;
            GUnits   m_nWidth;
            eCapType  m_capType;
            eJoinType m_joinType;
            BitmapPtr m_pTexture;
            GUnits    m_nOffset;
            TPenTemplates m_vecTemplates;
         };
    }
}