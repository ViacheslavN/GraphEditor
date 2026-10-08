#include "RasterBlock.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        CRasterBlock::CRasterBlock() :
            m_width(0)
            , m_height(0)
            , m_bandCount(0)
            , m_pixelSize(0)
            , m_pixelType(RasterPixelTypeUnknown)
            , m_sourceCol(0)
            , m_sourceRow(0)
            , m_sourceStep(1)
        {

        }

        CRasterBlock::~CRasterBlock()
        {

        }

        int CRasterBlock::GetSampleSize(eRasterPixelType pixelType)
        {
            switch(pixelType)
            {
                case RasterPixelType1Bit:
                case RasterPixelType2Bits:
                case RasterPixelType4Bits:
                case RasterPixelTypeUChar:
                case RasterPixelTypeChar:
                    return 1;
                case RasterPixelTypeUShort:
                case RasterPixelTypeShort:
                    return 2;
                case RasterPixelTypeULong:
                case RasterPixelTypeLong:
                case RasterPixelTypeFloat:
                    return 4;
                case RasterPixelTypeDouble:
                case RasterPixelTypeLongLong:
                case RasterPixelTypeULongLong:
                    return 8;
                default:
                    return 0;
            }
        }

        void CRasterBlock::Init(int width, int height, int bandCount, eRasterPixelType pixelType)
        {
            if(width < 0 || height < 0 || bandCount < 0)
                throw CommonLib::CExcBase("RasterBlock: invalid size");

            m_width = width;
            m_height = height;
            m_bandCount = bandCount;
            m_pixelType = pixelType;
            m_pixelSize = GetSampleSize(pixelType) * bandCount;
            m_data.resize(size_t(m_width) * size_t(m_height) * size_t(m_pixelSize));
            m_ptrExtent.reset();
        }

        int CRasterBlock::GetWidth() const
        {
            return m_width;
        }

        int CRasterBlock::GetHeight() const
        {
            return m_height;
        }

        int CRasterBlock::GetBandCount() const
        {
            return m_bandCount;
        }

        int CRasterBlock::GetPixelSize()
        {
            return m_pixelSize;
        }

        eRasterPixelType CRasterBlock::GetPixelType() const
        {
            return m_pixelType;
        }

        void* CRasterBlock::GetData()
        {
            return m_data.empty() ? nullptr : m_data.data();
        }

        const void* CRasterBlock::GetData() const
        {
            return m_data.empty() ? nullptr : m_data.data();
        }

        size_t CRasterBlock::GetDataSize() const
        {
            return m_data.size();
        }

        int CRasterBlock::GetRowSize() const
        {
            return m_width * m_pixelSize;
        }

        Geometry::IEnvelopePtr CRasterBlock::GetExtent() const
        {
            return m_ptrExtent;
        }

        void CRasterBlock::SetExtent(Geometry::IEnvelopePtr ptrExtent)
        {
            m_ptrExtent = ptrExtent;
        }

        void CRasterBlock::SetSourceWindow(int col, int row, int step)
        {
            m_sourceCol = col;
            m_sourceRow = row;
            m_sourceStep = step;
        }

        int CRasterBlock::GetSourceCol() const
        {
            return m_sourceCol;
        }

        int CRasterBlock::GetSourceRow() const
        {
            return m_sourceRow;
        }

        int CRasterBlock::GetSourceStep() const
        {
            return m_sourceStep;
        }
    }
}
