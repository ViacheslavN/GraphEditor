#pragma once
#include "../Cartography.h"
#include "DrawTask.h"
#include "TrackCancel.h"

namespace GraphEngine {
    namespace Cartography {

        class CMapDrawer;

        // draws the map (geography + selection) into the map graphics
        class CMapTask : public IDrawTask
        {
        public:

            CMapTask(CMapDrawer* pDrawer);
            virtual ~CMapTask();

            void Init(IMapPtr ptrMap, Display::IDisplayTransformationPtr ptrTrans, eDrawPhase phase, Display::IGraphicsPtr ptrGraphics);
            void SetDraw();
            void SetBackgroundColor(const Display::Color& color);

            virtual void Draw();
            virtual void SetTrackCancel(bool bSet);
            virtual void StopDraw(bool bWait = true);
            virtual bool IsDrawing() const;
            bool IsCanceled();

            eDrawPhase GetDrawPhase() const;
            void SetDrawPhase(eDrawPhase phase);

            std::string GetLastError() const;

        private:
            CMapTask(const CMapTask&);
            CMapTask& operator=(const CMapTask&);

        private:
            CMapDrawer* m_pDrawer;
            IMapPtr m_ptrMap;
            Display::IGraphicsPtr m_ptrGraphics;
            Display::IDisplayTransformationPtr m_ptrTransformation;
            std::atomic<int> m_drawPhase;
            Display::IDisplayPtr m_ptrDisplay;
            CTrackCancelPtr m_ptrTrackCancel;
            std::atomic<bool> m_bDrawing;
            Display::Color m_backgroundColor;
            mutable std::mutex m_mutex; // protects m_sLastError
            std::string m_sLastError;
        };

    }
}
