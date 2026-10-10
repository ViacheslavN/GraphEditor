#include "FeatureLayer.h"
#include "../GisGeometry/Envelope.h"
#include "../GeoDatabase/QueryFilter.h"
#include "../DisplayLib/DisplayUtils.h"
#include "renders/RenderersLoader.h"
#include "../GeoDatabase/DatasetLoader.h"

namespace GraphEngine {
    namespace Cartography {

        CFeatureLayer::CFeatureLayer() : m_bSelectable(true), m_hasReferenceScale(false),
                                         m_dDrawingWidth(0.), m_bDrawingWidthScaleDependent(false)
        {
            m_nLayerSymbolID = FeatureLayerID;
        }

        CFeatureLayer::~CFeatureLayer()
        {

        }

        std::string CFeatureLayer::GetOIDFieldName() const
        {
            if(!m_sOIDField.empty())
                return m_sOIDField;

            return m_ptrTable.get() ? m_ptrTable->GetOIDFieldName() : std::string();
        }

        GeoDatabase::ISpatialFilterPtr CFeatureLayer::CreateDisplayFilter(Display::IDisplayPtr ptrDisplay, bool& bIntersects)
        {
            bIntersects = false;
            Display::IDisplayTransformationPtr ptrTrans = ptrDisplay->GetTransformation();

            CommonLib::bbox bbox = ptrTrans->GetFittedBounds();
            CalcBB(ptrDisplay, bbox);

            Geometry::ISpatialReferencePtr outSpatRef = ptrTrans->GetSpatialReference();
            Geometry::IEnvelopePtr fullEnv = m_ptrTable->GetExtent();
            Geometry::ISpatialReferencePtr spatRefFC = fullEnv.get() ? fullEnv->GetSpatialReference() : Geometry::ISpatialReferencePtr();

            Geometry::CEnvelope env(bbox, outSpatRef);
            if (fullEnv.get() && !env.Intersect(fullEnv))
                return GeoDatabase::ISpatialFilterPtr();

            bIntersects = true;

            GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
            ptrFilter->SetOutputSpatialReference(outSpatRef);
            ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
            ptrFilter->SetBB(ptrTrans->GetFittedBounds());
            ptrFilter->SetJoins(m_vecJoins);

            if(!m_sQuery.empty())
                ptrFilter->SetWhereClause(m_sQuery);

            double precision = ptrTrans->DeviceToMapMeasure(0.25);
            if (spatRefFC.get() && outSpatRef.get())
            {
                CommonLib::bbox box = ptrTrans->GetFittedBounds();
                box.xMin = (box.xMin + box.xMax) / 2;
                box.yMin = (box.yMin + box.yMax) / 2;
                box.xMax = box.xMin + precision;
                box.yMax = box.yMin + precision;
                if (!outSpatRef->Project(spatRefFC, box))
                    precision = 0.0;
                else
                    precision = (std::min)(box.xMax - box.xMin, box.yMax - box.yMin); // (std::min): windows.h min macro
            }

            ptrFilter->SetPrecision(precision);
            return ptrFilter;
        }

        IAnnotationRenderPtr CFeatureLayer::PrepareAnnotationRenderer(GeoDatabase::IQueryFilterPtr ptrFilter, Display::IDisplayPtr ptrDisplay, bool checkScale) const
        {
            // annotation is enabled when the layer has the annotation field and the annotation renderer
            if(!HasAnnoField() || !m_ptrAnnotationRenderer.get())
                return IAnnotationRenderPtr();

            if(checkScale)
            {
                double scale = ptrDisplay->GetTransformation()->GetScale();
                double maxScale = m_ptrAnnotationRenderer->GetMaximumScale();
                double minScale = m_ptrAnnotationRenderer->GetMinimumScale();
                if((maxScale != 0.0 && scale < maxScale) || (minScale != 0.0 && scale > minScale))
                    return IAnnotationRenderPtr();
            }

            if(!m_ptrAnnotationRenderer->CanRender(m_ptrTable, ptrDisplay))
                return IAnnotationRenderPtr();

            m_ptrAnnotationRenderer->PrepareFilter(m_ptrTable, ptrFilter, GetAnnoFieldName());
            return m_ptrAnnotationRenderer;
        }

