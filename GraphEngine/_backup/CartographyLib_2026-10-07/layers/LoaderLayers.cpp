#include "LoaderLayers.h"
#include "FeatureLayer.h"

namespace GraphEngine {
    namespace Cartography {

       ILayerPtr CLayersLoader::LoadLayer(CommonLib::ISerializeObjPtr pObj)
       {
           try
           {
               uint32_t nLayerID = pObj->GetPropertyInt32U("LayerTypeID", UndefineLayerID);
               if (nLayerID == UndefineLayerID)
                   throw CommonLib::CExcBase("Undefined, type id: {0}", (uint32_t) nLayerID);

               ILayerPtr ptrLayer;
               switch (nLayerID) {
                   case FeatureLayerID: {
                       ptrLayer = std::make_shared<CFeatureLayer>();

                   }
                       break;
               }

               if (ptrLayer.get() == nullptr)
                   throw CommonLib::CExcBase("Unknown layer, type id: {0}", nLayerID);

              ptrLayer->Load(pObj);
              return ptrLayer;

           }
           catch (std::exception& exc)
           {
               CommonLib::CExcBase::RegenExc("Failed to load layer", exc);
               throw;
           }
       }




    }
}