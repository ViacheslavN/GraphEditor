#pragma once
#include "DisplayLib.h"

namespace GraphEngine {
    namespace Display {

        enum eSymbolID
        {
            UndefineSymbolID,
            SimpleLineSymbolID,
            HashLineSymbolID,
            SimpleMarketSymbolID,
            MarkerLineSymbolID,
            PictureLineSymbol,
            SimpleFillSymbolID,
            LineFillSymbolID,
            TextFillSymbolID,
            TextSymbolID,
            MultiLayerSymbolID
        };

        typedef std::shared_ptr<class ISymbol> ISymbolPtr;
        typedef std::shared_ptr<class ILineSymbol> ILineSymbolPtr;
        typedef std::shared_ptr<class IMarkerSymbol> IMarkerSymbolPtr;
        typedef std::shared_ptr<class ITextBackground> ITextBackgroundPtr;
        typedef std::shared_ptr<class ITextSymbol>  ITextSymbolPtr;


        class  ISymbol : public CommonLib::ISerialize
        {
        public:
            ISymbol(){}
            virtual ~ISymbol(){}

            virtual uint32_t GetSymbolID() const = 0;
            virtual void Init( IDisplayPtr display  ) = 0;
            virtual void Reset() = 0;
            virtual bool CanDraw(CommonLib::IGeoShapePtr pShape) const = 0;
            virtual void Draw(IDisplayPtr ptrDisplay, CommonLib::IGeoShapePtr pShape) = 0;
            virtual void FlushBuffers(IDisplayPtr ptrDisplay, ITrackCancelPtr ptrTrackCancel) = 0;
            virtual void GetBoundaryRect(CommonLib::IGeoShapePtr pShape, IDisplayPtr display,  GRect &rect) const = 0;
            virtual bool GetScaleDependent() const = 0;
            virtual void SetScaleDependent(bool flag) = 0;
            virtual bool GetDrawToBuffers() const = 0;
            virtual void SetDrawToBuffers(bool flag) = 0;
            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount ) = 0;

