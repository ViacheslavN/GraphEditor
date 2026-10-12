#include "Layers.h"

namespace GraphEngine {
    namespace Cartography {

        CLayers::CLayers()
        {

        }

        CLayers::~CLayers(){

        }

        int CLayers::GetLayerCount() const
        {
            std::lock_guard lock(m_mutex);
            return (int)m_vecLayers.size();
        }

        ILayerPtr CLayers::GetLayer(int index) const
        {
            std::lock_guard lock(m_mutex);
            if(index >= (int)m_vecLayers.size())
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

        void CLayers::AddLayer(ILayerPtr ptrLayer)
        {
            std::lock_guard lock(m_mutex);
            auto it = m_layersById.find(ptrLayer->GetLayerId());
            if(it != m_layersById.end())
                throw CommonLib::CExcBase("Layers: failed to add layer, layer with id: {0}, name: {1} exists", ptrLayer->GetLayerId().ToAstr(false), ptrLayer->GetName());

            m_vecLayers.push_back(ptrLayer);
            m_layersById.insert(std::make_pair(ptrLayer->GetLayerId(), ptrLayer));
        }

        void CLayers::InsertLayer(ILayerPtr ptrLayer, int index)
        {
            std::lock_guard lock(m_mutex);
            auto it = m_layersById.find(ptrLayer->GetLayerId());
            if(it != m_layersById.end())
                throw CommonLib::CExcBase("Layers: failed to insert layer, layer with id: {0}, name: {1} exists", ptrLayer->GetLayerId().ToAstr(false), ptrLayer->GetName());


            if(index > (int)m_vecLayers.size() - 1)
                m_vecLayers.push_back(ILayerPtr(ptrLayer));
            else
                m_vecLayers.insert(m_vecLayers.begin() + index, ILayerPtr(ptrLayer));

        }

        void CLayers::RemoveLayer(ILayerPtr ptrLayerToRemove)
        {
            std::lock_guard lock(m_mutex);
            auto it = m_layersById.find(ptrLayerToRemove->GetLayerId());
            if(it == m_layersById.end())
                throw CommonLib::CExcBase("Layers: failed to remove layer, layer with id: {0}, name: {1} dosen't exisit", ptrLayerToRemove->GetLayerId().ToAstr(false), ptrLayerToRemove->GetName());

            m_layersById.erase(ptrLayerToRemove->GetLayerId());
            m_vecLayers.erase( std::remove_if(m_vecLayers.begin(), m_vecLayers.end(), [&ptrLayerToRemove](ILayerPtr ptrLayer){return ptrLayerToRemove->GetLayerId() == ptrLayer->GetLayerId();}));
        }

        void CLayers::RemoveAllLayers()
        {
            std::lock_guard lock(m_mutex);
            m_layersById.clear();
            m_vecLayers.clear();

        }

        void CLayers::MoveLayer(ILayerPtr ptrLayer, int index)
        {
            std::lock_guard lock(m_mutex);

            RemoveLayer(ptrLayer);

            if(index > (int)m_vecLayers.size() - 1)
                m_vecLayers.push_back(ILayerPtr(ptrLayer));
            else
                m_vecLayers.insert(m_vecLayers.begin() + index, ptrLayer);
        }


    }
}