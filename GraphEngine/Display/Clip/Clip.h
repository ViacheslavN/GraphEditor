#pragma once

#include "CommonLib.h"
#include "../GraphTypes/GraphTypes.h"
#include "../GraphTypes/Rect.h"

namespace GraphEngine {
    namespace Display {
        class IClip {
        public:
            IClip() {}

            virtual ~IClip() {}

            virtual int ClipLine(const GRect &clipper, GPoint *pBeg, GPoint *pEnd) = 0;

            virtual void ClipLine(const GRect &clipper, GPoint **ppPoints, int **ppnPointCounts, int *pCount) = 0;

            virtual void ClipRing(const GRect &clipper, GPoint **ppPoints, int *pPointCount) = 0;

            virtual void ClipPolygon(const GRect &clipper, GPoint **ppPoints, int **ppPointCounts, int *pCount) = 0;

            virtual void ClipPolygonByPolygon(GPoint **ppClipPoints, int **ppClipPointCounts, int *pClipCount,
                                              GPoint **points, int **pointCounts, int *count) = 0;
        };
    }

}
