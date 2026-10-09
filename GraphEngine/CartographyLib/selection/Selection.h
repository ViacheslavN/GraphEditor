#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        class CSelection : public ISelection
        {
        public:
            // ptrLayers - layers of the map, used to resolve layer id -> layer on GetLayers/Draw
            CSelection(ILayersPtr ptrLayers);
            virtual ~CSelection();

        private:
            CSelection(const CSelection&);
            CSelection& operator=(const CSelection&);

        public:
            // ISelection
            virtual void                    AddRow(CommonLib::CGuid layerId, int64_t rowID);
            virtual void                    Clear();
            virtual void                    ClearForLayer(CommonLib::CGuid layerId);
            virtual void                    RemoveFeature(CommonLib::CGuid layerId, int64_t rowID);
            virtual std::vector<ILayerPtr>  GetLayers() const;
            virtual std::vector<int64_t>    GetFeatures(CommonLib::CGuid layerId) const;
            virtual void                    Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);
            virtual Display::ISymbolPtr     GetSymbol() const;
            virtual void                    SetSymbol(Display::ISymbolPtr ptrSymbol);
            virtual bool                    IsEmpty() const;
            virtual uint64_t                GetChangeCounter() const;

        private:
            typedef std::set<int64_t> TFeatureIDSet;
            typedef std::map<CommonLib::CGuid, TFeatureIDSet> TFeatureMap;

            std::weak_ptr<ILayers>   m_ptrLayers;
            TFeatureMap              m_features;
            Display::ISymbolPtr      m_ptrSymbol;
            mutable std::mutex       m_mutex;
            std::atomic<uint64_t>    m_nChangeCounter;
        };

    }
}
