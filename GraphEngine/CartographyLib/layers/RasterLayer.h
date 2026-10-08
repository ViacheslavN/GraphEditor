#pragma once
#include "../Cartography.h"
#include "LayerBase.h"

#include <mutex>


namespace GraphEngine {
    namespace Cartography {

        // Raster layer: draws an IRasterDataset with an IRasterRenderer.
        // SetRasterDataset creates a default renderer when none is set (CreateDefaultRenderer).
        class  CRasterLayer : public CLayerBase<IRasterLayer>
        {
        public:

            typedef CLayerBase<IRasterLayer> TBase;
            CRasterLayer();
            explicit CRasterLayer(GeoDatabase::IRasterDatasetPtr ptrDataset, IRasterRendererPtr ptrRenderer = IRasterRendererPtr());
            virtual  ~CRasterLayer();

            // ILayer
            virtual Geometry::IEnvelopePtr    GetExtent() const;
            virtual eDrawPhase                GetSupportedDrawPhases() const;
            virtual bool                      IsValid() const;

            // IRasterLayer
            virtual GeoDatabase::IRasterDatasetPtr  GetRasterDataset() const;
            virtual void                            SetRasterDataset(GeoDatabase::IRasterDatasetPtr ptrDataset);
            virtual IRasterRendererPtr              GetRenderer() const;
            virtual void                            SetRenderer(IRasterRendererPtr ptrRenderer);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            // RGB for 3+ bands and 8-bit single band rasters, stretch (std. deviation, black -> white) otherwise
            static IRasterRendererPtr CreateDefaultRenderer(GeoDatabase::IRasterDatasetPtr ptrDataset);

        protected:
            virtual void DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

        private:
            mutable std::mutex m_mutex;
            GeoDatabase::IRasterDatasetPtr m_ptrDataset;
            IRasterRendererPtr m_ptrRenderer;
        };

        typedef std::shared_ptr<CRasterLayer> CRasterLayerPtr;
    }
}
