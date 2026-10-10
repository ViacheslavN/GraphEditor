#pragma once

#include "../../Cartography.h"
#include "RasterStatistics.h"

#include <mutex>

namespace GraphEngine {
    namespace Cartography {

        // Common part of the raster renderers.
        //
        // Draw():
        //  1. the visible window of the raster is found by mapping a coarse grid of device points
        //     (every 16 px) to raster pixels (projected to the raster spatial reference when it differs
        //     from the display one), the pixel step is chosen from the display resolution;
        //  2. the window is read with the raster cursor and converted to BGRA by the concrete renderer
        //     (ConvertPixels: band selection, stretch, NoData/background -> transparent);
        //  3. every device pixel of the raster footprint takes the nearest raster pixel (positions
        //     interpolated inside the grid cells), so rotation, flips, reprojection and the 3D view work;
        //  4. the image is drawn with IGraphics::DrawBitmap 1:1 with the layer transparency.
        class CRasterRendererBase : public IRasterRenderer
        {
        public:
            explicit CRasterRendererBase(eRasterRendererID id);
            virtual ~CRasterRendererBase();

            // IRasterRenderer
            virtual uint32_t GetRasterRendererID() const;
            virtual bool     CanRender(GeoDatabase::IRasterDatasetPtr ptrRaster, Display::IDisplayPtr ptrDisplay) const;
            virtual void     Draw(GeoDatabase::IRasterDatasetPtr ptrRaster, eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

            // 0 - opaque .. 100 - invisible
            int  GetTransparency() const;
            void SetTransparency(int percent);

            eRasterStretchType GetStretchType() const;
            void SetStretchType(eRasterStretchType type);

            // number of standard deviations for RasterStretchTypeStandardDeviation (default 2)
            double GetStdDevCount() const;
            void   SetStdDevCount(double count);

            // the image adjustments, applied after the stretch in this order (as in QGIS):
            // brightness, contrast: -100 .. 100, 0 - unchanged;
            // gamma: 0.1 .. 10, 1 - unchanged, > 1 lighter (output = input ^ (1 / gamma));
            // saturation: -100 (grayscale) .. 100 (double), 0 - unchanged
            int    GetBrightness() const;
            void   SetBrightness(int value);
            int    GetContrast() const;
            void   SetContrast(int value);
            double GetGamma() const;
            void   SetGamma(double gamma);
            int    GetSaturation() const;
            void   SetSaturation(int value);

            static constexpr double kMinGamma = 0.1;
            static constexpr double kMaxGamma = 10.;

            // the percentiles for RasterStretchTypePercentClip (default 2 and 98)
            double GetClipLowPercent() const;
            double GetClipHighPercent() const;
            void   SetClipPercents(double low, double high);

            // NoData: pixels whose displayed bands all have these values (one value per raster band) are transparent
            const std::vector<double>& GetBackgroundValues() const;
            void SetBackgroundValues(const std::vector<double>& values);
            bool GetDisplayBackground() const;      // true - background pixels are drawn
            void SetDisplayBackground(bool flag);

            // statistics used by the stretch; calculated on the first draw when not set
            CRasterStatisticsPtr GetStatistics() const;
            void SetStatistics(CRasterStatisticsPtr ptrStats);

            // the values stretched to the ends of the color range for the band by the stretch type and the statistics
            // (the legend), false - the statistics aren't calculated yet (before the first drawing) or not needed (bytes as they are)
            bool GetStretchRange(int band, double& dMin, double& dMax) const;

            // the settings which don't depend on the kind of the renderer (transparency, stretch, NoData, adjustments,
            // statistics of the same dataset): switching RGB <-> stretch keeps them
            void CopyCommonSettings(const CRasterRendererBase& other);

            // ISerialize (common part)
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            static const int kGridStep = 16;

        protected:
            // bands which are displayed (statistics / NoData test)
            virtual std::vector<int> GetDisplayBands(int bandCount) const = 0;
            // called under the lock before the window is converted (cache the band list etc.)
            virtual void PrepareDraw(GeoDatabase::IRasterDatasetPtr ptrRaster) = 0;
            // converts count pixels (pixel-interleaved, bandCount bands of pixelType) to B,G,R,A bytes
            virtual void ConvertPixels(const uint8_t* pSrc, size_t count, int bandCount, GeoDatabase::eRasterPixelType pixelType, uint8_t* pBGRA) const = 0;

            // stretch of a value of the band to 0..1 (statistics must be prepared)
            double StretchToUnit(double value, int band) const;
            uint8_t StretchToByte(double value, int band) const;
            bool IsBackground(const uint8_t* pPixel, const std::vector<int>& bands, GeoDatabase::eRasterPixelType pixelType, int sampleSize) const;
            bool IsStatisticsNeeded(GeoDatabase::eRasterPixelType pixelType) const;
            // the brightness / contrast / gamma table and the saturation for Draw, false - nothing to adjust
            bool PrepareAdjustments();
            void ApplyAdjustments(uint8_t* pBGRA, size_t count) const;
            void PrepareStretch(GeoDatabase::IRasterDatasetPtr ptrRaster, Display::ITrackCancelPtr ptrTrackCancel);

        protected:
            struct SStretchRange
            {
                double dMin = 0.;
                double dMax = 255.;
            };

            mutable std::recursive_mutex m_mutex;
            eRasterRendererID m_rendererID;
            int m_nTransparency;
            eRasterStretchType m_stretchType;
            double m_dStdDevCount;
            int m_nBrightness;
            int m_nContrast;
            double m_dGamma;
            int m_nSaturation;
            uint8_t m_toneLut[256];         // valid during Draw
            bool m_bAdjust;
            double m_dClipLow;
            double m_dClipHigh;
            std::vector<double> m_vecBackgroundValues;
            bool m_bDisplayBackground;
            CRasterStatisticsPtr m_ptrStats;
            std::vector<SStretchRange> m_vecRanges;  // per raster band, valid during Draw
            GeoDatabase::eRasterPixelType m_drawPixelType;
        };
    }
}
