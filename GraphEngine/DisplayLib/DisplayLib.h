#pragma once

#include "GraphTypes/Pen.h"
#include "GraphTypes/Font.h"
#include "GraphTypes/Brush.h"
#include "GraphTypes/Rect.h"
#include "../CommonLib/SpatialData/IGeoShape.h"
#include "../GisGeometry/Geometry.h"
#include "../CommonLib/utils/delegate.h"
#include "DisplayTypes.h"

namespace GraphEngine
{
    namespace Display
    {



        enum  GraphicsType
        {
            GraphicsDefault    = 0,
            GraphicsAnnotation = 1,
            GraphicsSelection  = 2
        };

        struct ViewPosition
        {
            ViewPosition() :  m_dScale(0), m_dRotation(0)
            {
                m_Center.x = 0.;
                m_Center.y = 0.;
            }
            ViewPosition(const CommonLib::GisXYPoint &center, double scale, double rotation ) :
                    m_Center(center), m_dScale(scale), m_dRotation(rotation){}
            ~ViewPosition(){}
            CommonLib::GisXYPoint m_Center;
            double m_dScale;
            double m_dRotation;
        };

        class IDisplayTransformation;
        typedef std::shared_ptr<IDisplayTransformation> IDisplayTransformationPtr;
        typedef std::shared_ptr<class IGraphics> IGraphicsPtr;
        typedef std::shared_ptr<class ITrackCancel> ITrackCancelPtr;
        typedef std::shared_ptr<class IDisplay> IDisplayPtr;



        typedef CommonLib::delegate1_t<IDisplayTransformation*>         OnDeviceFrameChanged;
        typedef CommonLib::delegate1_t<IDisplayTransformation*>         OnResolutionChanged;
        typedef CommonLib::delegate1_t<IDisplayTransformation*>         OnRotationChanged;
        typedef CommonLib::delegate1_t<IDisplayTransformation*>         OnUnitsChanged;
        typedef CommonLib::delegate1_t<IDisplayTransformation*>         OnVisibleBoundsChanged;


        class IDisplayTransformation
        {
        public:
            IDisplayTransformation(){}
            virtual ~IDisplayTransformation(){}

            virtual void SetMapPos(const CommonLib::GisXYPoint &map_pos, double new_scale) = 0;
            virtual CommonLib::GisXYPoint GetMapPos() const= 0;

            virtual void SetMapVisibleRect(const CommonLib::bbox& bound) = 0;
            virtual const CommonLib::bbox& GetFittedBounds() const = 0;

            virtual void         SetDeviceRect(const GRect& bound, eDisplayTransformationPreserve preserve_type = DisplayTransformationPreserveCenterExtent) = 0;
            virtual const GRect& GetDeviceRect() const = 0;

            virtual void SetDeviceClipRect(const GRect& devRect) = 0;
            virtual const GRect& GetDeviceClipRect() const  = 0;

            virtual void SetReferenceScale(double dScale)= 0;
            virtual double GetReferenceScale() const= 0;
            virtual bool UseReferenceScale() const = 0;

            virtual  double GetScale() const = 0;
            virtual  void SetRotation(double degrees ) = 0;
            virtual  double GetRotation() = 0;


            virtual void SetResolution(double pDpi) = 0;
            virtual double GetResolution()= 0;

            virtual void SetUnits(CommonLib::Units units) = 0;
            virtual CommonLib::Units GetUnits() const = 0;


            virtual void   SetSpatialReference(Geometry::ISpatialReferencePtr ptrSpatialRef) = 0;
            virtual Geometry::ISpatialReferencePtr GetSpatialReference() const = 0;


            virtual void MapToDevice(const CommonLib::GisXYPoint *pIn, GPoint *pOut, int nPoints) = 0;
            virtual void MapToDevice(const CommonLib::IGeoShapePtr pGeom, GPoint **pOut, int** partCounts, int* count) = 0;
            virtual void MapToDevice(const CommonLib::bbox& mapBox, GRect& rect) = 0;

