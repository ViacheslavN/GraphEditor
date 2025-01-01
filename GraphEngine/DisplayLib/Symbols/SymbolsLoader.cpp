#include "SymbolsLoader.h"
#include "SimpleFillSymbol.h"
#include "SimpleLineSymbol.h"
#include "SimpleMarketSymbol.h"


namespace GraphEngine {
    namespace Display {

        ISymbolPtr CSymbolsLoader::LoadSymbol(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                uint32_t nSymbolID = pObj->GetPropertyInt32U("SymbolID", UndefineSymbolID);
                if(nSymbolID == UndefineSymbolID)
                    return ISymbolPtr();

                ISymbolPtr pSymbol;
                switch(nSymbolID)
                {
                    case SimpleLineSymbolID:
                    {
                        pSymbol = std::make_shared<CSimpleLineSymbol>();

                    }
                    break;
                    case SimpleFillSymbolID:
                    {
                        pSymbol = std::make_shared<CSimpleFillSymbol>();

                    }
                        break;
                    case TextSymbolID:
                    {

                    }
                        break;
                    case SimpleMarketSymbolID:
                    {
                        pSymbol = std::make_shared<CSimpleMarketSymbol>();
                    }
                    break;
                }
                if(pSymbol.get() == nullptr)
                    throw CommonLib::CExcBase("CSymbolsLoader: Unknown symbol id: {0}", nSymbolID);

                pSymbol->Load(pObj);

                return pSymbol;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load symbol", exc);
                throw;
            }
        }



    }
}