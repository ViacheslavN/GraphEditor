#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        class CSymbolSelectorsLoader
        {
        public:
            static ISymbolSelectorPtr LoadSymbolSelector(CommonLib::ISerializeObjPtr pObj);
        };

    }
}
