#pragma once
#include "../DisplayLib.h"
#include "../GraphTypes/Point.h"
#include "../GraphTypes/Rect.h"
#include "../Clip/ConvexClipper.h"
#include "../../GisGeometry/Geometry.h"

namespace GraphEngine
{
    namespace Display
    {
        // Map <-> device transformation of a plan (2D) view.
        //
        // The conversion goes through "flat" coordinates: a map point relative to the map anchor
        // (the map point shown in the window center), rotated, flipped and scaled to pixels:
        //      flat   = M * (map - anchorMap)        M - 2x2 rotation * scale(1/ratio) * flip
        //      device = Project(flat)                2D: flat + anchorDev (window center)
        // Derived transformations (CDisplayTransformation3D) change only the projection part:
        // FlatToDevice / DeviceToFlat and the visible area in flat coordinates (UpdateFlatClip).
        class CDisplayTransformation2D : public IDisplayTransformation
        {
        public:

            CDisplayTransformation2D(double resolution, CommonLib::Units units, const GRect &dev_rect = GRect(), double scale = 1.0);
            virtual ~CDisplayTransformation2D();

            virtual void SetMapPos(const CommonLib::GisXYPoint &map_pos, double new_scale);
            virtual CommonLib::GisXYPoint GetMapPos() const;

            virtual void SetMapVisibleRect(const CommonLib::bbox& bound);
            virtual const CommonLib::bbox& GetFittedBounds() const;

            virtual void         SetDeviceRect(const GRect& bound, eDisplayTransformationPreserve preserve_type = DisplayTransformationPreserveCenterExtent);
            virtual const GRect& GetDeviceRect() const;

            virtual void SetDeviceClipRect(const GRect& devRect);
            virtual const GRect& GetDeviceClipRect() const ;

            virtual void SetReferenceScale(double dScale);
            virtual double GetReferenceScale() const;
            virtual bool UseReferenceScale() const;

            virtual  double GetScale() const;
            virtual  void SetRotation(double degrees );
            virtual  double GetRotation();


            virtual void SetResolution(double pDpi);
            virtual double GetResolution();

            virtual void SetUnits(CommonLib::Units units);
            virtual CommonLib::Units GetUnits() const;


            virtual void   SetSpatialReference(Geometry::ISpatialReferencePtr ptrSp);
            virtual Geometry::ISpatialReferencePtr GetSpatialReference() const;

            virtual void MapToDevice(const CommonLib::GisXYPoint *pIn, GPoint *pOut, int nPoints);
            // The shape is clipped by the device clip rect (SetDeviceClipRect). The result points to internal
            // buffers: valid until the next call, so one transformation can be used by one thread only.
            virtual void MapToDevice(const CommonLib::IGeoShapePtr ptrGeom, GPoint **pOut, int** partCounts, int* count);
            virtual void MapToDevice(const CommonLib::bbox& mapBox, GRect& rect);
            virtual int MapToDeviceOpt(const CommonLib::GisXYPoint *pIn, GPoint *pOut, int nPoints, CommonLib::eShapeType);
            virtual void MapToDevicePoint(const CommonLib::GisXYPoint& pIn, GPoint& pOut);

            virtual void DeviceToMap(const GPoint *pIn,  CommonLib::GisXYPoint *pOut, int nPoints);
            virtual void DeviceToMap(const GRect& rect, CommonLib::bbox& mapBox);

            // measures at the window center (the map anchor)
            virtual double DeviceToMapMeasure(double deviceLen);
            virtual double MapToDeviceMeasure(double mapLen);


            virtual void SetVerticalFlip(bool flag);
            virtual bool GetVerticalFlip() const;
            virtual void SetHorizontalFlip(bool flag);
            virtual bool GetHorizontalFlip() const;

            // an additional clip of the graphics, only stored here (shapes are clipped by the device clip rect)
            virtual const GRect& GetClipRect() const;
            virtual void  SetClipRect(const GRect& rect);
            virtual bool  ClipExists();
            virtual void  RemoveClip();

            virtual void SetOnDeviceFrameChanged(OnDeviceFrameChanged* pFunck, bool bAdd);
            virtual void SetOnResolutionChanged(OnResolutionChanged* pFunck, bool bAdd);
            virtual void SetOnRotationChanged(OnRotationChanged* pFunck, bool bAdd);
            virtual void SetOnUnitsChanged(OnUnitsChanged* pFunck, bool bAdd);
            virtual void SetOnVisibleBoundsChanged(OnVisibleBoundsChanged* pFunck, bool bAdd);

        protected:
            // map <-> flat (pixels relative to the window center, before the projection)
            void MapToFlat(double mapX, double mapY, double& flatX, double& flatY) const;
            void FlatToMap(double flatX, double flatY, double& mapX, double& mapY) const;

            // projection: flat <-> device (double, not rounded)
            virtual void FlatToDevice(double flatX, double flatY, double& devX, double& devY) const;
            virtual void DeviceToFlat(double devX, double devY, double& flatX, double& flatY) const;

            // visible area in flat coordinates, built from the device clip rect
            virtual void UpdateFlatClip();
            virtual void UpdateFittedBounds();

            static GUnits ToDevice(double v);

            void SetClientRect(const GRect &arg);
            void UpdateScaleRatio();
            double CalcMapUnitPerInch();
            void SetMatrix();

        private:
            void AddDevicePart(const DPoint* pFlat, size_t nCount, size_t nMinPoints);

        protected:
            GRect m_devClipRect;
            GRect m_clipRect;
            bool m_bClipExists;
            CommonLib::bbox m_mapCurFittedExtent;
            GRect m_ClientRect;
            double m_dRefScale;
            double m_dCurScale;
            double m_dScaleRatio;   // map units per pixel
            double m_dResolution;   // dpi
            CommonLib::Units m_mapUnits;
            double m_dAngle;
            Geometry::ISpatialReferencePtr m_pSpatialRef;

            double     m_MatrixDev2Map[2][2];
            double     m_MatrixMap2Dev[2][2];

            GUnits m_AnchorDev[2];
            double m_AnchorMap[2];

            bool m_bVerticalFlip;
            bool m_bHorizontalFlip;

            CommonLib::Event1<IDisplayTransformation*>         OnDeviceFrameChangedEvent;
            CommonLib::Event1<IDisplayTransformation*>         OnResolutionChangedEvent;
            CommonLib::Event1<IDisplayTransformation*>         OnRotationChangedEvent;
            CommonLib::Event1<IDisplayTransformation*>         OnUnitsChangedEvent;
            CommonLib::Event1<IDisplayTransformation*>         OnVisibleBoundsChangedEvent;

            CConvexClipper m_flatClip;          // visible area in flat coordinates

        private:
            std::vector<GPoint> m_vecPoints;    // MapToDevice(shape) result
            std::vector<int> m_vecParts;
            TVecDPoints m_vecFlat;              // work buffers
            TVecDPoints m_vecClipped;
            std::vector<int> m_vecClippedParts;
        };
    }
}
