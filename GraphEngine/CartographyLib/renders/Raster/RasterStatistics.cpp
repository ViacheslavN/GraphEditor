#include "RasterStatistics.h"
#include "RasterPixelUtils.h"
#include "../../../GeoDatabase/GeoDatabaseRaster/RasterBlock.h"
#include "../../../GeoDatabase/GeoDatabaseRaster/RasterSpatialFilter.h"

#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        CRasterStatistics::CRasterStatistics() : m_bCalculated(false)
        {

        }

        CRasterStatistics::~CRasterStatistics()
        {

        }

        void CRasterStatistics::Calculate(GeoDatabase::IRasterDatasetPtr ptrDataset, const std::vector<double>* pNoData,
                                          Display::ITrackCancelPtr ptrTrackCancel, size_t maxSamples)
        {
            try
            {
                Clear();
                if(!ptrDataset.get())
                    return;

                const int bands = ptrDataset->GetBandCount();
                const GeoDatabase::eRasterPixelType pixelType = ptrDataset->GetPixelType();
                const int sampleSize = GetRasterSampleSize(pixelType);
                if(bands <= 0 || sampleSize == 0)
                    return;

                const double pixels = double(ptrDataset->GetWidth()) * double(ptrDataset->GetHeight());
                int step = 1;
                if(maxSamples > 0 && pixels > double(maxSamples))
                    step = (int)std::ceil(std::sqrt(pixels / double(maxSamples)));

                std::shared_ptr<GeoDatabase::CRasterSpatialFilter> ptrFilter = std::make_shared<GeoDatabase::CRasterSpatialFilter>();
                ptrFilter->SetPixelStep(step);
                ptrFilter->SetBlockSize(512, 512);

                // Welford's online mean / variance
                std::vector<SBandStats> stats(bands);
                std::vector<double> m2(bands, 0.);

                GeoDatabase::IRasterCursorPtr ptrCursor = ptrDataset->Search(ptrFilter);
                GeoDatabase::CRasterBlockPtr ptrBlock = std::make_shared<GeoDatabase::CRasterBlock>();
                while(ptrCursor->Next(ptrBlock))
                {
                    if(ptrTrackCancel.get() && !ptrTrackCancel->Continue())
                        return; // not calculated

                    const uint8_t* pData = static_cast<const uint8_t*>(ptrBlock->GetData());
                    const size_t count = size_t(ptrBlock->GetWidth()) * ptrBlock->GetHeight();
                    const size_t pixelSize = size_t(sampleSize) * bands;
                    for(size_t i = 0; i < count; ++i)
                    {
                        const uint8_t* pPixel = pData + i * pixelSize;
                        for(int b = 0; b < bands; ++b)
                        {
                            double v = ReadRasterSample(pPixel + size_t(b) * sampleSize, pixelType);
                            if(std::isnan(v))
                                continue;
                            if(pNoData && b < (int)pNoData->size() && v == (*pNoData)[b])
                                continue;

                            SBandStats& s = stats[b];
                            if(s.nCount == 0)
                            {
                                s.dMin = v;
                                s.dMax = v;
                            }
                            else
                            {
                                if(v < s.dMin) s.dMin = v;
                                if(v > s.dMax) s.dMax = v;
                            }
                            ++s.nCount;
                            double delta = v - s.dMean;
                            s.dMean += delta / double(s.nCount);
                            m2[b] += delta * (v - s.dMean);
                        }
                    }
                }

                for(int b = 0; b < bands; ++b)
                {
                    stats[b].bValid = stats[b].nCount > 0;
                    stats[b].dStdDev = stats[b].nCount > 1 ? std::sqrt(m2[b] / double(stats[b].nCount)) : 0.;
                }

                m_vecBands.swap(stats);
                m_bCalculated = true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to calculate raster statistics", exc);
                throw;
            }
        }

        bool CRasterStatistics::IsCalculated() const
        {
            return m_bCalculated;
        }

        int CRasterStatistics::GetBandCount() const
        {
            return (int)m_vecBands.size();
        }

        const CRasterStatistics::SBandStats& CRasterStatistics::GetBandStats(int band) const
        {
            if(band < 0 || band >= (int)m_vecBands.size())
                throw CommonLib::CExcBase("RasterStatistics: band out of range: {0}", band);
            return m_vecBands[band];
        }

        void CRasterStatistics::SetBandStats(int band, const SBandStats& stats)
        {
            if(band < 0)
                throw CommonLib::CExcBase("RasterStatistics: band out of range: {0}", band);
            if(band >= (int)m_vecBands.size())
                m_vecBands.resize(band + 1);
            m_vecBands[band] = stats;
            m_bCalculated = true;
        }

        void CRasterStatistics::Clear()
        {
            m_vecBands.clear();
            m_bCalculated = false;
        }

        void CRasterStatistics::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            pObj->AddPropertyBool("Calculated", m_bCalculated);
            for(size_t i = 0; i < m_vecBands.size(); ++i)
            {
                CommonLib::ISerializeObjPtr ptrBand = pObj->CreateChildNode("Band");
                ptrBand->AddPropertyBool("Valid", m_vecBands[i].bValid);
                ptrBand->AddPropertyDouble("Min", m_vecBands[i].dMin);
                ptrBand->AddPropertyDouble("Max", m_vecBands[i].dMax);
                ptrBand->AddPropertyDouble("Mean", m_vecBands[i].dMean);
                ptrBand->AddPropertyDouble("StdDev", m_vecBands[i].dStdDev);
            }
        }

        void CRasterStatistics::Load(CommonLib::ISerializeObjPtr pObj)
        {
            Clear();
            std::vector<CommonLib::ISerializeObjPtr> vecBands = pObj->GetChilds("Band");
            for(size_t i = 0; i < vecBands.size(); ++i)
            {
                CommonLib::ISerializeObjPtr ptrBand = vecBands[i];
                SBandStats s;
                s.bValid = ptrBand->GetPropertyBool("Valid", false);
                s.dMin = ptrBand->GetPropertyDouble("Min", 0.);
                s.dMax = ptrBand->GetPropertyDouble("Max", 0.);
                s.dMean = ptrBand->GetPropertyDouble("Mean", 0.);
                s.dStdDev = ptrBand->GetPropertyDouble("StdDev", 0.);
                s.nCount = s.bValid ? 1 : 0;
                m_vecBands.push_back(s);
            }
            m_bCalculated = pObj->GetPropertyBool("Calculated", false) && !m_vecBands.empty();
        }
    }
}
