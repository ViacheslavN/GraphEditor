#pragma once
#include "Cartography.h"
#include "../CommonLib/CommonLib.h"
#include "../DisplayLib/DisplayLib.h"
#include "../GisGeometry/Geometry.h"
#include "../GeoDatabase/GeoDatabase.h"
#include "../CommonLib/utils/PropertySet.h"
#include "../CommonLib/Serialize/SerializeObj.h"
#include "../CommonLib/exception/exc_base.h"
#include "../DisplayLib/Symbols.h"

namespace GraphEngine {
    namespace Cartography {

        enum eSymbolSelectorID
        {
            UndefineSymbolSelectorID,
            SimpleSymbolSelectorID,
            UniqueValueSymbolSelectorID,
            RangeSymbolSelectorID
        };

        enum eFeatureRendererID
        {
            UndefineFeatureRendererID,
            SimpleFeatureRendererID,
            AnnotationRendererID,
            LabelRendererID
        };

        enum eRasterRendererID
        {
            UndefineRasterRendererID,
            RasterRGBRendererID,
            RasterStretchRendererID
        };

        enum eRasterStretchType
        {
            RasterStretchTypeNone = 0,               // 8-bit values as is, other types - min/max
            RasterStretchTypeStandardDeviation = 1,  // mean -/+ N standard deviations
            RasterStretchTypeMinMax = 2
        };

        enum eDrawPhase
        {
            DrawPhaseNone       = 0,
            DrawPhaseGeography  = 1,
            DrawPhaseAnnotation = 2,
            DrawPhaseLabeling   = 4,
            DrawPhaseSelection  = 8,
            DrawPhaseGraphics   = 16,
            DrawPhaseAll        = 0xFFFF
        };


        enum eLayerTypeID
        {
            UndefineLayerID,
            FeatureLayerID,
            RasterLayerID

        };

        // how a label is placed (ported from UniGIS labeling)
        enum eLabelStrategy
        {
            LabelStrategyAlone   = 0,   // the first position, without the conflict check (the label still blocks the others)
            LabelStrategySimple  = 1,   // only the first position, skipped when it overlaps a placed label
            LabelStrategyComplex = 2    // all the positions of the placement are tried
        };

        enum eLineLabelOrientation
        {
            LineLabelOrientationHorizontal    = 0,
            LineLabelOrientationParallel      = 1,  // straight text along the line
            LineLabelOrientationCurved        = 2,  // every character follows the line
            LineLabelOrientationPerpendicular = 3
        };

        enum eLineLabelPosition
        {
            LineLabelPositionOnLine     = 0,
            LineLabelPositionAbove      = 1,
            LineLabelPositionBelow      = 2,
            LineLabelPositionAboveBelow = 3   // above, then below
        };

        enum ePolygonLabelPlacement
        {
            PolygonLabelPlacementHorizontal = 0,
            PolygonLabelPlacementStraight   = 1,  // along the main axis of the polygon
            PolygonLabelPlacementMixed      = 2   // along the main axis, then horizontal
        };

        enum eDuplicateStrategy
        {
            DuplicateStrategyAllow    = 0,
            DuplicateStrategyRemove   = 1,  // one label for the same text
            DuplicateStrategyDistance = 2   // labels with the same text not closer than DuplicateDistance
        };

        // positions of a point label (index in SLabelingOptions::m_pointPriorities)
        enum ePointLabelPosition
        {
            PointLabelLeftTop      = 0,
            PointLabelCenterTop    = 1,
            PointLabelRightTop     = 2,
            PointLabelRightCenter  = 3,
            PointLabelRightBottom  = 4,
            PointLabelCenterBottom = 5,
            PointLabelLeftBottom   = 6,
            PointLabelLeftCenter   = 7,
            PointLabelCenterCenter = 8,
            PointLabelPositionCount = 9
        };

        // label settings of a layer, sizes are in mm (symbol units)
        struct SLabelingOptions
        {
            eLabelStrategy          m_strategy;
            eLineLabelOrientation   m_lineOrientation;
            eLineLabelPosition      m_linePosition;
            ePolygonLabelPlacement  m_polygonPlacement;
            eDuplicateStrategy      m_duplicateStrategy;
            int                     m_pointPriorities[PointLabelPositionCount]; // 1 - the best, 0 - not used
            bool                    m_bPolygonAllowOutside;  // the label may cross the border when it doesn't fit into the polygon
            int                     m_nPriority;             // 0 - the highest, labels with a smaller value are placed first
            double                  m_dOffset;               // gap between the feature and the label
            double                  m_dDuplicateDistance;
            double                  m_dMaxCurvedCharAngle;   // degrees, max angle between the neighbour characters of a curved label

