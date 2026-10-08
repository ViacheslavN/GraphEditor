#include "SymbologyModel.h"
#include "../../selectors/SimpleSymbolSelector.h"
#include "../../selectors/UniqueValueSymbolSelector.h"
#include "../../selectors/RangeSymbolSelector.h"
#include "../../../DisplayLib/Symbols/SimpleLineSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleMarketSymbol.h"
#include "../../../DisplayLib/Symbols/ArrowMarkerSymbol.h"
#include "../../../DisplayLib/Symbols/CharacterMarkerSymbol.h"
#include "../../../DisplayLib/Symbols/PictureMarkerSymbol.h"
#include "../../../DisplayLib/Symbols/HashLineSymbol.h"
#include "../../../DisplayLib/Symbols/MarkerLineSymbol.h"
#include "../../../DisplayLib/Symbols/LineFillSymbol.h"
#include "../../../DisplayLib/Symbols/MarkerFillSymbol.h"
#include "../../../DisplayLib/Symbols/PictureFillSymbol.h"
#include "../../../DisplayLib/GraphTypes/img/ReadPng.h"
#include "../../../DisplayLib/GraphTypes/img/ReadJPG.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include "../../../CommonLib/variant/VariantVisitor.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>

using namespace GraphEngine;

namespace TestMapDraw
{
    namespace
    {
        const char* MarkerStyles[] = {"Circle", "Square", "Cross", "X", "Diamond"};
        const char* LineStyles[] = {"Solid", "Dash", "Dot", "Dash dot", "Dash dot dot"};
        const char* FillStyles[] = {"Solid", "Horizontal", "Vertical", "Forward diagonal", "Backward diagonal", "Cross", "Diagonal cross"};
        const char* MarkerFillStyles[] = {"Grid", "Random"};

        template<size_t N>
        std::vector<std::string> Choices(const char* (&items)[N])
        {
            return std::vector<std::string>(items, items + N);
        }

        SPropertyInfo Prop(eSymbolProperty id, const char* pszLabel, ePropertyKind kind, std::vector<std::string> vecChoices = std::vector<std::string>())
        {
            SPropertyInfo info;
            info.id = id;
            info.sLabel = pszLabel;
            info.kind = kind;
            info.vecChoices = vecChoices;
            return info;
        }

        Display::ILineSymbolPtr CreateOutline(const SSymbolParams& params)
        {
            if(params.outlineColor.GetA() == Display::Color::Transparent || params.dOutlineWidth <= 0.)
                return Display::ILineSymbolPtr();
            return std::make_shared<Display::CSimpleLineSymbol>(params.outlineColor, params.dOutlineWidth, Display::SimpleLineStyleSolid);
        }

        std::shared_ptr<Display::CSimpleMarketSymbol> CreateSimpleMarker(const Display::Color& color, double dSize, int nStyle)
        {
            std::shared_ptr<Display::CSimpleMarketSymbol> ptrMarker = std::make_shared<Display::CSimpleMarketSymbol>();
            ptrMarker->SetStyle((Display::eSimpleMarkerStyle)nStyle);
            ptrMarker->SetColor(color);
            ptrMarker->SetSize(dSize);
            ptrMarker->SetOutline(false);
            return ptrMarker;
        }

        double ParseNumber(const std::string& sText)
        {
            std::string s = sText;
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
            char* pEnd = nullptr;
            double d = 0.;
            if(s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                d = (double)strtol(s.c_str(), &pEnd, 16);
            else
                d = strtod(s.c_str(), &pEnd);
            if(s.empty() || !pEnd || *pEnd != 0)
                throw CommonLib::CExcBase("Not a number: {0}", sText);
            return d;
        }

        // the png / jpg readers make bottom-up bitmaps (as Windows DIB), the symbols draw them top-down
        Display::BitmapPtr FlipRows(Display::BitmapPtr ptrBitmap)
        {
            if(!ptrBitmap.get() || ptrBitmap->Height() < 2)
                return ptrBitmap;

            size_t nLine = ptrBitmap->LineSize();
            std::vector<unsigned char> tmp(nLine);
            unsigned char* pBits = ptrBitmap->Bits();
            for(size_t top = 0, bottom = ptrBitmap->Height() - 1; top < bottom; ++top, --bottom)
            {
                memcpy(tmp.data(), pBits + top * nLine, nLine);
                memcpy(pBits + top * nLine, pBits + bottom * nLine, nLine);
                memcpy(pBits + bottom * nLine, tmp.data(), nLine);
            }
            return ptrBitmap;
        }

        int ClampStyle(int nStyle, size_t nCount)
        {
            if(nStyle < 0 || nStyle >= (int)nCount)
                return 0;
            return nStyle;
        }
    }

