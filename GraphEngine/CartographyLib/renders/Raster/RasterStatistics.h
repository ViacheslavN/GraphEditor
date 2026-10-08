#pragma once

#include "../../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        // Per-band statistics of a raster, used by the stretch of the raster renderers.
        // Calculated on a decimated copy of the raster (about maxSamples pixels), NaN and
        // background (NoData) values are skipped.
        class CRasterStatistics
        {
        public:
            struct SBandStats
            {
                bool   bValid = false;
                double dMin = 0.;
                double dMax = 0.;
                double dMean = 0.;
                double dStdDev = 0.;
                uint64_t nCount = 0;
            };

            CRasterStatistics();
            ~CRasterStatistics();

            // pNoData: one value per band (empty - not used)
            void Calculate(GeoDatabase::IRasterDatasetPtr ptrDataset, const std::vector<double>* pNoData,
                           Display::ITrackCancelPtr ptrTrackCancel, size_t maxSamples = 1024 * 1024);

            bool IsCalculated() const;
            int GetBandCount() const;
            const SBandStats& GetBandStats(int band) const;
            void SetBandStats(int band, const SBandStats& stats);
            void Clear();

            void Save(CommonLib::ISerializeObjPtr pObj) const;
            void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            std::vector<SBandStats> m_vecBands;
            bool m_bCalculated;
        };

        typedef std::shared_ptr<CRasterStatistics> CRasterStatisticsPtr;
    }
}
