#pragma once

namespace GraphEngine {
    namespace Cartography {

        // a unit of work executed by CDrawThread
        class IDrawTask
        {
        public:
            IDrawTask(){}
            virtual ~IDrawTask(){}
            virtual void Draw() = 0;
            virtual void StopDraw(bool bWait = true) = 0;
            virtual void SetTrackCancel(bool bSet) = 0;
        };

    }
}
