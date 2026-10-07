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
            virtual ILayerPtr GetLayerById(CommonLib::CGuid layerId) const;
            virtual void      AddLayer(ILayerPtr ptrLayer);
            virtual void      InsertLayer(ILayerPtr ptrLayer, int index);
            virtual void      RemoveLayer(ILayerPtr ptrLayer);
            virtual void      RemoveAllLayers();
            virtual void      MoveLayer(ILayerPtr ptrLayer, int index);

            virtual void      SetOnRemoveAllLayers(OnRemoveAllLayers* pFunck, bool bAdd);
            virtual void      SetOnLayerAdded(OnLayerAdded* pFunck, bool bAdd);
            virtual void      SetOnLayerRemove(OnLayerRemove* pFunck, bool bAdd);
            virtual void      SetOnLayerMoved(OnLayerMoved* pFunck, bool bAdd);

        private:
            void InsertLayerImpl(ILayerPtr ptrLayer, int index);

        private:
            std::vector<ILayerPtr> m_vecLayers;
            std::map<CommonLib::CGuid, ILayerPtr> m_layersById;
            mutable std::mutex m_mutex;

            CommonLib::Event1<ILayers*>                 m_OnRemoveAllLayersEvent;
            CommonLib::Event2<ILayers*, ILayer*>        m_OnLayerAddedEvent;
            CommonLib::Event2<ILayers*, ILayer*>        m_OnLayerRemoveEvent;
            CommonLib::Event3<ILayers*, ILayer*, int>   m_OnLayerMovedEvent;
        };

    }
}
