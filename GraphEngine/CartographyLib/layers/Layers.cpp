#include "Layers.h"

namespace GraphEngine {
    namespace Cartography {

        CLayers::CLayers() : m_nChangeCounter(0)
        {

        }

        CLayers::~CLayers(){

        }

        uint64_t CLayers::GetChangeCounter() const
        {
            return m_nChangeCounter;
        }

        int CLayers::GetLayerCount() const
        {
            std::lock_guard lock(m_mutex);
            return (int)m_vecLayers.size();
        }

        ILayerPtr CLayers::GetLayer(int index) const
        {
            std::lock_guard lock(m_mutex);
            if(index < 0 || index >= (int)m_vecLayers.size())
                throw CommonLib::CExcBase("Layers: failed to get layer, out of range, index: {0}", index);

            return  m_vecLayers[index];
        }

        ILayerPtr CLayers::GetLayerById(CommonLib::CGuid layerId) const
        {
            std::lock_guard lock(m_mutex);

            auto it = m_layersById.find(layerId);
            if(it != m_layersById.end())
                return it->second;

            return  ILayerPtr();
        }

        void CLayers::InsertLayerImpl(ILayerPtr ptrLayer, int index)
        {
            if(!ptrLayer.get())
                throw CommonLib::CExcBase("Layers: failed to add layer, layer is null");

            auto it = m_layersById.find(ptrLayer->GetLayerId());
            if(it != m_layersById.end())
                throw CommonLib::CExcBase("Layers: failed to add layer, layer with id: {0}, name: {1} exists", ptrLayer->GetLayerId().ToAstr(false), ptrLayer->GetName());

            if(index < 0 || index >= (int)m_vecLayers.size())
                m_vecLayers.push_back(ptrLayer);
            else
                m_vecLayers.insert(m_vecLayers.begin() + index, ptrLayer);

            m_layersById.insert(std::make_pair(ptrLayer->GetLayerId(), ptrLayer));
            ++m_nChangeCounter;
        }

        void CLayers::AddLayer(ILayerPtr ptrLayer)
        {
            std::lock_guard lock(m_mutex);
            InsertLayerImpl(ptrLayer, -1);
        }

        void CLayers::InsertLayer(ILayerPtr ptrLayer, int index)
        {
            std::lock_guard lock(m_mutex);
            InsertLayerImpl(ptrLayer, index);
        }

        void CLayers::RemoveLayer(ILayerPtr ptrLayerToRemove)
        {
            std::lock_guard lock(m_mutex);
            CommonLib::CGuid layerId = ptrLayerToRemove->GetLayerId();
            auto it = m_layersById.find(layerId);
            if(it == m_layersById.end())
                throw CommonLib::CExcBase("Layers: failed to remove layer, layer with id: {0}, name: {1} dosen't exisit", layerId.ToAstr(false), ptrLayerToRemove->GetName());

            m_layersById.erase(it);
            m_vecLayers.erase(std::remove_if(m_vecLayers.begin(), m_vecLayers.end(), [&layerId](const ILayerPtr& ptrLayer){return layerId == ptrLayer->GetLayerId();}), m_vecLayers.end());
            ++m_nChangeCounter;
        }

        void CLayers::RemoveAllLayers()
        {
            std::lock_guard lock(m_mutex);
            if(m_vecLayers.empty())
                return;

            m_layersById.clear();
            m_vecLayers.clear();
            ++m_nChangeCounter;
        }

        void CLayers::MoveLayer(ILayerPtr ptrLayer, int index)
        {
            {
                std::lock_guard lock(m_mutex);
                CommonLib::CGuid layerId = ptrLayer->GetLayerId();
                auto it = std::find_if(m_vecLayers.begin(), m_vecLayers.end(), [&layerId](const ILayerPtr& ptr){return layerId == ptr->GetLayerId();});
                if(it == m_vecLayers.end())
                    throw CommonLib::CExcBase("Layers: failed to move layer, layer with id: {0}, name: {1} dosen't exisit", layerId.ToAstr(false), ptrLayer->GetName());

                ILayerPtr ptrStored = *it;
                m_vecLayers.erase(it);

                if(index < 0 || index >= (int)m_vecLayers.size())
                    m_vecLayers.push_back(ptrStored);
                else
                    m_vecLayers.insert(m_vecLayers.begin() + index, ptrStored);

                ++m_nChangeCounter;
            }
        }

    }
}