            SLabelingOptions() : m_strategy(LabelStrategyComplex), m_lineOrientation(LineLabelOrientationParallel),
                                 m_linePosition(LineLabelPositionOnLine), m_polygonPlacement(PolygonLabelPlacementHorizontal),
                                 m_duplicateStrategy(DuplicateStrategyAllow), m_bPolygonAllowOutside(false), m_nPriority(0),
                                 m_dOffset(1.0), m_dDuplicateDistance(40.0), m_dMaxCurvedCharAngle(30.0)
            {
                // right top is the best (cartographic convention), the center isn't used
                static const int defPriorities[PointLabelPositionCount] = {2, 3, 1, 2, 3, 3, 3, 3, 0};
                for(int i = 0; i < PointLabelPositionCount; ++i)
                    m_pointPriorities[i] = defPriorities[i];
            }

            void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyInt32("Strategy", m_strategy);
                pObj->AddPropertyInt32("LineOrientation", m_lineOrientation);
                pObj->AddPropertyInt32("LinePosition", m_linePosition);
                pObj->AddPropertyInt32("PolygonPlacement", m_polygonPlacement);
                pObj->AddPropertyInt32("DuplicateStrategy", m_duplicateStrategy);
                for(int i = 0; i < PointLabelPositionCount; ++i)
                    pObj->AddPropertyInt32("PointPriority" + std::to_string(i), m_pointPriorities[i]);
                pObj->AddPropertyBool("PolygonAllowOutside", m_bPolygonAllowOutside);
                pObj->AddPropertyInt32("Priority", m_nPriority);
                pObj->AddPropertyDouble("Offset", m_dOffset);
                pObj->AddPropertyDouble("DuplicateDistance", m_dDuplicateDistance);
                pObj->AddPropertyDouble("MaxCurvedCharAngle", m_dMaxCurvedCharAngle);
            }

            void Load(CommonLib::ISerializeObjPtr pObj)
            {
                m_strategy = (eLabelStrategy)pObj->GetPropertyInt32("Strategy", m_strategy);
                m_lineOrientation = (eLineLabelOrientation)pObj->GetPropertyInt32("LineOrientation", m_lineOrientation);
                m_linePosition = (eLineLabelPosition)pObj->GetPropertyInt32("LinePosition", m_linePosition);
                m_polygonPlacement = (ePolygonLabelPlacement)pObj->GetPropertyInt32("PolygonPlacement", m_polygonPlacement);
                m_duplicateStrategy = (eDuplicateStrategy)pObj->GetPropertyInt32("DuplicateStrategy", m_duplicateStrategy);
                for(int i = 0; i < PointLabelPositionCount; ++i)
                    m_pointPriorities[i] = pObj->GetPropertyInt32("PointPriority" + std::to_string(i), m_pointPriorities[i]);
                m_bPolygonAllowOutside = pObj->GetPropertyBool("PolygonAllowOutside", m_bPolygonAllowOutside);
                m_nPriority = pObj->GetPropertyInt32("Priority", m_nPriority);
                m_dOffset = pObj->GetPropertyDouble("Offset", m_dOffset);
                m_dDuplicateDistance = pObj->GetPropertyDouble("DuplicateDistance", m_dDuplicateDistance);
                m_dMaxCurvedCharAngle = pObj->GetPropertyDouble("MaxCurvedCharAngle", m_dMaxCurvedCharAngle);
            }
        };


        typedef std::shared_ptr< class ILayer> ILayerPtr;
        typedef std::shared_ptr< class IMap> IMapPtr;
        typedef std::shared_ptr< class ISelection> ISelectionPtr;
        typedef std::shared_ptr< class ILayers> ILayersPtr;
        typedef std::shared_ptr< class ILabelDrawer> ILabelDrawerPtr;
        typedef std::shared_ptr< class IBookmarks> IBookmarksPtr;
        typedef std::shared_ptr< class ILayers> ILayersPtr;
        typedef std::shared_ptr< class IElement> IElementPtr;
        typedef std::shared_ptr< class IGraphicsContainer> IGraphicsContainerPtr;
        typedef std::shared_ptr< class IMapBookmark> IMapBookmarkPtr;
        typedef std::shared_ptr< class ISymbolSelector> ISymbolSelectorPtr;
        typedef std::shared_ptr< class IFeatureRenderer> IFeatureRendererPtr;
        typedef std::shared_ptr< class ISimpleSymbolSelector> ISimpleSymbolSelectorPtr;
        typedef std::shared_ptr< class ILegendInfo> ILegendInfoPtr;
        typedef std::shared_ptr< class IUniqueValueSymbolSelector> IUniqueValueSymbolSelectorPtr;
        typedef std::shared_ptr< class IRangeSymbolSelector> IRangeSymbolSelectorPtr;
        typedef std::shared_ptr<class IFeatureLayer> IFeatureLayerPtr;
        typedef std::shared_ptr<class IRasterLayer> IRasterLayerPtr;
        typedef std::shared_ptr<class IRasterRenderer> IRasterRendererPtr;
        typedef std::shared_ptr<class IAnnotationRender> IAnnotationRenderPtr;
        typedef std::shared_ptr<class ILabelRenderer> ILabelRendererPtr;




