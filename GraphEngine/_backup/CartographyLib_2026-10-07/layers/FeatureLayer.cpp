#pragma once
#include "FeatureLayer.h"
#include "../GisGeometry/Envelope.h"
#include "../GeoDatabase/QueryFilter.h"
#include "../DisplayLib/DisplayUtils.h"
#include "renders/RenderersLoader.h"
#include "../GeoDatabase/DatasetLoader.h"

namespace GraphEngine {
    namespace Cartography {

        CFeatureLayer::CFeatureLayer()
        {
            m_nLayerSymbolID = FeatureLayerID;
        }

        CFeatureLayer::~CFeatureLayer()
        {

        }

        void CFeatureLayer::DrawFeatures(eDrawPhase phase, const std::vector<int64_t>& vecOids, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, Display::ISymbolPtr ptrCustomSymbol) const
        {
            if(!IsValid())
                return;

            double oldRefScale = ptrDisplay->GetTransformation()->GetReferenceScale();

            if(m_hasReferenceScale)
                ptrDisplay->GetTransformation()->SetReferenceScale(m_hasReferenceScale);

            if((phase & DrawPhaseGeography))
            {
            }


            if(m_hasReferenceScale)
                ptrDisplay->GetTransformation()->SetReferenceScale(oldRefScale);
        }

        void CFeatureLayer::DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            try
            {
                if (!IsValid())
                    return;

                double oldRefScale = ptrDisplay->GetTransformation()->GetReferenceScale();
                if (m_hasReferenceScale)
                    ptrDisplay->GetTransformation()->SetReferenceScale(m_hasReferenceScale);

                if ((phase & DrawPhaseGeography)) {

                    CommonLib::bbox bbox = ptrDisplay->GetTransformation()->GetFittedBounds();
                    Display::GRect oldClipRect = ptrDisplay->GetTransformation()->GetDeviceClipRect();


                    CalcBB(ptrDisplay, bbox);

                    Geometry::ISpatialReferencePtr outSpatRef = ptrDisplay->GetTransformation()->GetSpatialReference();

                    Geometry::IEnvelopePtr fullEnv = m_ptrTable->GetExtent();
                    Geometry::ISpatialReferencePtr spatRefFC = fullEnv->GetSpatialReference();

                    Geometry::CEnvelope env(bbox, outSpatRef);

                    if (!env.Intersect(fullEnv)) {
                        ptrDisplay->GetTransformation()->SetReferenceScale(oldRefScale);
                        return;
                    }
                    bbox = env.GetBoundingBox();

                    GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
                    ptrFilter->SetOutputSpatialReference(outSpatRef);
                    ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
                    ptrFilter->SetBB(ptrDisplay->GetTransformation()->GetFittedBounds());
                    double precision = ptrDisplay->GetTransformation()->DeviceToMapMeasure(0.25);

                    if (spatRefFC.get() && outSpatRef.get()) {
                        CommonLib::bbox box = ptrDisplay->GetTransformation()->GetFittedBounds();
                        box.xMin = (box.xMin + box.xMax) / 2;
                        box.yMin = (box.yMin + box.yMax) / 2;
                        box.xMax = box.xMin + precision;
                        box.yMax = box.yMin + precision;
                        if (!outSpatRef->Project(spatRefFC, box))
                            precision = 0.0;
                        else
                            precision = min(box.xMax - box.xMin, box.yMax - box.yMin);
                    }

                    ptrFilter->SetPrecision(precision);

                    std::vector<IFeatureRendererPtr> vecRenderes;
                    for (size_t i = 0, sz = m_vecRenderers.size(); i < sz; ++i) {
                        IFeatureRendererPtr ptrRender = m_vecRenderers[i];
                        if (!ptrRender->CanRender(m_ptrTable, ptrDisplay))
                            continue;

                        ptrRender->PrepareFilter(m_ptrTable, ptrFilter);
                        vecRenderes.push_back(ptrRender);
                    }

                    if (vecRenderes.empty()) {
                        ptrDisplay->GetTransformation()->SetReferenceScale(oldRefScale); //TO DO create holder
                        return;
                    }

                    GeoDatabase::ICursorPtr pCursor = m_ptrTable->Search(ptrFilter);
                    if (!pCursor.get()) {
                        ptrDisplay->GetTransformation()->SetReferenceScale(oldRefScale); //TO DO create holder
                        return;
                    }

                    GeoDatabase::IRowPtr pRow;
                    uint32_t nRow = 0;
                    ptrDisplay->Lock();
                    while (pCursor->Next()) {
                        if (!(nRow % GetCheckCancelStep())) {
                            if (!ptrTrackCancel->Continue())
                                break;

                            ptrDisplay->UnLock();
                            ptrDisplay->Lock();
                        }

                        for (size_t i = 0, sz = vecRenderes.size(); i < sz; ++i) {
                            vecRenderes[i]->DrawFeature(ptrDisplay, pRow);
                        }

                        nRow++;
                    }
                }

                if (m_hasReferenceScale)
                    ptrDisplay->GetTransformation()->SetReferenceScale(oldRefScale);
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
            return DrawPhaseGeography;
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
            return m_vecRenderers[index];
        }

