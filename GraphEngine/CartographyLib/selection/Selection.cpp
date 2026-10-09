#include "Selection.h"

namespace GraphEngine {
    namespace Cartography {

        CSelection::CSelection(ILayersPtr ptrLayers) : m_ptrLayers(ptrLayers), m_nChangeCounter(0)
        {

        }

        CSelection::~CSelection()
        {

        }

        uint64_t CSelection::GetChangeCounter() const
        {
            return m_nChangeCounter;
        }

        void CSelection::AddRow(CommonLib::CGuid layerId, int64_t rowID)
        {
            std::lock_guard lock(m_mutex);
            if(m_features[layerId].insert(rowID).second)
                ++m_nChangeCounter;
        }

        bool CSelection::IsEmpty() const
        {
            std::lock_guard lock(m_mutex);
            return m_features.empty();
        }

        void CSelection::Clear()
        {
            std::lock_guard lock(m_mutex);
            if(m_features.empty())
                return;

            m_features.clear();
            ++m_nChangeCounter;
        }

        void CSelection::ClearForLayer(CommonLib::CGuid layerId)
        {
            std::lock_guard lock(m_mutex);
            if(m_features.erase(layerId) != 0)
                ++m_nChangeCounter;
        }

        void CSelection::RemoveFeature(CommonLib::CGuid layerId, int64_t rowID)
        {
            std::lock_guard lock(m_mutex);
            auto it = m_features.find(layerId);
            if(it == m_features.end())
                return;

            if(it->second.erase(rowID) == 0)
                return;

            if(it->second.empty())
                m_features.erase(it);
            ++m_nChangeCounter;
        }

        std::vector<ILayerPtr> CSelection::GetLayers() const
        {
            std::vector<ILayerPtr> vecLayers;
            ILayersPtr ptrLayers = m_ptrLayers.lock();
            if(!ptrLayers.get())
                return vecLayers;

            std::lock_guard lock(m_mutex);
            for(auto it = m_features.begin(); it != m_features.end(); ++it)
            {
                ILayerPtr ptrLayer = ptrLayers->GetLayerById(it->first);
                if(ptrLayer.get())
                    vecLayers.push_back(ptrLayer);
            }

            return vecLayers;
        }

        std::vector<int64_t> CSelection::GetFeatures(CommonLib::CGuid layerId) const
        {
            std::lock_guard lock(m_mutex);
            auto it = m_features.find(layerId);
            if(it == m_features.end())
                return std::vector<int64_t>();

            return std::vector<int64_t>(it->second.begin(), it->second.end());
        }

        void CSelection::Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            try
            {
                ILayersPtr ptrLayers = m_ptrLayers.lock();
                if(!ptrLayers.get())
                    return;

                // copy under the lock, draw without it: drawing can take long and must not block AddRow from UI
                Display::ISymbolPtr ptrSymbol;
                std::vector<std::pair<CommonLib::CGuid, std::vector<int64_t> > > vecFeatures;
                {
                    std::lock_guard lock(m_mutex);
                    ptrSymbol = m_ptrSymbol;
                    for(auto it = m_features.begin(); it != m_features.end(); ++it)
                    {
                        if(!it->second.empty())
                            vecFeatures.push_back(std::make_pair(it->first, std::vector<int64_t>(it->second.begin(), it->second.end())));
                    }
                }

                if(!ptrSymbol.get())
                    return;

                for(size_t i = 0, sz = vecFeatures.size(); i < sz; ++i)
                {
                    if(ptrTrackCancel.get() && !ptrTrackCancel->Continue())
                        break;

                    ILayerPtr ptrLayer = ptrLayers->GetLayerById(vecFeatures[i].first);
                    if(!ptrLayer.get())
                        continue;

                    IFeatureLayer* pFeatureLayer = dynamic_cast<IFeatureLayer*>(ptrLayer.get());
                    if(!pFeatureLayer)
                        continue;

                    if(!pFeatureLayer->GetVisible() || !pFeatureLayer->IsValid() || !pFeatureLayer->GetSelectable())
                        continue;

                    pFeatureLayer->DrawFeatures(DrawPhaseGeography, vecFeatures[i].second, ptrDisplay, ptrTrackCancel, ptrSymbol);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Selection: failed to draw", exc);
            }
        }

        Display::ISymbolPtr CSelection::GetSymbol() const
        {
            std::lock_guard lock(m_mutex);
            return m_ptrSymbol;
        }

        void CSelection::SetSymbol(Display::ISymbolPtr ptrSymbol)
        {
            std::lock_guard lock(m_mutex);
            m_ptrSymbol = ptrSymbol;
        }

    }
}
