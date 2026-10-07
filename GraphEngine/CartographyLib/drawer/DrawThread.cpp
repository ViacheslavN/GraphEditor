#include "DrawThread.h"

namespace GraphEngine {
    namespace Cartography {

        CDrawThread::CDrawThread() :
                m_pTask(nullptr)
                , m_bStop(false)
                , m_bStartDraw(false)
                , m_bDraw(false)
        {
            m_thread = std::thread(&CDrawThread::ThreadProc, this);
        }

        CDrawThread::~CDrawThread()
        {
            StopThread();
        }

        void CDrawThread::SetTask(IDrawTask* pTask, bool bDraw)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_pTask = pTask;
            }

            if(bDraw)
                StartDraw();
        }

        void CDrawThread::StopDraw(bool bClearTask, bool bWait)
        {
            IDrawTask* pTask = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_bStartDraw = false;
                pTask = m_pTask;
            }

            if(pTask)
                pTask->StopDraw(bWait);

            if(bWait)
            {
                // wait until the current Draw() returns
                std::lock_guard<std::mutex> drawLock(m_drawMutex);
            }

            if(bClearTask)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_pTask = nullptr;
            }
        }

        void CDrawThread::StopThread()
        {
            if(!m_thread.joinable())
                return;

            StopDraw(true, true);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_bStop = true;
            }
            m_workEvent.notify_all();
            m_thread.join();
        }

        void CDrawThread::StartDraw()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_bStartDraw = true;
            }
            m_workEvent.notify_all();
        }

        bool CDrawThread::IsDraw() const
        {
            return m_bDraw;
        }

        void CDrawThread::ThreadProc()
        {
            while (true)
            {
                IDrawTask* pTask = nullptr;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_workEvent.wait(lock, [this]{return m_bStop || m_bStartDraw;});
                    if(m_bStop)
                        break;

                    m_bStartDraw = false;
                    pTask = m_pTask;
                    if(!pTask)
                        continue;

                    m_bDraw = true;
                    m_drawMutex.lock(); // taken before m_mutex is released: StopDraw(bWait) can't miss this draw
                }

                try
                {
                    pTask->Draw();
                }
                catch (...)
                {
                    // the task reports its own errors, the thread must survive
                }

                m_bDraw = false;
                m_drawMutex.unlock();
            }
        }

    }
}
