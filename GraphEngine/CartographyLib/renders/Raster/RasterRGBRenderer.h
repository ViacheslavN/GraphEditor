#pragma once

#include "RasterRendererBase.h"

namespace GraphEngine {
    namespace Cartography {

        // Draws raster bands as red, green, blue (and optionally alpha) channels.
        // Band index -1 switches the channel off (0). Defaults: R=0, G=1, B=2, no alpha;
        // for a single band raster use SetBandIndices(0, 0, 0) (grayscale) - see SetupForDataset.
        class CRasterRGBRenderer : public CRasterRendererBase
        {
        public:
            typedef CRasterRendererBase TBase;

            CRasterRGBRenderer();
            virtual ~CRasterRGBRenderer();

            // R,G,B,A band indices for the dataset: 3+ bands -> 0,1,2 (4 bands from the TIFF RGBA reader -> alpha 3), 1-2 bands -> grayscale
            void SetupForDataset(GeoDatabase::IRasterDatasetPtr ptrDataset);

            int  GetRedBandIndex() const;
            void SetRedBandIndex(int band);
            int  GetGreenBandIndex() const;
            void SetGreenBandIndex(int band);
            int  GetBlueBandIndex() const;
            void SetBlueBandIndex(int band);
            int  GetAlphaBandIndex() const;
            void SetAlphaBandIndex(int band);
            void SetBandIndices(int red, int green, int blue, int alpha = -1);

            // -100 .. 100, 0 - unchanged
            int  GetBrightness() const;
            void SetBrightness(int value);
            int  GetContrast() const;
            void SetContrast(int value);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        protected:
            virtual std::vector<int> GetDisplayBands(int bandCount) const;
            virtual void PrepareDraw(GeoDatabase::IRasterDatasetPtr ptrRaster);
            virtual void ConvertPixels(const uint8_t* pSrc, size_t count, int bandCount, GeoDatabase::eRasterPixelType pixelType, uint8_t* pBGRA) const;

        private:
            uint8_t ChannelValue(const uint8_t* pPixel, int band, GeoDatabase::eRasterPixelType pixelType, int sampleSize) const;

        private:
            int m_nRed;
            int m_nGreen;
            int m_nBlue;
            int m_nAlpha;
            int m_nBrightness;
            int m_nContrast;

            // valid during Draw
            std::vector<int> m_vecDrawBands;
            std::vector<std::vector<uint8_t> > m_vecByteLut;  // per band for byte rasters
            uint8_t m_toneLut[256];                            // brightness / contrast
            bool m_bByteRaster;
        };

        typedef std::shared_ptr<CRasterRGBRenderer> CRasterRGBRendererPtr;
    }
}
