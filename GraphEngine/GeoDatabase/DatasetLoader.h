#pragma once

#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CDatasetLoader
        {
        public:
            // the table's workspace must be registered in CWorkspaceHolder
            static ITablePtr LoadTable(CommonLib::ISerializeObjPtr pObj);
            // the raster's workspace must be registered in CWorkspaceHolder
            static IRasterDatasetPtr LoadRasterDataset(CommonLib::ISerializeObjPtr pObj);
            // opens a workspace saved with IWorkspace::Save (shapefile, SQLite)
            static IWorkspacePtr LoadWorkspace(CommonLib::ISerializeObjPtr pObj);
        };

    }
}
