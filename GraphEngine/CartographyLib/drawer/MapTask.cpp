#include "MapTask.h"
#include "MapDrawer.h"
#include "../../DisplayLib/Display/Display.h"
#include "../../DisplayLib/Transformation/DisplayTransformation3D.h"

#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        CMapTask::CMapTask(CMapDrawer* pDrawer) :
                m_pDrawer(pDrawer)
                , m_drawPhase(DrawPhaseNone)
                , m_bDrawing(false)
                , m_backgroundColor(255, 255, 255, 255)
        {
            m_ptrDisplay = std::make_shared<Display::CDisplay>();
            m_ptrTrackCancel = std::make_shared<CTrackCancel>();
        }

        CMapTask::~CMapTask()
        {

        }

        void CMapTask::Init(IMapPtr ptrMap, Display::IDisplayTransformationPtr ptrTrans, eDrawPhase phase, Display::IGraphicsPtr ptrGraphics)
        {
            m_ptrMap = ptrMap;
            m_ptrTransformation = ptrTrans;
            m_drawPhase = phase;
            m_ptrGraphics = ptrGraphics;
        }

        void CMapTask::SetDraw()
        {
            m_ptrTrackCancel->Reset();
        }

        void CMapTask::SetBackgroundColor(const Display::Color& color)
        {
            m_backgroundColor = color;
        }

        namespace
        {
            // the sky above the far edge of the ground in the 3D view: a gradient from the top to the horizon
            void DrawSky(Display::IGraphicsPtr ptrGraphics, Display::IDisplayTransformationPtr ptrTrans)
            {
                Display::CDisplayTransformation3DPtr ptr3D = std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(ptrTrans);
                if(!ptr3D.get())
                    return;

                const Display::GRect& rect = ptr3D->GetDeviceRect();
                double top = (double)rect.yMin;
                double sky = std::ceil(ptr3D->GetSkyLine());
                if(sky <= top)
                    return;

                const Display::Color colorTop(96, 150, 220, 255);
                const Display::Color colorHorizon(222, 234, 248, 255);
                const double band = 4.;
                for (double y = top; y < sky; y += band)
                {
                    double t = (y - top) / (sky - top);
                    Display::Color color((unsigned char)(colorTop.GetR() + (colorHorizon.GetR() - colorTop.GetR()) * t),
                                         (unsigned char)(colorTop.GetG() + (colorHorizon.GetG() - colorTop.GetG()) * t),
                                         (unsigned char)(colorTop.GetB() + (colorHorizon.GetB() - colorTop.GetB()) * t), 255);
                    double yEnd = y + band < sky ? y + band : sky;
                    Display::GRect bandRect(rect.xMin, (Display::GUnits)y, rect.xMax, (Display::GUnits)yEnd);
                    ptrGraphics->Erase(color, &bandRect);
                }
            }
        }

        void CMapTask::Draw()
        {
            if(!m_ptrMap.get() || !m_ptrTransformation.get() || !m_ptrGraphics.get())
                return;

            m_bDrawing = true;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sLastError.clear();
            }

            eDrawPhase phase = GetDrawPhase();
            try
            {
                if(phase & DrawPhaseGeography)
                {
                    m_ptrGraphics->Lock();
                    m_ptrGraphics->Erase(m_backgroundColor);
                    DrawSky(m_ptrGraphics, m_ptrTransformation);
                    m_ptrGraphics->UnLock();
                }

                m_ptrDisplay->SetTransformation(m_ptrTransformation);
                m_ptrDisplay->StartDrawing(m_ptrGraphics);
                m_ptrMap->PartialDraw(phase, m_ptrDisplay, m_ptrTrackCancel);
                m_ptrDisplay->FinishDrawing();
            }
            catch (std::exception& exc)
            {
                m_ptrDisplay->FinishDrawing();
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_sLastError = exc.what();
                }
                m_ptrTrackCancel->Cancel();
            }

            m_bDrawing = false;
            m_pDrawer->OnFinishedDrawMapTask(this, !m_ptrTrackCancel->Continue());
        }

        void CMapTask::SetTrackCancel(bool bSet)
        {
            bSet ? m_ptrTrackCancel->Reset(): m_ptrTrackCancel->Cancel();
        }

        void CMapTask::StopDraw(bool bWait)
        {
            m_ptrTrackCancel->Cancel();
        }

        bool CMapTask::IsDrawing() const
        {
            return m_bDrawing;
        }

        bool CMapTask::IsCanceled()
        {
            return !m_ptrTrackCancel->Continue();
        }

        eDrawPhase CMapTask::GetDrawPhase() const
        {
            return (eDrawPhase)m_drawPhase.load();
        }

        void CMapTask::SetDrawPhase(eDrawPhase phase)
        {
            m_drawPhase = phase;
        }

        std::string CMapTask::GetLastError() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_sLastError;
        }

    }
}
