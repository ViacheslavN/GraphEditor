#include "Map.h"
#include "layers/Layers.h"
#include "../GisGeometry/Envelope.h"
#include "../DisplayLib/Transformation/DisplayTransformation2D.h"
#include "../DisplayLib/Symbols/SymbolsLoader.h"
#include "../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"
#include "layers/LoaderLayers.h"
#include "selection/Selection.h"
#include "Labeling/LabelDrawer.h"

namespace GraphEngine {
    namespace Cartography {

        CMap::CMap() :
                m_dMinScale(0.0),
                m_dMaxScale(0.0),
                m_MapUnits(CommonLib::UnitsUnknown),
                m_bFlipVertical(false),
                m_bflipHorizontal(false),
                m_bViewPos(false),
                m_bHasReferenceScale(false),
                m_bDelayDrawing(false),
                m_dReferenceScale(0.),
                m_bCalcBB(false),
                m_bUserFullExtent(false),
                m_nFullExtentLayersCounter(0)
        {
            m_ptrLayers  = std::make_shared<CLayers>();
            m_ptrSelection = std::make_shared<CSelection>(m_ptrLayers);
            m_ptrLabelDrawer = std::make_shared<CLabelDrawer>();
        }

        CMap::~CMap()
        {

        }

        void CMap::ResetFullExtentIfLayersChanged() const
        {
            uint64_t nCounter = m_ptrLayers->GetChangeCounter();
            if(nCounter == m_nFullExtentLayersCounter)
                return;

            m_nFullExtentLayersCounter = nCounter;
            if(!m_bUserFullExtent)
                m_ptrFullExtent.reset();
        }

        const std::string& CMap::GetName() const
        {
            return m_sName;
        }

        void   CMap::SetName(const  std::string& sName)
        {
            m_sName = sName;
        }

        ILayersPtr   CMap::GetLayers() const
        {
            return m_ptrLayers;
        }