        ILabelRendererPtr CFeatureLayer::PrepareLabelRenderer(GeoDatabase::IQueryFilterPtr ptrFilter, Display::IDisplayPtr ptrDisplay) const
        {
            // labels are enabled when the layer has the label field and the label renderer
            if(!HasLabelField() || !m_ptrLabelRenderer.get())
                return ILabelRendererPtr();

            double scale = ptrDisplay->GetTransformation()->GetScale();
            double maxScale = m_ptrLabelRenderer->GetMaximumScale();
            double minScale = m_ptrLabelRenderer->GetMinimumScale();
            if((maxScale != 0.0 && scale < maxScale) || (minScale != 0.0 && scale > minScale))
                return ILabelRendererPtr();

            if(!m_ptrLabelRenderer->CanRender(m_ptrTable, ptrDisplay))
                return ILabelRendererPtr();

            m_ptrLabelRenderer->PrepareFilter(m_ptrTable, ptrFilter, m_sLabelField);
            return m_ptrLabelRenderer;
        }

        void CFeatureLayer::DrawRows(GeoDatabase::ISelectCursorPtr ptrCursor, const std::vector<IFeatureRendererPtr>& vecRenderers, IAnnotationRenderPtr ptrAnnoRenderer,
                                     ILabelRendererPtr ptrLabelRenderer, ILabelDrawerPtr ptrLabelDrawer,
                                     Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, const std::unordered_set<int64_t>* pOids, Display::ISymbolPtr ptrCustomSymbol) const
        {
            int32_t nOidIndex = -1;
            if(pOids != nullptr)
            {
                nOidIndex = ptrCursor->FindFieldByName(GetOIDFieldName());
                if(nOidIndex < 0)
                    throw CommonLib::CExcBase("OID field {0} not found", GetOIDFieldName());
            }

            uint32_t nCheckCancelStep = GetCheckCancelStep() != 0 ? GetCheckCancelStep() : 100;
            GeoDatabase::IRowPtr ptrRow = ptrCursor->CreateRow();
            uint32_t nRow = 0;

            ptrDisplay->Lock();
            try
            {
                while (ptrCursor->Next())
                {
                    if (!(nRow % nCheckCancelStep))
                    {
                        if (ptrTrackCancel.get() && !ptrTrackCancel->Continue())
                            break;

                        ptrDisplay->UnLock();
                        ptrDisplay->Lock();
                    }
                    nRow++;

                    if(pOids != nullptr && pOids->find(ptrCursor->ReadInt64(nOidIndex)) == pOids->end())
                        continue;

                    ptrCursor->FillRow(ptrRow);

                    if(ptrCustomSymbol.get())
                    {
                        // custom symbol (selection): draw once, renderer is used only to get the shape
                        vecRenderers[0]->DrawFeature(ptrDisplay, ptrRow, ptrCustomSymbol);
                        continue;
                    }

                    for (size_t i = 0, sz = vecRenderers.size(); i < sz; ++i)
                        vecRenderers[i]->DrawFeature(ptrDisplay, ptrRow);

                    // annotation is drawn right after its feature
                    if (ptrAnnoRenderer.get())
                        ptrAnnoRenderer->DrawFeature(ptrDisplay, ptrRow);

                    // labels are only collected, the label drawer places and draws them after all layers
                    if (ptrLabelRenderer.get())
                        ptrLabelRenderer->AddLabel(ptrLabelDrawer, ptrRow, m_labelingOptions);

                }

                // symbols which collect the features (multi layer symbol with UseCache) draw them now
                if(!ptrCustomSymbol.get())
                {
                    for (size_t i = 0, sz = vecRenderers.size(); i < sz; ++i)
                    {
                        ISymbolSelectorPtr ptrSelector = vecRenderers[i]->GetSymbolSelector();
                        if(ptrSelector.get())
                            ptrSelector->FlushBuffers(ptrDisplay, ptrTrackCancel);
                    }
                }
            }
            catch (...)
            {
                ptrDisplay->UnLock();
                throw;
            }
            ptrDisplay->UnLock();
        }

