#pragma once
#include "../Symbols.h"
#include "../DisplayMath.h"

namespace GraphEngine {
    namespace Display {

        template< class I>
        class CSymbolBase : public I
        {
        public:

            CSymbolBase(): m_bScaleDependent(false), m_bDrawToBuffers(false), m_nSymbolID(UndefineSymbolID),
                           m_dLastScale(0.), m_dLastDpi(0.), m_dLastRotation(0.)
            {

            }
            virtual ~CSymbolBase()
            {

            }

            //ISymbol
            virtual uint32_t GetSymbolID() const
            {
                return m_nSymbolID;
            }
            virtual void Init( IDisplayPtr ptrDisplay )
            {
                if(m_bDirty || !CDisplayMath::Equals(ptrDisplay->GetTransformation()->GetResolution(), m_dLastDpi) || (m_bScaleDependent && !(CDisplayMath::Equals(m_dLastScale, ptrDisplay->GetTransformation()->GetScale()))) ||
                   !CDisplayMath::Equals(m_dLastRotation, ptrDisplay->GetTransformation()->GetRotation()))
                {
                    this->Prepare(ptrDisplay);
                    m_bDirty = false;
                    m_dLastDpi = ptrDisplay->GetTransformation()->GetResolution();
                    m_dLastScale = ptrDisplay->GetTransformation()->GetScale();
                    m_dLastRotation = ptrDisplay->GetTransformation()->GetRotation();
                }
            }
            virtual void Reset(){}

            virtual bool CanDraw(CommonLib::IGeoShapePtr pShape) const
            {
                return pShape->GetPointCnt() > 0;
            }

            virtual void Draw(IDisplayPtr ptrDisplay, CommonLib::IGeoShapePtr ptrShape)
            {
                GPoint* points;
                int*    parts;
                int     count;
                ptrDisplay->GetTransformation()->MapToDevice(ptrShape, &points, &parts, &count);
                if(count > 0)
                {
                    if(!m_bDrawToBuffers)
                    {
                        m_ptrGeom = ptrShape;
                        this->DrawGeometryEx(ptrDisplay, points, parts, count);
                        m_ptrGeom = nullptr;
                    }
                    else
                    {
                        //TO DO Draw To Buffer
                    }
                }
            }
            virtual void FlushBuffers(IDisplayPtr ptrDisplay ,  ITrackCancelPtr ptrTrackCancel)
            {

            }

            virtual void GetBoundaryRect(CommonLib::IGeoShapePtr ptrShape, IDisplayPtr ptrDisplay, GRect &rect) const
            {
                if(!CanDraw(ptrShape))
                    return;

                GPoint* points;
                int*    parts;
                int     count;
                ptrDisplay->GetTransformation()->MapToDevice(ptrShape, &points, &parts, &count);
                if(count > 0)
                    this->QueryBoundaryRectEx(ptrDisplay, points, parts, count, rect);
            }

            bool GetScaleDependent() const
            {
                return m_bScaleDependent;
            }

            void SetScaleDependent(bool flag)
            {
                m_bScaleDependent = flag;
                m_bDirty = true;
            }

            bool GetDrawToBuffers() const
            {
                return m_bDrawToBuffers;
            }
            void SetDrawToBuffers(bool flag)
            {
                m_bDrawToBuffers = flag;
                m_bDirty = true;
            }

            void SetDirty(bool  bDirty)
            {
                m_bDirty = bDirty;
            }

            //Serialize

            void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyInt32U("SymbolID", GetSymbolID());
                pObj->AddPropertyBool("SD", m_bScaleDependent);
                pObj->AddPropertyBool("DB", m_bDrawToBuffers);
            }

            void Load(CommonLib::ISerializeObjPtr pObj)
            {
                m_bScaleDependent = pObj->GetPropertyBool("SD", m_bScaleDependent);
                m_bDrawToBuffers = pObj->GetPropertyBool("DB", m_bDrawToBuffers);
            }

        protected:

            CommonLib::IGeoShapePtr m_ptrGeom;
            bool m_bScaleDependent;
            bool m_bDrawToBuffers;
            uint32_t m_nSymbolID;
            bool m_bDirty;

            double  m_dLastScale;
            double  m_dLastDpi;
            double  m_dLastRotation;


        };

    }
    }