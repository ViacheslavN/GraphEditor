#include "DisplayMath.h"

namespace GraphEngine
{
    namespace Display
    {
        const double CDisplayMath::ROUNDING_ERROR = 0.00001f;
        const float  CDisplayMath::ROUNDING_ERROR_FLOAT = 0.001f;
        const double CDisplayMath::PI = 3.1415926535897932384626433832795;
        const double CDisplayMath::GRAD_PI = 180.0 / 3.1415926535897932384626433832795;
        const double CDisplayMath::GRAD_PI2 = 3.1415926535897932384626433832795 / 180.0;
        const double CDisplayMath::intersection_epsilon = 1.0e-30;
    }
}