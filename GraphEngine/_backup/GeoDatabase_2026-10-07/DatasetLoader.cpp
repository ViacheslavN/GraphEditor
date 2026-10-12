#include "DatasetLoader.h"

namespace GraphEngine {
    namespace GeoDatabase {

    ITablePtr CDatasetLoader::LoadTable(CommonLib::ISerializeObjPtr ptrObj)
    {
        try
        {
            CommonLib::CGuid wksId = ptrObj->GetPropertyGuid("WorkspaceId");
            eDatasetType datasetType = (eDatasetType)ptrObj->GetPropertyInt32("DatasetType");
            std::string sDatasetName =  ptrObj->GetPropertyString("DatasetName");
            std::string sDatasetViewName = ptrObj->GetPropertyString("DatasetViewName");



        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to LoadTable", exc);
            throw;
        }
    }

    }
}