        void	CMap::SelectFeatures(const CommonLib::bbox& extent, bool resetSelection)
        {
            try
            {
                if(resetSelection)
                    m_ptrSelection->Clear();

                int layerCount = m_ptrLayers->GetLayerCount();
                for(int i = 0; i < layerCount; ++i)
                {
                    ILayerPtr pLayer = m_ptrLayers->GetLayer(i);
                    if(!pLayer->GetVisible() || !pLayer->IsValid() || pLayer->GetLayerTypeID() != FeatureLayerID)
                        continue;

                    IFeatureLayer *pFeatureLayer = dynamic_cast< IFeatureLayer *>(pLayer.get());
                    if(!pFeatureLayer)
                        throw CommonLib::CExcBase("Layer id {0} isn't feature layer", pLayer->GetLayerId().ToAstr(false));

                    if(!pFeatureLayer->GetSelectable())
                        continue;

                    pFeatureLayer->SelectFeatures(extent, m_ptrSelection, m_ptrSpatialRef);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: failed to select", exc);
            }
        }

        ISelectionPtr CMap::GetSelection() const
        {
            return m_ptrSelection;
        }

        Geometry::IEnvelopePtr   CMap::GetFullExtent(Geometry::ISpatialReferencePtr ptrSpatRef) const
        {
            try
            {
            ResetFullExtentIfLayersChanged();
            if(m_ptrFullExtent.get())
                return Geometry::IEnvelopePtr( m_ptrFullExtent->Clone());

            int layerCount = m_ptrLayers->GetLayerCount();
            if( layerCount == 0 )
            {
                CommonLib::bbox bbox;
                bbox.type = CommonLib::bbox_type_normal;
                bbox.xMin = bbox.yMin = -1;
                bbox.xMax = bbox.yMax = 1;
                return Geometry::IEnvelopePtr(new Geometry::CEnvelope(bbox, ptrSpatRef.get() ? ptrSpatRef : m_ptrSpatialRef));
            }

            Geometry::IEnvelopePtr pEnvelope = Geometry::IEnvelopePtr(new Geometry::CEnvelope(CommonLib::bbox(), ptrSpatRef.get() ? ptrSpatRef : m_ptrSpatialRef));
            for(int i = 0; i < layerCount; ++i)
            {
                Geometry::IEnvelopePtr ptrLayerExtent = m_ptrLayers->GetLayer(i)->GetExtent();
                if(ptrLayerExtent.get())
                    pEnvelope->Expand(ptrLayerExtent);
            }

            if(!(pEnvelope->GetBoundingBox().type & CommonLib::bbox_type_normal))
            {
                // no valid layer extent yet, don't cache
                CommonLib::bbox bbox;
                bbox.type = CommonLib::bbox_type_normal;
                bbox.xMin = bbox.yMin = -1;
                bbox.xMax = bbox.yMax = 1;
                pEnvelope->SetBoundingBox(bbox);
                return pEnvelope;
            }

            m_ptrFullExtent = pEnvelope;
            return m_ptrFullExtent->Clone();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: failed to get full extent", exc);
            }
        }

        void   CMap::SetFullExtent(Geometry::IEnvelopePtr ptrEnv)
        {
            if(!ptrEnv.get())
                return;

            m_ptrFullExtent = ptrEnv->Clone();
            m_bUserFullExtent = true;
        }

        Geometry::ISpatialReferencePtr CMap::GetSpatialReference() const
        {
            return m_ptrSpatialRef;
        }

        void   CMap::SetSpatialReference(Geometry::ISpatialReferencePtr spatRef)
        {
            m_ptrSpatialRef = spatRef;
        }

        namespace
        {
            // gives the label drawer to the layers for the time of drawing and takes it back
            // (also when the drawing throws), so the layers don't keep the drawer and its display
            class CLabelDrawerScope
            {
            public:
                CLabelDrawerScope(ILabelDrawerPtr ptrLabelDrawer, ILayersPtr ptrLayers, Display::IDisplayPtr ptrDisplay) :
                        m_ptrLabelDrawer(ptrLabelDrawer)
                {
                    if(!m_ptrLabelDrawer.get())
                        return;

                    m_ptrLabelDrawer->BeginLabeling(ptrDisplay);
                    for(int i = 0, sz = ptrLayers->GetLayerCount(); i < sz; ++i)
                    {
                        ILayerPtr ptrLayer = ptrLayers->GetLayer(i);
                        if(ptrLayer.get())
                        {
                            ptrLayer->SetLabelDrawer(m_ptrLabelDrawer);
                            m_vecLayers.push_back(ptrLayer);
                        }
                    }
                }

                ~CLabelDrawerScope()
                {
                    if(!m_ptrLabelDrawer.get())
                        return;

                    for(size_t i = 0; i < m_vecLayers.size(); ++i)
                        m_vecLayers[i]->SetLabelDrawer(ILabelDrawerPtr());
                    m_ptrLabelDrawer->EndLabeling();
                }

            private:
                ILabelDrawerPtr m_ptrLabelDrawer;
                std::vector<ILayerPtr> m_vecLayers;
            };
        }

        void  CMap::Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel) {
            PartialDraw(DrawPhaseAll, ptrDisplay, ptrTrackCancel);

        }

