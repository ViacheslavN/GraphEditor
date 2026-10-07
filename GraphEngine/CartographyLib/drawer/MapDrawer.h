#pragma once
#include "../Cartography.h"
#include "DrawThread.h"
#include "MapTask.h"

namespace GraphEngine {
    namespace Cartography {

        // Draws the map in a background thread (CDrawThread + CMapTask) into an off-screen graphics.
        // While drawing, OnInvalidate is fired periodically so the window can show the progress;
        // the window paints with Update(). Pan moves the last picture until StopPan, then redraws.
        // Ported from GisFramework::CMapDrawer of the old engine.
        class CMapDrawer : public IMapDrawer
        {
        public:
            CMapDrawer(double dpi = 96.);
            virtual ~CMapDrawer();

            virtual Display::IDisplayTransformationPtr GetTransformation() const;
            virtual Display::IDisplayTransformationPtr GetCalcTransformation() const;
            virtual Display::IGraphicsPtr GetMapGraphics() const;
            virtual Display::IGraphicsPtr GetLabelGraphics() const;
            virtual Display::IGraphicsPtr GetOutGraphics() const;

            virtual IMapPtr GetMap() const;
            virtual void SetMap(IMapPtr ptrMap);

            virtual void   SetResolution(double dpi);
            virtual double GetResolution() const;
            virtual void   SetBackgroundColor(const Display::Color& color);

            virtual void SetSize(int cx , int cy, bool bDraw = true);
            virtual void Update(Display::IGraphicsPtr ptrGraphics, const Display::GPoint *pPoint, const Display::GRect* pRect);
            virtual void Redraw(Display::IGraphicsPtr ptrGraphics = Display::IGraphicsPtr());
            virtual bool IsDrawing() const;
            virtual std::string GetLastError() const;

            virtual void ZoomIn(const Display::GRect& rect);
            virtual void ZoomIn(const CommonLib::bbox& bb);
            virtual void ZoomToFullExtent();
            virtual void SetScale(double scale);

            virtual void   Set3DMode(bool b3D);
            virtual bool   Is3DMode() const;
            virtual void   SetTilt(double degrees);
            virtual double GetTilt() const;
            virtual void   SetRotation(double degrees);
            virtual double GetRotation() const;

            virtual void StartPan(const Display::GPoint& pt);
            virtual void MovePan(const Display::GPoint& pt);
            virtual void StopPan(const Display::GPoint& pt);
            virtual void StopDraw(bool bWait = true);

            virtual void SetOnInvalidate(OnInvalidate* pFunck, bool bAdd);
            virtual void SetOnFinishMapDrawing(OnFinishMapDrawing* pFunck, bool bAdd);

            // period of OnInvalidate while the map is being drawn, 0 - only when finished
            void SetProgressInterval(uint32_t nMilliseconds);

            // called by CMapTask from the draw thread
            void OnFinishedDrawMapTask(CMapTask *pTask, bool bCanceled);

        private:
            CMapDrawer(const CMapDrawer&);
            CMapDrawer& operator=(const CMapDrawer&);

            void Init(bool bResetTransformation);
            Display::IDisplayTransformationPtr CreateTransformation(const Display::GRect& wndRect) const;
            void CopyTrans();
            void StartTimer();
            void StopTimer();
            void TimerProc();
            void FireInvalidate();

            void AddFlags(uint32_t add_flag = 0, uint32_t remove_flag = 0);
            void SetFlag(uint32_t flag);
            bool IsFlag(uint32_t flag) const;

        private:
            Display::IDisplayTransformationPtr m_ptrDispTran;
            Display::IDisplayTransformationPtr m_ptrDispCalcTran;
            Display::IGraphicsPtr m_ptrMapGraphics;
            Display::IGraphicsPtr m_ptrLabelGraphics;
            Display::IGraphicsPtr m_ptrOutGraphics;
            IMapPtr m_ptrMap;

            double m_dDpi;
            bool m_b3D;
            double m_dTilt;
            uint32_t m_nWidth;
            uint32_t m_nHeight;
            uint32_t m_nFlags;
            Display::Color m_backgroundColor;
            Display::GPoint m_panStart;
            Display::GPoint m_panOffset;
            mutable std::recursive_mutex m_mutex;

            CommonLib::Event3<const Display::GPoint*, const Display::GRect*, bool> m_OnInvalidateEvent;
            CommonLib::Event1<bool> m_OnFinishMapDrawingEvent;

            // progress timer
            uint32_t m_nProgressInterval;
            bool m_bTimerActive;
            bool m_bTimerStop;
            std::mutex m_timerMutex;
            std::condition_variable m_timerEvent;
            std::thread m_timerThread;

            // order matters: the thread is destroyed (joined) before the task
            CMapTask m_mapTask;
            CDrawThread m_drawThread;
        };

        typedef std::shared_ptr<CMapDrawer> CMapDrawerPtr;
    }
}
