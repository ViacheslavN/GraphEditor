#pragma once
#include "DrawTask.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace GraphEngine {
    namespace Cartography {

        // worker thread that runs one draw task at a time (CMapDrawer owns it and the tasks)
        class CDrawThread
        {
        public:
            CDrawThread();
            ~CDrawThread();

            void SetTask(IDrawTask* pTask, bool bDraw = true);
            void StopDraw(bool bClearTask = true, bool bWait = true);
            void StopThread();
            void StartDraw();
            bool IsDraw() const;

        private:
            CDrawThread(const CDrawThread&);
            CDrawThread& operator=(const CDrawThread&);

            void ThreadProc();

        private:
            IDrawTask* m_pTask;
            bool m_bStop;
            bool m_bStartDraw;
            std::atomic<bool> m_bDraw;

            mutable std::mutex m_mutex;          // protects m_pTask, m_bStop, m_bStartDraw
            std::mutex m_drawMutex;              // held while a task is drawing
            std::condition_variable m_workEvent;
            std::thread m_thread;
        };

    }
}