        class IMap :  public CommonLib::ISerialize
        {
        public:
            IMap(){}
            ~IMap(){}
            virtual const std::string&                GetName() const = 0;
            virtual void                              SetName(const  std::string& name) = 0;
            virtual ILayersPtr                        GetLayers() const = 0;
            virtual  void							  SelectFeatures(const CommonLib::bbox& extent, bool resetSelection) = 0;
            virtual ISelectionPtr                     GetSelection() const = 0;
            virtual Geometry::IEnvelopePtr            GetFullExtent(Geometry::ISpatialReferencePtr ptrSpatRef) const = 0;
            virtual void                              SetFullExtent(Geometry::IEnvelopePtr ptrEnv) = 0;
            virtual Geometry::ISpatialReferencePtr    GetSpatialReference() const = 0;
            virtual void                              SetSpatialReference(Geometry::ISpatialReferencePtr ptrSpatRef) = 0;
            virtual void                              Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr trackCancel) = 0;
            virtual void                              PartialDraw( eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr  ptrTrackCancel) = 0;
            virtual ILabelDrawerPtr                   GetLabelDrawer() const = 0;
            virtual void                              SetLabelDrawer(ILabelDrawerPtr ptrLabelDrawer) = 0;
            virtual  CommonLib::Units		          GetMapUnits() const = 0;
            virtual void                              SetMapUnits( CommonLib::Units units ) = 0;
            virtual IGraphicsContainerPtr             GetGraphicsContainer() const = 0;
            virtual void                              SetDelayDrawing(bool delay) = 0;
            virtual IBookmarksPtr                     GetBookmarks() const = 0;
            virtual CommonLib::IPropertySetPtr        GetMapProperties() = 0;
            virtual Display::IFillSymbolPtr			  GetBackgroundSymbol() const= 0;
            virtual void							  SetBackgroundSymbol(Display::IFillSymbolPtr ptrSymbol) = 0;
            virtual Display::IFillSymbolPtr			  GetForegroundSymbol() const= 0;
            virtual void							  SetForegroundSymbol(Display::IFillSymbolPtr ptrSymbol) = 0;
            virtual void							  SetViewPos(const Display::ViewPosition& pos) = 0;
            virtual Display::ViewPosition			  GetViewPos(bool calc_if_absent, Display::IDisplayTransformationPtr ptrTrans) = 0;
            virtual void							  SetExtent(Geometry::IEnvelopePtr ptrExtent) = 0;
            virtual Geometry::IEnvelopePtr		      GetExtent(Geometry::ISpatialReferencePtr ptrSpatRef, bool calc_if_absent, Display::IDisplayTransformationPtr ptrTrans) = 0;
            virtual void							  SetVerticalFlip(bool flag) = 0;
            virtual bool							  GetVerticalFlip() const = 0;
            virtual void							  SetHorizontalFlip(bool flag) = 0;
            virtual bool							  GetHorizontalFlip() const = 0;

