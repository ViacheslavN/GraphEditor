#include "RenderersLoader.h"
#include "FeatureRenderer.h"

namespace GraphEngine {
    namespace Cartography {

        IFeatureRendererPtr CLoaderRenderers::LoadRenderer(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                uint32_t nRendererID = pObj->GetPropertyInt32U("FeatureRendererID", UndefineFeatureRendererID);
                if(nRendererID == UndefineFeatureRendererID)
                    throw CommonLib::CExcBase("Undefined renderer, type id: {0}", nRendererID);

                IFeatureRendererPtr ptrRenderer;
                switch(nRendererID)
                {
                    case SimpleFeatureRendererID:
                    {
                        ptrRenderer = std::make_shared<CFeatureRenderer>();
                    }
                    break;
                }

                if(ptrRenderer.get() == nullptr)
                    throw CommonLib::CExcBase("Unknown renderer, type id: {0}", nRendererID);

                ptrRenderer->Load(pObj);
                return ptrRenderer;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to LoadRenderer", exc);
                throw;
            }
        }

    }
}
