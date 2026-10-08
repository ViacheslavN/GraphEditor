#pragma once

#include "../../Cartography.h"
#include <cstring>

namespace GraphEngine {
    namespace Cartography {

        // reads one sample of the given type as double (1/2/4-bit types are stored as one byte per sample)
        inline double ReadRasterSample(const uint8_t* p, GeoDatabase::eRasterPixelType type)
        {
            switch(type)
            {
                case GeoDatabase::RasterPixelType1Bit:
                case GeoDatabase::RasterPixelType2Bits:
                case GeoDatabase::RasterPixelType4Bits:
                case GeoDatabase::RasterPixelTypeUChar:
                    return *p;
                case GeoDatabase::RasterPixelTypeChar:
                    return *reinterpret_cast<const int8_t*>(p);
                case GeoDatabase::RasterPixelTypeUShort: { uint16_t v; memcpy(&v, p, sizeof(v)); return v; }
                case GeoDatabase::RasterPixelTypeShort:  { int16_t v;  memcpy(&v, p, sizeof(v)); return v; }
                case GeoDatabase::RasterPixelTypeULong:  { uint32_t v; memcpy(&v, p, sizeof(v)); return v; }
                case GeoDatabase::RasterPixelTypeLong:   { int32_t v;  memcpy(&v, p, sizeof(v)); return v; }
                case GeoDatabase::RasterPixelTypeFloat:  { float v;    memcpy(&v, p, sizeof(v)); return v; }
                case GeoDatabase::RasterPixelTypeDouble: { double v;   memcpy(&v, p, sizeof(v)); return v; }
                case GeoDatabase::RasterPixelTypeLongLong:  { int64_t v;  memcpy(&v, p, sizeof(v)); return (double)v; }
                case GeoDatabase::RasterPixelTypeULongLong: { uint64_t v; memcpy(&v, p, sizeof(v)); return (double)v; }
                default:
                    return 0.;
            }
        }

        inline int GetRasterSampleSize(GeoDatabase::eRasterPixelType type)
        {
            switch(type)
            {
                case GeoDatabase::RasterPixelType1Bit:
                case GeoDatabase::RasterPixelType2Bits:
                case GeoDatabase::RasterPixelType4Bits:
                case GeoDatabase::RasterPixelTypeUChar:
                case GeoDatabase::RasterPixelTypeChar:
                    return 1;
                case GeoDatabase::RasterPixelTypeUShort:
                case GeoDatabase::RasterPixelTypeShort:
                    return 2;
                case GeoDatabase::RasterPixelTypeULong:
                case GeoDatabase::RasterPixelTypeLong:
                case GeoDatabase::RasterPixelTypeFloat:
                    return 4;
                case GeoDatabase::RasterPixelTypeDouble:
                case GeoDatabase::RasterPixelTypeLongLong:
                case GeoDatabase::RasterPixelTypeULongLong:
                    return 8;
                default:
                    return 0;
            }
        }

        // types whose values are displayed without a stretch (0..255 or less)
        inline bool IsRasterByteType(GeoDatabase::eRasterPixelType type)
        {
            return type == GeoDatabase::RasterPixelTypeUChar || type == GeoDatabase::RasterPixelType1Bit
                || type == GeoDatabase::RasterPixelType2Bits || type == GeoDatabase::RasterPixelType4Bits;
        }
    }
}
