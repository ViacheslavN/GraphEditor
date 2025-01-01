#pragma once
#include "SymbolBase.h"
#include "../DisplayMath.h"
#include "../DisplayUtils.h"

namespace GraphEngine {
    namespace Display {

        template< class I>
        class CMarkerSymbolBase : public CSymbolBase<I>
        {
        public:
            typedef CSymbolBase<I> TSymbolBase;

            CMarkerSymbolBase()
            {
                m_dAngle = 0;
                m_Color = 0;
                m_dSize = 0;
                m_dOffsetX = 0;
                m_dOffsetY = 0;
                m_dDisplayAngle = 0;
                m_bIgnoreRotation = 0;
                m_dDeviceSize = 0.;
                m_dDeviceOffsetX = 0.;
                m_dDeviceOffsetY = 0.;
                m_dDisplayAngle = 0.;
            }
            ~CMarkerSymbolBase(){}



            virtual double GetAngle() const
            {
                return m_dAngle;
            }
            virtual void SetAngle(double dAngle)
            {
                m_dAngle = dAngle;
            }

            virtual Color GetColor() const
            {
                return m_Color;
            }
            virtual void SetColor(const Color &color)
            {
                m_Color = color;
            }

            virtual double GetSize() const
            {
                return m_dSize;
            }
            virtual void SetSize(double size)
            {
                m_dSize = size;
            }

            virtual double GetXOffset() const
            {
                return m_dOffsetX;
            }
            virtual void SetXOffset(double offsetX)
            {
                m_dOffsetX = offsetX;
            }

            virtual double GetYOffset() const
            {
                return m_dOffsetY;
            }
            virtual void   SetYOffset(double offsetY)
            {
                m_dOffsetY = offsetY;
            }
            virtual bool GetIgnoreRotation() const
            {
                return m_bIgnoreRotation;
            }

            virtual void SetIgnoreRotation(bool gnore)
            {
                m_bIgnoreRotation = gnore;
            }

            virtual void Prepare(IDisplayPtr ptrDisplay)
            {
                m_dDeviceSize = (GUnits)floor(CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dSize, this->GetScaleDependent()) / 2.0);
                m_dDeviceOffsetX = CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dOffsetX, this->GetScaleDependent());
                m_dDeviceOffsetY = -CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dOffsetY, this->GetScaleDependent());
                m_dDisplayAngle = m_dAngle + (m_bIgnoreRotation) ? 0 : ptrDisplay->GetTransformation()->GetRotation();
            }

            void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                TSymbolBase::Save(pObj);

                pObj->AddPropertyDouble("Angle", m_dAngle);
                pObj->AddPropertyDouble("Size", m_dSize);
                pObj->AddPropertyDouble("XOffset", m_dOffsetX);
                pObj->AddPropertyDouble("YOffset", m_dOffsetY);
                pObj->AddPropertyBool("IgnoreRotation", m_bIgnoreRotation);
                m_Color.Save(pObj, "Color");

            }

            void Load(CommonLib::ISerializeObjPtr pObj)
            {
                TSymbolBase::Load(pObj);

                m_dAngle = pObj->GetPropertyDouble("Angle", m_dAngle);
                m_dSize = pObj->GetPropertyDouble("Size", m_dSize);
                m_dOffsetX = pObj->GetPropertyDouble("XOffset", m_dOffsetX);
                m_dOffsetY = pObj->GetPropertyDouble("YOffset", m_dOffsetY);
                m_bIgnoreRotation = pObj->GetPropertyBool("IgnoreRotation", m_bIgnoreRotation);
                m_Color.Load(pObj, "Color");
            }


        protected:
            double m_dAngle;
            Color  m_Color;
            double m_dSize;
            double m_dOffsetX;
            double m_dOffsetY;
            double m_dDeviceSize;
            double m_dDeviceOffsetX;
            double m_dDeviceOffsetY;
            double m_dDisplayAngle;
            bool   m_bIgnoreRotation;

        };
    }
 }