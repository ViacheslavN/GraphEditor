#pragma once
#include "DisplayTransformation2D.h"

namespace GraphEngine
{
    namespace Display
    {
        // Pseudo 3D (perspective) view of the map plane, like a car navigator.
        //
        // The map is a ground plane, the viewer looks at the window center (the map anchor) from above,
        // the view direction is tilted by GetTilt() degrees from the vertical. The upper part of the window
        // shows the ground far ahead (smaller), the lower part the ground near the viewer (bigger).
        // Rotation (SetRotation) turns the map around the anchor, so the direction of travel can be up.
        //
        // For the flat point (u, v) - pixels relative to the window center at the anchor scale, v down -
        // and the eye distance D = FocalFactor * window height:
        //      z  = D - v * sin(tilt)               depth of the point
        //      x' = u * D / z                       device x relative to the window center
        //      y' = v * cos(tilt) * D / z           device y relative to the window center
        // The ground farther than D / MinPerspectiveScale is not shown: the device area above
        // GetSkyLine() is the "sky" (nothing is drawn there by the transformation).
        //
        // Scale, measures (DeviceToMapMeasure, ...) and SetMapVisibleRect are the values at the window center.
        // Lines are projected by vertices: straight lines stay straight in a perspective projection,
        // so the result is exact for the vertices and the clipping is done before the projection.
        class CDisplayTransformation3D : public CDisplayTransformation2D
        {
        public:
            CDisplayTransformation3D(double resolution, CommonLib::Units units, const GRect &dev_rect = GRect(), double scale = 1.0, double tiltDegrees = 55.);
            virtual ~CDisplayTransformation3D();

            static constexpr double MaxTilt = 80.;

            // 0 - plan view (the same as CDisplayTransformation2D), up to MaxTilt degrees
            void   SetTilt(double degrees);
            double GetTilt() const;

            // eye distance in window heights (smaller - stronger perspective), default 1.0
            void   SetFocalFactor(double factor);
            double GetFocalFactor() const;

            // the farthest shown ground is not smaller than this part of the scale at the window center, default 0.15
            void   SetMinPerspectiveScale(double scale);
            double GetMinPerspectiveScale() const;

            // device y of the far edge of the shown ground, the sky is above. Less than the device rect top if there is no sky
            double GetSkyLine() const;

            // how much bigger (> 1) or smaller (< 1) than at the window center the ground is drawn at the device point
            double GetPerspectiveScale(const GPoint& devPoint) const;

            using CDisplayTransformation2D::MapToDevice;
            // only the part of the box between the viewer and the far edge
            virtual void MapToDevice(const CommonLib::bbox& mapBox, GRect& rect);

        protected:
            virtual void FlatToDevice(double flatX, double flatY, double& devX, double& devY) const;
            virtual void DeviceToFlat(double devX, double devY, double& flatX, double& flatY) const;
            virtual void UpdateFlatClip();

        private:
            void UpdateProjection();
            double EyeDistance() const;

        private:
            double m_dTilt;
            double m_dSin;
            double m_dCos;
            double m_dFocalFactor;
            double m_dMinPerspectiveScale;

            // calculated by UpdateProjection
            double m_dEye;      // D, pixels
            double m_dFarV;     // flat v of the far edge (negative), -inf without the tilt
            double m_dNearV;    // flat v where the depth is too small (behind the viewer)
            double m_dSkyY;     // device y (relative to the window center) of the far edge
            CConvexClipper m_validClip; // the ground between the far edge and the viewer
            TVecDPoints m_vecTmp;
        };

        typedef std::shared_ptr<CDisplayTransformation3D> CDisplayTransformation3DPtr;
    }
}