        void   CMap::PartialDraw( eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            try
            {
                if(m_bDelayDrawing)
                    return;

                Display::IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();
                double scale = ptrTrans->GetScale();

                double maximumScale = GetMaximumScale();
                double minimumScale = GetMinimumScale();

                double oldScale = ptrTrans->GetReferenceScale();
                //if(GetHasReferenceScale() && GetReferenceScale() != 0.0)
                if(m_bHasReferenceScale && m_dReferenceScale)
                    ptrTrans->SetReferenceScale(m_dReferenceScale);
                else
                    ptrTrans->SetReferenceScale(ptrTrans->GetScale());

                int layerCount = m_ptrLayers->GetLayerCount();

                // labeling: the layers give their labels to the label drawer while they are drawn (one pass over the features),
                // the labels are placed and drawn after the layers
                ILabelDrawerPtr ptrLabelDrawer = (phase & DrawPhaseLabeling) ? m_ptrLabelDrawer : ILabelDrawerPtr();
                CLabelDrawerScope labelScope(ptrLabelDrawer, m_ptrLayers, ptrDisplay);

                eDrawPhase phaseMask = (eDrawPhase)(phase & (DrawPhaseGeography | DrawPhaseAnnotation | (ptrLabelDrawer.get() ? DrawPhaseLabeling : DrawPhaseNone)));

                if ( phaseMask & DrawPhaseGeography )
                {

                    if ( m_ptrBackgroundSymbol.get() )
                    {
                        m_ptrBackgroundSymbol->Prepare(ptrDisplay);
                        m_ptrBackgroundSymbol->FillRect(ptrDisplay, ptrTrans->GetDeviceRect());
                        m_ptrBackgroundSymbol->Reset();
                    }
                }

                if(phaseMask)
                {
                    for(int i = 0; i < layerCount; i++)
                    {
                        if(!ptrTrackCancel->Continue())
                            break;


                        ILayerPtr pLayer = m_ptrLayers->GetLayer(i);
                        if(pLayer.get())
                        {
                            pLayer->Draw(phaseMask, ptrDisplay, ptrTrackCancel);
                        }

                    }

                }
                if ( phase & DrawPhaseSelection )
                {
                    if(m_ptrSelection.get())
                        m_ptrSelection->Draw(ptrDisplay, ptrTrackCancel);
                }

                // labels are above the features and the selection
                if (ptrLabelDrawer.get() && ptrTrackCancel->Continue())
                    ptrLabelDrawer->DrawLabels(ptrTrackCancel);

                if ( phase & DrawPhaseGraphics )
                {
                    for(int i = 0; i < layerCount; i++)
                    {
                        ILayerPtr pLayer = m_ptrLayers->GetLayer(i);
                        if(pLayer.get())
                        {
                            pLayer->Draw(DrawPhaseGraphics, ptrDisplay, ptrTrackCancel);
                        }
                    }

                    if(m_ptrGraphicsContainer.get())
                    {
                        for (uint32_t i = 0, sz = m_ptrGraphicsContainer->GetEnumCount(); i < sz; ++i)
                        {
                            IElementPtr pElement = m_ptrGraphicsContainer->GetElement(i);
                            pElement->Activate(ptrDisplay);
                            pElement->Draw(ptrDisplay, ptrTrackCancel);
                            pElement->Deactivate();

                        }
                    }

                }

                if(m_ptrForegroundSymbol.get())
                {
                    m_ptrForegroundSymbol->Prepare(ptrDisplay);
                    m_ptrForegroundSymbol->FillRect(ptrDisplay, ptrTrans->GetDeviceRect());
                    m_ptrForegroundSymbol->Reset();
                }

                ptrTrans->SetReferenceScale(oldScale);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: failed to draw", exc);
            }

        }

        ILabelDrawerPtr  CMap::GetLabelDrawer() const
        {
            return m_ptrLabelDrawer;
        }

        void   CMap::SetLabelDrawer(ILabelDrawerPtr ptrLabelDrawer)
        {
            m_ptrLabelDrawer = ptrLabelDrawer;
        }

        CommonLib::Units	CMap::GetMapUnits() const
        {
            if(!m_ptrSpatialRef.get())
                return m_MapUnits;

            CommonLib::Units ret = m_ptrSpatialRef->GetUnits();
            return (CommonLib::UnitsUnknown == ret) ? m_MapUnits : ret;
        }

        void   CMap::SetMapUnits( CommonLib::Units units )
        {
            m_MapUnits = units;
        }

        IGraphicsContainerPtr  CMap::GetGraphicsContainer() const
        {
            return m_ptrGraphicsContainer;
        }

        void       CMap::SetDelayDrawing(bool bDelay)
        {
            m_bDelayDrawing = bDelay;
        }

        IBookmarksPtr  CMap::GetBookmarks() const
        {
            return m_ptrBookmarks;
        }

        CommonLib::IPropertySetPtr  CMap::GetMapProperties()
        {
            return m_ptrPropertySet;
        }

        Display::IFillSymbolPtr	CMap::GetBackgroundSymbol() const
        {
            return m_ptrBackgroundSymbol;
        }

        void CMap::SetBackgroundSymbol(Display::IFillSymbolPtr ptrSymbol)
        {
            m_ptrBackgroundSymbol = ptrSymbol;
        }

        Display::IFillSymbolPtr	CMap::GetForegroundSymbol() const
        {
            return m_ptrForegroundSymbol;
        }

        void	 CMap::SetForegroundSymbol(Display::IFillSymbolPtr ptrSymbol)
        {
            m_ptrForegroundSymbol = ptrSymbol;
        }

        void	CMap::SetViewPos(const Display::ViewPosition& pos)
        {
            m_ViewPos = pos;
            m_bViewPos = true;
        }