    eGeometryKind GeometryKindOf(CommonLib::eShapeType shapeType)
    {
        switch(CommonLib::CGeoShape::GetGeneralType(shapeType))
        {
            case CommonLib::shape_type_general_point:
            case CommonLib::shape_type_general_multipoint:
                return GeometryPoint;
            case CommonLib::shape_type_general_polyline:
                return GeometryLine;
            default:
                return GeometryPolygon;
        }
    }

    // ---------------- CSymbolFactory

    std::vector<eSymbolKind> CSymbolFactory::GetSymbolKinds(eGeometryKind geometry)
    {
        switch(geometry)
        {
            case GeometryPoint:
                return {SymbolSimpleMarker, SymbolArrowMarker, SymbolCharacterMarker, SymbolPictureMarker};
            case GeometryLine:
                return {SymbolSimpleLine, SymbolHashLine, SymbolMarkerLine};
            default:
                return {SymbolSimpleFill, SymbolLineFill, SymbolMarkerFill, SymbolPictureFill};
        }
    }

    const char* CSymbolFactory::GetSymbolKindName(eSymbolKind kind)
    {
        switch(kind)
        {
            case SymbolSimpleMarker:    return "Simple marker";
            case SymbolArrowMarker:     return "Arrow marker";
            case SymbolCharacterMarker: return "Character marker";
            case SymbolPictureMarker:   return "Picture marker";
            case SymbolSimpleLine:      return "Simple line";
            case SymbolHashLine:        return "Hash line";
            case SymbolMarkerLine:      return "Marker line";
            case SymbolSimpleFill:      return "Simple fill";
            case SymbolLineFill:        return "Line fill";
            case SymbolMarkerFill:      return "Marker fill";
            case SymbolPictureFill:     return "Picture fill";
        }
        return "Symbol";
    }

    eGeometryKind CSymbolFactory::GetGeometryKind(eSymbolKind kind)
    {
        switch(kind)
        {
            case SymbolSimpleMarker:
            case SymbolArrowMarker:
            case SymbolCharacterMarker:
            case SymbolPictureMarker:
                return GeometryPoint;
            case SymbolSimpleLine:
            case SymbolHashLine:
            case SymbolMarkerLine:
                return GeometryLine;
            default:
                return GeometryPolygon;
        }
    }

