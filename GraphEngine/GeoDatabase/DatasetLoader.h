#pragma once

#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CDatasetLoader
        {
        public:
            static ITablePtr LoadTable(CommonLib::ISerializeObjPtr pObj);
        };

    }
    }