            virtual void  DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount) = 0;
            virtual void  QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount,   GRect &rect) const= 0;
            virtual void  Prepare(IDisplayPtr ptrDisplay) = 0;
        };


        class   IMultiLayerSymbol
        {
        public:
            IMultiLayerSymbol(){}
            virtual ~IMultiLayerSymbol(){}
            virtual int               AddLayer( ISymbolPtr ptrSym ) = 0;
            virtual void              DeleteLayer( int index ) = 0;
            virtual void              ClearLayers() = 0;
            virtual int               GetCount() const = 0;
            virtual ISymbolPtr        GetLayer( int index ) const  = 0;
            virtual void              MoveLayer( int indexFrom, int indexTo ) = 0;
            virtual bool              GetUseCache() const = 0;
            virtual void              SetUseCache(bool use) = 0;
        };

        class ILineSymbol  : public ISymbol
        {
        public:
            ILineSymbol(){}
            virtual ~ILineSymbol(){}
            virtual Color  GetColor() const = 0;
            virtual void   SetColor(const Color &color) = 0;
            virtual double GetWidth() const = 0;
            virtual void   SetWidth(double width) = 0;

        };

        class  ISimpleLineSymbol  : public ILineSymbol
        {
        public:
            ISimpleLineSymbol(){}
            virtual ~ISimpleLineSymbol(){}
            virtual ePenType				 GetStyle() const = 0;
            virtual void                SetStyle( ePenType style ) = 0;
            virtual eCapType             GetCapType() const = 0;
            virtual void                SetCapType( eCapType cap ) = 0;
            virtual eJoinType            GetJoinType() const = 0;
            virtual void                SetJoinType( eJoinType join ) = 0;
        };

        class  IMarkerSymbol : public ISymbol
        {
        public:
            IMarkerSymbol(){}
            virtual ~IMarkerSymbol(){}
            virtual double GetAngle() const = 0;
            virtual void SetAngle(double angle) = 0;

            virtual Color GetColor() const = 0;
            virtual void SetColor(const Color &color) = 0;

            virtual double GetSize() const = 0;
            virtual void SetSize(double size) = 0;

            virtual double GetXOffset() const = 0;
            virtual void SetXOffset(double offset) = 0;

            virtual double GetYOffset() const = 0;
            virtual void   SetYOffset(double offset) = 0;

            virtual bool GetIgnoreRotation() const = 0;
            virtual void SetIgnoreRotation(bool gnore) = 0;
        };


        class  ISimpleMarkerSymbol : public IMarkerSymbol
        {
        public:
            ISimpleMarkerSymbol(){}
            virtual ~ISimpleMarkerSymbol(){}
            virtual void                 SetStyle(eSimpleMarkerStyle style) = 0;
            virtual void                 SetOutline(bool bOutline) = 0;
            virtual bool                 IsOutline() const = 0;
            virtual Color                GetOutlineColor() const = 0;
            virtual void                 SetOutlineColor( const Color &color) = 0;
            virtual double               GetOutlineSize() const = 0;
            virtual void                 SetOutlineSize(double size) = 0;
        };

        class ICharacterMarkerSymbol  : public IMarkerSymbol
        {
        public:
            ICharacterMarkerSymbol(){}
            virtual ~ICharacterMarkerSymbol(){}
            virtual FontPtr GetFont() const = 0;
            virtual void        SetFont( FontPtr font ) = 0;
            virtual int         GetCharacterIndex() const = 0;
            virtual void        SetCharacterIndex( int index ) = 0;
        };

        class IPictureMarkerSymbol : public IMarkerSymbol
        {
        public:
            IPictureMarkerSymbol(){}
            virtual ~IPictureMarkerSymbol(){}
            virtual Color         GetBitmapTransparencyColor() const = 0;
            virtual void          SetBitmapTransparencyColor( const Color &color ) = 0;
            virtual BitmapPtr		 GetBitmap() const = 0;
            virtual void          SetBitmap(BitmapPtr pImage) = 0;
            virtual Color         GetBackgroundColor() const = 0;
            virtual void          SetBackgroundColor( const Color &color ) = 0;
            virtual bool          GetDrawExact() const = 0;
            virtual void          SetDrawExact( bool drawExact ) = 0;
        };

        class IFillSymbol : public ISymbol
        {
        public:
            IFillSymbol(){}
            virtual ~IFillSymbol(){}
            virtual ILineSymbolPtr GetOutlineSymbol() const = 0;
            virtual void SetOutlineSymbol(ILineSymbolPtr ptrLine) = 0;
            virtual void FillRect(IDisplayPtr pDisplay, const Display::GRect& rect) = 0;

            virtual Color GetColor() const = 0;
            virtual void  SetColor(const Color &color) = 0;
        };

        class ISimpleFillSymbol : public IFillSymbol
        {
        public:
            ISimpleFillSymbol(){}
            virtual ~ISimpleFillSymbol(){}
            virtual eSimpleFillStyle GetStyle() const = 0;
            virtual void SetStyle(eSimpleFillStyle style) = 0;
            virtual Color GetBackgroundColor() const = 0;
            virtual void  SetBackgroundColor( Color color ) = 0;
        };

        class IPictureFillSymbol : public IFillSymbol
        {
        public:
            IPictureFillSymbol(){}
            virtual ~IPictureFillSymbol(){}
            virtual Color           GetBitmapTransparencyColor() const = 0;
            virtual void            SetBitmapTransparencyColor( const Color &color ) = 0;
            virtual Color           GetBackgroundColor() const = 0;
            virtual void            SetBackgroundColor( const Color &color ) = 0;
            virtual BitmapPtr		GetBitmap() const = 0;
            virtual void            SetBitmap(BitmapPtr pImage) = 0;
        };

        class IMarkerFillSymbol  : public IFillSymbol
        {
        public:
            IMarkerFillSymbol(){}
            virtual ~IMarkerFillSymbol(){}
            virtual IMarkerSymbolPtr   GetMarkerSymbol() const = 0;
            virtual void               SetMarkerSymbol(IMarkerSymbol *pMarker) = 0;
            virtual eMarkerFillStyle GetStyle() const = 0;
            virtual void               SetStyle(eMarkerFillStyle style) = 0;
            virtual double GetXSeparation() const = 0;
            virtual void   SetXSeparation( double sep) = 0;
            virtual double GetYSeparation() const = 0;
            virtual void   SetYSeparation( double sep) = 0;
            virtual double GetXOffset() const = 0;
            virtual void   SetXOffset( double offset) = 0;
            virtual double GetYOffset() const = 0;
            virtual void   SetYOffset( double offset) = 0;
        };

        class  ILineFillSymbol  : public IFillSymbol
        {
        public:
            ILineFillSymbol(){}
            virtual ~ILineFillSymbol(){}
            virtual ILineSymbolPtr GetLineSymbol() const  = 0;
            virtual void           SetLineSymbol(ILineSymbol *pLine) = 0;
            virtual double         GetAngle() const = 0;
            virtual void           SetAngle( double angle ) = 0;
            virtual double         GetOffset() const = 0;
            virtual void           SetOffset( double offset) = 0;
            virtual double         GetSeparation() const = 0;
            virtual void           SetSeparation( double sep) = 0;
        };

        class ITextSymbol : public ISymbol
        {
        public:
            ITextSymbol(){}
            virtual ~ITextSymbol(){}
            virtual GUnits GetAngle() const = 0;
            virtual void   SetAngle( GUnits dAngle ) = 0;
            virtual Color  GetColor() const = 0;
            virtual void   SetColor(const Color &color ) = 0;
            virtual  FontPtr GetFont() const = 0;
            virtual void   SetFont(FontPtr font ) = 0;
            virtual void GetTextSize(IDisplayPtr ptrDisplay, const std::wstring& szText, GUnits *pxSize , GUnits *pySize, GUnits* baseLine) const = 0;
            virtual GUnits GetSize() const = 0;
            virtual void SetSize(GUnits size) = 0;
            virtual  const std::wstring& GetText() const = 0;
            virtual void SetText(const std::wstring& szText ) = 0;
            virtual ITextBackgroundPtr GetTextBackground() const = 0;
            virtual void SetTextBackground( ITextBackgroundPtr ptrBg ) = 0;
            virtual int   GetTextDrawFlags() const = 0;
            virtual void  SetTextDrawFlags(int flags) = 0;
        };


        class ITextBackground :  public CommonLib::ISerialize
        {
        public:
            ITextBackground(){}
            virtual ~ITextBackground(){}
            virtual void                      Setup(IDisplayPtr ptrDisplay ) = 0;
            virtual void                      Draw(IDisplayPtr ptrDisplay, const GRect& rect) = 0;
            virtual void                      Reset() = 0;
            virtual void                      GetBoundaryRect(IDisplayPtr ptrDisplay, CommonLib::bbox &bbox) const = 0;
            virtual ITextSymbolPtr            GetTextSymbol() const = 0;
            virtual void                      SetTextSymbol( ITextSymbol *symbol ) = 0;
            virtual void                      SetTextBox( const CommonLib::bbox &box ) = 0;
        };

        class  IMarkerTextBackground : public ITextBackground
        {
        public:
            IMarkerTextBackground(){}
            virtual ~IMarkerTextBackground(){}
            virtual IMarkerSymbolPtr GetSymbol() const = 0;
            virtual void             SetSymbol( IMarkerSymbolPtr ptrSymbol ) = 0;
            virtual bool             GetScaleToFit() const = 0;
            virtual void             SetScaleToFit( bool bFit ) = 0;
        };


    }
}