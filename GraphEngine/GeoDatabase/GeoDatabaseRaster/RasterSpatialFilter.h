#pragma once

#include "../GeoDatabase.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        // Raster search parameters.
        // BB is in the output spatial reference (or in the dataset's one when no output reference is set);
        // a BB that is not bbox_type_normal means the whole raster.
        class CRasterSpatialFilter : public IRasterSpatialFilter
        {
        public:
            CRasterSpatialFilter();
            CRasterSpatialFilter(const CommonLib::bbox& bbox, Geometry::ISpatialReferencePtr ptrOutputSpatRef = nullptr);
            virtual ~CRasterSpatialFilter();

            // IRasterSpatialFilter
            virtual Geometry::ISpatialReferencePtr  GetOutputSpatialReference() const;
            virtual void							SetOutputSpatialReference(Geometry::ISpatialReferencePtr spatRef);
            virtual CommonLib::bbox				    GetBB() const;
            virtual void						    SetBB(const CommonLib::bbox& bbox);

            // CRasterSpatialFilter
            // size of the blocks returned by the cursor, in output pixels (default 256 x 256)
            void SetBlockSize(int width, int height);
            int GetBlockWidth() const;
            int GetBlockHeight() const;

            // take every step-th source pixel (nearest neighbour decimation), default 1
            void SetPixelStep(int step);
            int GetPixelStep() const;

        private:
            Geometry::ISpatialReferencePtr m_ptrOutputSpatRef;
            CommonLib::bbox m_bbox;
            int m_blockWidth;
            int m_blockHeight;
            int m_pixelStep;
        };

        typedef std::shared_ptr<CRasterSpatialFilter> CRasterSpatialFilterPtr;
    }
}
