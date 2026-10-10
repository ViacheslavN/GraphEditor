#pragma once
// Blob of a compressed shape (stored in the Shape field instead of the CGeoShape buffer):
//
//   [0]     flag             ShapeCompressedFlag (a CGeoShape buffer has 0 here)
//   [1..2]  shape type       int16, as in CGeoShape (CGeoShape::Type reads it from both blobs)
//   [3]     options          bits 0-1: eValueCoding of the coordinates, bit 2: closed rings, bits 4-7: version
//   [4]     scale exponent   int8 k: the coordinates are stored as integers in units of 10^-k
//   point:        zigzag varint x, y
//   multipoint:   varint point count, bbox, values
//   polyline,
//   polygon:      varint point count, varint part count, the sizes of the parts but the last (varints), bbox, values
//
//   bbox:   zigzag varint xMin, yMin, varint width, height (integers)
//   values: x, y deltas (zigzag) of every point from the previous one, the first one from (xMin, yMin);
//           closed rings: the last point of every part (= the first one) isn't stored.
//
// Z, M, curves and multipatches aren't compressed (the shape is kept as it is).

#include "CompressionUtils.h"

namespace GraphEngine
{
    namespace GeometryCompression
    {
        const TByte ShapeCompressedFlag = 0x01;
        const TByte ShapeFormatVersion = 1;   // 1 - the bit lengths of NumLen are range coded
        const uint32_t ShapeHeaderSize = 5;

        enum eShapeOptions
        {
            ShapeOptionCodingMask  = 0x03,
            ShapeOptionClosedRings = 0x04,
            ShapeOptionVersionShift = 4
        };

        inline bool IsCompressedShape(const TByte* pData, size_t nSize)
        {
            return pData != nullptr && nSize >= ShapeHeaderSize && pData[0] == ShapeCompressedFlag;
        }

        // 10^k as a double, exact for |k| <= 22
        inline double Pow10(int k)
        {
            double d = 1.;
            for(int i = 0; i < (k < 0 ? -k : k); ++i)
                d *= 10.;
            return k < 0 ? 1. / d : d;
        }
    }
}
