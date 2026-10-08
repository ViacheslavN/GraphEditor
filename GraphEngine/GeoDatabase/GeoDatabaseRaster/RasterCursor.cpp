#include "RasterCursor.h"
#include "RasterBlock.h"
#include "../../GisGeometry/Envelope.h"

#include <algorithm>
#include <cmath>

namespace GraphEngine
{
    namespace GeoDatabase {

        CRasterCursor::CRasterCursor(IRasterReaderPtr ptrReader, IRasterSpatialFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrDatasetSpatRef) :
            m_ptrReader(ptrReader)
            , m_ptrDatasetSpatRef(ptrDatasetSpatRef)
            , m_bNeedTransform(false)
            , m_colBegin(0)
            , m_rowBegin(0)
            , m_colEnd(0)
            , m_rowEnd(0)
            , m_step(1)
            , m_outWidth(0)
            , m_outHeight(0)
            , m_blockWidth(256)
            , m_blockHeight(256)
            , m_blocksX(0)
            , m_blocksY(0)
            , m_nCurrentBlock(0)
        {
            if(!m_ptrReader.get())
                throw CommonLib::CExcBase("RasterCursor: reader is null");

            m_geoInfo = m_ptrReader->GetGeoInfo();

            if(ptrFilter.get())
            {
                m_blockWidth = (std::max)(1, ptrFilter->GetBlockWidth());
                m_blockHeight = (std::max)(1, ptrFilter->GetBlockHeight());
                m_step = (std::max)(1, ptrFilter->GetPixelStep());
                m_ptrOutputSpatRef = ptrFilter->GetOutputSpatialReference();
                m_bNeedTransform = m_ptrOutputSpatRef.get() && m_ptrDatasetSpatRef.get() && !m_ptrDatasetSpatRef->IsEqual(m_ptrOutputSpatRef);
            }

            CalcWindow(ptrFilter);
        }

        CRasterCursor::~CRasterCursor()
        {

        }

        void CRasterCursor::CalcWindow(IRasterSpatialFilterPtr ptrFilter)
        {
            const int width = m_ptrReader->GetWidth();
            const int height = m_ptrReader->GetHeight();

            m_colBegin = 0;
            m_rowBegin = 0;
            m_colEnd = width;
            m_rowEnd = height;

            CommonLib::bbox bb;
            bb.type = CommonLib::bbox_type_null;
            if(ptrFilter.get())
                bb = ptrFilter->GetBB();

            if((bb.type & CommonLib::bbox_basic_type_mask) == CommonLib::bbox_type_normal)
            {
                // filter BB is in the output spatial reference; if it can't be projected the whole raster is used
                bool bValid = true;
                if(m_bNeedTransform)
                {
                    CommonLib::bbox bbDataset = bb;
                    bValid = m_ptrOutputSpatRef->Project(m_ptrDatasetSpatRef, bbDataset);
                    if(bValid)
                        bb = bbDataset;
                }

                if(bValid)
                {
                    // map -> pixel, pixel sizes can be negative
                    double c1 = (bb.xMin - m_geoInfo.originX) / m_geoInfo.pixelSizeX;
                    double c2 = (bb.xMax - m_geoInfo.originX) / m_geoInfo.pixelSizeX;
                    double r1 = (bb.yMin - m_geoInfo.originY) / m_geoInfo.pixelSizeY;
                    double r2 = (bb.yMax - m_geoInfo.originY) / m_geoInfo.pixelSizeY;

                    double colMin = std::floor((std::min)(c1, c2));
                    double colMax = std::ceil((std::max)(c1, c2));
                    double rowMin = std::floor((std::min)(r1, r2));
                    double rowMax = std::ceil((std::max)(r1, r2));

                    // a degenerate BB (point / line) still selects the pixel it lies in
                    if(colMax == colMin)
                        colMax += 1.;
                    if(rowMax == rowMin)
                        rowMax += 1.;

                    m_colBegin = (int)std::clamp(colMin, 0., double(width));
                    m_colEnd = (int)std::clamp(colMax, 0., double(width));
                    m_rowBegin = (int)std::clamp(rowMin, 0., double(height));
                    m_rowEnd = (int)std::clamp(rowMax, 0., double(height));
                }
            }

            const int windowWidth = (std::max)(0, m_colEnd - m_colBegin);
            const int windowHeight = (std::max)(0, m_rowEnd - m_rowBegin);
            m_outWidth = (windowWidth + m_step - 1) / m_step;
            m_outHeight = (windowHeight + m_step - 1) / m_step;
            m_blocksX = (m_outWidth + m_blockWidth - 1) / m_blockWidth;
            m_blocksY = (m_outHeight + m_blockHeight - 1) / m_blockHeight;
            m_nCurrentBlock = 0;
        }

