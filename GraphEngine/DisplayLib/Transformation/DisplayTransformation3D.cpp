#include "DisplayTransformation3D.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace GraphEngine
{
    namespace Display
    {
        namespace
        {
            const double kPi = 3.14159265358979323846;
            const double NearDepthPart = 0.05;    // the nearest depth, part of the eye distance
            const double kInfinity = 1e30;
        }

        CDisplayTransformation3D::CDisplayTransformation3D(double resolution, CommonLib::Units units, const GRect &dev_rect, double scale, double tiltDegrees) :
                CDisplayTransformation2D(resolution, units, dev_rect, scale),
                m_dTilt(0.),
                m_dSin(0.),
                m_dCos(1.),
                m_dFocalFactor(1.),
                m_dMinPerspectiveScale(0.15),
                m_dEye(1.),
                m_dFarV(-kInfinity),
                m_dNearV(kInfinity),
                m_dSkyY(-kInfinity)
        {
            // the base constructor called the base versions of the virtual functions
            SetTilt(tiltDegrees);
            UpdateFlatClip();
            UpdateFittedBounds();
        }

        CDisplayTransformation3D::~CDisplayTransformation3D()
        {

        }

        void CDisplayTransformation3D::SetTilt(double degrees)
        {
            if (!(degrees >= 0.))
                degrees = 0.;
            if (degrees > MaxTilt)
                degrees = MaxTilt;

            m_dTilt = degrees;
            m_dSin = std::sin(degrees * kPi / 180.);
            m_dCos = std::cos(degrees * kPi / 180.);
            if (degrees == 0.)
            {
                m_dSin = 0.;
                m_dCos = 1.;
            }

            UpdateFlatClip();
            UpdateFittedBounds();
        }

        double CDisplayTransformation3D::GetTilt() const
        {
            return m_dTilt;
        }

        void CDisplayTransformation3D::SetFocalFactor(double factor)
        {
            if (!(factor > 0.05))
                factor = 0.05;

            m_dFocalFactor = factor;
            UpdateFlatClip();
            UpdateFittedBounds();
        }

        double CDisplayTransformation3D::GetFocalFactor() const
        {
            return m_dFocalFactor;
        }

        void CDisplayTransformation3D::SetMinPerspectiveScale(double scale)
        {
            if (!(scale > 0.01))
                scale = 0.01;
            if (scale > 1.)
                scale = 1.;

            m_dMinPerspectiveScale = scale;
            UpdateFlatClip();
            UpdateFittedBounds();
        }

        double CDisplayTransformation3D::GetMinPerspectiveScale() const
        {
            return m_dMinPerspectiveScale;
        }

        double CDisplayTransformation3D::GetSkyLine() const
        {
            return m_dSkyY + m_AnchorDev[1];
        }

        double CDisplayTransformation3D::GetPerspectiveScale(const GPoint& devPoint) const
        {
            double fx, fy;
            DeviceToFlat(double(devPoint.x), double(devPoint.y), fx, fy);
            double z = m_dEye - fy * m_dSin;
            return z > 0. ? m_dEye / z : 0.;
        }

        double CDisplayTransformation3D::EyeDistance() const
        {
            double height = double(m_ClientRect.Height());
            if (height <= 0.)
                height = 1.;
            return m_dFocalFactor * height;
        }

        void CDisplayTransformation3D::UpdateProjection()
        {
            m_dEye = EyeDistance();
            if (m_dSin <= 0.)
            {
                m_dFarV = -kInfinity;
                m_dNearV = kInfinity;
                m_dSkyY = -kInfinity;
                m_validClip.Clear();
                return;
            }

            // z = D - v*sin  ->  v = (D - z) / sin
            double zFar = m_dEye / m_dMinPerspectiveScale;
            double zNear = m_dEye * NearDepthPart;
            m_dFarV = (m_dEye - zFar) / m_dSin;
            m_dNearV = (m_dEye - zNear) / m_dSin;
            m_dSkyY = m_dFarV * m_dCos * m_dEye / zFar;

            // the ground that can be projected: between the far edge and the near depth
            const double wide = 1e12;
            m_validClip.SetClipRect(-wide, m_dFarV, wide, m_dNearV);
        }

        void CDisplayTransformation3D::FlatToDevice(double flatX, double flatY, double& devX, double& devY) const
        {
            double z = m_dEye - flatY * m_dSin;
            double zNear = m_dEye * NearDepthPart;
            if (z < zNear)
                z = zNear; // behind the viewer, such points are clipped before the projection

            double k = m_dEye / z;
            devX = flatX * k + m_AnchorDev[0];
            devY = flatY * m_dCos * k + m_AnchorDev[1];
        }

        void CDisplayTransformation3D::DeviceToFlat(double devX, double devY, double& flatX, double& flatY) const
        {
            double x = devX - m_AnchorDev[0];
            double y = devY - m_AnchorDev[1];

            if (m_dSin <= 0.)
            {
                flatX = x;
                flatY = y;
                return;
            }

            // the sky: the far edge of the ground
            if (y < m_dSkyY)
                y = m_dSkyY;

            // y = v*cos*D / (D - v*sin)  ->  v = y*D / (D*cos + y*sin)
            double denom = m_dEye * m_dCos + y * m_dSin;
            flatY = y * m_dEye / denom;
            double z = m_dEye - flatY * m_dSin;
            flatX = x * z / m_dEye;
        }

        void CDisplayTransformation3D::UpdateFlatClip()
        {
            UpdateProjection();

            if (m_devClipRect.IsEmpty())
            {
                m_flatClip.Clear();
                return;
            }

            // the inverse projection of the device clip rect (without the sky) is a convex quadrangle
            double top = (std::max)(double(m_devClipRect.yMin), GetSkyLine());
            double bottom = double(m_devClipRect.yMax);
            if (!(top < bottom))
            {
                m_flatClip.Clear();
                return;
            }

            const double corners[4][2] = {
                    {double(m_devClipRect.xMin), top},
                    {double(m_devClipRect.xMax), top},
                    {double(m_devClipRect.xMax), bottom},
                    {double(m_devClipRect.xMin), bottom}
            };

            DPoint flat[4];
            for (int i = 0; i < 4; ++i)
                DeviceToFlat(corners[i][0], corners[i][1], flat[i].x, flat[i].y);

            m_flatClip.SetClipPolygon(flat, 4);
        }

        void CDisplayTransformation3D::MapToDevice(const CommonLib::bbox& mapBox, GRect& rect)
        {
            if (m_dSin <= 0.)
            {
                CDisplayTransformation2D::MapToDevice(mapBox, rect);
                return;
            }

            // only the part of the box in front of the viewer can be projected
            DPoint corners[4];
            const double cornersMap[4][2] = { {mapBox.xMin, mapBox.yMin}, {mapBox.xMax, mapBox.yMin}, {mapBox.xMax, mapBox.yMax}, {mapBox.xMin, mapBox.yMax} };
            for (int i = 0; i < 4; ++i)
                MapToFlat(cornersMap[i][0], cornersMap[i][1], corners[i].x, corners[i].y);

            m_validClip.ClipPolygon(corners, 4, m_vecTmp);
            if (m_vecTmp.empty())
            {
                rect = GRect(); // behind the viewer or beyond the far edge: empty (IsEmpty)
                return;
            }

            for (size_t i = 0; i < m_vecTmp.size(); ++i)
            {
                double dx, dy;
                FlatToDevice(m_vecTmp[i].x, m_vecTmp[i].y, dx, dy);
                GUnits x = ToDevice(dx);
                GUnits y = ToDevice(dy);
                if (i == 0)
                {
                    rect.Set(x, y, x, y);
                    continue;
                }
                rect.xMin = (std::min)(rect.xMin, x);
                rect.xMax = (std::max)(rect.xMax, x);
                rect.yMin = (std::min)(rect.yMin, y);
                rect.yMax = (std::max)(rect.yMax, y);
            }
        }
    }
}
