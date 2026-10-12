#include "MapTask.h"
#include "MapDrawer.h"
#include "../../DisplayLib/Display/Display.h"

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
