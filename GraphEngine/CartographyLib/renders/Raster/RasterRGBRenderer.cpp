#include "RasterRGBRenderer.h"
#include "RasterPixelUtils.h"

#include <algorithm>
#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        CRasterRGBRenderer::CRasterRGBRenderer() : TBase(RasterRGBRendererID),
            m_nRed(0)
            , m_nGreen(1)
            , m_nBlue(2)
            , m_nAlpha(-1)
            , m_nBrightness(0)
            , m_nContrast(0)
            , m_bByteRaster(true)
        {
            for(int i = 0; i < 256; ++i)
                m_toneLut[i] = (uint8_t)i;
        }

        CRasterRGBRenderer::~CRasterRGBRenderer()
        {

        }

        void CRasterRGBRenderer::SetupForDataset(GeoDatabase::IRasterDatasetPtr ptrDataset)
        {
            if(!ptrDataset.get())
                return;

            const int bands = ptrDataset->GetBandCount();
            if(bands >= 3)
                SetBandIndices(0, 1, 2, bands == 4 && ptrDataset->GetPixelType() == GeoDatabase::RasterPixelTypeUChar ? 3 : -1);
            else
                SetBandIndices(0, 0, 0, -1);
        }

        int CRasterRGBRenderer::GetRedBandIndex() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nRed;
        }

        void CRasterRGBRenderer::SetRedBandIndex(int band)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nRed = band;
        }

        int CRasterRGBRenderer::GetGreenBandIndex() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nGreen;
        }

        void CRasterRGBRenderer::SetGreenBandIndex(int band)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nGreen = band;
        }

        int CRasterRGBRenderer::GetBlueBandIndex() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nBlue;
        }

        void CRasterRGBRenderer::SetBlueBandIndex(int band)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nBlue = band;
        }

        int CRasterRGBRenderer::GetAlphaBandIndex() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nAlpha;
        }

        void CRasterRGBRenderer::SetAlphaBandIndex(int band)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nAlpha = band;
        }

        void CRasterRGBRenderer::SetBandIndices(int red, int green, int blue, int alpha)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nRed = red;
            m_nGreen = green;
            m_nBlue = blue;
            m_nAlpha = alpha;
        }

        int CRasterRGBRenderer::GetBrightness() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nBrightness;
        }

        void CRasterRGBRenderer::SetBrightness(int value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nBrightness = std::clamp(value, -100, 100);
        }

        int CRasterRGBRenderer::GetContrast() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nContrast;
        }

        void CRasterRGBRenderer::SetContrast(int value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nContrast = std::clamp(value, -100, 100);
        }

        std::vector<int> CRasterRGBRenderer::GetDisplayBands(int /*bandCount*/) const
        {
            std::vector<int> bands;
            for(int band : {m_nRed, m_nGreen, m_nBlue, m_nAlpha})
            {
                if(band >= 0 && std::find(bands.begin(), bands.end(), band) == bands.end())
                    bands.push_back(band);
            }
            return bands;
        }

        void CRasterRGBRenderer::PrepareDraw(GeoDatabase::IRasterDatasetPtr ptrRaster)
        {
            const int bandCount = ptrRaster->GetBandCount();

            // background test uses only the color bands
            m_vecDrawBands.clear();
            for(int band : {m_nRed, m_nGreen, m_nBlue})
            {
                if(band >= 0 && band < bandCount && std::find(m_vecDrawBands.begin(), m_vecDrawBands.end(), band) == m_vecDrawBands.end())
                    m_vecDrawBands.push_back(band);
            }

            // byte rasters: value -> stretched byte tables
            m_bByteRaster = ptrRaster->GetPixelType() == GeoDatabase::RasterPixelTypeUChar;
            m_vecByteLut.clear();
            if(m_bByteRaster)
            {
                m_vecByteLut.resize(bandCount);
                for(int b = 0; b < bandCount; ++b)
                {
                    m_vecByteLut[b].resize(256);
                    for(int v = 0; v < 256; ++v)
                        m_vecByteLut[b][v] = StretchToByte(v, b);
                }
            }

            // brightness / contrast
            const double contrast = 1. + m_nContrast / 100.;          // 0 .. 2
            const double brightness = m_nBrightness * 255. / 100.;    // -255 .. 255
            for(int v = 0; v < 256; ++v)
            {
                double res = (v - 127.5) * contrast + 127.5 + brightness;
                m_toneLut[v] = (uint8_t)std::clamp(std::lround(res), 0L, 255L);
            }
        }

        uint8_t CRasterRGBRenderer::ChannelValue(const uint8_t* pPixel, int band, GeoDatabase::eRasterPixelType pixelType, int sampleSize) const
        {
            if(m_bByteRaster)
                return m_vecByteLut[band][pPixel[band]];
            return StretchToByte(ReadRasterSample(pPixel + size_t(band) * sampleSize, pixelType), band);
        }

        void CRasterRGBRenderer::ConvertPixels(const uint8_t* pSrc, size_t count, int bandCount, GeoDatabase::eRasterPixelType pixelType, uint8_t* pBGRA) const
        {
            const int sampleSize = GetRasterSampleSize(pixelType);
            const size_t pixelSize = size_t(sampleSize) * bandCount;
            const bool bFloat = pixelType == GeoDatabase::RasterPixelTypeFloat || pixelType == GeoDatabase::RasterPixelTypeDouble;
            const int red = m_nRed < bandCount ? m_nRed : -1;
            const int green = m_nGreen < bandCount ? m_nGreen : -1;
            const int blue = m_nBlue < bandCount ? m_nBlue : -1;
            const int alpha = m_nAlpha < bandCount ? m_nAlpha : -1;

            for(size_t i = 0; i < count; ++i, pSrc += pixelSize, pBGRA += 4)
            {
                if(IsBackground(pSrc, m_vecDrawBands, pixelType, sampleSize))
                {
                    pBGRA[0] = pBGRA[1] = pBGRA[2] = pBGRA[3] = 0;
                    continue;
                }

                if(bFloat)
                {
                    bool bNaN = false;
                    for(int band : m_vecDrawBands)
                        bNaN = bNaN || std::isnan(ReadRasterSample(pSrc + size_t(band) * sampleSize, pixelType));
                    if(bNaN)
                    {
                        pBGRA[0] = pBGRA[1] = pBGRA[2] = pBGRA[3] = 0;
                        continue;
                    }
                }

                pBGRA[2] = red >= 0 ? m_toneLut[ChannelValue(pSrc, red, pixelType, sampleSize)] : 0;
                pBGRA[1] = green >= 0 ? m_toneLut[ChannelValue(pSrc, green, pixelType, sampleSize)] : 0;
                pBGRA[0] = blue >= 0 ? m_toneLut[ChannelValue(pSrc, blue, pixelType, sampleSize)] : 0;
                if(alpha >= 0)
                    pBGRA[3] = m_bByteRaster ? pSrc[alpha] : ChannelValue(pSrc, alpha, pixelType, sampleSize);
                else
                    pBGRA[3] = 255;
            }
        }

        void CRasterRGBRenderer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            TBase::Save(pObj);
            pObj->AddPropertyInt32("RedBand", m_nRed);
            pObj->AddPropertyInt32("GreenBand", m_nGreen);
            pObj->AddPropertyInt32("BlueBand", m_nBlue);
            pObj->AddPropertyInt32("AlphaBand", m_nAlpha);
            pObj->AddPropertyInt32("Brightness", m_nBrightness);
            pObj->AddPropertyInt32("Contrast", m_nContrast);
        }

        void CRasterRGBRenderer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            TBase::Load(pObj);
            m_nRed = pObj->GetPropertyInt32("RedBand", m_nRed);
            m_nGreen = pObj->GetPropertyInt32("GreenBand", m_nGreen);
            m_nBlue = pObj->GetPropertyInt32("BlueBand", m_nBlue);
            m_nAlpha = pObj->GetPropertyInt32("AlphaBand", m_nAlpha);
            m_nBrightness = std::clamp((int)pObj->GetPropertyInt32("Brightness", m_nBrightness), -100, 100);
            m_nContrast = std::clamp((int)pObj->GetPropertyInt32("Contrast", m_nContrast), -100, 100);
        }
    }
}