        void  CFeatureLayer::AddRenderer(IFeatureRendererPtr renderer)
        {
            m_vecRenderers.push_back(renderer);
        }

        void  CFeatureLayer::RemoveRenderer(IFeatureRendererPtr renderer)
        {
            TFeatureRenderer::iterator it = std::find(m_vecRenderers.begin(), m_vecRenderers.end(), IFeatureRendererPtr(renderer));
            if(it != m_vecRenderers.end())
                m_vecRenderers.erase(it);
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

        void  CFeatureLayer::SelectFeatures(const CommonLib::bbox& extent, ISelectionPtr ptrSelection,  Geometry::ISpatialReferencePtr ptrOutSpatRef)
        {
            try
            {

                if(!GetSelectable())
                    return;

                if(!m_ptrTable.get())
                    return;

                Geometry::IEnvelopePtr fullEnv  = m_ptrTable->GetExtent();
                Geometry::ISpatialReferencePtr spatRefFC = fullEnv->GetSpatialReference();

                Geometry::CEnvelope env(extent, ptrOutSpatRef);
                if(!env.Intersect(fullEnv))
                {
                    return;
                }

                GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();

                ptrFilter->SetOutputSpatialReference(ptrOutSpatRef);
                ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
                ptrFilter->SetBB(extent);
                //ptrFilter->GetFieldSet()->Add(m_ptrTable->GetShapeFieldName());
                ptrFilter->GetFieldSet()->Add(m_ptrTable->GetOIDFieldName());
                /*	double precision = pDisplay->GetTransformation()->DeviceToMapMeasure(0.25);

                    if(spatRefFC.get() && outSpatRef.get())
                    {
                        GisBoundingBox box = pDisplay->GetTransformation()->GetFittedBounds();
                        box.xMin = (box.xMin + box.xMax) / 2;
                        box.yMin = (box.yMin + box.yMax) / 2;
                        box.xMax = box.xMin + precision;
                        box.yMax = box.yMin + precision;
                        if(!outSpatRef->Project(spatRefFC.get(), box))
                            precision = 0.0;
                        else
                            precision = min(box.xMax - box.xMin, box.yMax - box.yMin);
                    }

                    filter.SetPrecision(precision);*/



                GeoDatabase::ISelectCursorPtr pCursor = m_ptrTable->Search(ptrFilter);
                if(!pCursor.get())
                {
                    return;
                }

                int nOidIndex = -1;
                while(pCursor->Next())
                {
                    if(nOidIndex == 1) {
                        pCursor->FindFieldByName(m_sOIDField);
                        if(nOidIndex == -1)
                            throw CommonLib::CExcBase("FeatureLayer: Failed to select, OID field not found");
                    }

                    ptrSelection->AddRow(GetLayerId(), pCursor->ReadInt64(nOidIndex));
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
                pObj->AddPropertyString("Query", m_sQuery);
                pObj->AddPropertyBool("Selectable", m_bSelectable);
                pObj->AddPropertyBool("HasReferenceScale", m_hasReferenceScale);

                CommonLib::ISerializeObjPtr ptrRenders = pObj->CreateChildNode("Renderers");

                for (size_t i = 0, sz = m_vecRenderers.size(); i < sz; ++i)
                {
                    CommonLib::ISerializeObjPtr  ptrRenderer = ptrRenders->CreateChildNode("Renderer");
                    m_vecRenderers[i]->Save(ptrRenderer);
                }

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
                m_sQuery = pObj->GetPropertyString("Query", m_sQuery);
                m_bSelectable = pObj->GetPropertyBool("Selectable", m_bSelectable);
                m_hasReferenceScale = pObj->GetPropertyBool("HasReferenceScale", m_hasReferenceScale);

                CommonLib::ISerializeObjPtr ptrRenderens = pObj->GetChild("Renderers");
                if(ptrRenderens.get())
                {
                    for (int32_t i = 0, sz = ptrRenderens->GetChildCnt(); i < sz; ++i)
                    {
                        CommonLib::ISerializeObjPtr ptrRenderNode = ptrRenderens->GetChild(i);
                        IFeatureRendererPtr pRenderer =  CLoaderRenderers::LoadRenderer(ptrRenderNode);
                        if(pRenderer.get())
                            m_vecRenderers.push_back(pRenderer);
                    }
                }

                CommonLib::ISerializeObjPtr ptrTableNode = pObj->GetChild("Table");
                if(ptrTableNode.get())
                {
                    m_ptrTable = GeoDatabase::CDatasetLoader::LoadTable(ptrTableNode);
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load layer: {0}", m_sName, exc);
            }
        }

    }
}