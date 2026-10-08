#include "RasterLayer.h"
#include "../renders/RenderersLoader.h"
#include "../renders/Raster/RasterRGBRenderer.h"
#include "../renders/Raster/RasterStretchRenderer.h"
#include "../../GeoDatabase/DatasetLoader.h"

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

            const int bands = ptrDataset->GetBandCount();
            if(bands >= 3 || ptrDataset->GetPixelType() == GeoDatabase::RasterPixelTypeUChar)
            {
                CRasterRGBRendererPtr ptrRenderer = std::make_shared<CRasterRGBRenderer>();
                ptrRenderer->SetupForDataset(ptrDataset);
                return ptrRenderer;
            }

            return std::make_shared<CRasterStretchRenderer>();
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