        void CFeatureLayer::DrawFeatures(eDrawPhase phase, const std::vector<int64_t>& vecOids, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, Display::ISymbolPtr ptrCustomSymbol) const
        {
            try
            {
                if(!IsValid() || vecOids.empty() || !(phase & DrawPhaseGeography))
                    return;

                CFeatureLayer* pThis = const_cast<CFeatureLayer*>(this); // CalcBB changes device clip rect of the display
                bool bIntersects = false;
                GeoDatabase::ISpatialFilterPtr ptrFilter = pThis->CreateDisplayFilter(ptrDisplay, bIntersects);
                if(!bIntersects)
                    return;

                std::vector<IFeatureRendererPtr> vecRenderers;
                for (size_t i = 0, sz = m_vecRenderers.size(); i < sz; ++i)
                {
                    if (!m_vecRenderers[i]->CanRender(m_ptrTable, ptrDisplay))
                        continue;

                    m_vecRenderers[i]->PrepareFilter(m_ptrTable, ptrFilter);
                    vecRenderers.push_back(m_vecRenderers[i]);
                }

                if (vecRenderers.empty())
                    return;

                IAnnotationRenderPtr ptrAnnoRenderer;
                if (!ptrCustomSymbol.get()) // custom symbol - selection, drawn without annotations
                    ptrAnnoRenderer = PrepareAnnotationRenderer(ptrFilter, ptrDisplay, false);

                std::string sOIDField = GetOIDFieldName();
                if(ptrFilter->GetFieldSet()->Find(sOIDField) < 0)
                    ptrFilter->GetFieldSet()->Add(sOIDField);

                GeoDatabase::ISelectCursorPtr ptrCursor = m_ptrTable->Search(ptrFilter);
                if (!ptrCursor.get())
                    return;

                std::unordered_set<int64_t> oids(vecOids.begin(), vecOids.end());
                DrawRows(ptrCursor, vecRenderers, ptrAnnoRenderer, ILabelRendererPtr(), ILabelDrawerPtr(), ptrDisplay, ptrTrackCancel, &oids, ptrCustomSymbol);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to draw features, layer: {0}", m_sName, exc);
            }
        }

        void CFeatureLayer::DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            try
            {
                // geography and labels are drawn by one pass over the features:
                // the features are drawn, their labels are given to the label drawer of the map
                bool bGeography = (phase & DrawPhaseGeography) != 0;
                ILabelDrawerPtr ptrLabelDrawer = (phase & DrawPhaseLabeling) ? m_pLabelDrawer : ILabelDrawerPtr();
                if (!IsValid() || (!bGeography && !ptrLabelDrawer.get()))
                    return;

                bool bIntersects = false;
                GeoDatabase::ISpatialFilterPtr ptrFilter = CreateDisplayFilter(ptrDisplay, bIntersects);
                if(!bIntersects)
                    return;

                std::vector<IFeatureRendererPtr> vecRenderers;
                double scale = ptrDisplay->GetTransformation()->GetScale();
                for (size_t i = 0, sz = m_vecRenderers.size(); i < sz && bGeography; ++i)
                {
                    IFeatureRendererPtr ptrRender = m_vecRenderers[i];
                    double maxScale = ptrRender->GetMaximumScale();
                    double minScale = ptrRender->GetMinimumScale();
                    if((maxScale != 0.0 && scale < maxScale) || (minScale != 0.0 && scale > minScale))
                        continue;

                    if (!ptrRender->CanRender(m_ptrTable, ptrDisplay))
                        continue;

                    ptrRender->PrepareFilter(m_ptrTable, ptrFilter);
                    vecRenderers.push_back(ptrRender);
                }

                IAnnotationRenderPtr ptrAnnoRenderer;
                if (bGeography && !vecRenderers.empty())
                    ptrAnnoRenderer = PrepareAnnotationRenderer(ptrFilter, ptrDisplay, true);

                ILabelRendererPtr ptrLabelRenderer;
                if (ptrLabelDrawer.get())
                    ptrLabelRenderer = PrepareLabelRenderer(ptrFilter, ptrDisplay);

                if (vecRenderers.empty() && !ptrLabelRenderer.get())
                    return;

                GeoDatabase::ISelectCursorPtr ptrCursor = m_ptrTable->Search(ptrFilter);
                if (!ptrCursor.get())
                    return;

                DrawRows(ptrCursor, vecRenderers, ptrAnnoRenderer, ptrLabelRenderer, ptrLabelDrawer, ptrDisplay, ptrTrackCancel, nullptr, Display::ISymbolPtr());
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to draw, layer: {0}", m_sName, exc);
            }
        }

        bool CFeatureLayer::IsValid() const
        {
            return m_ptrTable.get() != 0 && m_vecRenderers.size() > 0;
        }

