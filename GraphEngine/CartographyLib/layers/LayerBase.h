#pragma once
#include "../Cartography.h"


namespace GraphEngine {
    namespace Cartography {

        template< class I>
        class CLayerBase : public I
        {
        public:

            CLayerBase() : m_bVisible(false), m_dMinimumScale(0.), m_dMaximumScale(0.), m_nLayerSymbolID(UndefineLayerID),
                           m_nCheckCancelStep(100)
            {}

            ~CLayerBase()
            {}

            virtual	uint32_t	GetLayerTypeID() const {return m_nLayerSymbolID;}

            virtual void   Draw(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
            {
                if(!ptrDisplay)
                    return;

                if(!this->IsValid() || !(this->GetSupportedDrawPhases() & phase) || !GetVisible())
                    return;

                double scale = ptrDisplay->GetTransformation()->GetScale();

                double maximumScale = GetMaximumScale();
                double minimumScale = GetMinimumScale();
                if((maximumScale != 0.0 && scale < maximumScale) || (minimumScale != 0.0 && scale > minimumScale))
                    return;

                DrawEx(phase, ptrDisplay, ptrTrackCancel);
            }

            virtual double   GetMaximumScale() const
            {
                return m_dMaximumScale;
            }

            virtual void  SetMaximumScale(double dScale)
            {
                m_dMaximumScale = dScale;
            }

            virtual double   GetMinimumScale() const
            {
                return m_dMinimumScale;
            }

            virtual void  SetMinimumScale(double dScale)
            {
                m_dMinimumScale = dScale;
            }
            virtual const std::string&	GetName() const
            {
                return m_sName;
            }
            virtual void   SetName(const std::string& sName)
            {
                m_sName = sName;
            }
            virtual bool  GetVisible() const
            {
                return m_bVisible;
            }
            virtual void  SetVisible(bool flag)
            {
                m_bVisible = flag;
            }
            virtual bool  IsActiveOnScale(double scale) const
            {
                if((GetMinimumScale() != 0 && GetMinimumScale() < scale) || (GetMaximumScale() != 0 && GetMaximumScale() > scale))
                    return false;

                return true;
            }

            virtual uint32_t	GetCheckCancelStep() const
            {
                return m_nCheckCancelStep;
            }

            virtual void	SetCheckCancelStep(uint32_t nCount)
            {
                m_nCheckCancelStep = nCount;
            }

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyInt32U("LayerTypeID", GetLayerTypeID());
                pObj->AddPropertyString("LayerID", m_guid.ToAstr(true));
                pObj->AddPropertyString("Name", m_sName);
                pObj->AddPropertyBool("Visible", m_bVisible);
                pObj->AddPropertyDouble("MinScale", m_dMinimumScale);
                pObj->AddPropertyDouble("MaxScale", m_dMaximumScale);
                pObj->AddPropertyInt32U("CheckCancelStep", m_nCheckCancelStep);
            }

            virtual void Load(CommonLib::ISerializeObjPtr pObj)
            {
                m_nLayerSymbolID =  pObj->GetPropertyInt32U("LayerTypeID");
                m_guid.FromAstr(pObj->GetPropertyString("LayerID"));
                m_sName = pObj->GetPropertyString("Name", m_sName);
                m_bVisible = pObj->GetPropertyBool("Visible", m_bVisible);
                m_dMinimumScale = pObj->GetPropertyDouble("MinScale", m_dMinimumScale);
                m_dMaximumScale = pObj->GetPropertyDouble("MaxScale", m_dMaximumScale);
                m_nCheckCancelStep = pObj->GetPropertyInt32U("CheckCancelStep", m_nCheckCancelStep);
            }

        protected:
            virtual void   DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel) = 0;
        protected:
            std::string                        m_sName;
            Geometry::ISpatialReferencePtr    m_pSpatialRef;
            bool                              m_bVisible;
            double                            m_dMinimumScale;
            double                            m_dMaximumScale;
            uint32_t						  m_nLayerSymbolID;
            uint32_t						  m_nCheckCancelStep;
            CommonLib::CGuid                  m_guid;
        };
    }
}