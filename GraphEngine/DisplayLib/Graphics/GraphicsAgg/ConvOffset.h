#pragma once

#include "../../GraphTypes/Point.h"
#include "../../GraphTypes/GraphTypes.h"

namespace GraphEngine {
    namespace Display {

        class CConvOffset
        {
        public:
            CConvOffset();
            ~CConvOffset();
        public:

            static void Gen(GUnits offset, const GPoint** points, int* pointCounts, int count);
        private:
            static bool CalcSegment(const GPoint& p1,
                                     const GPoint& p2,
                                     const GPoint& p3,
                                     GUnits offset,
                                     GPoint& beg,
                                     GPoint& end,
                                     bool first_seg,
                                     bool *use_both);

            static bool CalcSegment(const GPoint& p1,
                                     const GPoint& p2,
                                     GUnits offset,
                                     GPoint& beg,
                                     GPoint& end);

            static bool GenLineKoef(const GPoint& p1, const GPoint& p2, double* a, double* b, double* c);
            static bool IsParallel(double A1, double B1, double A2, double B2);
            static bool IsSameOrientation(double A1, double B1, double A2, double B2);
            static bool Cross(double A1, double B1, double C1, double A2, double B2, double C2, GPoint& p);
        public:
            static double Len(const GPoint& p1, const GPoint& p2);
        };
    }
}