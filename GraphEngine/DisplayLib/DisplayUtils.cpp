#include "DisplayUtils.h"
#include "Agg/agg_trans_affine.h"

namespace GraphEngine
{
    namespace Display {

        const static double dInchesPerMillimeters = 0.03937;

        double Round(double num)
        {
            return floor(num + 0.5);
        }

        GUnits CDisplayUtils::SymbolSizeToDeviceSize(IDisplayTransformationPtr trans, double sizeSymb, bool scaleDependent)
        {

            if (!trans || !sizeSymb)
                return static_cast<GUnits>(Round(sizeSymb));

            double scale = (scaleDependent && trans->GetScale() && trans->UseReferenceScale())
                           ? (static_cast<double>(trans->GetReferenceScale()) / static_cast<double>(trans->GetScale()))
                           : 1.0;
            return SymbolSizeToDeviceSize(trans->GetResolution() * scale, sizeSymb);
        }

        double CDisplayUtils::DeviceSizeToSymbolSize(IDisplayTransformationPtr trans, GUnits sizeDev, bool scaleDependent)
        {

            if (!trans || !sizeDev)
                return static_cast<double>(sizeDev);

            double scale = (scaleDependent && trans->GetScale() && trans->UseReferenceScale())
                           ? (static_cast<double>(trans->GetReferenceScale()) / static_cast<double>(trans->GetScale()))
                           : 1.0;
            return DeviceSizeToSymbolSize(trans->GetResolution() * scale, sizeDev);
        }

        GUnits CDisplayUtils::SymbolSizeToDeviceSize(double dpi, double sizeSymb)
        {
            return (GUnits)(dpi * dInchesPerMillimeters * sizeSymb);
        }

        double CDisplayUtils::DeviceSizeToSymbolSize(double dpi, GUnits sizeDev)
        {
            return (GUnits)(sizeDev / (dpi * dInchesPerMillimeters));
        }

        void CDisplayUtils::RotateCoords(const GPoint& center, double angle, GPoint* points, int count)
        {
            agg::trans_affine tr;
            tr *= agg::trans_affine_translation(-center.x, -center.y);
            tr *= agg::trans_affine_rotation(DEG2RAD(angle));
            tr *= agg::trans_affine_translation(center.x, center.y);

            for(int i = 0; i < count; i++)
            {
                double x = points[i].x;
                double y = points[i].y;
                tr.transform(&x, &y);
                points[i].x = (GUnits)x;
                points[i].y = (GUnits)y;
            }
        }

        bool CDisplayUtils::IsClockwise(const GPoint* points, int pointCount)
        {
        #ifdef _FLOAT_GUNITS_
                double S = 0;
        #else
                int64 S = 0;
        #endif
                for(int i = 0; i < pointCount - 1; i++)
                S += (points[i + 1].y + points[i].y) * (points[i + 1].x - points[i].x);
                return S >= 0;
        }

    }
    }