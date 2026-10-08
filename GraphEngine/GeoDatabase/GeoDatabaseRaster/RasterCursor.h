#pragma once

#include "RasterReader.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        // Iterates the pixel window selected by an IRasterSpatialFilter block by block (row-major).
        // Next() refills the block created by CreateBlock() (a CRasterBlock), so one block object
        // can be reused for the whole iteration.
        // The pixels are not resampled: with an output spatial reference different from the dataset one
        // only the window (filter BB -> dataset) and the block extents (dataset -> output) are projected.
        class CRasterCursor : public IRasterCursor
        {
        public:
            CRasterCursor(IRasterReaderPtr ptrReader, IRasterSpatialFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrDatasetSpatRef);
            virtual ~CRasterCursor();

            // IRasterCursor
            virtual IRasterBlockPtr CreateBlock() const;
            virtual bool	Next(IRasterBlockPtr ptrBlock);
            virtual void	Reset();
            virtual void    GetPixelWindow(int& col, int& row, int& width, int& height) const;
            virtual int     GetPixelStep() const;
            virtual int     GetBlockCount() const;

            // CRasterCursor
            int GetWindowCol() const {return m_colBegin;}
            int GetWindowRow() const {return m_rowBegin;}
            int GetWindowWidth() const {return m_colEnd - m_colBegin;}    // source pixels
            int GetWindowHeight() const {return m_rowEnd - m_rowBegin;}

        private:
            void CalcWindow(IRasterSpatialFilterPtr ptrFilter);

        private:
            IRasterReaderPtr m_ptrReader;
            Geometry::ISpatialReferencePtr m_ptrDatasetSpatRef;
            Geometry::ISpatialReferencePtr m_ptrOutputSpatRef;
            bool m_bNeedTransform;
            SRasterGeoInfo m_geoInfo;

            int m_colBegin;
            int m_rowBegin;
            int m_colEnd;     // exclusive
            int m_rowEnd;
            int m_step;
            int m_outWidth;   // window size in output pixels
            int m_outHeight;
            int m_blockWidth;
            int m_blockHeight;
            int m_blocksX;
            int m_blocksY;
            int m_nCurrentBlock;
        };
    }
}
