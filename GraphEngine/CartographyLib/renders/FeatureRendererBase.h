#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        template< class I>
        class CFeatureRendererBase : public I
        {
        public:
            CFeatureRendererBase() : m_dMinimumScale(0.), m_dMaximumScale(0.), m_nShapeFieldIndex(-1),
                                     m_nFeatureRendererID(UndefineFeatureRendererID)
            {}

            virtual ~CFeatureRendererBase()
            {}

            virtual uint32_t GetFeatureRendererID()  const
            {
                return m_nFeatureRendererID;
            }

            virtual double GetMaximumScale() const
            {
                return m_dMaximumScale;
            }

            virtual void SetMaximumScale(double scale)
            {
                m_dMaximumScale = scale;
            }

            virtual double GetMinimumScale() const
            {
                return m_dMinimumScale;
            }

            virtual void SetMinimumScale(double scale)
            {
                m_dMinimumScale = scale;
            }

            virtual const std::string& GetShapeField() const
            {
                return m_sShapeField;
            }

            virtual void SetShapeField(const std::string& field)
            {
                m_sShapeField = field;
                m_nShapeFieldIndex = -1;
            }

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyInt32U("FeatureRendererID", GetFeatureRendererID());
                pObj->AddPropertyDouble("MinScale", m_dMinimumScale);
                pObj->AddPropertyDouble("MaxScale", m_dMaximumScale);
                pObj->AddPropertyString("ShapeField", m_sShapeField);
            }

            virtual void Load(CommonLib::ISerializeObjPtr pObj)
            {
                m_dMinimumScale = pObj->GetPropertyDouble("MinScale", m_dMinimumScale);
                m_dMaximumScale = pObj->GetPropertyDouble("MaxScale", m_dMaximumScale);
                m_sShapeField = pObj->GetPropertyString("ShapeField", m_sShapeField);
                m_nShapeFieldIndex = -1;
            }

        protected:
            double                    m_dMinimumScale;
            double                    m_dMaximumScale;
            mutable std::string       m_sShapeField;
            mutable int32_t           m_nShapeFieldIndex;
            uint32_t                  m_nFeatureRendererID;
        };

    }
}