            virtual int MapToDeviceOpt(const CommonLib::GisXYPoint *pIn, GPoint *pOut, int nPoints, CommonLib::eShapeType) = 0;



            virtual void DeviceToMap(const GPoint *pIn,  CommonLib::GisXYPoint *pOut, int nPoints) = 0;
            virtual void DeviceToMap(const GRect& rect, CommonLib::bbox& mapBox) = 0;

            virtual double DeviceToMapMeasure(double deviceLen) = 0;
            virtual double MapToDeviceMeasure(double mapLen) = 0;


            virtual void SetVerticalFlip(bool flag) = 0;
            virtual bool GetVerticalFlip() const = 0;
            virtual void SetHorizontalFlip(bool flag) = 0;
            virtual bool GetHorizontalFlip() const = 0;

            virtual const GRect& GetClipRect() const = 0;
            virtual void  SetClipRect(const GRect& rect) = 0;
            virtual bool  ClipExists() = 0;
            virtual void  RemoveClip() = 0;

            virtual void SetOnDeviceFrameChanged(OnDeviceFrameChanged* pFunck, bool bAdd) = 0;
            virtual void SetOnResolutionChanged(OnResolutionChanged* pFunck, bool bAdd) = 0;
            virtual void SetOnRotationChanged(OnRotationChanged* pFunck, bool bAdd) = 0;
            virtual void SetOnUnitsChanged(OnUnitsChanged* pFunck, bool bAdd) = 0;
            virtual void SetOnVisibleBoundsChanged(OnVisibleBoundsChanged* pFunck, bool bAdd) = 0;
        };




        class IGraphics
        {
        public:
            IGraphics(){}
            virtual ~IGraphics(){}

            virtual eDeviceType  GetDeviceType() const = 0;
            virtual void        StartDrawing(IDisplayTransformationPtr ptrDT = nullptr) = 0;
            virtual void        EndDrawing() = 0;
            virtual GRect       GetClipRect() const = 0;
            virtual void        SetClipRect(const GRect& rect) = 0;
            virtual void        SetClipRgn(const GPoint* lpPoints, const int *lpPolyCounts, int nCount) = 0;
            virtual void        RemoveClip() = 0;
            virtual void        Erase(const Color& color, const GRect *rect = 0) = 0;
            virtual IGraphicsPtr   CreateCompatibleGraphics(GUnits width, GUnits height) = 0;
            virtual void        Copy(IGraphicsPtr ptrSrc, const GPoint& srcPoint, const GRect& dstRect, bool bBlend = true) = 0;

            virtual void DrawPoint(const PenPtr ptrPen, const BrushPtr  ptrBrush, const GPoint& Pt) = 0;
            virtual void DrawPoint(const PenPtr ptrPen, const BrushPtr ptrBrush, GUnits dX, GUnits dY) = 0;
            virtual void DrawPixel(GUnits dX, GUnits dY, const Color &color) = 0;

            virtual void DrawLineSeg(const PenPtr ptrPen, const GPoint& P1, const GPoint& P2) = 0;
            virtual void DrawLineSeg(const PenPtr ptrPen, GUnits dX1, GUnits dY1, GUnits dX2, GUnits dY2) = 0;

            virtual void DrawLine(const PenPtr ptrPen, const GPoint* pPoints, int nNumPoints) = 0;

            virtual void DrawRoundRect(const PenPtr ptrPen, const BrushPtr  ptrBbrush, const GRect& Rect, GUnits radius) = 0;
            virtual void DrawRect(const PenPtr ptrPen, const BrushPtr  ptrBbrush, const GRect& Rect) = 0;
            virtual void DrawRect(PenPtr ptrPen, BrushPtr  ptrBrush, const GPoint& LTPoint, const GPoint& RBPoint) = 0;
            virtual void DrawRect(PenPtr ptrPen, BrushPtr ptrBrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY) = 0;