            virtual double							  GetMinimumScale() = 0;
            virtual void							  SetMinimumScale(double scale) = 0;
            virtual double							  GetMaximumScale() = 0;
            virtual void							  SetMaximumScale(double scale) = 0;
            virtual bool							  GetHasReferenceScale() const = 0;
            virtual void							  SetHasReferenceScale(bool flag) = 0;
            virtual double							  GetReferenceScale() const = 0;
            virtual void							  SetReferenceScale(double scale) = 0;
        };


        class  ILayer :  public CommonLib::ISerialize
        {
        public:
            ILayer(){}
            virtual ~ILayer(){}
            virtual	uint32_t				  GetLayerTypeID() const = 0;
            virtual CommonLib::CGuid          GetLayerId() const = 0;
            virtual Geometry::IEnvelopePtr    GetExtent() const = 0;
            virtual void                      Draw(eDrawPhase phase, Display::IDisplayPtr display, Display::ITrackCancelPtr ptrTrackCancel) = 0;
            virtual double                    GetMaximumScale() const = 0;
            virtual void                      SetMaximumScale(double scale) = 0;
            virtual double                    GetMinimumScale() const = 0;
            virtual void                      SetMinimumScale(double scale) = 0;
            virtual const std::string&        GetName() const = 0;
            virtual void                      SetName(const std::string& name) = 0;
            virtual eDrawPhase                GetSupportedDrawPhases() const = 0;
            virtual bool                      IsValid() const = 0;
            virtual bool                      GetVisible() const = 0;
            virtual void                      SetVisible(bool flag) = 0;
            virtual bool                      IsActiveOnScale(double scale) const = 0;
            virtual uint32_t				  GetCheckCancelStep() const = 0;
            virtual void					  SetCheckCancelStep(uint32_t nCount) = 0;
            // set by the map for the time of drawing with DrawPhaseLabeling, the layer adds its labels to it
            virtual ILabelDrawerPtr           GetLabelDrawer() const = 0;
            virtual void                      SetLabelDrawer(ILabelDrawerPtr ptrLabelDrawer) = 0;

        };


        struct IFeatureLayer : public ILayer
        {

            IFeatureLayer(){};
            virtual ~IFeatureLayer(){};
            virtual const std::string&               GetDisplayField() const = 0;
            virtual void                             SetDisplayField(const  std::string& sField) = 0;
            virtual const std::string&               GetOIDField() const = 0;
            virtual void                             SetOIDField(const  std::string& sField) = 0;
            virtual const std::string&               GetShapeField() const = 0;
            virtual void                             SetShapeField(const  std::string& sField) = 0;
            virtual GeoDatabase::ITablePtr           GetLayerTable() const = 0;
            virtual void                             SetLayerTable(GeoDatabase::ITablePtr ptrTable) = 0;
            virtual bool                             GetSelectable() const = 0;
            virtual void                             SetSelectable(bool flag) = 0;
            virtual const std::string&  	         GetDefinitionQuery() const= 0;
            virtual void							 SetDefinitionQuery(const std::string& )= 0;
            virtual int								 GetRendererCount() const = 0;
            virtual IFeatureRendererPtr				 GetRenderer(int index) const = 0;
            virtual void							 AddRenderer(IFeatureRendererPtr ptrRenderer) = 0;
            virtual void							 RemoveRenderer(IFeatureRendererPtr ptrRenderer) = 0;
            virtual void							 ClearRenders() = 0;
            virtual void							 SelectFeatures(const CommonLib::bbox& extent, ISelectionPtr ptrSelection,  Geometry::ISpatialReferencePtr ptrSpRef) = 0;
            virtual void                             DrawFeatures(eDrawPhase phase, const std::vector<int64_t>& vecOids, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, Display::ISymbolPtr ptrCustomSymbol) const = 0;
            virtual bool                             HasAnnoField() const = 0;
            virtual const std::string&               GetAnnoFieldName() const = 0;
            virtual void                             SetAnnoFieldName(const std::string& filedName)  = 0;
            virtual IAnnotationRenderPtr			 GetAnnotationRenderer() const = 0;
            virtual void							 SetAnnotationRenderer(IAnnotationRenderPtr ptrRenderer) = 0;
            // labels: drawn when the layer has the label field and the label renderer,
            // placed without conflicts by the label drawer of the map (IMap::GetLabelDrawer)
            virtual bool                             HasLabelField() const = 0;
            virtual const std::string&               GetLabelFieldName() const = 0;
            virtual void                             SetLabelFieldName(const std::string& labelName)  = 0;
            virtual ILabelRendererPtr                GetLabelRenderer() const = 0;
            virtual void                             SetLabelRenderer(ILabelRendererPtr ptrRenderer) = 0;
            virtual const SLabelingOptions&          GetLabelingOptions() const = 0;
            virtual void                             SetLabelingOptions(const SLabelingOptions& options) = 0;


        };

        class IRasterLayer : public ILayer {
            public:
            IRasterLayer(){}
            virtual ~IRasterLayer(){}

            virtual GeoDatabase::IRasterDatasetPtr           GetRasterDataset() const = 0;
            virtual void                                     SetRasterDataset(GeoDatabase::IRasterDatasetPtr ptrDataset) = 0;
            virtual IRasterRendererPtr                       GetRenderer() const = 0;
            virtual void                                     SetRenderer(IRasterRendererPtr ptrRenderer) = 0;

        };


        class  ILayers
        {
        public:
            ILayers(){}
            virtual ~ILayers(){}
            virtual int       GetLayerCount() const = 0;
            virtual ILayerPtr GetLayer(int index) const = 0;
            virtual ILayerPtr GetLayerById(CommonLib::CGuid layerId) const = 0;
            virtual void      AddLayer(ILayerPtr ptrLayer) = 0;
            virtual void      InsertLayer(ILayerPtr ptrLayer, int index) = 0;
            virtual void      RemoveLayer(ILayerPtr ptrLayer) = 0;
            virtual void      RemoveAllLayers() = 0;
            virtual void      MoveLayer(ILayerPtr ptrLayer, int index) = 0;

            // no events: incremented on every change of the layer list (add, insert, remove, move, clear),
            // compare it with the remembered value to find out that the list has changed
            virtual uint64_t  GetChangeCounter() const = 0;
        };

        class ISelection
        {
        public:
            ISelection(){}
            virtual ~ISelection(){}
            virtual void                                 AddRow(CommonLib::CGuid layerId, int64_t rowID) = 0;
            virtual void                                 Clear() = 0;
            virtual void                                 ClearForLayer(CommonLib::CGuid nLayerId) = 0;
            virtual void                                 RemoveFeature(CommonLib::CGuid nLayerId, int64_t rowID) = 0;
            // returned by value: the selection is shared between threads (draw / UI), a reference to internal state is not safe
            virtual std::vector<ILayerPtr>       		 GetLayers() const = 0;
            virtual std::vector<int64_t>         	     GetFeatures(CommonLib::CGuid layerId) const = 0;
            virtual void                                 Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr trackCancel) = 0;
            virtual Display::ISymbolPtr		             GetSymbol() const = 0;
            virtual void                                 SetSymbol(Display::ISymbolPtr ptrSymbol) = 0;
            virtual bool                                 IsEmpty() const = 0;
            // no events: incremented on every real change of the selection, compare it with the remembered value
            virtual uint64_t                             GetChangeCounter() const = 0;
        };

        class  IElement
        {
        public:
            IElement(){}
            virtual ~IElement(){}
            virtual void                      Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel) = 0;
            virtual void                      Activate(Display::IDisplayPtr ptrDisplay) = 0;
            virtual void                      Deactivate() = 0;
            virtual void                      GetBounds(Display::IDisplayPtr ptrDisplay, CommonLib::bbox& box) const = 0;
        };

        class  IGraphicsContainer
        {
        public:
            IGraphicsContainer(){}
            virtual ~IGraphicsContainer(){}
            virtual bool                   IsEmpty() const = 0;
            virtual void                   AddElement( IElementPtr ptrElement ) = 0;
            virtual void                   RemoveAllElements() = 0;
            virtual bool                   RemoveElement(IElementPtr ptrElement) = 0;
            virtual bool                   BringToFront(IElementPtr ptrElement) = 0;
            virtual bool                   SendToBack(IElementPtr ptrElement) = 0;
            virtual	uint32_t			   GetEnumCount() const = 0;
            virtual IElementPtr			   GetElement(int nIdx) const = 0;

        };


        class  IBookmarks
        {
        public:
            IBookmarks(){}
            virtual ~IBookmarks(){}
            virtual int                 GetBookmarkCount() const = 0;
            virtual IMapBookmarkPtr		GetBookmark(int index) const = 0;
            virtual void                AddBookmark(IMapBookmarkPtr ptrBookmark) = 0;
            virtual void                RemoveBookmark(IMapBookmarkPtr  ptrBookmark) = 0;
            virtual void                RemoveAllBookmarks() = 0;
        };

        class  IMapBookmark
        {
        public:
            IMapBookmark(){}
            virtual ~IMapBookmark(){}
            virtual const std::string&            GetName() const = 0;
            virtual void                          SetName(const std::string& csName ) = 0;
            virtual Display::ViewPosition		  GetPosition() const = 0;
            virtual void                          SetPosition(const Display::ViewPosition& pos) = 0;
            virtual Geometry::IEnvelopePtr        GetExtent() const = 0;
            virtual void                          SetExtent( Geometry::IEnvelopePtr ptrExtent ) = 0 ;
            virtual void                          ZoomTo( Display::IDisplayTransformationPtr ptrTrans ) const = 0;
        };


        class  ISymbolSelector  :  public CommonLib::ISerialize
        {
        public:
            ISymbolSelector(){}
            virtual ~ISymbolSelector(){}
            virtual uint32_t			   GetSymbolSelectorID() const = 0;
            virtual bool                   CanAssign(GeoDatabase::ITablePtr ptrTable) const = 0;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const = 0;
            virtual Display::ISymbolPtr	   GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const = 0;
            virtual void                   SetupSymbols(Display::IDisplayPtr ptrDisplay) = 0;
            virtual void                   ResetSymbols() = 0;
            virtual void				   FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel) = 0;
        };




        class  IFeatureRenderer :  public CommonLib::ISerialize
        {
        public:
            IFeatureRenderer(){};
            virtual ~IFeatureRenderer(){};
            virtual	uint32_t     		   GetFeatureRendererID()  const = 0;
            virtual bool                   CanRender(GeoDatabase::ITablePtr ptrTable, Display::IDisplayPtr ptrDisplay) const = 0;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const = 0;
            virtual Display::ISymbolPtr    GetSymbolByRow(GeoDatabase::IRowPtr ptrRow) const = 0;
            virtual double                 GetMaximumScale() const = 0;
            virtual void                   SetMaximumScale(double scale) = 0;
            virtual double                 GetMinimumScale() const = 0;
            virtual void                   SetMinimumScale(double scale) = 0;
            virtual const std::string&     GetShapeField() const = 0;
            virtual void                   SetShapeField(const std::string&  field) = 0;
            virtual ISymbolSelectorPtr	   GetSymbolSelector() const = 0;
            virtual void				   SetSymbolSelector(ISymbolSelectorPtr ptrAssigner) = 0;
            virtual void                   DrawFeature(Display::IDisplayPtr ptrDisplay, GeoDatabase::IRowPtr ptrRow, Display::ISymbolPtr ptrCustomSymbol = Display::ISymbolPtr()) = 0;
        };



        class IAnnotationRender : public IFeatureRenderer {
            public:
            IAnnotationRender(){}
            virtual ~IAnnotationRender(){}

            using IFeatureRenderer::PrepareFilter;
            // symbol selector of the renderer gives a text symbol per row (it can depend on the row attributes)
            virtual void  PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter, const std::string& annotationName) const = 0;

        };


        class  IRasterRenderer :  public CommonLib::ISerialize {
        public:
            IRasterRenderer(){}
            virtual ~IRasterRenderer(){}

            virtual uint32_t GetRasterRendererID() const = 0;
            virtual bool    CanRender(GeoDatabase::IRasterDatasetPtr ptrRaster, Display::IDisplayPtr ptrDisplay) const = 0;
            virtual void    Draw(GeoDatabase::IRasterDatasetPtr ptrRaster, eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr trackCancel) = 0;

        };


        // Label renderer of a feature layer: takes the label text from the label field and the text symbol
        // from the symbol selector, the label is added to the label drawer, which places it without conflicts.
        // DrawFeature draws the label at once at the first position (no conflict check).
        class ILabelRenderer : public IFeatureRenderer
        {
        public:
            ILabelRenderer(){}
            virtual ~ILabelRenderer(){}

            using IFeatureRenderer::PrepareFilter;
            virtual void  PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter, const std::string& labelFieldName) const = 0;
            virtual void  AddLabel(ILabelDrawerPtr ptrLabelDrawer, GeoDatabase::IRowPtr ptrRow, const SLabelingOptions& options) = 0;
            // labels with the same priority: the bigger class index is placed first
            virtual int   GetClassIndex() const = 0;
            virtual void  SetClassIndex(int nIndex) = 0;
        };

        // Collects the labels of all layers while the map is drawn and places them without overlapping:
        //   BeginLabeling -> AddLabel (layers) -> DrawLabels -> EndLabeling
        // Positions are calculated in device coordinates, so it works with any transformation (rotation, 3D).
        class ILabelDrawer :  public CommonLib::ISerialize {
            public:
            ILabelDrawer(){}
            virtual ~ILabelDrawer(){}

            virtual void           BeginLabeling(Display::IDisplayPtr ptrDisplay) = 0;
            virtual void           Clear() = 0;
            virtual void           AddLabel(const std::wstring& text,  CommonLib::IGeoShapePtr ptrShape,
                                Display::ITextSymbolPtr ptrSymbol,  int classIndex, const SLabelingOptions& options) = 0;
            virtual void           DrawLabels(Display::ITrackCancelPtr ptrTrackCancel) = 0;
            virtual void           EndLabeling() = 0;
            // statistics of the last labeling, kept after EndLabeling
            virtual uint32_t       GetLabelCount() const = 0;        // labels added
            virtual uint32_t       GetPlacedLabelCount() const = 0;  // labels placed and drawn

        };

        class ISimpleSymbolSelector : public ISymbolSelector
        {
        public:
            ISimpleSymbolSelector(){}
            virtual ~ISimpleSymbolSelector(){}
            virtual const std::string&     GetDescription() const = 0;
            virtual void                   SetDescription(const std::string& sDesc) = 0;
            virtual const std::string&     GetLabel() const = 0;
            virtual void                   SetLabel(const std::string& sLabel) = 0;
            virtual Display::ISymbolPtr    GetSymbol() const = 0;
            virtual void                   SetSymbol(Display::ISymbolPtr ptrSymbol) = 0;
        };

        // symbol by the values of one or several fields (ported from UniGIS UniqueValueSymbolAssigner).
        // Numbers are compared by value whatever their type is (int32 field, int64 value ...), texts as UTF-8;
        // an empty (null) value matches null field values. Rows without a matching value get the default symbol
        // when it is used, otherwise no symbol (they are not drawn)
        class IUniqueValueSymbolSelector : public ISymbolSelector
        {
        public:
            IUniqueValueSymbolSelector(){}
            virtual ~IUniqueValueSymbolSelector(){}

            virtual const std::string&     GetHeadingLabel() const = 0;
            virtual void                   SetHeadingLabel(const std::string& sLabel) = 0;

            virtual int                    GetFieldCount() const = 0;
            virtual void                   SetFieldCount(int nCount) = 0;
            virtual const std::string&     GetField(int nFieldIndex) const = 0;
            virtual void                   SetField(int nFieldIndex, const std::string& sFieldName) = 0;

            virtual int                    GetValueCount() const = 0;
            virtual void                   SetValueCount(int nCount) = 0;
            // adds a value (one item per field), returns its index
            virtual int                    AddValue(const std::vector<CommonLib::CVariant>& values, Display::ISymbolPtr ptrSymbol, const std::string& sLabel = std::string()) = 0;
            virtual void                   RemoveValue(int nIndex) = 0;
            virtual CommonLib::CVariant    GetValue(int nIndex, int nFieldIndex) const = 0;
            virtual void                   SetValue(int nIndex, int nFieldIndex, const CommonLib::CVariant& value) = 0;
            virtual const std::string&     GetLabel(int nIndex) const = 0;
            virtual void                   SetLabel(int nIndex, const std::string& sLabel) = 0;
            virtual const std::string&     GetDescription(int nIndex) const = 0;
            virtual void                   SetDescription(int nIndex, const std::string& sDescription) = 0;
            virtual Display::ISymbolPtr    GetSymbol(int nIndex) const = 0;
            virtual void                   SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol) = 0;
            virtual int                    GetGroup(int nIndex) const = 0;
            virtual void                   SetGroup(int nIndex, int nGroup) = 0;

            virtual Display::ISymbolPtr    GetDefaultSymbol() const = 0;
            virtual void                   SetDefaultSymbol(Display::ISymbolPtr ptrSymbol) = 0;
            virtual const std::string&     GetDefaultLabel() const = 0;
            virtual void                   SetDefaultLabel(const std::string& sLabel) = 0;
            virtual bool                   GetUseDefaultSymbol() const = 0;
            virtual void                   SetUseDefaultSymbol(bool bUse) = 0;
        };

        // symbol by the numeric value of a field (ported from UniGIS RangeSymbolAssigner):
        // the first range with from <= value <= to gives the symbol, otherwise the default symbol (when it is used)
        class IRangeSymbolSelector : public ISymbolSelector
        {
        public:
            IRangeSymbolSelector(){}
            virtual ~IRangeSymbolSelector(){}

            virtual const std::string&     GetField() const = 0;
            virtual void                   SetField(const std::string& sFieldName) = 0;

            virtual int                    GetRangeCount() const = 0;
            virtual void                   SetRangeCount(int nCount) = 0;
            virtual int                    AddRange(double dFrom, double dTo, Display::ISymbolPtr ptrSymbol, const std::string& sLabel = std::string()) = 0;
            virtual void                   RemoveRange(int nIndex) = 0;
            virtual void                   GetRange(int nIndex, double* pFrom, double* pTo) const = 0;
            virtual void                   SetRange(int nIndex, double dFrom, double dTo) = 0;
            virtual const std::string&     GetLabel(int nIndex) const = 0;
            virtual void                   SetLabel(int nIndex, const std::string& sLabel) = 0;
            virtual const std::string&     GetDescription(int nIndex) const = 0;
            virtual void                   SetDescription(int nIndex, const std::string& sDescription) = 0;
            virtual Display::ISymbolPtr    GetSymbol(int nIndex) const = 0;
            virtual void                   SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol) = 0;
            virtual void                   SortRanges() = 0;   // by the start of the range

            virtual Display::ISymbolPtr    GetDefaultSymbol() const = 0;
            virtual void                   SetDefaultSymbol(Display::ISymbolPtr ptrSymbol) = 0;
            virtual const std::string&     GetDefaultLabel() const = 0;
            virtual void                   SetDefaultLabel(const std::string& sLabel) = 0;
            virtual bool                   GetUseDefaultSymbol() const = 0;
            virtual void                   SetUseDefaultSymbol(bool bUse) = 0;
        };

        class ILegendInfo
        {
        public:
            ILegendInfo(){}
            virtual ~ILegendInfo(){}
            virtual int                    GetSymbolCount() const = 0;
            virtual Display::ISymbolPtr    GetSymbolByIndex(int index) const = 0;
            virtual void                   SetSymbolByIndex(int index, Display::ISymbolPtr ptrSymbol) = 0;
        };

        // Map drawer: draws the map in a background thread into an off-screen graphics,
        // the window copies the result with Update() (ported from GisFramework of the old engine).
        // The drawer has no callbacks, the window polls its state by a timer:
        //  - while IsDrawing() - repaint periodically to show the progress,
        //  - GetDrawCounter() changed - a drawing is finished, repaint and check GetLastError().
        enum eMapDrawerFlags
        {
            MapDrawerDrawMap          = 1,
            MapDrawerDrawLabel        = 2,
            MapDrawerPanState         = 4,
            MapDrawerStoppingPan      = 8,
            MapDrawerFinishedPan      = 16,
            MapDrawerPanAfterMap      = 32,
            MapDrawerFinishedDrawMap  = 64,
            MapDrawerFinishedDrawLabel = 128
        };

        typedef std::shared_ptr<class IMapDrawer> IMapDrawerPtr;

        class IMapDrawer
        {
        public:
            IMapDrawer(){}
            virtual ~IMapDrawer(){}

            virtual Display::IDisplayTransformationPtr GetTransformation() const = 0;      // used by the draw thread
            virtual Display::IDisplayTransformationPtr GetCalcTransformation() const = 0;  // changed by UI (pan, zoom), applied on Redraw
            virtual Display::IGraphicsPtr GetMapGraphics() const = 0;
            virtual Display::IGraphicsPtr GetLabelGraphics() const = 0;
            virtual Display::IGraphicsPtr GetOutGraphics() const = 0;

            virtual IMapPtr GetMap() const = 0;
            virtual void SetMap(IMapPtr ptrMap) = 0;

            virtual void   SetResolution(double dpi) = 0;
            virtual double GetResolution() const = 0;
            virtual void   SetBackgroundColor(const Display::Color& color) = 0;

            virtual void SetSize(int cx , int cy, bool bDraw = true) = 0;
            virtual void Update(Display::IGraphicsPtr ptrGraphics, const Display::GPoint *pPoint, const Display::GRect* pRect) = 0;
            virtual void Redraw(Display::IGraphicsPtr ptrGraphics = Display::IGraphicsPtr()) = 0;
            virtual bool IsDrawing() const = 0;
            virtual uint64_t GetDrawCounter() const = 0;  // number of finished drawings (completed, stopped or failed)
            virtual bool IsDrawCompleted() const = 0;     // the last drawing reached the end (not stopped, no error)
            virtual std::string GetLastError() const = 0;

            virtual void ZoomIn(const Display::GRect& rect) = 0;
            virtual void ZoomIn(const CommonLib::bbox& bb) = 0;
            virtual void ZoomToFullExtent() = 0;
            virtual void SetScale(double scale) = 0;

            // pseudo 3D (perspective) view like in a car navigator, see Display::CDisplayTransformation3D
            virtual void   Set3DMode(bool b3D) = 0;
            virtual bool   Is3DMode() const = 0;
            virtual void   SetTilt(double degrees) = 0;      // 3D view tilt, kept when the 3D mode is off
            virtual double GetTilt() const = 0;
            virtual void   SetRotation(double degrees) = 0;  // map rotation around the window center
            virtual double GetRotation() const = 0;

            virtual void StartPan(const Display::GPoint& pt) = 0;
            virtual void MovePan(const Display::GPoint& pt) = 0;
            virtual void StopPan(const Display::GPoint& pt) = 0;
            virtual void StopDraw(bool bWait = true) = 0;
        };

    }
}