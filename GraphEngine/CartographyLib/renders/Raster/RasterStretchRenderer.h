#pragma once

#include "RasterRendererBase.h"

namespace GraphEngine {
    namespace Cartography {

        // Draws one band through a linear color ramp (FromColor -> ToColor, default black -> white).
        // Default stretch: standard deviation (2), as for elevation / single band imagery.
        class CRasterStretchRenderer : public CRasterRendererBase
        {
        public:
            typedef CRasterRendererBase TBase;

            CRasterStretchRenderer();
            virtual ~CRasterStretchRenderer();

            int  GetBand() const;
            void SetBand(int band);

            Display::Color GetFromColor() const;
            void SetFromColor(const Display::Color& color);
            Display::Color GetToColor() const;
            void SetToColor(const Display::Color& color);
            void SetColorRamp(const Display::Color& fromColor, const Display::Color& toColor);

            bool GetInvert() const;
            void SetInvert(bool flag);

            // color of a stretched value t (0..1), as used for drawing
            Display::Color GetRampColor(double t) const;

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        protected:
            virtual std::vector<int> GetDisplayBands(int bandCount) const;
            virtual void PrepareDraw(GeoDatabase::IRasterDatasetPtr ptrRaster);
            virtual void ConvertPixels(const uint8_t* pSrc, size_t count, int bandCount, GeoDatabase::eRasterPixelType pixelType, uint8_t* pBGRA) const;

        private:
            int m_nBand;
            Display::Color m_fromColor;
            Display::Color m_toColor;
            bool m_bInvert;

            // valid during Draw
            std::vector<int> m_vecDrawBands;
            uint8_t m_rampLut[256][4];         // B,G,R,A by stretched value
            std::vector<uint8_t> m_byteLut;    // byte rasters: value -> stretched value
            bool m_bByteRaster;
        };

        typedef std::shared_ptr<CRasterStretchRenderer> CRasterStretchRendererPtr;
    }
}
