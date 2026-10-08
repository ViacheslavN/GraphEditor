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

            // NoData: pixels whose displayed bands all have these values (one value per raster band) are transparent
            const std::vector<double>& GetBackgroundValues() const;
            void SetBackgroundValues(const std::vector<double>& values);
            bool GetDisplayBackground() const;      // true - background pixels are drawn
            void SetDisplayBackground(bool flag);

            // statistics used by the stretch; calculated on the first draw when not set
            CRasterStatisticsPtr GetStatistics() const;
            void SetStatistics(CRasterStatisticsPtr ptrStats);

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
            std::vector<double> m_vecBackgroundValues;
            bool m_bDisplayBackground;
            CRasterStatisticsPtr m_ptrStats;
            std::vector<SStretchRange> m_vecRanges;  // per raster band, valid during Draw
            GeoDatabase::eRasterPixelType m_drawPixelType;
        };
    }
}
