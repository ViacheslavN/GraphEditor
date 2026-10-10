#pragma once
#include "../Cartography.h"


namespace GraphEngine {
    namespace Cartography {

        class   CLayers : public ILayers
        {
        public:
            CLayers();
            virtual ~CLayers();

        private:
            CLayers(const CLayers&);
            CLayers& operator=(const CLayers&);

        public:
            // ILayers
            virtual int       GetLayerCount() const;
            virtual ILayerPtr GetLayer(int index) const;
            // searches the children of the group layers too
            virtual ILayerPtr GetLayerById(CommonLib::CGuid layerId) const;
            virtual void      AddLayer(ILayerPtr ptrLayer);
            virtual void      InsertLayer(ILayerPtr ptrLayer, int index);
            virtual void      RemoveLayer(ILayerPtr ptrLayer);
            virtual void      RemoveAllLayers();
            virtual void      MoveLayer(ILayerPtr ptrLayer, int index);

            // changes of the children of the group layers are counted too
            virtual uint64_t  GetChangeCounter() const;

        private:
            void InsertLayerImpl(ILayerPtr ptrLayer, int index);
            std::vector<ILayersPtr> GetGroupChildren() const;   // children of the group layers of the list
            static uint64_t GetChildrenCounter(const ILayerPtr& ptrLayer);

        private:
            std::vector<ILayerPtr> m_vecLayers;
            std::map<CommonLib::CGuid, ILayerPtr> m_layersById;
            mutable std::mutex m_mutex;
            std::atomic<uint64_t> m_nChangeCounter;
        };

    }
}