        void CFeatureLayer::CalcBB(Display::IDisplayPtr ptrDisplay, CommonLib::bbox& bb)
        {
            Display::GRect wndRC = ptrDisplay->GetTransformation()->GetDeviceRect();

            Display::GUnits dx = Display::CDisplayUtils::SymbolSizeToDeviceSize(ptrDisplay->GetTransformation(), m_dDrawingWidth, m_bDrawingWidthScaleDependent);
            if(dx < 2)
                dx = 2;

            double map_x = ptrDisplay->GetTransformation()->DeviceToMapMeasure(dx);

            bb.xMin -= map_x;
            bb.xMax += map_x;

            bb.yMin -= map_x;
            bb.yMax += map_x;

            wndRC.Inflate(dx, dx);
            ptrDisplay->GetTransformation()->SetDeviceClipRect(wndRC);
        }

        Geometry::IEnvelopePtr CFeatureLayer::GetExtent() const
        {

            if(!m_ptrTable.get())
                return Geometry::IEnvelopePtr();

            return m_ptrTable->GetExtent();
        }

        eDrawPhase CFeatureLayer::GetSupportedDrawPhases() const
        {
            return (eDrawPhase)(DrawPhaseGeography | DrawPhaseLabeling);
        }

        bool CFeatureLayer::IsActiveOnScale(double scale) const
        {
            if(!TBase::IsActiveOnScale(scale))
                return false;

            for(size_t i = 0; i < m_vecRenderers.size(); ++i)
            {
                double minScale = m_vecRenderers[i]->GetMinimumScale();
                double maxScale = m_vecRenderers[i]->GetMaximumScale();
                if((minScale == 0.0 || minScale > scale) && (maxScale == 0.0 || maxScale < scale))
                    return true;
            }

            return false;
        }

        void CFeatureLayer::SetJoins(const std::vector<GeoDatabase::IJoinPtr>& joins)
        {
            m_vecJoins = joins;
        }

        const std::string& CFeatureLayer::GetDisplayField() const
        {
            return m_sDisplayField;
        }

        void CFeatureLayer::SetDisplayField(const std::string& field)
        {
            m_sDisplayField = field;
        }

        const std::string&  CFeatureLayer::GetOIDField() const
        {
            return m_sOIDField;
        }

        void CFeatureLayer::SetOIDField(const  std::string& sField)
        {
            m_sOIDField = sField;
        }

        const std::string&  CFeatureLayer::GetShapeField() const
        {
            return m_sShapeField;
        }

        void  CFeatureLayer::SetShapeField(const  std::string& sField)
        {
            m_sShapeField = sField;
        }

        GeoDatabase::ITablePtr CFeatureLayer::GetLayerTable() const
        {
            return m_ptrTable;
        }

        void  CFeatureLayer::SetLayerTable( GeoDatabase::ITablePtr ptrTable)
        {
            m_ptrTable = ptrTable;
        }

        bool  CFeatureLayer::GetSelectable() const
        {
            return m_bSelectable;
        }

        void  CFeatureLayer::SetSelectable(bool flag)
        {
            m_bSelectable = flag;
        }

        int	  CFeatureLayer::GetRendererCount() const
        {
            return (int)m_vecRenderers.size();
        }

        IFeatureRendererPtr	CFeatureLayer::GetRenderer(int index) const
        {
            if(index < 0 || index >= (int)m_vecRenderers.size())
                throw CommonLib::CExcBase("FeatureLayer: failed to get renderer, out of range, index: {0}", index);

            return m_vecRenderers[index];
        }

        void  CFeatureLayer::AddRenderer(IFeatureRendererPtr renderer)
        {
            m_vecRenderers.push_back(renderer);
        }

        void  CFeatureLayer::RemoveRenderer(IFeatureRendererPtr renderer)
        {
            TFeatureRenderer::iterator it = std::find(m_vecRenderers.begin(), m_vecRenderers.end(), renderer);
            if(it != m_vecRenderers.end())
                m_vecRenderers.erase(it);
        }

        IAnnotationRenderPtr CFeatureLayer::GetAnnotationRenderer() const
        {
            return m_ptrAnnotationRenderer;
        }

        void  CFeatureLayer::SetAnnotationRenderer(IAnnotationRenderPtr ptrRenderer)
        {
            m_ptrAnnotationRenderer = ptrRenderer;
        }

