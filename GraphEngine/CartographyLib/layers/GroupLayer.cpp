#include "GroupLayer.h"
#include "Layers.h"
#include "LoaderLayers.h"
#include "../../GisGeometry/Envelope.h"

namespace GraphEngine {
    namespace Cartography {

        CGroupLayer::CGroupLayer() : m_ptrChildren(std::make_shared<CLayers>()), m_bExpanded(true)
        {
            m_nLayerSymbolID = GroupLayerID;
            m_bVisible = true;
        }

        CGroupLayer::CGroupLayer(const std::string& sName) : CGroupLayer()
        {
            m_sName = sName;
        }

        CGroupLayer::~CGroupLayer()
        {

        }

        std::vector<ILayerPtr> CGroupLayer::GetChildList() const
        {
            // the children can be changed by UI while the map is drawn, the list is taken once
            std::vector<ILayerPtr> vecLayers;
            for(int i = 0, sz = m_ptrChildren->GetLayerCount(); i < sz; ++i)
            {
                try
                {
                    vecLayers.push_back(m_ptrChildren->GetLayer(i));
                }
                catch (std::exception&)
                {
                    break;   // removed meanwhile
                }
            }
            return vecLayers;
        }

        Geometry::IEnvelopePtr CGroupLayer::GetExtent() const
        {
            Geometry::IEnvelopePtr ptrExtent;
            std::vector<ILayerPtr> vecLayers = GetChildList();
            for(size_t i = 0; i < vecLayers.size(); ++i)
            {
                Geometry::IEnvelopePtr ptrLayerExtent = vecLayers[i]->GetExtent();
                if(!ptrLayerExtent.get() || !(ptrLayerExtent->GetBoundingBox().type & CommonLib::bbox_type_normal))
                    continue;

                // the extent is in the coordinate system of the first child with an extent
                if(!ptrExtent.get())
                    ptrExtent = std::make_shared<Geometry::CEnvelope>(CommonLib::bbox(), ptrLayerExtent->GetSpatialReference());
                ptrExtent->Expand(ptrLayerExtent);
            }
            return ptrExtent;
        }

        eDrawPhase CGroupLayer::GetSupportedDrawPhases() const
        {
            int phases = DrawPhaseNone;
            std::vector<ILayerPtr> vecLayers = GetChildList();
            for(size_t i = 0; i < vecLayers.size(); ++i)
                phases |= vecLayers[i]->GetSupportedDrawPhases();
            return (eDrawPhase)phases;
        }

        bool CGroupLayer::IsValid() const
        {
            return true;   // an empty group is valid, it draws nothing
        }

        void CGroupLayer::SetLabelDrawer(ILabelDrawerPtr ptrLabelDrawer)
        {
            TBase::SetLabelDrawer(ptrLabelDrawer);
            std::vector<ILayerPtr> vecLayers = GetChildList();
            for(size_t i = 0; i < vecLayers.size(); ++i)
                vecLayers[i]->SetLabelDrawer(ptrLabelDrawer);
        }

        ILayersPtr CGroupLayer::GetChildren() const
        {
            return m_ptrChildren;
        }

        bool CGroupLayer::GetExpanded() const
        {
            return m_bExpanded;
        }

        void CGroupLayer::SetExpanded(bool flag)
        {
            m_bExpanded = flag;
        }

        void CGroupLayer::DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            std::vector<ILayerPtr> vecLayers = GetChildList();
            for(size_t i = 0; i < vecLayers.size(); ++i)
            {
                if(ptrTrackCancel.get() && !ptrTrackCancel->Continue())
                    break;

                vecLayers[i]->Draw(phase, ptrDisplay, ptrTrackCancel);
            }
        }

        void CGroupLayer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            TBase::Save(pObj);
            pObj->AddPropertyBool("Expanded", m_bExpanded);

            CommonLib::ISerializeObjPtr pLayersNode = pObj->CreateChildNode("Layers");
            std::vector<ILayerPtr> vecLayers = GetChildList();
            for(size_t i = 0; i < vecLayers.size(); ++i)
                vecLayers[i]->Save(pLayersNode->CreateChildNode("Layer"));
        }

        void CGroupLayer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);
                m_bExpanded = pObj->GetPropertyBool("Expanded", m_bExpanded);

                m_ptrChildren->RemoveAllLayers();
                if(pObj->IsChildExists("Layers"))
                {
                    CommonLib::ISerializeObjPtr pLayersNode = pObj->GetChild("Layers");
                    for(uint32_t i = 0, sz = pLayersNode->GetChildCnt(); i < sz; ++i)
                        m_ptrChildren->AddLayer(CLayersLoader::LoadLayer(pLayersNode->GetChild(i)));
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load group layer {0}", m_sName, exc);
                throw;
            }
        }
    }
}
