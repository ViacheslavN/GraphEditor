#include "SymbolsLoader.h"
#include "SimpleFillSymbol.h"
#include "SimpleLineSymbol.h"
#include "SimpleMarketSymbol.h"
#include "TextSymbol.h"
#include "HashLineSymbol.h"
#include "MarkerLineSymbol.h"
#include "LineFillSymbol.h"
#include "MarkerFillSymbol.h"
#include "PictureFillSymbol.h"
#include "PictureMarkerSymbol.h"
#include "CharacterMarkerSymbol.h"
#include "MultiLayerSymbol.h"
#include "ArrowMarkerSymbol.h"


namespace GraphEngine {
    namespace Display {

        ISymbolPtr CSymbolsLoader::LoadSymbol(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                uint32_t nSymbolID = pObj->GetPropertyInt32U("SymbolID", UndefineSymbolID);
                if(nSymbolID == UndefineSymbolID)
                    throw CommonLib::CExcBase("Undefined symbol, type id: {0}", (uint32_t )nSymbolID);

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
                        pSymbol = std::make_shared<CTextSymbol>();
                    }
                        break;
                    case SimpleMarketSymbolID:
                    {
                        pSymbol = std::make_shared<CSimpleMarketSymbol>();
                    }
                    break;
                    case HashLineSymbolID:
                        pSymbol = std::make_shared<CHashLineSymbol>();
                        break;
                    case MarkerLineSymbolID:
                        pSymbol = std::make_shared<CMarkerLineSymbol>();
                        break;
                    case LineFillSymbolID:
                        pSymbol = std::make_shared<CLineFillSymbol>();
                        break;
                    case MarkerFillSymbolID:
                        pSymbol = std::make_shared<CMarkerFillSymbol>();
                        break;
                    case PictureFillSymbolID:
                        pSymbol = std::make_shared<CPictureFillSymbol>();
                        break;
                    case PictureMarkerSymbolID:
                        pSymbol = std::make_shared<CPictureMarkerSymbol>();
                        break;
                    case CharacterMarkerSymbolID:
                        pSymbol = std::make_shared<CCharacterMarkerSymbol>();
                        break;
                    case MultiLayerMarkerSymbolID:
                        pSymbol = std::make_shared<CMultiLayerMarkerSymbol>();
                        break;
                    case MultiLayerLineSymbolID:
                        pSymbol = std::make_shared<CMultiLayerLineSymbol>();
                        break;
                    case MultiLayerFillSymbolID:
                        pSymbol = std::make_shared<CMultiLayerFillSymbol>();
                        break;
                    case ArrowMarkerSymbolID:
                        pSymbol = std::make_shared<CArrowMarkerSymbol>();
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