        void CFeatureLayer::ClearRenders()
        {
            m_vecRenderers.clear();
        }

        const std::string&	CFeatureLayer::GetDefinitionQuery() const
        {
            return m_sQuery;
        }

        void	CFeatureLayer::SetDefinitionQuery(const std::string& sQuery)
        {
            m_sQuery = sQuery;
        }

        bool  CFeatureLayer::HasAnnoField() const {
            return  !m_sAnnotateField.empty();
        }

        const std::string&  CFeatureLayer::GetAnnoFieldName() const {
            return m_sAnnotateField;
        }

        void  CFeatureLayer::SetAnnoFieldName(const std::string& filedName) {
            m_sAnnotateField = filedName;
        }

        bool CFeatureLayer::HasLabelField() const
        {
            return !m_sLabelField.empty();
        }

        const std::string& CFeatureLayer::GetLabelFieldName() const
        {
            return m_sLabelField;
        }

        void CFeatureLayer::SetLabelFieldName(const std::string& labelName)
        {
            m_sLabelField = labelName;
        }

        ILabelRendererPtr CFeatureLayer::GetLabelRenderer() const
        {
            return m_ptrLabelRenderer;
        }

        void CFeatureLayer::SetLabelRenderer(ILabelRendererPtr ptrRenderer)
        {
            m_ptrLabelRenderer = ptrRenderer;
        }

        const SLabelingOptions& CFeatureLayer::GetLabelingOptions() const
        {
            return m_labelingOptions;
        }

        void CFeatureLayer::SetLabelingOptions(const SLabelingOptions& options)
        {
            m_labelingOptions = options;
        }

        void  CFeatureLayer::SelectFeatures(const CommonLib::bbox& extent, ISelectionPtr ptrSelection,  Geometry::ISpatialReferencePtr ptrOutSpatRef)
        {
            try
            {
                if(!GetSelectable() || !m_ptrTable.get() || !ptrSelection.get())
                    return;

                Geometry::IEnvelopePtr fullEnv  = m_ptrTable->GetExtent();

                Geometry::CEnvelope env(extent, ptrOutSpatRef);
                if(fullEnv.get() && !env.Intersect(fullEnv))
                    return;

                std::string sOIDField = GetOIDFieldName();

                GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
                ptrFilter->SetOutputSpatialReference(ptrOutSpatRef);
                ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
                ptrFilter->SetBB(extent);
                ptrFilter->SetJoins(m_vecJoins);
                if(!m_sQuery.empty())
                    ptrFilter->SetWhereClause(m_sQuery);
                ptrFilter->GetFieldSet()->Add(sOIDField);

                GeoDatabase::ISelectCursorPtr pCursor = m_ptrTable->Search(ptrFilter);
                if(!pCursor.get())
                    return;

                int32_t nOidIndex = pCursor->FindFieldByName(sOIDField);
                if(nOidIndex < 0)
                    throw CommonLib::CExcBase("OID field {0} not found", sOIDField);

                CommonLib::CGuid layerId = GetLayerId();
                while(pCursor->Next())
                {
                    ptrSelection->AddRow(layerId, pCursor->ReadInt64(nOidIndex));
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to select, layer: {0}", m_sName, exc);
            }
        }


        void CFeatureLayer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                TBase::Save(pObj);

                pObj->AddPropertyString("DisplayField", m_sDisplayField);
                pObj->AddPropertyString("OIDField", m_sOIDField);
                pObj->AddPropertyString("ShapeField", m_sShapeField);
                pObj->AddPropertyString("Query", m_sQuery);
                pObj->AddPropertyBool("Selectable", m_bSelectable);
                pObj->AddPropertyBool("HasReferenceScale", m_hasReferenceScale);
                pObj->AddPropertyDouble("DrawingWidth", m_dDrawingWidth);
                pObj->AddPropertyBool("DrawingWidthScaleDependent", m_bDrawingWidthScaleDependent);
                pObj->AddPropertyString("AnnotateField", m_sAnnotateField);
                pObj->AddPropertyString("LabelField", m_sLabelField);
                CommonLib::ISerializeObjPtr ptrRenders = pObj->CreateChildNode("Renderers");

                for (size_t i = 0, sz = m_vecRenderers.size(); i < sz; ++i)
                {
                    CommonLib::ISerializeObjPtr  ptrRenderer = ptrRenders->CreateChildNode("Renderer");
                    m_vecRenderers[i]->Save(ptrRenderer);
                }

                if(m_ptrAnnotationRenderer.get())
                {
                    CommonLib::ISerializeObjPtr ptrAnnoNode = pObj->CreateChildNode("AnnotationRenderer");
                    m_ptrAnnotationRenderer->Save(ptrAnnoNode);
                }

                if(m_ptrLabelRenderer.get())
                {
                    CommonLib::ISerializeObjPtr ptrLabelNode = pObj->CreateChildNode("LabelRenderer");
                    m_ptrLabelRenderer->Save(ptrLabelNode);
                }

                CommonLib::ISerializeObjPtr ptrLabelingOptionsNode = pObj->CreateChildNode("LabelingOptions");
                m_labelingOptions.Save(ptrLabelingOptionsNode);

                if(m_ptrTable.get())
                {
                    CommonLib::ISerializeObjPtr ptrTableNode= pObj->CreateChildNode("Table");
                    m_ptrTable->Save(ptrTableNode);
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save layer: {0}", m_sName, exc);
            }
        }

