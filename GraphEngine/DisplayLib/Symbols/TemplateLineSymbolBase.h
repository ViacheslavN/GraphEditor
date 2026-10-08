#pragma once
#include "SymbolBase.h"
#include "LineTemplate.h"
#include "../DisplayUtils.h"

namespace GraphEngine {
    namespace Display {

        // common part of the hash / marker line symbols: template and offset
        template<class I>
        class CTemplateLineSymbolBase : public CSymbolBase<I>
        {
        public:
            typedef CSymbolBase<I> TSymbolBase;

            CTemplateLineSymbolBase() : m_dOffset(0.), m_dDeviceOffset(0.)
            {
                m_ptrTemplate = std::make_shared<CLineTemplate>();
            }

            virtual ~CTemplateLineSymbolBase(){}

            virtual LineTemplatePtr GetTemplate() const
            {
                return m_ptrTemplate;
            }

            virtual void SetTemplate(LineTemplatePtr ptrTemplate)
            {
                m_ptrTemplate = ptrTemplate;
                this->m_bDirty = true;
            }

            virtual double GetOffset() const
            {
                return m_dOffset;
            }

            virtual void SetOffset(double dOffset)
            {
                m_dOffset = dOffset;
                this->m_bDirty = true;
            }

            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const
            {
                return ptrShape.get() && ptrShape->GetPointCnt() > 1 && m_ptrTemplate.get();
            }

            virtual void Prepare(IDisplayPtr ptrDisplay)
            {
                IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
                if(m_ptrTemplate.get())
                    m_ptrTemplate->Prepare(ptrTrans, this->GetScaleDependent());
                m_dDeviceOffset = CDisplayUtils::SymbolSizeToDeviceSize(ptrTrans, m_dOffset, this->GetScaleDependent());
            }

            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
            {
                this->DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
            }

            void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                TSymbolBase::Save(pObj);
                pObj->AddPropertyDouble("Offset", m_dOffset);
                if(m_ptrTemplate.get())
                    m_ptrTemplate->Save(pObj, "Template");
            }

            void Load(CommonLib::ISerializeObjPtr pObj)
            {
                TSymbolBase::Load(pObj);
                m_dOffset = pObj->GetPropertyDouble("Offset", m_dOffset);
                m_ptrTemplate = std::make_shared<CLineTemplate>();
                m_ptrTemplate->Load(pObj, "Template");
                this->m_bDirty = true;
            }

        protected:
            LineTemplatePtr m_ptrTemplate;
            double          m_dOffset;
            GUnits          m_dDeviceOffset;
        };

    }
}
