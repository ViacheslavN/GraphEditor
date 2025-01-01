#pragma once

#include "../Symbols.h"

namespace GraphEngine {
    namespace Display {

        class CSymbolsLoader
        {
        public:
            CSymbolsLoader(){}
            ~CSymbolsLoader(){}
            static ISymbolPtr LoadSymbol(CommonLib::ISerializeObjPtr pObj);

        };



    }
}