        int CRasterCursor::GetBlockCount() const
        {
            return m_blocksX * m_blocksY;
        }

        IRasterBlockPtr CRasterCursor::CreateBlock() const
        {
            return std::make_shared<CRasterBlock>();
        }

        void CRasterCursor::GetPixelWindow(int& col, int& row, int& width, int& height) const
        {
            col = m_colBegin;
            row = m_rowBegin;
            width = m_colEnd - m_colBegin;
            height = m_rowEnd - m_rowBegin;
        }

        int CRasterCursor::GetPixelStep() const
        {
            return m_step;
        }

        void CRasterCursor::Reset()
        {
            m_nCurrentBlock = 0;
        }

        bool CRasterCursor::Next(IRasterBlockPtr ptrBlock)
        {
            if(m_nCurrentBlock >= GetBlockCount())
                return false;

            CRasterBlock* pBlock = dynamic_cast<CRasterBlock*>(ptrBlock.get());
            if(!pBlock)
                throw CommonLib::CExcBase("RasterCursor: Next expects a block created by CreateBlock()");

            try
            {
                const int bx = m_nCurrentBlock % m_blocksX;
                const int by = m_nCurrentBlock / m_blocksX;
                ++m_nCurrentBlock;

                const int outX = bx * m_blockWidth;
                const int outY = by * m_blockHeight;
                const int outW = (std::min)(m_blockWidth, m_outWidth - outX);
                const int outH = (std::min)(m_blockHeight, m_outHeight - outY);
                const int col = m_colBegin + outX * m_step;
                const int row = m_rowBegin + outY * m_step;

                pBlock->Init(outW, outH, m_ptrReader->GetBandCount(), m_ptrReader->GetPixelType());
                pBlock->SetSourceWindow(col, row, m_step);
                m_ptrReader->ReadWindow(col, row, outW, outH, m_step, pBlock->GetData());

                // extent of the source pixels covered by the block
                const int colEnd = (std::min)(col + outW * m_step, m_colEnd);
                const int rowEnd = (std::min)(row + outH * m_step, m_rowEnd);
                double x1 = m_geoInfo.originX + col * m_geoInfo.pixelSizeX;
                double x2 = m_geoInfo.originX + colEnd * m_geoInfo.pixelSizeX;
                double y1 = m_geoInfo.originY + row * m_geoInfo.pixelSizeY;
                double y2 = m_geoInfo.originY + rowEnd * m_geoInfo.pixelSizeY;

                CommonLib::bbox bb;
                bb.type = CommonLib::bbox_type_normal;
                bb.xMin = (std::min)(x1, x2);
                bb.xMax = (std::max)(x1, x2);
                bb.yMin = (std::min)(y1, y2);
                bb.yMax = (std::max)(y1, y2);

                Geometry::ISpatialReferencePtr ptrSpatRef = m_ptrDatasetSpatRef;
                if(m_bNeedTransform)
                {
                    CommonLib::bbox bbOutput = bb;
                    if(m_ptrDatasetSpatRef->Project(m_ptrOutputSpatRef, bbOutput))
                    {
                        bb = bbOutput;
                        ptrSpatRef = m_ptrOutputSpatRef;
                    }
                }

                pBlock->SetExtent(std::make_shared<Geometry::CEnvelope>(bb, ptrSpatRef));
                return true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("RasterCursor failed to read block", exc);
                throw;
            }
        }
    }
}
