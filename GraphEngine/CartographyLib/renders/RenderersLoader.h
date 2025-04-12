#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        class  CLoaderRenderers
        {
        public:
            static IFeatureRendererPtr LoadRenderer(CommonLib::ISerializeObjPtr pObj);

        };
    }
    }