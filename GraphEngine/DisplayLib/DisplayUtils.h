#pragma once
#include "../CommonLib/CommonLib.h"
#include "DisplayLib.h"

namespace GraphEngine
{
    namespace Display {
        class CDisplayUtils
        {
        public:
            static GUnits SymbolSizeToDeviceSize(IDisplayTransformationPtr ptrTrans, double sizeSymb, bool scaleDependent);
            static double DeviceSizeToSymbolSize(IDisplayTransformationPtr ptrTrans, GUnits sizeDev, bool scaleDependent);
            static GUnits SymbolSizeToDeviceSize(double dpi, double sizeSymb);
            static double DeviceSizeToSymbolSize(double dpi, GUnits sizeDev);
            static void RotateCoords(const GPoint& center, double angle, GPoint* points, int count);
            static bool IsClockwise(const GPoint* points, int pointCount);

        };


    }
    }