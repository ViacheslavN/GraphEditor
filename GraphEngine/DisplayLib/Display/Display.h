#pragma once
#include "../DisplayLib.h"

namespace GraphEngine
{
    namespace Display {

        class CDisplay : public IDisplay
        {
        public:
            CDisplay();
            CDisplay( IDisplayTransformationPtr pTD);
            ~CDisplay();
            virtual void SetClipGeometry( const CommonLib::IGeoShapePtr ptrClipGeom );
            virtual void SetClipRect( const CommonLib::bbox& clipGeom );

            virtual void StartDrawing( IGraphicsPtr ptrGraphics );
            virtual void FinishDrawing();
            virtual IGraphicsPtr GetGraphics();

            virtual IDisplayTransformationPtr GetTransformation();
            virtual void SetTransformation( IDisplayTransformationPtr ptrDisplayTransformation );

            virtual void Lock();
            virtual void UnLock();
        private:
            IGraphicsPtr m_pGraphics;
            IDisplayTransformationPtr m_pDisplayTransformation;
        };

    }

}