    std::vector<SPropertyInfo> CSymbolFactory::GetProperties(eSymbolKind kind)
    {
        switch(kind)
        {
            case SymbolSimpleMarker:
                return {Prop(PropStyle, "Style", PropertyChoice, Choices(MarkerStyles)), Prop(PropColor, "Color", PropertyColor),
                        Prop(PropSize, "Size, mm", PropertyNumber), Prop(PropOutlineColor, "Outline", PropertyColor),
                        Prop(PropOutlineWidth, "Outline, mm", PropertyNumber), Prop(PropAngle, "Angle", PropertyNumber)};
            case SymbolArrowMarker:
                return {Prop(PropColor, "Color", PropertyColor), Prop(PropSize, "Length, mm", PropertyNumber),
                        Prop(PropSeparation, "Width, mm", PropertyNumber), Prop(PropAngle, "Angle", PropertyNumber)};
            case SymbolCharacterMarker:
                return {Prop(PropFontFace, "Font", PropertyText), Prop(PropCharCode, "Character", PropertyNumber),
                        Prop(PropColor, "Color", PropertyColor), Prop(PropSize, "Size, mm", PropertyNumber), Prop(PropAngle, "Angle", PropertyNumber)};
            case SymbolPictureMarker:
                return {Prop(PropBitmapFile, "Image", PropertyFile), Prop(PropSize, "Size, mm", PropertyNumber), Prop(PropAngle, "Angle", PropertyNumber)};
            case SymbolSimpleLine:
                return {Prop(PropColor, "Color", PropertyColor), Prop(PropSize, "Width", PropertyNumber),
                        Prop(PropStyle, "Style", PropertyChoice, Choices(LineStyles))};
            case SymbolHashLine:
                return {Prop(PropColor, "Color", PropertyColor), Prop(PropSize, "Length, mm", PropertyNumber),
                        Prop(PropSeparation, "Interval, mm", PropertyNumber), Prop(PropAngle, "Angle", PropertyNumber),
                        Prop(PropOutlineWidth, "Hash width", PropertyNumber)};
            case SymbolMarkerLine:
                return {Prop(PropStyle, "Marker", PropertyChoice, Choices(MarkerStyles)), Prop(PropColor, "Color", PropertyColor),
                        Prop(PropSize, "Size, mm", PropertyNumber), Prop(PropSeparation, "Interval, mm", PropertyNumber)};
            case SymbolSimpleFill:
                return {Prop(PropColor, "Color", PropertyColor), Prop(PropStyle, "Style", PropertyChoice, Choices(FillStyles)),
                        Prop(PropOutlineColor, "Outline", PropertyColor), Prop(PropOutlineWidth, "Outline width", PropertyNumber)};
            case SymbolLineFill:
                return {Prop(PropColor, "Line color", PropertyColor), Prop(PropSize, "Line width", PropertyNumber),
                        Prop(PropAngle, "Angle", PropertyNumber), Prop(PropSeparation, "Separation, mm", PropertyNumber),
                        Prop(PropOutlineColor, "Outline", PropertyColor), Prop(PropOutlineWidth, "Outline width", PropertyNumber)};
            case SymbolMarkerFill:
                return {Prop(PropStyle, "Placement", PropertyChoice, Choices(MarkerFillStyles)), Prop(PropColor, "Marker color", PropertyColor),
                        Prop(PropSize, "Marker size, mm", PropertyNumber), Prop(PropSeparation, "Separation, mm", PropertyNumber),
                        Prop(PropOutlineColor, "Outline", PropertyColor), Prop(PropOutlineWidth, "Outline width", PropertyNumber)};
            case SymbolPictureFill:
                return {Prop(PropBitmapFile, "Image", PropertyFile), Prop(PropColor, "Mono color", PropertyColor),
                        Prop(PropOutlineColor, "Outline", PropertyColor), Prop(PropOutlineWidth, "Outline width", PropertyNumber)};
        }
        return std::vector<SPropertyInfo>();
    }

    SSymbolParams CSymbolFactory::CreateDefault(eSymbolKind kind, const Display::Color& color)
    {
        SSymbolParams params;
        params.kind = kind;
        params.color = color;
        switch(kind)
        {
            case SymbolSimpleMarker:    params.dSize = 2.;  params.dOutlineWidth = 0.2; break;
            case SymbolArrowMarker:     params.dSize = 4.;  params.dSeparation = 2.; break;
            case SymbolCharacterMarker: params.dSize = 4.;  break;
            case SymbolPictureMarker:   params.dSize = 5.;  break;
            case SymbolSimpleLine:      params.dSize = 1.;  break;
            case SymbolHashLine:        params.dSize = 2.;  params.dSeparation = 2.; params.dAngle = 90.; params.dOutlineWidth = 1.; break;
            case SymbolMarkerLine:      params.dSize = 1.5; params.dSeparation = 4.; break;
            case SymbolSimpleFill:      params.dOutlineWidth = 1.; break;
            case SymbolLineFill:        params.dSize = 1.;  params.dAngle = 45.; params.dSeparation = 2.; params.dOutlineWidth = 1.; break;
            case SymbolMarkerFill:      params.dSize = 1.;  params.dSeparation = 4.; params.dOutlineWidth = 1.; break;
            case SymbolPictureFill:     params.dOutlineWidth = 1.; break;
        }
        return params;
    }

    SSymbolParams CSymbolFactory::CreateDefault(eGeometryKind geometry, const Display::Color& color)
    {
        return CreateDefault(GetSymbolKinds(geometry).front(), color);
    }

