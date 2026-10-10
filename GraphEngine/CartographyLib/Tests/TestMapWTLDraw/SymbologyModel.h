#pragma once
// Platform independent model of the symbology edited in the Add layer dialogs:
// symbol parameters -> symbols, symbology (simple / unique values / ranges) -> symbol selector.

#include "../../Cartography.h"

namespace TestMapDraw
{
    enum eGeometryKind
    {
        GeometryPoint,
        GeometryLine,
        GeometryPolygon
    };

    eGeometryKind GeometryKindOf(CommonLib::eShapeType shapeType);

    enum eSymbolKind
    {
        SymbolSimpleMarker,
        SymbolArrowMarker,
        SymbolCharacterMarker,
        SymbolPictureMarker,
        SymbolSimpleLine,
        SymbolHashLine,
        SymbolMarkerLine,
        SymbolSimpleFill,
        SymbolLineFill,
        SymbolMarkerFill,
        SymbolPictureFill
    };

    // editable properties, which of them a symbol kind uses - see CSymbolFactory::GetProperties
    enum eSymbolProperty
    {
        PropColor,
        PropSize,           // marker size, line width, hash length
        PropStyle,          // marker style, line style, fill style, marker fill style (grid / random)
        PropOutlineColor,   // empty text - no outline
        PropOutlineWidth,
        PropAngle,
        PropSeparation,     // interval along a line, distance between fill lines / markers
        PropBitmapFile,
        PropFontFace,
        PropCharCode,
        PropScaleDependent  // the sizes are scaled by the map reference scale (choice No / Yes)
    };

    enum ePropertyKind
    {
        PropertyColor,      // "#RRGGBB", empty - transparent (none)
        PropertyNumber,
        PropertyChoice,     // index in choices
        PropertyFile,
        PropertyText
    };

    struct SPropertyInfo
    {
        eSymbolProperty          id;
        std::string              sLabel;
        ePropertyKind            kind;
        std::vector<std::string> vecChoices;
    };

    struct SSymbolParams
    {
        eSymbolKind kind = SymbolSimpleFill;
        GraphEngine::Display::Color color = GraphEngine::Display::Color(255, 204, 0);
        double      dSize = 1.;
        int         nStyle = 0;
        GraphEngine::Display::Color outlineColor = GraphEngine::Display::Color(64, 64, 64);
        double      dOutlineWidth = 0.2;
        double      dAngle = 0.;
        double      dSeparation = 3.;
        std::string sBitmapFile;   // UTF-8
        GraphEngine::Display::BitmapPtr ptrBitmap;   // image of a picture symbol of a layer (no file), used when sBitmapFile is empty
        std::string sFontFace = "Arial";
        int         nCharCode = 0x2605;   // star
        bool        bScaleDependent = false;   // sizes are kept at the map reference scale, scaled on other scales
    };

    class CSymbolFactory
    {
    public:
        static std::vector<eSymbolKind>   GetSymbolKinds(eGeometryKind geometry);
        static const char*                GetSymbolKindName(eSymbolKind kind);
        static eGeometryKind              GetGeometryKind(eSymbolKind kind);
        static std::vector<SPropertyInfo> GetProperties(eSymbolKind kind);

        // default parameters of the kind with the main color
        static SSymbolParams CreateDefault(eSymbolKind kind, const GraphEngine::Display::Color& color);
        static SSymbolParams CreateDefault(eGeometryKind geometry, const GraphEngine::Display::Color& color);
        // another kind keeping the color (and the size when it means the same)
        static SSymbolParams ChangeKind(const SSymbolParams& params, eSymbolKind kind);

        // throws when a bitmap file can't be read
        static GraphEngine::Display::ISymbolPtr CreateSymbol(const SSymbolParams& params);
        // parameters of a symbol made by CreateSymbol (symbols of a layer), false - the symbol can't be described
        // by the parameters (multi layer symbol, outline / marker of another type ...), params get the main color only
        static bool FromSymbol(GraphEngine::Display::ISymbolPtr ptrSymbol, SSymbolParams& params);
        // scale dependence of the symbol and all the symbols it is made of (outline, layers ...)
        enum { ScaleDependentNo = 0, ScaleDependentYes = 1, ScaleDependentMixed = 2 };
        static int  GetScaleDependent(GraphEngine::Display::ISymbolPtr ptrSymbol);
        static void SetScaleDependent(GraphEngine::Display::ISymbolPtr ptrSymbol, bool bScaleDependent);
        static GraphEngine::Display::BitmapPtr  LoadBitmapFile(const std::string& sFileUtf8);   // png, jpg

        // property as text for the editor and back, SetPropertyText throws on a wrong value
        static std::string GetPropertyText(const SSymbolParams& params, eSymbolProperty prop);
        static void        SetPropertyText(SSymbolParams& params, eSymbolProperty prop, const std::string& sText);
        static std::string Describe(const SSymbolParams& params);   // "Simple fill #FF0000"

        static std::string ColorToText(const GraphEngine::Display::Color& color);
        static GraphEngine::Display::Color TextToColor(const std::string& sText);   // throws on a wrong text
    };

    enum eSelectorKind
    {
        SelectorSimple,
        SelectorUniqueValues,
        SelectorRanges
    };

    struct SUniqueValueItem
    {
        CommonLib::CVariant value;
        std::string         sLabel;
        SSymbolParams       symbol;
    };

    struct SRangeItem
    {
        double        dFrom = 0.;
        double        dTo = 0.;
        std::string   sLabel;
        SSymbolParams symbol;
    };

    struct SSymbology
    {
        eSelectorKind                 selector = SelectorSimple;
        std::string                   sField;
        SSymbolParams                 simpleSymbol;
        std::vector<SUniqueValueItem> vecValues;
        std::vector<SRangeItem>       vecRanges;
        SSymbolParams                 otherSymbol;      // values out of the list / ranges
        bool                          bDrawOther = true;
    };

    class CSymbologyBuilder
    {
    public:
        static GraphEngine::Cartography::ISymbolSelectorPtr CreateSelector(const SSymbology& symbology);
        // symbology of a selector of a layer (simple, unique values of one field, ranges);
        // false - the selector or some of its symbols can't be described (they are replaced by the defaults for the geometry)
        static bool FromSelector(GraphEngine::Cartography::ISymbolSelectorPtr ptrSelector, eGeometryKind geometry, SSymbology& symbology);

        // items with distinct colors, the symbol kind / properties are taken from the base
        static std::vector<SUniqueValueItem> MakeUniqueItems(const std::vector<CommonLib::CVariant>& values, const SSymbolParams& base);
        // equal intervals from min to max, colors from light to dark of the base color
        static std::vector<SRangeItem> MakeRanges(double dMin, double dMax, int nClasses, const SSymbolParams& base);

        static GraphEngine::Display::Color PaletteColor(int nIndex);
        static GraphEngine::Display::Color RampColor(const GraphEngine::Display::Color& base, double t);   // t 0..1 light..dark
        static std::string ValueToText(const CommonLib::CVariant& value);
        static std::string NumberToText(double dValue);
    };
}
