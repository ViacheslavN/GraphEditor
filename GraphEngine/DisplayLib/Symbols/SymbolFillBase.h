#pragma once

#include "SymbolsLoader.h"
#include "SymbolBase.h"


namespace GraphEngine {
    namespace Display {


        template< class I>
        class CSymbolFillBase : public CSymbolBase<I>
        {
        public:
            typedef CSymbolBase<I> TSymbolBase;

            CSymbolFillBase(){

                m_ptrBrush = std::make_shared<CBrush>();
            }

            virtual ~CSymbolFillBase(){}

            virtual ILineSymbolPtr GetOutlineSymbol() const
            {
                return m_ptrBorderSymbol;
            }

            virtual void SetOutlineSymbol(ILineSymbolPtr ptrLine)
            {
                m_ptrBorderSymbol = ptrLine;
                this->m_bDirty = true;
            }

            virtual Color GetColor() const
            {
                return m_ptrBrush->GetColor();
            }

            virtual void  SetColor(const Color &color)
            {
                m_ptrBrush->SetColor(color);
                this->m_bDirty = true;
            }

            virtual void  Prepare(IDisplayPtr ptrDisplay){}

            void DrawOutline(IDisplayPtr  ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
            {
                if (m_ptrBorderSymbol.get())
                    m_ptrBorderSymbol->DrawGeometryEx(ptrDisplay, points, polyCounts, polyCount);
            }

            void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                TSymbolBase::Save(pObj);

                if(m_ptrBorderSymbol.get())
                {
                    CommonLib::ISerializeObjPtr ptrSymbolObj =  pObj->CreateChildNode("BorderSymbol");
                    m_ptrBorderSymbol->Save(ptrSymbolObj);
                }

                m_ptrBrush->Save(pObj, "Brush");
            }

            void Load(CommonLib::ISerializeObjPtr pObj)
            {

                TSymbolBase::Load(pObj);
                if(pObj->IsChildExists("BorderSymbol"))
                {
                    CommonLib::ISerializeObjPtr ptrSymbolObj = pObj->GetChild("BorderSymbol");
                    m_ptrBorderSymbol = std::dynamic_pointer_cast<ILineSymbol>(CSymbolsLoader::LoadSymbol(ptrSymbolObj));
                }

                m_ptrBrush->Load(pObj, "Brush");
            }


        protected:
            ILineSymbolPtr m_ptrBorderSymbol;
            BrushPtr m_ptrBrush;

        };

    }
    }

