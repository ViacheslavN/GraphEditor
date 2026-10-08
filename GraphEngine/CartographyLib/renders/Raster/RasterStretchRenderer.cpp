#include "RasterStretchRenderer.h"
#include "RasterPixelUtils.h"

#include <algorithm>
#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        CRasterStretchRenderer::CRasterStretchRenderer() : TBase(RasterStretchRendererID),
            m_nBand(0)
            , m_fromColor(0, 0, 0)
            , m_toColor(255, 255, 255)
            , m_bInvert(false)
            , m_bByteRaster(false)
        {
            m_stretchType = RasterStretchTypeStandardDeviation;
            memset(m_rampLut, 0, sizeof(m_rampLut));
        }

        CRasterStretchRenderer::~CRasterStretchRenderer()
        {

        }

        int CRasterStretchRenderer::GetBand() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nBand;
        }

        void CRasterStretchRenderer::SetBand(int band)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nBand = band;
        }

        Display::Color CRasterStretchRenderer::GetFromColor() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_fromColor;
        }

        void CRasterStretchRenderer::SetFromColor(const Display::Color& color)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_fromColor = color;
        }

        Display::Color CRasterStretchRenderer::GetToColor() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_toColor;
        }

        void CRasterStretchRenderer::SetToColor(const Display::Color& color)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_toColor = color;
        }

        void CRasterStretchRenderer::SetColorRamp(const Display::Color& fromColor, const Display::Color& toColor)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_fromColor = fromColor;
            m_toColor = toColor;
        }

        bool CRasterStretchRenderer::GetInvert() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_bInvert;
        }

        void CRasterStretchRenderer::SetInvert(bool flag)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_bInvert = flag;
        }

        Display::Color CRasterStretchRenderer::GetRampColor(double t) const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            t = std::clamp(t, 0., 1.);
            if(m_bInvert)
                t = 1. - t;
            auto lerp = [t](int a, int b) { return (Display::Color::ColorComponent)std::lround(a + (b - a) * t); };
            return Display::Color(lerp(m_fromColor.GetR(), m_toColor.GetR()), lerp(m_fromColor.GetG(), m_toColor.GetG()),
                                  lerp(m_fromColor.GetB(), m_toColor.GetB()), lerp(m_fromColor.GetA(), m_toColor.GetA()));
        }

        std::vector<int> CRasterStretchRenderer::GetDisplayBands(int /*bandCount*/) const
        {
            return std::vector<int>(1, m_nBand);
        }

        void CRasterStretchRenderer::PrepareDraw(GeoDatabase::IRasterDatasetPtr ptrRaster)
        {
            m_vecDrawBands.assign(1, m_nBand);

            for(int v = 0; v < 256; ++v)
            {
                Display::Color color = GetRampColor(v / 255.);
                m_rampLut[v][0] = color.GetB();
                m_rampLut[v][1] = color.GetG();
                m_rampLut[v][2] = color.GetR();
                m_rampLut[v][3] = color.GetA();
            }

            m_bByteRaster = ptrRaster->GetPixelType() == GeoDatabase::RasterPixelTypeUChar;
            m_byteLut.clear();
            if(m_bByteRaster)
            {
                m_byteLut.resize(256);
                for(int v = 0; v < 256; ++v)
                    m_byteLut[v] = StretchToByte(v, m_nBand);
            }
        }

        void CRasterStretchRenderer::ConvertPixels(const uint8_t* pSrc, size_t count, int bandCount, GeoDatabase::eRasterPixelType pixelType, uint8_t* pBGRA) const
        {
            const int sampleSize = GetRasterSampleSize(pixelType);
            const size_t pixelSize = size_t(sampleSize) * bandCount;
            if(m_nBand < 0 || m_nBand >= bandCount)
                return;

            const size_t bandOffset = size_t(m_nBand) * sampleSize;
            for(size_t i = 0; i < count; ++i, pSrc += pixelSize, pBGRA += 4)
            {
                if(IsBackground(pSrc, m_vecDrawBands, pixelType, sampleSize))
                {
                    pBGRA[0] = pBGRA[1] = pBGRA[2] = pBGRA[3] = 0;
                    continue;
                }

                uint8_t index;
                if(m_bByteRaster)
                    index = m_byteLut[pSrc[bandOffset]];
                else
                {
                    double v = ReadRasterSample(pSrc + bandOffset, pixelType);
                    if(std::isnan(v))
                    {
                        pBGRA[0] = pBGRA[1] = pBGRA[2] = pBGRA[3] = 0;
                        continue;
                    }
                    index = StretchToByte(v, m_nBand);
                }
                memcpy(pBGRA, m_rampLut[index], 4);
            }
        }

        void CRasterStretchRenderer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            TBase::Save(pObj);
            pObj->AddPropertyInt32("Band", m_nBand);
            pObj->AddPropertyBool("Invert", m_bInvert);
            m_fromColor.Save(pObj, "FromColor");
            m_toColor.Save(pObj, "ToColor");
        }

        void CRasterStretchRenderer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            TBase::Load(pObj);
            m_nBand = pObj->GetPropertyInt32("Band", m_nBand);
            m_bInvert = pObj->GetPropertyBool("Invert", m_bInvert);
            m_fromColor.Load(pObj, "FromColor");
            m_toColor.Load(pObj, "ToColor");
        }
    }
}
