#include "RasterSpatialFilter.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        CRasterSpatialFilter::CRasterSpatialFilter() :
            m_blockWidth(256)
            , m_blockHeight(256)
            , m_pixelStep(1)
        {
            m_bbox.type = CommonLib::bbox_type_null;
        }

        CRasterSpatialFilter::CRasterSpatialFilter(const CommonLib::bbox& bbox, Geometry::ISpatialReferencePtr ptrOutputSpatRef) :
            m_ptrOutputSpatRef(ptrOutputSpatRef)
            , m_bbox(bbox)
            , m_blockWidth(256)
            , m_blockHeight(256)
            , m_pixelStep(1)
        {

        }

        CRasterSpatialFilter::~CRasterSpatialFilter()
        {

        }

        Geometry::ISpatialReferencePtr CRasterSpatialFilter::GetOutputSpatialReference() const
        {
            return m_ptrOutputSpatRef;
        }

        void CRasterSpatialFilter::SetOutputSpatialReference(Geometry::ISpatialReferencePtr spatRef)
        {
            m_ptrOutputSpatRef = spatRef;
        }

        CommonLib::bbox CRasterSpatialFilter::GetBB() const
        {
            return m_bbox;
        }

        void CRasterSpatialFilter::SetBB(const CommonLib::bbox& bbox)
        {
            m_bbox = bbox;
        }

        void CRasterSpatialFilter::SetBlockSize(int width, int height)
        {
            if(width <= 0 || height <= 0)
                throw CommonLib::CExcBase("RasterSpatialFilter: invalid block size");
            m_blockWidth = width;
            m_blockHeight = height;
        }

        int CRasterSpatialFilter::GetBlockWidth() const
        {
            return m_blockWidth;
        }

        int CRasterSpatialFilter::GetBlockHeight() const
        {
            return m_blockHeight;
        }

        void CRasterSpatialFilter::SetPixelStep(int step)
        {
            if(step <= 0)
                throw CommonLib::CExcBase("RasterSpatialFilter: invalid pixel step");
            m_pixelStep = step;
        }

        int CRasterSpatialFilter::GetPixelStep() const
        {
            return m_pixelStep;
        }
    }
}
