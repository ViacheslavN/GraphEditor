#pragma once

#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        class CLayersLoader
        {
        public:

            static ILayerPtr LoadLayer(CommonLib::ISerializeObjPtr pObj);

        };



    }
}