        Display::ViewPosition	CMap::GetViewPos(bool bCalcIfAbsent, Display::IDisplayTransformationPtr ptrTrans)
        {
            try
            {
                if (!m_bViewPos && bCalcIfAbsent)
                {
                    Geometry::ISpatialReferencePtr pSprefUse;
                    if (m_ptrExtent.get())
                    {
                        pSprefUse = m_ptrExtent->GetSpatialReference();
                    }
                    if (!pSprefUse)
                    {
                        pSprefUse = m_ptrSpatialRef;
                    }

                    if(ptrTrans.get() == nullptr)
                        ptrTrans = std::make_shared<Display::CDisplayTransformation2D>(96, m_MapUnits, Display::GRect(0, 0, 1024, 768), m_ViewPos.m_dScale);

                    ptrTrans->SetSpatialReference(pSprefUse);
                    ptrTrans->SetMapVisibleRect(GetExtent(pSprefUse, true, ptrTrans)->GetBoundingBox());
                    m_ViewPos = Display::ViewPosition(ptrTrans->GetMapPos(), ptrTrans->GetScale(),ptrTrans->GetRotation());
                    m_bViewPos = true;
                }

                return m_ViewPos;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: get view pos", exc);
                throw;
            }
        }

        void	 CMap::SetExtent(Geometry::IEnvelopePtr ptrExtent)
        {
            if(!ptrExtent)
                return;

            m_ptrExtent = ptrExtent->Clone();
        }

        Geometry::IEnvelopePtr	CMap::GetExtent(Geometry::ISpatialReferencePtr pSpatRef, bool calc_if_absent, Display::IDisplayTransformationPtr ptrTrans)
        {
            try
            {

                if(pSpatRef.get()== nullptr)
                    pSpatRef = m_ptrSpatialRef;

                if (!m_ptrExtent.get() && calc_if_absent)
                {
                    if (m_bViewPos)
                    {
                        if(ptrTrans.get() == nullptr)
                            ptrTrans = std::make_shared<Display::CDisplayTransformation2D>(96, m_MapUnits, Display::GRect(0, 0, 1024, 768), m_ViewPos.m_dScale);


                        ptrTrans->SetSpatialReference(pSpatRef);
                        ptrTrans->SetMapPos(m_ViewPos.m_Center, m_ViewPos.m_dScale);
                        m_ptrExtent = std::make_shared<Geometry::CEnvelope>(ptrTrans->GetFittedBounds(), pSpatRef);
                    }
                    else
                    {
                        m_ptrExtent = GetFullExtent(pSpatRef);
                    }
                }

                m_ptrExtent->Project(pSpatRef);
                return m_ptrExtent;

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: failed to get extent", exc);
                throw;
            }
        }

        void 	  CMap::SetVerticalFlip(bool bFlag)
        {
            m_bFlipVertical = bFlag;
        }

        bool	  CMap::GetVerticalFlip() const
        {
            return m_bFlipVertical;
        }

        void	  CMap::SetHorizontalFlip(bool bFlag)
        {
            m_bflipHorizontal = bFlag;
        }
        bool	  CMap::GetHorizontalFlip() const
        {
            return m_bflipHorizontal;
        }

        double	CMap::GetMinimumScale()
        {
            return m_dMinScale;
        }
        void	CMap::SetMinimumScale(double scale)
        {
            m_dMinScale = scale;
        }
        double	CMap::GetMaximumScale()
        {
            return m_dMaxScale;
        }
        void	CMap::SetMaximumScale(double scale)
        {
            m_dMaxScale = scale;
        }
        bool	CMap::GetHasReferenceScale() const
        {
            return m_bHasReferenceScale;
        }
        void	CMap::SetHasReferenceScale(bool flag)
        {
            m_bHasReferenceScale = flag;
        }
        double	CMap::GetReferenceScale() const
        {
            return m_dReferenceScale;
        }
        void	CMap::SetReferenceScale(double scale)
        {
            m_dReferenceScale = scale;
        }


        void CMap::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                CommonLib::ISerializeObjPtr  pMapNode = pObj->CreateChildNode("Map");

                pMapNode->AddPropertyString("Name", m_sName);
                pMapNode->AddPropertyInt16U("MapUnits", m_MapUnits);
                pMapNode->AddPropertyDouble("MinScale", m_dMinScale);
                pMapNode->AddPropertyDouble("MaxScale", m_dMaxScale);
                pMapNode->AddPropertyBool("HasReferenceScale", m_bHasReferenceScale);
                pMapNode->AddPropertyDouble("ReferenceScale", m_dReferenceScale);
                pMapNode->AddPropertyBool("FlipVertical", m_bFlipVertical);
                pMapNode->AddPropertyBool("FlipHorizonta", m_bflipHorizontal);
                if(m_ptrSpatialRef.get())
                {
                    CommonLib::ISerializeObjPtr  pSpRefNode = pMapNode->CreateChildNode("SPRef");
                    m_ptrSpatialRef->Save(pSpRefNode);
                }

