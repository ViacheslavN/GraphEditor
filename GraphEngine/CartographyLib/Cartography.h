#pragma once
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
            UniqueValueSymbolSelectorID
        };

        enum eDrawPhase
        {
            DrawPhaseNone       = 0,
            DrawPhaseGeography  = 1,
            DrawPhaseAnnotation = 2,
            DrawPhaseDrawAnnoCache = 4,
            DrawPhaseSelection  = 8,
            DrawPhaseGraphics   = 16,
            DrawPhaseAll        = 0xFFFF
        };


        enum eLayerTypeID
        {
            UndefineLayerID,
            FeatureLayerID

        };


        typedef std::shared_ptr< class ILayer> ILayerPtr;
        typedef std::shared_ptr< class IMap> IMapPtr;
        typedef std::shared_ptr< class ISelection> ISelectionPtr;
        typedef std::shared_ptr< class ILayers> ILayersPtr;
        typedef std::shared_ptr< class ILabelEngine> ILabelEnginePtr;
        typedef std::shared_ptr< class IBookmarks> IBookmarksPtr;
        typedef std::shared_ptr< class ILayers> ILayersPtr;
        typedef std::shared_ptr< class IElement> IElementPtr;
        typedef std::shared_ptr< class IGraphicsContainer> IGraphicsContainerPtr;
        typedef std::shared_ptr< class IMapBookmar> IMapBookmarkPtr;
        typedef std::shared_ptr< class ISymbolSelector> ISymbolSelectorPtr;
        typedef std::shared_ptr< class IFeatureRenderer> IFeatureRendererPtr;


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
            virtual ILabelEnginePtr                   GetLabelEngine() const = 0;
            virtual void                              SetLabelEngine(ILabelEnginePtr ptrEngine) = 0;
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

        };

        class ISelection
        {
        public:
            virtual void                                 AddRow(CommonLib::CGuid layerId, int64_t rowID) = 0;
            virtual void                                 Clear() = 0;
            virtual void                                 ClearForLayer(CommonLib::CGuid nLayerId) = 0;
            virtual void                                 RemoveFeature(CommonLib::CGuid nLayerId, int64_t rowID) = 0;
            virtual const std::vector<ILayerPtr>& 		 GetLayers() const = 0;
            virtual const std::vector<int64_t>& 	     GetFeatures(CommonLib::CGuid layerId) const = 0;
            virtual void                                 Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr trackCancel) = 0;
            virtual Display::ISymbolPtr		             GetSymbol() const = 0;
            virtual void                                 SetSymbol(Display::ISymbolPtr ptrSymbol) = 0;
            virtual bool                                 IsEmpty() const = 0;

        };

        class  IElement
        {
        public:
            IElement();
            virtual ~IElement(){}
            virtual void                      Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel) = 0;
            virtual void                      Activate(Display::IDisplayPtr ptrDisplay) = 0;
            virtual void                      Deactivate() = 0;
            virtual void                      GetBounds(Display::IDisplayPtr ptrDisplay, CommonLib::bbox& box) const = 0;
        };

        class  IGraphicsContainer
        {
        public:
            IGraphicsContainer();
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


    }
}