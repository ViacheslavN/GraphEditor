#pragma once

#include "../GeoDatabase.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        // A block of raster pixels filled by IRasterCursor::Next.
        // Data layout: rows top to bottom, pixel-interleaved bands (b0 b1 .. bN per pixel),
        // row size = width * GetPixelSize() bytes, no padding.
        class CRasterBlock : public IRasterBlock
        {
        public:
            CRasterBlock();
            virtual ~CRasterBlock();

            // IRasterBlock
            virtual int						GetWidth() const;
            virtual int						GetHeight() const;
            virtual int						GetBandCount() const;
            virtual int 					GetPixelSize();         // bytes per pixel (all bands)
            virtual eRasterPixelType	    GetPixelType() const;
            virtual void*	                GetData();
            virtual Geometry::IEnvelopePtr	GetExtent() const;

            // CRasterBlock
            void Init(int width, int height, int bandCount, eRasterPixelType pixelType);  // keeps the allocated buffer when possible
            void SetExtent(Geometry::IEnvelopePtr ptrExtent);

            const void* GetData() const;
            size_t GetDataSize() const;
            int GetRowSize() const;

            // position of the block in the source raster (pixels) and the source pixel step used to fill it
            void SetSourceWindow(int col, int row, int step);
            int GetSourceCol() const;
            int GetSourceRow() const;
            int GetSourceStep() const;

            template<class T>
            T GetValue(int x, int y, int band) const
            {
                return reinterpret_cast<const T*>(m_data.data() + size_t(y) * GetRowSize() + size_t(x) * m_pixelSize)[band];
            }

            // bytes per sample, 1/2/4-bit types are stored as one byte per sample
            static int GetSampleSize(eRasterPixelType pixelType);

        private:
            int m_width;
            int m_height;
            int m_bandCount;
            int m_pixelSize;
            eRasterPixelType m_pixelType;
            std::vector<uint8_t> m_data;
            Geometry::IEnvelopePtr m_ptrExtent;
            int m_sourceCol;
            int m_sourceRow;
            int m_sourceStep;
        };

        typedef std::shared_ptr<CRasterBlock> CRasterBlockPtr;
    }
}
