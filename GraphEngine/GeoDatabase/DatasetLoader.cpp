#include "DatasetLoader.h"

namespace GraphEngine {
    namespace GeoDatabase {

    ITablePtr CDatasetLoader::LoadTable(CommonLib::ISerializeObjPtr pObj)
    {
        try
        {
            throw std::exception("Not implement");
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to LoadTable", exc);
            throw;
        }
    }

    }
}