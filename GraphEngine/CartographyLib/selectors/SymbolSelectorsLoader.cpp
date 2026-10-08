#include "SymbolSelectorsLoader.h"
#include "SimpleSymbolSelector.h"
#include "UniqueValueSymbolSelector.h"
#include "RangeSymbolSelector.h"

namespace GraphEngine {
    namespace Cartography {

        ISymbolSelectorPtr CSymbolSelectorsLoader::LoadSymbolSelector(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                uint32_t nSelectorID = pObj->GetPropertyInt32U("SymbolSelectorID", UndefineSymbolSelectorID);
                if(nSelectorID == UndefineSymbolSelectorID)
                    throw CommonLib::CExcBase("Undefined symbol selector, type id: {0}", nSelectorID);

                ISymbolSelectorPtr ptrSelector;
                switch(nSelectorID)
                {
                    case SimpleSymbolSelectorID:
                    {
                        ptrSelector = std::make_shared<CSimpleSymbolSelector>();
                    }
                    break;
                    case UniqueValueSymbolSelectorID:
                        ptrSelector = std::make_shared<CUniqueValueSymbolSelector>();
                        break;
                    case RangeSymbolSelectorID:
                        ptrSelector = std::make_shared<CRangeSymbolSelector>();
                        break;
                }

                if(ptrSelector.get() == nullptr)
                    throw CommonLib::CExcBase("Unknown symbol selector, type id: {0}", nSelectorID);

                ptrSelector->Load(pObj);
                return ptrSelector;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load symbol selector", exc);
                throw;
            }
        }

    }
}
