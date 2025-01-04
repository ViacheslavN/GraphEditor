#pragma once

#include "../GeoDatabase.h"
#include "../../CommonLib/SpatialData/IGeoShape.h"
#include "../../ThirdParty/ShapeLib/shapefil.h"
#include "SHPObject.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        class CShapefileUtils
        {
        public:
            static std::string  NormalizePath(const std::string& path);
            static CommonLib::eShapeType SHPTypeToGeometryType(int shpType, bool* hasZ = NULL, bool* hasM = NULL);
            static eDataTypes SHPFieldInfoToFieldInfo(DBFFieldType ftype, int width, int dec, int* length, int* precision, int* scale);
            static void SHPObjectToGeometry(CSHPObjectPtr ptrObject, CommonLib::IGeoShapePtr ptrResult);

        };

    }
    }