        void CFeatureLayer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                TBase::Load(pObj);

                m_sDisplayField = pObj->GetPropertyString("DisplayField", m_sDisplayField);
                m_sOIDField = pObj->GetPropertyString("OIDField", m_sOIDField);
                m_sShapeField = pObj->GetPropertyString("ShapeField", m_sShapeField);
                m_sQuery = pObj->GetPropertyString("Query", m_sQuery);
                m_bSelectable = pObj->GetPropertyBool("Selectable", m_bSelectable);
                m_hasReferenceScale = pObj->GetPropertyBool("HasReferenceScale", m_hasReferenceScale);
                m_dDrawingWidth = pObj->GetPropertyDouble("DrawingWidth", m_dDrawingWidth);
                m_bDrawingWidthScaleDependent = pObj->GetPropertyBool("DrawingWidthScaleDependent", m_bDrawingWidthScaleDependent);
                m_sAnnotateField = pObj->GetPropertyString("AnnotateField", m_sAnnotateField);
                m_sLabelField = pObj->GetPropertyString("LabelField", m_sLabelField);
                m_vecRenderers.clear();
                if(pObj->IsChildExists("Renderers"))
                {
                    CommonLib::ISerializeObjPtr ptrRenderens = pObj->GetChild("Renderers");
                    for (uint32_t i = 0, sz = ptrRenderens->GetChildCnt(); i < sz; ++i)
                    {
                        CommonLib::ISerializeObjPtr ptrRenderNode = ptrRenderens->GetChild(i);
                        IFeatureRendererPtr pRenderer =  CLoaderRenderers::LoadRenderer(ptrRenderNode);
                        if(pRenderer.get())
                            m_vecRenderers.push_back(pRenderer);
                    }
                }

                m_ptrAnnotationRenderer.reset();
                if(pObj->IsChildExists("AnnotationRenderer"))
                {
                    m_ptrAnnotationRenderer = std::dynamic_pointer_cast<IAnnotationRender>(CLoaderRenderers::LoadRenderer(pObj->GetChild("AnnotationRenderer")));
                    if(!m_ptrAnnotationRenderer.get())
                        throw CommonLib::CExcBase("AnnotationRenderer node doesn't contain an annotation renderer");
                }

                m_ptrLabelRenderer.reset();
                if(pObj->IsChildExists("LabelRenderer"))
                {
                    m_ptrLabelRenderer = std::dynamic_pointer_cast<ILabelRenderer>(CLoaderRenderers::LoadRenderer(pObj->GetChild("LabelRenderer")));
                    if(!m_ptrLabelRenderer.get())
                        throw CommonLib::CExcBase("LabelRenderer node doesn't contain a label renderer");
                }

                m_labelingOptions = SLabelingOptions();
                if(pObj->IsChildExists("LabelingOptions"))
                    m_labelingOptions.Load(pObj->GetChild("LabelingOptions"));

                if(pObj->IsChildExists("Table"))
                {
                    m_ptrTable = GeoDatabase::CDatasetLoader::LoadTable(pObj->GetChild("Table"));
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load layer: {0}", m_sName, exc);
            }
        }

    }
}