    SSymbolParams CSymbolFactory::ChangeKind(const SSymbolParams& params, eSymbolKind kind)
    {
        if(params.kind == kind)
            return params;

        SSymbolParams result = CreateDefault(kind, params.color);
        result.outlineColor = params.outlineColor;
        result.sBitmapFile = params.sBitmapFile;
        result.sFontFace = params.sFontFace;
        result.nCharCode = params.nCharCode;
        return result;
    }

    Display::BitmapPtr CSymbolFactory::LoadBitmapFile(const std::string& sFileUtf8)
    {
        if(sFileUtf8.empty())
            return Display::BitmapPtr();

#ifdef _WIN32
        std::filesystem::path path(CommonLib::StringEncoding::str_utf82w_safe(sFileUtf8));
#else
        std::filesystem::path path(sFileUtf8);
#endif
        std::ifstream file(path, std::ios::binary);
        if(!file)
            throw CommonLib::CExcBase("Can't open image {0}", sFileUtf8);

        std::vector<byte_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if(data.empty())
            throw CommonLib::CExcBase("Empty image {0}", sFileUtf8);

        std::string sExt = path.extension().string();
        std::transform(sExt.begin(), sExt.end(), sExt.begin(), [](unsigned char c) { return (char)tolower(c); });
        if(sExt == ".png")
            return FlipRows(Display::ReadPng(data.data(), (int)data.size()).Read());
        if(sExt == ".jpg" || sExt == ".jpeg")
            return FlipRows(Display::CReadJPG(data.data(), (int)data.size()).Read());

        throw CommonLib::CExcBase("Unsupported image format {0} (png, jpg)", sFileUtf8);
    }

    Display::ISymbolPtr CSymbolFactory::CreateSymbol(const SSymbolParams& params)
    {
        switch(params.kind)
        {
            case SymbolSimpleMarker:
            {
                std::shared_ptr<Display::CSimpleMarketSymbol> ptrMarker = CreateSimpleMarker(params.color, params.dSize, ClampStyle(params.nStyle, 5));
                bool bOutline = params.outlineColor.GetA() != Display::Color::Transparent && params.dOutlineWidth > 0.;
                ptrMarker->SetOutline(bOutline);
                ptrMarker->SetOutlineColor(params.outlineColor);
                ptrMarker->SetOutlineSize(params.dOutlineWidth);
                ptrMarker->SetAngle(params.dAngle);
                return ptrMarker;
            }
            case SymbolArrowMarker:
            {
                std::shared_ptr<Display::CArrowMarkerSymbol> ptrArrow = std::make_shared<Display::CArrowMarkerSymbol>(params.dSize, params.dSeparation, params.color);
                ptrArrow->SetAngle(params.dAngle);
                return ptrArrow;
            }
            case SymbolCharacterMarker:
            {
                std::shared_ptr<Display::CCharacterMarkerSymbol> ptrChar = std::make_shared<Display::CCharacterMarkerSymbol>(params.sFontFace, params.nCharCode, params.dSize, params.color);
                ptrChar->SetAngle(params.dAngle);
                return ptrChar;
            }
            case SymbolPictureMarker:
            {
                std::shared_ptr<Display::CPictureMarkerSymbol> ptrPicture = std::make_shared<Display::CPictureMarkerSymbol>(LoadBitmapFile(params.sBitmapFile), params.dSize);
                ptrPicture->SetAngle(params.dAngle);
                return ptrPicture;
            }
            case SymbolSimpleLine:
                return std::make_shared<Display::CSimpleLineSymbol>(params.color, params.dSize, (Display::eSimpleLineStyle)ClampStyle(params.nStyle, 5));
            case SymbolHashLine:
            {
                Display::ILineSymbolPtr ptrHash = std::make_shared<Display::CSimpleLineSymbol>(params.color, params.dOutlineWidth > 0. ? params.dOutlineWidth : 1., Display::SimpleLineStyleSolid);
                std::shared_ptr<Display::CHashLineSymbol> ptrHashLine = std::make_shared<Display::CHashLineSymbol>(ptrHash, params.dSize, params.dSeparation);
                ptrHashLine->SetAngle(params.dAngle);
                return ptrHashLine;
            }
            case SymbolMarkerLine:
                return std::make_shared<Display::CMarkerLineSymbol>(CreateSimpleMarker(params.color, params.dSize, ClampStyle(params.nStyle, 5)), params.dSeparation);
            case SymbolSimpleFill:
            {
                std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                ptrFill->SetStyle((Display::eSimpleFillStyle)(ClampStyle(params.nStyle, 7) == 0 ? 0 : ClampStyle(params.nStyle, 7) + 1));   // no "Null" in the list
                ptrFill->SetColor(params.color);
                if(ptrFill->GetStyle() != Display::SimpleFillStyleSolid)
                    ptrFill->SetBackgroundColor(Display::Color(Display::Color::White, Display::Color::Transparent));
                ptrFill->SetOutlineSymbol(CreateOutline(params));
                return ptrFill;
            }
            case SymbolLineFill:
            {
                Display::ILineSymbolPtr ptrLine = std::make_shared<Display::CSimpleLineSymbol>(params.color, params.dSize, Display::SimpleLineStyleSolid);
                std::shared_ptr<Display::CLineFillSymbol> ptrFill = std::make_shared<Display::CLineFillSymbol>(ptrLine, params.dAngle, params.dSeparation);
                ptrFill->SetOutlineSymbol(CreateOutline(params));
                return ptrFill;
            }
            case SymbolMarkerFill:
            {
                std::shared_ptr<Display::CMarkerFillSymbol> ptrFill = std::make_shared<Display::CMarkerFillSymbol>(
                    CreateSimpleMarker(params.color, params.dSize, Display::SimpleMarkerStyleCircle), params.dSeparation, params.dSeparation);
                ptrFill->SetStyle(ClampStyle(params.nStyle, 2) == 1 ? Display::MarkerFillStyleRandom : Display::MarkerFillStyleGrid);
                ptrFill->SetOutlineSymbol(CreateOutline(params));
                return ptrFill;
            }
            case SymbolPictureFill:
            {
                std::shared_ptr<Display::CPictureFillSymbol> ptrFill = std::make_shared<Display::CPictureFillSymbol>(LoadBitmapFile(params.sBitmapFile));
                ptrFill->SetColor(params.color);
                ptrFill->SetOutlineSymbol(CreateOutline(params));
                return ptrFill;
            }
        }
        throw CommonLib::CExcBase("Unknown symbol kind {0}", (int)params.kind);
    }

