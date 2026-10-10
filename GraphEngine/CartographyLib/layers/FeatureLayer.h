#pragma once
#include "../Cartography.h"
#include "LayerBase.h"
#include <unordered_set>


namespace GraphEngine {
    namespace Cartography {

        // the legend of the layer: the groups of the symbol selectors of its feature renderers
        // (annotation and labels are not in the legend)
        class  CFeatureLayer : public CLayerBase<IFeatureLayer>, public ILegendInfo
        {
        public:

            typedef CLayerBase<IFeatureLayer> TBase;
            CFeatureLayer();
            ~CFeatureLayer();

            virtual Geometry::IEnvelopePtr GetExtent() const;
            virtual eDrawPhase                GetSupportedDrawPhases() const;
            virtual bool                      IsValid() const;
            virtual bool                      IsActiveOnScale(double scale) const;

            // IFeatureLayer

            virtual const std::string&			     GetDefinitionQuery() const;
            virtual void							 SetDefinitionQuery(const std::string& );
            virtual void                             SetJoins(const std::vector<GeoDatabase::IJoinPtr>& joins);
            virtual const std::string&               GetDisplayField() const;
            virtual void                             SetDisplayField(const std::string& sField);
            virtual const std::string&               GetOIDField() const;
            virtual void                             SetOIDField(const  std::string& sField);
            virtual const std::string&               GetShapeField() const;
            virtual void                             SetShapeField(const  std::string& sField);
            virtual GeoDatabase::ITablePtr           GetLayerTable() const;
            virtual void                             SetLayerTable(GeoDatabase::ITablePtr ptrTable);
            virtual bool                             GetSelectable() const;
            virtual void                             SetSelectable(bool flag);
            virtual int								 GetRendererCount() const;
            virtual IFeatureRendererPtr				 GetRenderer(int index) const;
            virtual void							 AddRenderer(IFeatureRendererPtr ptrRenderer);
            virtual void							 RemoveRenderer(IFeatureRendererPtr	 ptrRenderer);
            virtual void							 ClearRenders();
            virtual void                             DrawFeatures(eDrawPhase phase, const std::vector<int64_t>& vecOids, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, Display::ISymbolPtr ptrCustomSymbol) const;
            virtual bool                             HasAnnoField() const ;
            virtual const std::string&               GetAnnoFieldName() const;
            virtual void                             SetAnnoFieldName(const std::string& filedName);
            virtual IAnnotationRenderPtr			 GetAnnotationRenderer() const;
            virtual void							 SetAnnotationRenderer(IAnnotationRenderPtr ptrRenderer);
            virtual bool                             HasLabelField() const;
            virtual const std::string&               GetLabelFieldName() const;
            virtual void                             SetLabelFieldName(const std::string& labelName);
            virtual ILabelRendererPtr                GetLabelRenderer() const;
            virtual void                             SetLabelRenderer(ILabelRendererPtr ptrRenderer);
            virtual const SLabelingOptions&          GetLabelingOptions() const;
            virtual void                             SetLabelingOptions(const SLabelingOptions& options);

            void DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr trackCancel);

            // ILegendInfo
            virtual int                              GetLegendGroupCount() const;
            virtual ILegendGroupPtr                  GetLegendGroup(int nIndex) const;

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);


            virtual void  SelectFeatures(const CommonLib::bbox& extent, ISelectionPtr ptrSelection,  Geometry::ISpatialReferencePtr ptrSpRef);
        private:
            void CalcBB(Display::IDisplayPtr ptrDisplay, CommonLib::bbox& bb);
            std::string GetOIDFieldName() const;
            std::vector<ILegendGroupPtr> CollectLegendGroups() const;
            GeoDatabase::ISpatialFilterPtr CreateDisplayFilter(Display::IDisplayPtr ptrDisplay, bool& bIntersects);
            IAnnotationRenderPtr PrepareAnnotationRenderer(GeoDatabase::IQueryFilterPtr ptrFilter, Display::IDisplayPtr ptrDisplay, bool checkScale) const;
            ILabelRendererPtr PrepareLabelRenderer(GeoDatabase::IQueryFilterPtr ptrFilter, Display::IDisplayPtr ptrDisplay) const;
            void DrawRows(GeoDatabase::ISelectCursorPtr ptrCursor, const std::vector<IFeatureRendererPtr>& vecRenderers, IAnnotationRenderPtr ptrAnnoRenderer,
                          ILabelRendererPtr ptrLabelRenderer, ILabelDrawerPtr ptrLabelDrawer,
                          Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, const std::unordered_set<int64_t>* pOids, Display::ISymbolPtr ptrCustomSymbol) const;
        private:
            typedef std::vector<IFeatureRendererPtr> TFeatureRenderer;

            std::string  m_sDisplayField;
            std::string   m_sQuery;
            std::string  m_sOIDField;
            std::string  m_sShapeField;
            TFeatureRenderer m_vecRenderers;
            bool m_bSelectable;
            bool m_hasReferenceScale;
            GeoDatabase::ITablePtr m_ptrTable;
            std::string  m_sAnnotateField;

            double                        m_dDrawingWidth;
            bool                          m_bDrawingWidthScaleDependent;

            std::vector<GeoDatabase::IJoinPtr> m_vecJoins;
            IAnnotationRenderPtr m_ptrAnnotationRenderer;

            std::string          m_sLabelField;
            ILabelRendererPtr    m_ptrLabelRenderer;
            SLabelingOptions     m_labelingOptions;

        };


    }

}
