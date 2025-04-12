#pragma once
#include "GeoDatabase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CGeoDatabaseUtils
        {
        public:
            static void FillRow(ISelectCursor* ptrCursor,  IRowPtr ptrRow);
        };



    }
}