                if(m_ptrBackgroundSymbol.get())
                {
                    CommonLib::ISerializeObjPtr pBgSymbolNode = pMapNode->CreateChildNode("BackgroundSymbol");
                    m_ptrBackgroundSymbol->Save(pBgSymbolNode);
                }

                if(m_ptrForegroundSymbol.get())
                {
                    CommonLib::ISerializeObjPtr pFgSymbolNode = pMapNode->CreateChildNode("ForegroundSymbol");
                    m_ptrForegroundSymbol->Save(pFgSymbolNode);
                }

                if(m_ptrLabelDrawer.get())
                {
                    CommonLib::ISerializeObjPtr pLabelDrawerNode = pMapNode->CreateChildNode("LabelDrawer");
                    m_ptrLabelDrawer->Save(pLabelDrawerNode);
                }

                CommonLib::ISerializeObjPtr pLayersNode = pMapNode->CreateChildNode("Layers");
                for (int i = 0, sz = m_ptrLayers->GetLayerCount(); i < sz; ++i)
                {
                    CommonLib::ISerializeObjPtr pLayerNode = pLayersNode->CreateChildNode("Layer");
                    ILayerPtr pLayer = m_ptrLayers->GetLayer(i);
                    pLayer->Save(pLayerNode);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: failed to save", exc);
                throw;
            }

        }


        void CMap::Load(CommonLib::ISerializeObjPtr pObj)
        {

            try
            {
                // Save() writes the map into a "Map" child node
                CommonLib::ISerializeObjPtr pMapNode = pObj->IsChildExists("Map") ? pObj->GetChild("Map") : pObj;

                m_sName = pMapNode->GetPropertyString("Name", m_sName);
                m_MapUnits = (CommonLib::Units)pMapNode->GetPropertyInt16U("MapUnits", m_MapUnits);
                m_dMinScale = pMapNode->GetPropertyDouble("MinScale", m_dMinScale);
                m_dMaxScale = pMapNode->GetPropertyDouble("MaxScale", m_dMaxScale);
                m_bHasReferenceScale = pMapNode->GetPropertyBool("HasReferenceScale", m_bHasReferenceScale);
                m_dReferenceScale = pMapNode->GetPropertyDouble("ReferenceScale", m_dReferenceScale);
                m_bFlipVertical = pMapNode->GetPropertyBool("FlipVertical", m_bFlipVertical);
                m_bflipHorizontal = pMapNode->GetPropertyBool("FlipHorizonta", m_bflipHorizontal);

                if(pMapNode->IsChildExists("SPRef"))
                {
                    CommonLib::ISerializeObjPtr pSpRefNode = pMapNode->GetChild("SPRef");
                    m_ptrSpatialRef =  std::make_shared<Geometry::CSpatialReferenceProj4>();
                    m_ptrSpatialRef->Load(pSpRefNode);
                }

                if(pMapNode->IsChildExists("BackgroundSymbol"))
                {
                    CommonLib::ISerializeObjPtr pBgSymbolNode = pMapNode->GetChild("BackgroundSymbol");
                    m_ptrBackgroundSymbol = std::static_pointer_cast<Display::IFillSymbol>(Display::CSymbolsLoader::LoadSymbol(pBgSymbolNode));
                }

                if(pMapNode->IsChildExists("ForegroundSymbol"))
                {
                    CommonLib::ISerializeObjPtr pFgSymbolNode = pMapNode->GetChild("ForegroundSymbol");
                    m_ptrForegroundSymbol = std::static_pointer_cast<Display::IFillSymbol>(Display::CSymbolsLoader::LoadSymbol(pFgSymbolNode));
                }

                if(pMapNode->IsChildExists("LabelDrawer"))
                {
                    if(!m_ptrLabelDrawer.get())
                        m_ptrLabelDrawer = std::make_shared<CLabelDrawer>();
                    m_ptrLabelDrawer->Load(pMapNode->GetChild("LabelDrawer"));
                }

                if(pMapNode->IsChildExists("Layers"))
                {
                    CommonLib::ISerializeObjPtr pLayersNode = pMapNode->GetChild("Layers");
                    for (int i = 0, nCount = pLayersNode->GetChildCnt();  i< nCount; ++i)
                    {
                        CommonLib::ISerializeObjPtr pLayerNode = pLayersNode->GetChild(i);
                        ILayerPtr pLayer  =  CLayersLoader::LoadLayer(pLayerNode);
                        m_ptrLayers->AddLayer(pLayer);
                    }
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Map: failed to load", exc);
                throw;
            }

        }

    }
}