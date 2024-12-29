#pragma once
#include "CommonLib.h"

namespace GraphEngine
{
    namespace Display
    {

        class CDisplayMath
        {
            //! Rounding error constant often used when comparing double values.
        public:
            static const double ROUNDING_ERROR;

            static const float  ROUNDING_ERROR_FLOAT;

            //! Constant for PI.
            static const double PI;

            //! Constant for converting bettween degrees and radiants.
            static const double GRAD_PI;

            //! Constant for converting bettween degrees and radiants.
            static const double GRAD_PI2;

            //! returns minimum of two values. Own implementation to get rid of the STL.
            template<class T>
            static const T Min(const T a, const T b)
            {
                return a < b ? a : b;
            }

            //! returns minimum of two values. Own implementation to get rid of the STL
            template<class T>
            static T Max(const T a, const T b)
            {
                return a < b ? b : a;
            }

            //! returns if a float equals the other one, taking floating
            //! point rounding errors into account
            static bool Equals(double a, double b)
            {
                return (a + ROUNDING_ERROR > b) && (a - ROUNDING_ERROR < b);
            }

            static bool Equals(float a, float b)
            {
                return (a + ROUNDING_ERROR_FLOAT > b) && (a - ROUNDING_ERROR_FLOAT < b);
            }


            static const double intersection_epsilon;



            static double CrossProduct(double x1, double y1, double x2, double y2, double x, double y)
            {
                return (x - x2) * (y2 - y1) - (y - y2) * (x2 - x1);
            }

            static bool CalcIntersection(double ax, double ay, double bx, double by,
                                         double cx, double cy, double dx, double dy,
                                         double* x, double* y)
            {
                double num = (ay - cy) * (dx - cx) - (ax - cx) * (dy - cy);
                double den = (bx - ax) * (dy - cy) - (by - ay) * (dx - cx);
                if (fabs(den) < intersection_epsilon) return false;
                double r = num / den;
                *x = ax + r * (bx - ax);
                *y = ay + r * (by - ay);
                return true;
            }

            static double CalcDistance(double x1, double y1, double x2, double y2)
            {
                double dx = x2 - x1;
                double dy = y2 - y1;
                return sqrt(dx * dx + dy * dy);
            }

        };
#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif
#define RAD2DEG(r) ((double)(r) * (180.0 / M_PI))
#define DEG2RAD(d) ((double)(d) * (M_PI / 180.0))
    }
}