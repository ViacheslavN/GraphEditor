#include "RasterRendererBase.h"
#include "RasterPixelUtils.h"
#include "../../../GisGeometry/Envelope.h"
#include "../../../DisplayLib/GraphTypes/Bitmap.h"
#include "../../../GeoDatabase/GeoDatabaseRaster/RasterSpatialFilter.h"

#include <algorithm>
#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        namespace
        {
            const size_t kMaxWindowPixels = 32 * 1024 * 1024;

            // max value of the sub-byte types, 255 for bytes
            double ByteTypeMax(GeoDatabase::eRasterPixelType type)
            {
                switch(type)
                {
                    case GeoDatabase::RasterPixelType1Bit: return 1.;
                    case GeoDatabase::RasterPixelType2Bits: return 3.;
                    case GeoDatabase::RasterPixelType4Bits: return 15.;
                    default: return 255.;
                }
            }

            bool IsCanceled(const Display::ITrackCancelPtr& ptrTrackCancel)
            {
                return ptrTrackCancel.get() && !ptrTrackCancel->Continue();
            }
        }

        CRasterRendererBase::CRasterRendererBase(eRasterRendererID id) :
            m_rendererID(id)
            , m_nTransparency(0)
            , m_stretchType(RasterStretchTypeNone)
            , m_dStdDevCount(2.)
            , m_bDisplayBackground(true)
            , m_drawPixelType(GeoDatabase::RasterPixelTypeUnknown)
        {

        }

        CRasterRendererBase::~CRasterRendererBase()
        {

        }

        uint32_t CRasterRendererBase::GetRasterRendererID() const
        {
            return m_rendererID;
        }

        bool CRasterRendererBase::CanRender(GeoDatabase::IRasterDatasetPtr ptrRaster, Display::IDisplayPtr ptrDisplay) const
        {
            if(!ptrRaster.get() || !ptrDisplay.get())
                return false;
            if(GetRasterSampleSize(ptrRaster->GetPixelType()) == 0 || ptrRaster->GetBandCount() <= 0)
                return false;

            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            std::vector<int> bands = GetDisplayBands(ptrRaster->GetBandCount());
            for(int band : bands)
            {
                if(band >= ptrRaster->GetBandCount())
                    return false;
            }
            return !bands.empty();
        }

        int CRasterRendererBase::GetTransparency() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_nTransparency;
        }

        void CRasterRendererBase::SetTransparency(int percent)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nTransparency = std::clamp(percent, 0, 100);
        }

        eRasterStretchType CRasterRendererBase::GetStretchType() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_stretchType;
        }

        void CRasterRendererBase::SetStretchType(eRasterStretchType type)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_stretchType = type;
        }

        double CRasterRendererBase::GetStdDevCount() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_dStdDevCount;
        }

        void CRasterRendererBase::SetStdDevCount(double count)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if(count <= 0.)
                throw CommonLib::CExcBase("RasterRenderer: invalid standard deviation count");
            m_dStdDevCount = count;
        }

        const std::vector<double>& CRasterRendererBase::GetBackgroundValues() const
        {
            return m_vecBackgroundValues;
        }

        void CRasterRendererBase::SetBackgroundValues(const std::vector<double>& values)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_vecBackgroundValues = values;
            if(m_ptrStats.get())
                m_ptrStats->Clear(); // NoData values are excluded from the statistics
        }

        bool CRasterRendererBase::GetDisplayBackground() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_bDisplayBackground;
        }

        void CRasterRendererBase::SetDisplayBackground(bool flag)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_bDisplayBackground = flag;
        }

        CRasterStatisticsPtr CRasterRendererBase::GetStatistics() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrStats;
        }

        void CRasterRendererBase::SetStatistics(CRasterStatisticsPtr ptrStats)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_ptrStats = ptrStats;
        }

        bool CRasterRendererBase::IsStatisticsNeeded(GeoDatabase::eRasterPixelType pixelType) const
        {
            return m_stretchType != RasterStretchTypeNone || !IsRasterByteType(pixelType);
        }

        void CRasterRendererBase::PrepareStretch(GeoDatabase::IRasterDatasetPtr ptrRaster, Display::ITrackCancelPtr ptrTrackCancel)
        {
            const int bandCount = ptrRaster->GetBandCount();
            const bool bNeedStats = IsStatisticsNeeded(m_drawPixelType);

            if(bNeedStats)
            {
                if(!m_ptrStats.get())
                    m_ptrStats = std::make_shared<CRasterStatistics>();
                if(!m_ptrStats->IsCalculated() || m_ptrStats->GetBandCount() != bandCount)
                {
                    const std::vector<double>* pNoData = (!m_bDisplayBackground && !m_vecBackgroundValues.empty()) ? &m_vecBackgroundValues : nullptr;
                    m_ptrStats->Calculate(ptrRaster, pNoData, ptrTrackCancel);
                }
            }

            m_vecRanges.assign(bandCount, SStretchRange());
            for(int b = 0; b < bandCount; ++b)
            {
                SStretchRange& range = m_vecRanges[b];
                range.dMin = 0.;
                range.dMax = ByteTypeMax(m_drawPixelType);

                if(!bNeedStats || !m_ptrStats.get() || !m_ptrStats->IsCalculated() || b >= m_ptrStats->GetBandCount())
                    continue;

                const CRasterStatistics::SBandStats& stats = m_ptrStats->GetBandStats(b);
                if(!stats.bValid)
                    continue;

                if(m_stretchType == RasterStretchTypeStandardDeviation)
                {
                    range.dMin = (std::max)(stats.dMin, stats.dMean - stats.dStdDev * m_dStdDevCount);
                    range.dMax = (std::min)(stats.dMax, stats.dMean + stats.dStdDev * m_dStdDevCount);
                }
                else
                {
                    range.dMin = stats.dMin;
                    range.dMax = stats.dMax;
                }
            }
        }

        double CRasterRendererBase::StretchToUnit(double value, int band) const
        {
            if(band < 0 || band >= (int)m_vecRanges.size())
                return 0.;
            const SStretchRange& range = m_vecRanges[band];
            if(range.dMax <= range.dMin)
                return value < range.dMin ? 0. : (value > range.dMax ? 1. : 0.5);
            double t = (value - range.dMin) / (range.dMax - range.dMin);
            return std::clamp(t, 0., 1.);
        }

        uint8_t CRasterRendererBase::StretchToByte(double value, int band) const
        {
            return (uint8_t)std::lround(StretchToUnit(value, band) * 255.);
        }

        bool CRasterRendererBase::IsBackground(const uint8_t* pPixel, const std::vector<int>& bands, GeoDatabase::eRasterPixelType pixelType, int sampleSize) const
        {
            if(m_bDisplayBackground || m_vecBackgroundValues.empty() || bands.empty())
                return false;

            for(int band : bands)
            {
                if(band < 0 || band >= (int)m_vecBackgroundValues.size())
                    return false;
                if(ReadRasterSample(pPixel + size_t(band) * sampleSize, pixelType) != m_vecBackgroundValues[band])
                    return false;
            }
            return true;
        }

        void CRasterRendererBase::Draw(GeoDatabase::IRasterDatasetPtr ptrRaster, eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            if(!(phase & DrawPhaseGeography) || !ptrRaster.get() || !ptrDisplay.get())
                return;

            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            try
            {
                if(m_nTransparency >= 100)
                    return;

                Display::IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
                Geometry::IEnvelopePtr ptrExtent = ptrRaster->GetExtent();
                const int width = ptrRaster->GetWidth();
                const int height = ptrRaster->GetHeight();
                if(!ptrTrans.get() || !ptrExtent.get() || width <= 0 || height <= 0)
                    return;

                const CommonLib::bbox ext = ptrExtent->GetBoundingBox();
                const double psx = (ext.xMax - ext.xMin) / width;
                const double psy = (ext.yMax - ext.yMin) / height;
                if(!(psx > 0.) || !(psy > 0.))
                    return;

                Geometry::ISpatialReferencePtr ptrOutSpatRef = ptrTrans->GetSpatialReference();
                Geometry::ISpatialReferencePtr ptrRasterSpatRef = ptrRaster->GetSpatialReference();
                const bool bNeedTransform = ptrOutSpatRef.get() && ptrRasterSpatRef.get() && !ptrRasterSpatRef->IsEqual(ptrOutSpatRef);

                m_drawPixelType = ptrRaster->GetPixelType();
                // 1/2/4 bit rasters are delivered as bytes, the source type gives the value range
                if(m_drawPixelType == GeoDatabase::RasterPixelTypeUChar && IsRasterByteType(ptrRaster->GetSourcePixelType()))
                    m_drawPixelType = ptrRaster->GetSourcePixelType();

                // 1. device grid -> raster pixel coordinates
                const Display::GRect& devRect = ptrTrans->GetDeviceRect();
                const int x0 = (int)std::floor(devRect.xMin);
                const int y0 = (int)std::floor(devRect.yMin);
                const int x1 = (int)std::ceil(devRect.xMax);
                const int y1 = (int)std::ceil(devRect.yMax);
                if(x1 <= x0 || y1 <= y0)
                    return;

                const int nx = (x1 - x0 + kGridStep - 1) / kGridStep + 1;
                const int ny = (y1 - y0 + kGridStep - 1) / kGridStep + 1;
                auto nodeX = [&](int i) { return (std::min)(x0 + i * kGridStep, x1); };
                auto nodeY = [&](int j) { return (std::min)(y0 + j * kGridStep, y1); };

                const size_t nodes = size_t(nx) * ny;
                std::vector<Display::GPoint> devPts(nodes);
                std::vector<CommonLib::GisXYPoint> mapPts(nodes);
                for(int j = 0; j < ny; ++j)
                    for(int i = 0; i < nx; ++i)
                    {
                        devPts[size_t(j) * nx + i].x = nodeX(i);
                        devPts[size_t(j) * nx + i].y = nodeY(j);
                    }
                ptrTrans->DeviceToMap(devPts.data(), mapPts.data(), (int)nodes);

                std::vector<double> nodeCol(nodes);
                std::vector<double> nodeRow(nodes);
                std::vector<char> nodeValid(nodes, 1);
                for(size_t k = 0; k < nodes; ++k)
                {
                    CommonLib::GisXYPoint pt = mapPts[k];
                    if(bNeedTransform && !ptrOutSpatRef->Project(ptrRasterSpatRef, &pt))
                        nodeValid[k] = 0;
                    if(!std::isfinite(pt.x) || !std::isfinite(pt.y))
                        nodeValid[k] = 0;
                    nodeCol[k] = (pt.x - ext.xMin) / psx;
                    nodeRow[k] = (ext.yMax - pt.y) / psy;
                }

                // cells covering the raster, raster window, device footprint, pixel step
                const int cellsX = nx - 1;
                const int cellsY = ny - 1;
                std::vector<char> cellUsed(size_t(cellsX) * cellsY, 0);
                double winCol0 = width, winCol1 = 0., winRow0 = height, winRow1 = 0.;
                int fx0 = x1, fy0 = y1, fx1 = x0, fy1 = y0;
                double scaleSum = 0.;
                int scaleCount = 0;

                for(int j = 0; j < cellsY; ++j)
                {
                    for(int i = 0; i < cellsX; ++i)
                    {
                        const size_t k00 = size_t(j) * nx + i;
                        const size_t k10 = k00 + 1;
                        const size_t k01 = k00 + nx;
                        const size_t k11 = k01 + 1;
                        if(!nodeValid[k00] || !nodeValid[k10] || !nodeValid[k01] || !nodeValid[k11])
                            continue;

                        const double cMin = (std::min)((std::min)(nodeCol[k00], nodeCol[k10]), (std::min)(nodeCol[k01], nodeCol[k11]));
                        const double cMax = (std::max)((std::max)(nodeCol[k00], nodeCol[k10]), (std::max)(nodeCol[k01], nodeCol[k11]));
                        const double rMin = (std::min)((std::min)(nodeRow[k00], nodeRow[k10]), (std::min)(nodeRow[k01], nodeRow[k11]));
                        const double rMax = (std::max)((std::max)(nodeRow[k00], nodeRow[k10]), (std::max)(nodeRow[k01], nodeRow[k11]));
                        if(cMax < 0. || cMin > width || rMax < 0. || rMin > height)
                            continue;

                        cellUsed[size_t(j) * cellsX + i] = 1;
                        winCol0 = (std::min)(winCol0, cMin);
                        winCol1 = (std::max)(winCol1, cMax);
                        winRow0 = (std::min)(winRow0, rMin);
                        winRow1 = (std::max)(winRow1, rMax);
                        fx0 = (std::min)(fx0, nodeX(i));
                        fx1 = (std::max)(fx1, nodeX(i + 1));
                        fy0 = (std::min)(fy0, nodeY(j));
                        fy1 = (std::max)(fy1, nodeY(j + 1));

                        const double dx = nodeX(i + 1) - nodeX(i);
                        const double dy = nodeY(j + 1) - nodeY(j);
                        if(dx > 0. && dy > 0.)
                        {
                            // raster pixels per device pixel (square root of the Jacobian determinant)
                            const double dcdx = (nodeCol[k10] - nodeCol[k00]) / dx;
                            const double drdx = (nodeRow[k10] - nodeRow[k00]) / dx;
                            const double dcdy = (nodeCol[k01] - nodeCol[k00]) / dy;
                            const double drdy = (nodeRow[k01] - nodeRow[k00]) / dy;
                            scaleSum += std::sqrt(std::fabs(dcdx * drdy - dcdy * drdx));
                            ++scaleCount;
                        }
                    }
                }

                if(fx1 <= fx0 || fy1 <= fy0)
                    return;

                const int col0 = (std::max)(0, (int)std::floor(winCol0) - 1);
                const int col1 = (std::min)(width, (int)std::ceil(winCol1) + 1);
                const int row0 = (std::max)(0, (int)std::floor(winRow0) - 1);
                const int row1 = (std::min)(height, (int)std::ceil(winRow1) + 1);
                if(col1 <= col0 || row1 <= row0)
                    return;

                int step = scaleCount > 0 ? (int)std::floor(scaleSum / scaleCount) : 1;
                step = (std::max)(1, step);
                while(size_t((col1 - col0 + step - 1) / step) * size_t((row1 - row0 + step - 1) / step) > kMaxWindowPixels)
                    ++step;

                if(IsCanceled(ptrTrackCancel))
                    return;

                PrepareStretch(ptrRaster, ptrTrackCancel);
                PrepareDraw(ptrRaster);
                if(IsCanceled(ptrTrackCancel))
                    return;

                // 2. read and convert the raster window (raster spatial reference, no output reference)
                CommonLib::bbox winBB;
                winBB.type = CommonLib::bbox_type_normal;
                winBB.xMin = ext.xMin + (col0 + 0.01) * psx;
                winBB.xMax = ext.xMin + (col1 - 0.01) * psx;
                winBB.yMax = ext.yMax - (row0 + 0.01) * psy;
                winBB.yMin = ext.yMax - (row1 - 0.01) * psy;

                GeoDatabase::IRasterSpatialFilterPtr ptrFilter = std::make_shared<GeoDatabase::CRasterSpatialFilter>(winBB);
                ptrFilter->SetPixelStep(step);
                ptrFilter->SetBlockSize(512, 512);

                GeoDatabase::IRasterCursorPtr ptrCursor = ptrRaster->Search(ptrFilter);
                // the window selected by the cursor (the same rounding as the blocks positions)
                int c0 = 0, r0 = 0, winSrcW = 0, winSrcH = 0;
                ptrCursor->GetPixelWindow(c0, r0, winSrcW, winSrcH);
                step = ptrCursor->GetPixelStep();
                const int winW = (winSrcW + step - 1) / step;
                const int winH = (winSrcH + step - 1) / step;
                if(winW <= 0 || winH <= 0)
                    return;

                std::vector<uint8_t> window(size_t(winW) * winH * 4, 0);
                GeoDatabase::IRasterBlockPtr ptrBlock = ptrCursor->CreateBlock();
                while(ptrCursor->Next(ptrBlock))
                {
                    if(IsCanceled(ptrTrackCancel))
                        return;

                    const int ox = (ptrBlock->GetSourceCol() - c0) / step;
                    const int oy = (ptrBlock->GetSourceRow() - r0) / step;
                    const int bw = (std::min)(ptrBlock->GetWidth(), winW - ox);
                    if(ox < 0 || oy < 0 || bw <= 0)
                        continue;

                    const uint8_t* pData = static_cast<const uint8_t*>(ptrBlock->GetData());
                    const size_t rowSize = size_t(ptrBlock->GetWidth()) * ptrBlock->GetPixelSize();
                    for(int y = 0; y < ptrBlock->GetHeight() && oy + y < winH; ++y)
                    {
                        ConvertPixels(pData + size_t(y) * rowSize, bw, ptrBlock->GetBandCount(), ptrBlock->GetPixelType(),
                                      window.data() + (size_t(oy + y) * winW + ox) * 4);
                    }
                }

                // 3. resample to the device footprint (nearest raster pixel)
                const int fw = fx1 - fx0;
                const int fh = fy1 - fy0;
                std::vector<uint8_t> image(size_t(fw) * fh * 4, 0);

                for(int j = 0; j < cellsY; ++j)
                {
                    if(IsCanceled(ptrTrackCancel))
                        return;

                    for(int i = 0; i < cellsX; ++i)
                    {
                        if(!cellUsed[size_t(j) * cellsX + i])
                            continue;

                        const size_t k00 = size_t(j) * nx + i;
                        const size_t k10 = k00 + 1;
                        const size_t k01 = k00 + nx;
                        const size_t k11 = k01 + 1;
                        const int cx0 = nodeX(i), cx1 = nodeX(i + 1);
                        const int cy0 = nodeY(j), cy1 = nodeY(j + 1);
                        const double cw = cx1 - cx0;
                        const double ch = cy1 - cy0;

                        for(int py = cy0; py < cy1; ++py)
                        {
                            const double v = (py + 0.5 - cy0) / ch;
                            const double colL = nodeCol[k00] + (nodeCol[k01] - nodeCol[k00]) * v;
                            const double rowL = nodeRow[k00] + (nodeRow[k01] - nodeRow[k00]) * v;
                            const double colR = nodeCol[k10] + (nodeCol[k11] - nodeCol[k10]) * v;
                            const double rowR = nodeRow[k10] + (nodeRow[k11] - nodeRow[k10]) * v;
                            uint8_t* pOutRow = image.data() + size_t(py - fy0) * fw * 4;

                            for(int px = cx0; px < cx1; ++px)
                            {
                                const double u = (px + 0.5 - cx0) / cw;
                                const double col = colL + (colR - colL) * u;
                                const double row = rowL + (rowR - rowL) * u;
                                if(col < 0. || row < 0. || col >= width || row >= height)
                                    continue;

                                const int wx = ((int)col - c0) / step;
                                const int wy = ((int)row - r0) / step;
                                if((int)col < c0 || (int)row < r0 || wx >= winW || wy >= winH)
                                    continue;

                                memcpy(pOutRow + size_t(px - fx0) * 4, window.data() + (size_t(wy) * winW + wx) * 4, 4);
                            }
                        }
                    }
                }

                // 4. draw
                const unsigned char alpha = (unsigned char)std::lround(255. * (100 - m_nTransparency) / 100.);
                Display::BitmapPtr ptrBitmap = std::make_shared<Display::CBitmap>(image.data(), (size_t)fw, (size_t)fh, Display::BitmapFormatType32bppARGB, nullptr, false);
                Display::IGraphicsPtr ptrGraphics = ptrDisplay->GetGraphics();
                if(!ptrGraphics.get())
                    return;

                ptrDisplay->Lock();
                try
                {
                    // rows are top-down
                    ptrGraphics->DrawBitmap(ptrBitmap, Display::GRect(fx0, fy0, fx1, fy1), true, alpha);
                }
                catch (...)
                {
                    ptrDisplay->UnLock();
                    throw;
                }
                ptrDisplay->UnLock();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("RasterRenderer failed to draw", exc);
                throw;
            }
        }

        void CRasterRendererBase::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            pObj->AddPropertyInt32U("RasterRendererID", (uint32_t)m_rendererID);
            pObj->AddPropertyInt32("Transparency", m_nTransparency);
            pObj->AddPropertyInt32("StretchType", (int32_t)m_stretchType);
            pObj->AddPropertyDouble("StdDevCount", m_dStdDevCount);
            pObj->AddPropertyBool("DisplayBackground", m_bDisplayBackground);

            CommonLib::ISerializeObjPtr ptrValues = pObj->CreateChildNode("BackgroundValues");
            for(double value : m_vecBackgroundValues)
                ptrValues->CreateChildNode("Value")->AddPropertyDouble("Value", value);

            if(m_ptrStats.get() && m_ptrStats->IsCalculated())
                m_ptrStats->Save(pObj->CreateChildNode("Statistics"));
        }

        void CRasterRendererBase::Load(CommonLib::ISerializeObjPtr pObj)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nTransparency = std::clamp((int)pObj->GetPropertyInt32("Transparency", m_nTransparency), 0, 100);
            m_stretchType = (eRasterStretchType)pObj->GetPropertyInt32("StretchType", (int32_t)m_stretchType);
            m_dStdDevCount = pObj->GetPropertyDouble("StdDevCount", m_dStdDevCount);
            m_bDisplayBackground = pObj->GetPropertyBool("DisplayBackground", m_bDisplayBackground);

            m_vecBackgroundValues.clear();
            if(pObj->IsChildExists("BackgroundValues"))
            {
                std::vector<CommonLib::ISerializeObjPtr> vecValues = pObj->GetChild("BackgroundValues")->GetChilds("Value");
                for(size_t i = 0; i < vecValues.size(); ++i)
                    m_vecBackgroundValues.push_back(vecValues[i]->GetPropertyDouble("Value", 0.));
            }

            m_ptrStats.reset();
            if(pObj->IsChildExists("Statistics"))
            {
                m_ptrStats = std::make_shared<CRasterStatistics>();
                m_ptrStats->Load(pObj->GetChild("Statistics"));
            }
        }
    }
}
