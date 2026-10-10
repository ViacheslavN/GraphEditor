#include "RasterLayer.h"
#include "../renders/RenderersLoader.h"
#include "../renders/Raster/RasterRGBRenderer.h"
#include "../renders/Raster/RasterStretchRenderer.h"
#include "../../GeoDatabase/DatasetLoader.h"
#include "../legend/Legend.h"
#include "../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        CRasterLayer::CRasterLayer()
        {
            m_nLayerSymbolID = RasterLayerID;
        }

        CRasterLayer::CRasterLayer(GeoDatabase::IRasterDatasetPtr ptrDataset, IRasterRendererPtr ptrRenderer)
        {
            m_nLayerSymbolID = RasterLayerID;
            m_ptrRenderer = ptrRenderer;
            SetRasterDataset(ptrDataset);
            if(ptrDataset.get())
                m_sName = ptrDataset->GetDatasetViewName();
        }

        CRasterLayer::~CRasterLayer()
        {

        }

        IRasterRendererPtr CRasterLayer::CreateDefaultRenderer(GeoDatabase::IRasterDatasetPtr ptrDataset)
        {
            if(!ptrDataset.get())
                return std::make_shared<CRasterRGBRenderer>();

            // the pixels without data (GDAL_NODATA) are transparent and aren't in the statistics of the stretch
            const int bands = ptrDataset->GetBandCount();
            double dNoData = 0.;
            const bool bNoData = ptrDataset->GetNoDataValue(dNoData) && !std::isnan(dNoData);

            if(bands >= 3 || ptrDataset->GetPixelType() == GeoDatabase::RasterPixelTypeUChar)
            {
                CRasterRGBRendererPtr ptrRenderer = std::make_shared<CRasterRGBRenderer>();
                ptrRenderer->SetupForDataset(ptrDataset);
                if(bNoData && ptrRenderer->GetBackgroundValues().empty())
                {
                    ptrRenderer->SetBackgroundValues(std::vector<double>(bands, dNoData));
                    ptrRenderer->SetDisplayBackground(false);
                }
                return ptrRenderer;
            }

            std::shared_ptr<CRasterStretchRenderer> ptrRenderer = std::make_shared<CRasterStretchRenderer>();
            if(bNoData)
            {
                ptrRenderer->SetBackgroundValues(std::vector<double>(bands, dNoData));
                ptrRenderer->SetDisplayBackground(false);
            }
            return ptrRenderer;
        }

        IRasterRendererPtr CRasterLayer::CreateRenderer(eRasterRendererID id, GeoDatabase::IRasterDatasetPtr ptrDataset, IRasterRendererPtr ptrFrom)
        {
            std::shared_ptr<CRasterRendererBase> ptrRenderer;
            if(id == RasterRGBRendererID)
            {
                CRasterRGBRendererPtr ptrRGB = std::make_shared<CRasterRGBRenderer>();
                ptrRGB->SetupForDataset(ptrDataset);
                ptrRenderer = ptrRGB;
            }
            else if(id == RasterStretchRendererID)
            {
                std::shared_ptr<CRasterStretchRenderer> ptrStretch = std::make_shared<CRasterStretchRenderer>();
                if(ptrDataset.get() && ptrDataset->GetPixelType() == GeoDatabase::RasterPixelTypeUChar)
                    ptrStretch->SetStretchType(RasterStretchTypeMinMax);
                ptrRenderer = ptrStretch;
            }
            else
                throw CommonLib::CExcBase("Unknown raster renderer {0}", (int)id);

            if(CRasterRendererBase* pFrom = dynamic_cast<CRasterRendererBase*>(ptrFrom.get()))
            {
                ptrRenderer->CopyCommonSettings(*pFrom);
            }
            else if(ptrDataset.get())
            {
                double dNoData = 0.;
                if(ptrDataset->GetNoDataValue(dNoData) && !std::isnan(dNoData))
                {
                    ptrRenderer->SetBackgroundValues(std::vector<double>(ptrDataset->GetBandCount(), dNoData));
                    ptrRenderer->SetDisplayBackground(false);
                }
            }
            return ptrRenderer;
        }

        namespace
        {
            Display::ISymbolPtr ColorBox(const Display::Color& color)
            {
                std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                ptrFill->SetColor(color);
                return ptrFill;
            }

            std::string ValueLabel(double dValue)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.6g", dValue);
                return buf;
            }

            std::string BandName(int band)
            {
                return "Band " + std::to_string(band + 1);
            }
        }

        int CRasterLayer::GetLegendGroupCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_ptrRenderer.get() ? 1 : 0;
        }

        ILegendGroupPtr CRasterLayer::GetLegendGroup(int nIndex) const
        {
            IRasterRendererPtr ptrRenderer;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                ptrRenderer = m_ptrRenderer;
            }
            if(nIndex != 0 || !ptrRenderer.get())
                throw CommonLib::CExcBase("Raster layer: legend group index out of range: {0}", nIndex);

            if(CRasterRGBRenderer* pRGB = dynamic_cast<CRasterRGBRenderer*>(ptrRenderer.get()))
            {
                CLegendGroupPtr ptrGroup = std::make_shared<CLegendGroup>("");
                const int red = pRGB->GetRedBandIndex(), green = pRGB->GetGreenBandIndex(), blue = pRGB->GetBlueBandIndex();
                if(red >= 0 && red == green && green == blue)
                {
                    ptrGroup->AddClass(std::make_shared<CLegendClass>("Gray: " + BandName(red), ColorBox(Display::Color(128, 128, 128))));
                    return ptrGroup;
                }
                if(red >= 0)
                    ptrGroup->AddClass(std::make_shared<CLegendClass>("Red: " + BandName(red), ColorBox(Display::Color(255, 0, 0))));
                if(green >= 0)
                    ptrGroup->AddClass(std::make_shared<CLegendClass>("Green: " + BandName(green), ColorBox(Display::Color(0, 255, 0))));
                if(blue >= 0)
                    ptrGroup->AddClass(std::make_shared<CLegendClass>("Blue: " + BandName(blue), ColorBox(Display::Color(0, 0, 255))));
                return ptrGroup;
            }

            if(CRasterStretchRenderer* pStretch = dynamic_cast<CRasterStretchRenderer*>(ptrRenderer.get()))
            {
                // the high value on the top, as QGIS shows it
                const int band = pStretch->GetBand();
                CLegendGroupPtr ptrGroup = std::make_shared<CLegendGroup>(BandName(band));
                double dMin = 0., dMax = 0.;
                bool bRange = pStretch->GetStretchRange(band, dMin, dMax);
                ptrGroup->AddClass(std::make_shared<CLegendClass>(bRange ? ValueLabel(dMax) : std::string("High"),
                                                                  ColorBox(pStretch->GetRampColor(1.))));
                ptrGroup->AddClass(std::make_shared<CLegendClass>(bRange ? ValueLabel(dMin) : std::string("Low"),
                                                                  ColorBox(pStretch->GetRampColor(0.))));
                return ptrGroup;
            }

            return std::make_shared<CLegendGroup>("");
        }

        Geometry::IEnvelopePtr CRasterLayer::GetExtent() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if(!m_ptrDataset.get())
                return Geometry::IEnvelopePtr();
            return m_ptrDataset->GetExtent();
        }

        eDrawPhase CRasterLayer::GetSupportedDrawPhases() const
        {
            return DrawPhaseGeography;
        }

        bool CRasterLayer::IsValid() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_ptrDataset.get() != nullptr && m_ptrRenderer.get() != nullptr;
        }

        GeoDatabase::IRasterDatasetPtr CRasterLayer::GetRasterDataset() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_ptrDataset;
        }

        void CRasterLayer::SetRasterDataset(GeoDatabase::IRasterDatasetPtr ptrDataset)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ptrDataset = ptrDataset;
            if(!m_ptrRenderer.get() && m_ptrDataset.get())
                m_ptrRenderer = CreateDefaultRenderer(m_ptrDataset);
        }

        IRasterRendererPtr CRasterLayer::GetRenderer() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_ptrRenderer;
        }

        void CRasterLayer::SetRenderer(IRasterRendererPtr ptrRenderer)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ptrRenderer = ptrRenderer;
        }

        void CRasterLayer::DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            try
            {
                if(!(phase & DrawPhaseGeography))
                    return;

                GeoDatabase::IRasterDatasetPtr ptrDataset;
                IRasterRendererPtr ptrRenderer;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    ptrDataset = m_ptrDataset;
                    ptrRenderer = m_ptrRenderer;
                }

                if(!ptrDataset.get() || !ptrRenderer.get())
                    return;
                if(!ptrRenderer->CanRender(ptrDataset, ptrDisplay))
                    return;

                ptrRenderer->Draw(ptrDataset, phase, ptrDisplay, ptrTrackCancel);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to draw, layer: {0}", m_sName, exc);
            }
        }

        void CRasterLayer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                TBase::Save(pObj);
                if(m_ptrRenderer.get())
                    m_ptrRenderer->Save(pObj->CreateChildNode("Renderer"));
                if(m_ptrDataset.get())
                    m_ptrDataset->Save(pObj->CreateChildNode("Dataset"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save layer: {0}", m_sName, exc);
            }
        }

        void CRasterLayer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                TBase::Load(pObj);

                m_ptrRenderer.reset();
                if(pObj->IsChildExists("Renderer"))
                    m_ptrRenderer = CLoaderRenderers::LoadRasterRenderer(pObj->GetChild("Renderer"));

                m_ptrDataset.reset();
                if(pObj->IsChildExists("Dataset"))
                    m_ptrDataset = GeoDatabase::CDatasetLoader::LoadRasterDataset(pObj->GetChild("Dataset"));

                if(!m_ptrRenderer.get() && m_ptrDataset.get())
                    m_ptrRenderer = CreateDefaultRenderer(m_ptrDataset);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load layer: {0}", m_sName, exc);
            }
        }
    }
}
