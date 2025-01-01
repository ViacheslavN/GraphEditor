#include "Display.h"


namespace GraphEngine
{
    namespace Display {

        CDisplay::CDisplay()
        {

        }
        CDisplay::CDisplay( IDisplayTransformationPtr pTD)
        {
            m_pDisplayTransformation = pTD;
        }
        CDisplay::~CDisplay()
        {

        }
        void CDisplay::SetClipGeometry( const CommonLib::IGeoShapePtr clipGeom )
        {

        }
        void CDisplay::SetClipRect( const CommonLib::bbox& clipGeom )
        {

        }

        void CDisplay::StartDrawing( IGraphicsPtr pGraphics )
        {
            m_pGraphics = pGraphics;
            m_pGraphics->StartDrawing();
        }
        void CDisplay::FinishDrawing()
        {
            m_pGraphics->EndDrawing();
            m_pGraphics = nullptr;
        }
        IGraphicsPtr CDisplay::GetGraphics()
        {
            return m_pGraphics;
        }

        IDisplayTransformationPtr CDisplay::GetTransformation()
        {
            return m_pDisplayTransformation;
        }
        void CDisplay::SetTransformation( IDisplayTransformationPtr pDisplayTransformation )
        {
            m_pDisplayTransformation = pDisplayTransformation;
        }

        void CDisplay::Lock()
        {
            if(m_pGraphics.get())
                m_pGraphics->Lock();
        }
        void CDisplay::UnLock()
        {
            if(m_pGraphics.get())
                m_pGraphics->UnLock();
        }
    }

}