            virtual void DrawRectEx(const PenPtr ptrPen, const BrushPtr ptrBrush, const GRect& Rect, const GPoint& originMin, const GPoint& originMax) = 0;
            virtual void DrawEllipse(const PenPtr ptrPen, const BrushPtr ptrBrush, const GRect& Rect) = 0;
            virtual void DrawEllipse(const PenPtr ptrPen, const BrushPtr ptrBrush, const GPoint& LTPoint, const GPoint& RBPoint) = 0;
            virtual void DrawEllipse(const PenPtr ptrPen, const BrushPtr ptrBrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY) = 0;

            virtual void DrawPolygon(const PenPtr ptrPen, const BrushPtr ptrBrush, const GPoint* pPoints, int nNumPoints) = 0;
            virtual void DrawPolyPolygon(const PenPtr ptrPen, const BrushPtr ptrBrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount) = 0;

            virtual void DrawPolygonEx(const PenPtr ptrPen, const BrushPtr ptrBrush, const GPoint* pPoints, int nNumPoints, const GPoint& originMin, const GPoint& originMax) = 0;
            virtual void DrawPolyPolygonEx(const PenPtr ptrPen, const BrushPtr ptrBrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount, const GPoint& originMin, const GPoint& originMax) = 0;

            virtual void QueryTextMetrics(const FontPtr ptrFont, GUnits* height, GUnits* baseLine, GUnits* lineSpacing) = 0;
            virtual void QueryTextMetrics(const FontPtr ptrFont, const wchar_t* text, int len, GUnits* width, GUnits* height, GUnits* baseLine) = 0;
            virtual void DrawText(const FontPtr ptrFont, const wchar_t * text, int len, const GPoint& point, int drawFlags = TextDrawAll) = 0;
            virtual void DrawTextByLine(const FontPtr ptrFont, const wchar_t* text, int len, const GPoint* pPoints, int nNumPoints) = 0;

            virtual void DrawBitmap(const BitmapPtr bitmap, const GRect& dstRect, bool flip_y, unsigned char alpha = 255) = 0;
            virtual void DrawRotatedBitmap(const BitmapPtr bitmap, const GPoint& center, double angle, bool flip_y = false, unsigned char alpha = 255, double scale_x = 1.0, double scale_y = 1.0, bool clip = true, bool onGrid = false) = 0;

            virtual Color GetPixel(GUnits x, GUnits y) = 0;

            virtual GPoint GetViewportOrg() const= 0;
            virtual void   SetViewportOrg(const GPoint& org) = 0;

            virtual GPoint GetBrushOrg() const = 0;
            virtual void   SetBrushOrg(const GPoint& org) = 0;

            virtual const BitmapPtr GetSurface() const = 0;
            virtual BitmapPtr GetSurface() = 0;

            virtual size_t GetWidth() const = 0;
            virtual size_t GetHeight() const= 0;

            virtual void Lock() = 0;
            virtual void UnLock() = 0;


           static IGraphicsPtr  CreateCGraphicsAgg(unsigned char* buf, GUnits  width, GUnits height, bool bRelease, bool flipY = false);
           static IGraphicsPtr  CreateCGraphicsAgg( GUnits  width, GUnits height, bool flipY = false);


#ifdef _WIN32
            virtual HDC GetDC() const = 0;
#endif


        };

        class ITrackCancel
        {
        public:
            ITrackCancel(){}
            virtual ~ITrackCancel(){}
            virtual void Cancel() = 0;
            virtual bool Continue() = 0;
            virtual void Reset() = 0;
        };


        class  IDisplay
        {
        public:
            IDisplay(){}
            virtual ~IDisplay(){}

            virtual void SetClipGeometry( const CommonLib::IGeoShapePtr ptrClipGeom )= 0;
            virtual void SetClipRect( const CommonLib::bbox& clipGeom )= 0;

            virtual void StartDrawing( IGraphicsPtr pGraphics )= 0;
            virtual void FinishDrawing()= 0;
            virtual IGraphicsPtr GetGraphics() = 0;

            virtual IDisplayTransformationPtr GetTransformation() = 0;
            virtual void SetTransformation( IDisplayTransformationPtr ptrDisplayTransformation )= 0;

            virtual void Lock() = 0;
            virtual void UnLock() = 0;
        };




    }
}
