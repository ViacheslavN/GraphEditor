#pragma once
#include "../Cartography.h"
#include "LayerBase.h"

namespace GraphEngine {
    namespace Cartography {

        // Group layer (ported from UniGIS GisDev GroupLayer): draws its child layers in their order,
        // the visibility and the scale range of the group are checked before the children (CLayerBase::Draw),
        // the label drawer given by the map is passed to the children.
        class CGroupLayer : public CLayerBase<IGroupLayer>
        {
        public:
            typedef CLayerBase<IGroupLayer> TBase;

            CGroupLayer();
            explicit CGroupLayer(const std::string& sName);
            virtual ~CGroupLayer();

            // ILayer
            virtual Geometry::IEnvelopePtr    GetExtent() const;
            virtual eDrawPhase                GetSupportedDrawPhases() const;
            virtual bool                      IsValid() const;
            virtual void                      SetLabelDrawer(ILabelDrawerPtr ptrLabelDrawer);

            // IGroupLayer
            virtual ILayersPtr                GetChildren() const;
            virtual bool                      GetExpanded() const;
            virtual void                      SetExpanded(bool flag);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        protected:
            virtual void DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

        private:
            std::vector<ILayerPtr> GetChildList() const;

        private:
            ILayersPtr m_ptrChildren;
            bool       m_bExpanded;
        };

        typedef std::shared_ptr<CGroupLayer> CGroupLayerPtr;
    }
}