    std::string CSymbolFactory::ColorToText(const Display::Color& color)
    {
        if(color.GetA() == Display::Color::Transparent)
            return std::string();

        char buf[16];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", color.GetR(), color.GetG(), color.GetB());
        return buf;
    }

    Display::Color CSymbolFactory::TextToColor(const std::string& sText)
    {
        std::string s = sText;
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
        if(s.empty())
            return Display::Color(Display::Color::Black, Display::Color::Transparent);

        if(s[0] == '#')
            s = s.substr(1);
        char* pEnd = nullptr;
        unsigned long rgb = strtoul(s.c_str(), &pEnd, 16);
        if(s.size() != 6 || !pEnd || *pEnd != 0)
            throw CommonLib::CExcBase("Wrong color {0}, expected #RRGGBB", sText);

        return Display::Color((Display::Color::ColorComponent)((rgb >> 16) & 0xFF), (Display::Color::ColorComponent)((rgb >> 8) & 0xFF),
                              (Display::Color::ColorComponent)(rgb & 0xFF));
    }

    std::string CSymbolFactory::GetPropertyText(const SSymbolParams& params, eSymbolProperty prop)
    {
        switch(prop)
        {
            case PropColor:        return ColorToText(params.color);
            case PropSize:         return CSymbologyBuilder::NumberToText(params.dSize);
            case PropStyle:        return CSymbologyBuilder::NumberToText(params.nStyle);
            case PropOutlineColor: return ColorToText(params.outlineColor);
            case PropOutlineWidth: return CSymbologyBuilder::NumberToText(params.dOutlineWidth);
            case PropAngle:        return CSymbologyBuilder::NumberToText(params.dAngle);
            case PropSeparation:   return CSymbologyBuilder::NumberToText(params.dSeparation);
            case PropBitmapFile:   return params.sBitmapFile;
            case PropFontFace:     return params.sFontFace;
            case PropCharCode:
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "0x%X", params.nCharCode);
                return buf;
            }
        }
        return std::string();
    }

    void CSymbolFactory::SetPropertyText(SSymbolParams& params, eSymbolProperty prop, const std::string& sText)
    {
        switch(prop)
        {
            case PropColor:        params.color = TextToColor(sText); break;
            case PropOutlineColor: params.outlineColor = TextToColor(sText); break;
            case PropStyle:        params.nStyle = (int)ParseNumber(sText); break;
            case PropAngle:        params.dAngle = ParseNumber(sText); break;
            case PropBitmapFile:   params.sBitmapFile = sText; break;
            case PropFontFace:     params.sFontFace = sText; break;
            case PropSize:
            case PropOutlineWidth:
            case PropSeparation:
            {
                double d = ParseNumber(sText);
                if(d < 0.)
                    throw CommonLib::CExcBase("The value can't be negative: {0}", sText);
                if(prop == PropSize)
                    params.dSize = d;
                else if(prop == PropOutlineWidth)
                    params.dOutlineWidth = d;
                else
                    params.dSeparation = d;
                break;
            }
            case PropCharCode:
            {
                double d = ParseNumber(sText);
                if(d <= 0. || d > 0xFFFF)
                    throw CommonLib::CExcBase("Wrong character code: {0}", sText);
                params.nCharCode = (int)d;
                break;
            }
        }
    }

    std::string CSymbolFactory::Describe(const SSymbolParams& params)
    {
        std::string s = GetSymbolKindName(params.kind);
        if(params.kind == SymbolPictureMarker || params.kind == SymbolPictureFill)
        {
            std::filesystem::path path(params.sBitmapFile);
            return s + " " + path.filename().string();
        }
        return s + " " + ColorToText(params.color);
    }

    // ---------------- CSymbologyBuilder

    Cartography::ISymbolSelectorPtr CSymbologyBuilder::CreateSelector(const SSymbology& symbology)
    {
        switch(symbology.selector)
        {
            case SelectorSimple:
                return std::make_shared<Cartography::CSimpleSymbolSelector>(CSymbolFactory::CreateSymbol(symbology.simpleSymbol));

            case SelectorUniqueValues:
            {
                if(symbology.sField.empty())
                    throw CommonLib::CExcBase("Select the field for the unique values");

                std::shared_ptr<Cartography::CUniqueValueSymbolSelector> ptrSelector = std::make_shared<Cartography::CUniqueValueSymbolSelector>(symbology.sField);
                for(size_t i = 0; i < symbology.vecValues.size(); ++i)
                {
                    const SUniqueValueItem& item = symbology.vecValues[i];
                    ptrSelector->AddValue({item.value}, CSymbolFactory::CreateSymbol(item.symbol), item.sLabel);
                }
                ptrSelector->SetDefaultSymbol(CSymbolFactory::CreateSymbol(symbology.otherSymbol));
                ptrSelector->SetDefaultLabel("Other values");
                ptrSelector->SetUseDefaultSymbol(symbology.bDrawOther);
                ptrSelector->SetHeadingLabel(symbology.sField);
                return ptrSelector;
            }

            case SelectorRanges:
            {
                if(symbology.sField.empty())
                    throw CommonLib::CExcBase("Select the field for the ranges");

                std::shared_ptr<Cartography::CRangeSymbolSelector> ptrSelector = std::make_shared<Cartography::CRangeSymbolSelector>(symbology.sField);
                for(size_t i = 0; i < symbology.vecRanges.size(); ++i)
                {
                    const SRangeItem& item = symbology.vecRanges[i];
                    ptrSelector->AddRange(item.dFrom, item.dTo, CSymbolFactory::CreateSymbol(item.symbol), item.sLabel);
                }
                ptrSelector->SetDefaultSymbol(CSymbolFactory::CreateSymbol(symbology.otherSymbol));
                ptrSelector->SetDefaultLabel("Other values");
                ptrSelector->SetUseDefaultSymbol(symbology.bDrawOther);
                return ptrSelector;
            }
        }
        throw CommonLib::CExcBase("Unknown symbol selector {0}", (int)symbology.selector);
    }

    Display::Color CSymbologyBuilder::PaletteColor(int nIndex)
    {
        // distinct hues by the golden angle, alternating lightness
        double dHue = fmod(nIndex * 137.508, 360.);
        double s = 0.65;
        double l = (nIndex % 2) ? 0.45 : 0.6;
        double c = (1. - fabs(2. * l - 1.)) * s;
        double x = c * (1. - fabs(fmod(dHue / 60., 2.) - 1.));
        double m = l - c / 2.;
        double r = 0., g = 0., b = 0.;
        if(dHue < 60.)       { r = c; g = x; }
        else if(dHue < 120.) { r = x; g = c; }
        else if(dHue < 180.) { g = c; b = x; }
        else if(dHue < 240.) { g = x; b = c; }
        else if(dHue < 300.) { r = x; b = c; }
        else                 { r = c; b = x; }
        return Display::Color((Display::Color::ColorComponent)std::lround((r + m) * 255.), (Display::Color::ColorComponent)std::lround((g + m) * 255.),
                              (Display::Color::ColorComponent)std::lround((b + m) * 255.));
    }

    Display::Color CSymbologyBuilder::RampColor(const Display::Color& base, double t)
    {
        t = (std::max)(0., (std::min)(1., t));
        // from 85% white mix (light) to 40% black mix (dark)
        auto mix = [&](int c) -> Display::Color::ColorComponent
        {
            double light = c + (255. - c) * 0.85;
            double dark = c * 0.6;
            return (Display::Color::ColorComponent)std::lround(light + (dark - light) * t);
        };
        return Display::Color(mix(base.GetR()), mix(base.GetG()), mix(base.GetB()));
    }

    std::vector<SUniqueValueItem> CSymbologyBuilder::MakeUniqueItems(const std::vector<CommonLib::CVariant>& values, const SSymbolParams& base)
    {
        std::vector<SUniqueValueItem> vecItems;
        for(size_t i = 0; i < values.size(); ++i)
        {
            SUniqueValueItem item;
            item.value = values[i];
            item.sLabel = ValueToText(values[i]);
            item.symbol = base;
            item.symbol.color = PaletteColor((int)i);
            vecItems.push_back(item);
        }
        return vecItems;
    }

    std::vector<SRangeItem> CSymbologyBuilder::MakeRanges(double dMin, double dMax, int nClasses, const SSymbolParams& base)
    {
        if(nClasses < 1)
            throw CommonLib::CExcBase("Wrong number of classes: {0}", nClasses);
        if(dMax < dMin)
            std::swap(dMin, dMax);

        std::vector<SRangeItem> vecItems;
        double dStep = (dMax - dMin) / nClasses;
        if(dStep <= 0.)
            nClasses = 1;

        for(int i = 0; i < nClasses; ++i)
        {
            SRangeItem item;
            item.dFrom = dMin + dStep * i;
            item.dTo = (i == nClasses - 1) ? dMax : dMin + dStep * (i + 1);
            item.sLabel = NumberToText(item.dFrom) + " - " + NumberToText(item.dTo);
            item.symbol = base;
            item.symbol.color = RampColor(base.color, nClasses == 1 ? 0.5 : (double)i / (nClasses - 1));
            vecItems.push_back(item);
        }
        return vecItems;
    }

    std::string CSymbologyBuilder::NumberToText(double dValue)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.10g", dValue);
        return buf;
    }

    std::string CSymbologyBuilder::ValueToText(const CommonLib::CVariant& value)
    {
        if(value.IsNull())
            return "<null>";
        if(value.IsType<CommonLib::astr_t>())
            return value.Get<CommonLib::astr_t>();
        if(value.IsType<CommonLib::wstr_t>())
            return CommonLib::StringEncoding::str_w2utf8_safe(value.Get<CommonLib::wstr_t>());
        if(value.IsType<double>())
            return NumberToText(value.Get<double>());
        if(value.IsType<float>())
            return NumberToText(value.Get<float>());

        CommonLib::CStringVisitor visitor;
        value.Accept(visitor);
        return visitor.GetString();
    }
}
