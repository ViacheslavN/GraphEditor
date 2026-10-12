#include "RenderersLoader.h"

namespace GraphEngine {
    namespace Cartography {

        IFeatureRendererPtr CLoaderRenderers::LoadRenderer(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                throw std::exception("Not implement");
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to LoadRenderer", exc);
            }
        }

    }
}