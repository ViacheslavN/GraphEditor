// tests of the platform independent part of the TestMapWTLDraw dialogs: symbol parameters, symbology, data queries
#include "TestCommon.h"
#include "../TestMapWTLDraw/SymbologyModel.h"
#include "../TestMapWTLDraw/MapProject.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../DisplayLib/Symbols/PictureMarkerSymbol.h"
#include "../../../DisplayLib/Symbols/PictureFillSymbol.h"
#include "../../../DisplayLib/Symbols/LineFillSymbol.h"
#include "../../../ThirdParty/ShapeLib/shapefil.h"
#include "../../../CommonLib/compress/zlib/zlib.h"
#include <filesystem>
#include <fstream>

using namespace GraphEngine;
using namespace TestMapDraw;

namespace
{
    std::string TestDir()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineSymbologyTests";
        std::filesystem::create_directories(dir);
        return dir.string();
    }

    // three squares: NAME A, B, A; VALUE 1.5, 10, 20
    std::string CreateShapefile()
    {
        std::string base = (std::filesystem::path(TestDir()) / "parcels").string();
        SHPHandle hShp = SHPCreate(base.c_str(), SHPT_POLYGON);
        DBFHandle hDbf = DBFCreate(base.c_str());
        DBFAddField(hDbf, "NAME", FTString, 10, 0);
        DBFAddField(hDbf, "VALUE", FTDouble, 10, 2);
        const char* names[3] = {"A", "B", "A"};
        const double values[3] = {1.5, 10., 20.};
        for(int i = 0; i < 3; ++i)
        {
            double x0 = i * 20.;
            double x[5] = {x0, x0, x0 + 10., x0 + 10., x0};
            double y[5] = {0., 10., 10., 0., 0.};
            SHPObject* pObj = SHPCreateSimpleObject(SHPT_POLYGON, 5, x, y, nullptr);
            SHPWriteObject(hShp, -1, pObj);
            SHPDestroyObject(pObj);
            DBFWriteStringAttribute(hDbf, i, 0, names[i]);
            DBFWriteDoubleAttribute(hDbf, i, 1, values[i]);
        }
        SHPClose(hShp);
        DBFClose(hDbf);
        return base + ".shp";
    }

    void PutBE32(std::vector<unsigned char>& data, uint32_t v)
    {
        data.push_back((unsigned char)(v >> 24)); data.push_back((unsigned char)(v >> 16));
        data.push_back((unsigned char)(v >> 8)); data.push_back((unsigned char)v);
    }

    void PutChunk(std::vector<unsigned char>& png, const char* type, const std::vector<unsigned char>& body)
    {
        PutBE32(png, (uint32_t)body.size());
        std::vector<unsigned char> crcData(type, type + 4);
        crcData.insert(crcData.end(), body.begin(), body.end());
        png.insert(png.end(), crcData.begin(), crcData.end());
        PutBE32(png, (uint32_t)crc32(0, crcData.data(), (uInt)crcData.size()));
    }

    // 2x2 RGBA png: top-left red, top-right green, bottom-left blue, bottom-right white
    std::string CreatePng()
    {
        std::vector<unsigned char> rows;   // filter byte 0 + RGBA pixels per row
        unsigned char pixels[2][2][4] = {{{255, 0, 0, 255}, {0, 255, 0, 255}}, {{0, 0, 255, 255}, {255, 255, 255, 255}}};
        for(int r = 0; r < 2; ++r)
        {
            rows.push_back(0);
            for(int c = 0; c < 2; ++c)
                rows.insert(rows.end(), pixels[r][c], pixels[r][c] + 4);
        }

        uLongf nCompressed = compressBound((uLong)rows.size());
        std::vector<unsigned char> compressed(nCompressed);
        compress(compressed.data(), &nCompressed, rows.data(), (uLong)rows.size());
        compressed.resize(nCompressed);

        std::vector<unsigned char> png = {137, 80, 78, 71, 13, 10, 26, 10};
        std::vector<unsigned char> header;
        PutBE32(header, 2);
        PutBE32(header, 2);
        header.push_back(8);   // bit depth
        header.push_back(6);   // RGBA
        header.push_back(0); header.push_back(0); header.push_back(0);
        PutChunk(png, "IHDR", header);
        PutChunk(png, "IDAT", compressed);
        PutChunk(png, "IEND", std::vector<unsigned char>());

        std::string sFile = (std::filesystem::path(TestDir()) / "test.png").string();
        std::ofstream file(sFile, std::ios::binary);
        file.write((const char*)png.data(), png.size());
        return sFile;
    }

    Display::Color ColorOf(Display::ISymbolPtr ptrSymbol)
    {
        std::shared_ptr<Display::IFillSymbol> ptrFill = std::dynamic_pointer_cast<Display::IFillSymbol>(ptrSymbol);
        REQUIRE(ptrFill != nullptr);
        return ptrFill->GetColor();
    }

    GeoDatabase::IRowPtr Row(const char* pszName, double dValue)
    {
        GeoDatabase::IFieldsPtr ptrFields = std::make_shared<GeoDatabase::CFields>();
        ptrFields->AddField(cartography_test::CreateField("NAME", GeoDatabase::dtString));
        ptrFields->AddField(cartography_test::CreateField("VALUE", GeoDatabase::dtDouble));
        GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(ptrFields);
        ptrRow->SetText(0, pszName);
        ptrRow->SetDouble(1, dValue);
        return ptrRow;
    }
}

TEST_CASE("Symbol factory makes every symbol kind", "[symbology]")
{
    const eGeometryKind geometries[3] = {GeometryPoint, GeometryLine, GeometryPolygon};
    for(int g = 0; g < 3; ++g)
    {
        std::vector<eSymbolKind> vecKinds = CSymbolFactory::GetSymbolKinds(geometries[g]);
        REQUIRE_FALSE(vecKinds.empty());
        for(size_t k = 0; k < vecKinds.size(); ++k)
        {
            REQUIRE(CSymbolFactory::GetGeometryKind(vecKinds[k]) == geometries[g]);
            SSymbolParams params = CSymbolFactory::CreateDefault(vecKinds[k], Display::Color(10, 20, 30));
            REQUIRE(params.kind == vecKinds[k]);
            REQUIRE(CSymbolFactory::CreateSymbol(params) != nullptr);   // picture without an image too

            // every property shown in the editor can be read and written back
            std::vector<SPropertyInfo> vecProps = CSymbolFactory::GetProperties(vecKinds[k]);
            REQUIRE_FALSE(vecProps.empty());
            for(size_t p = 0; p < vecProps.size(); ++p)
            {
                std::string sText = CSymbolFactory::GetPropertyText(params, vecProps[p].id);
                SSymbolParams copy = params;
                CSymbolFactory::SetPropertyText(copy, vecProps[p].id, sText);
                REQUIRE(CSymbolFactory::GetPropertyText(copy, vecProps[p].id) == sText);
                if(vecProps[p].kind == PropertyChoice)
                    REQUIRE_FALSE(vecProps[p].vecChoices.empty());
            }
        }
    }
}

TEST_CASE("Symbol properties as text", "[symbology]")
{
    SSymbolParams params = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(255, 0, 0));
    REQUIRE(CSymbolFactory::GetPropertyText(params, PropColor) == "#FF0000");

    CSymbolFactory::SetPropertyText(params, PropColor, " #00ff80 ");
    REQUIRE(params.color == Display::Color(0, 255, 128));
    CSymbolFactory::SetPropertyText(params, PropOutlineColor, "");   // no outline
    REQUIRE(params.outlineColor.GetA() == Display::Color::Transparent);
    REQUIRE(std::dynamic_pointer_cast<Display::IFillSymbol>(CSymbolFactory::CreateSymbol(params))->GetOutlineSymbol() == nullptr);

    CSymbolFactory::SetPropertyText(params, PropOutlineWidth, "0.5");
    REQUIRE(params.dOutlineWidth == 0.5);
    REQUIRE_THROWS(CSymbolFactory::SetPropertyText(params, PropColor, "red"));
    REQUIRE_THROWS(CSymbolFactory::SetPropertyText(params, PropOutlineWidth, "-1"));
    REQUIRE_THROWS(CSymbolFactory::SetPropertyText(params, PropSize, "1,5"));

    SSymbolParams character = CSymbolFactory::CreateDefault(SymbolCharacterMarker, Display::Color(0, 0, 0));
    CSymbolFactory::SetPropertyText(character, PropCharCode, "0x6C");
    REQUIRE(character.nCharCode == 0x6C);
    CSymbolFactory::SetPropertyText(character, PropCharCode, "65");
    REQUIRE(CSymbolFactory::GetPropertyText(character, PropCharCode) == "0x41");

    REQUIRE(CSymbolFactory::Describe(params) == "Simple fill #00FF80");

    SSymbolParams line = CSymbolFactory::ChangeKind(params, SymbolLineFill);
    REQUIRE(line.kind == SymbolLineFill);
    REQUIRE(line.color == params.color);
}

TEST_CASE("Picture symbols load png images top-down", "[symbology]")
{
    std::string sPng = CreatePng();
    Display::BitmapPtr ptrBitmap = CSymbolFactory::LoadBitmapFile(sPng);
    REQUIRE(ptrBitmap != nullptr);
    REQUIRE(ptrBitmap->Width() == 2);
    REQUIRE(ptrBitmap->Pixel(0, 0).GetRGB() == Display::Color(255, 0, 0).GetRGB());   // row 0 is the top
    REQUIRE(ptrBitmap->Pixel(1, 0).GetRGB() == Display::Color(0, 0, 255).GetRGB());

    SSymbolParams params = CSymbolFactory::CreateDefault(SymbolPictureMarker, Display::Color(0, 0, 0));
    params.sBitmapFile = sPng;
    std::shared_ptr<Display::CPictureMarkerSymbol> ptrMarker = std::dynamic_pointer_cast<Display::CPictureMarkerSymbol>(CSymbolFactory::CreateSymbol(params));
    REQUIRE(ptrMarker != nullptr);
    REQUIRE(ptrMarker->GetBitmap() != nullptr);

    params.sBitmapFile = sPng + ".missing.png";
    REQUIRE_THROWS(CSymbolFactory::CreateSymbol(params));
    params.sBitmapFile = sPng + ".bmp";
    REQUIRE_THROWS(CSymbolFactory::CreateSymbol(params));
}

TEST_CASE("Symbology: unique values and ranges make the selectors", "[symbology]")
{
    SSymbolParams base = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(200, 100, 50));

    SSymbology symbology;
    symbology.selector = SelectorUniqueValues;
    symbology.sField = "NAME";
    symbology.vecValues = CSymbologyBuilder::MakeUniqueItems({CommonLib::CVariant(CommonLib::astr_t("A")), CommonLib::CVariant(CommonLib::astr_t("B"))}, base);
    symbology.otherSymbol = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(1, 1, 1));
    REQUIRE(symbology.vecValues[1].sLabel == "B");
    REQUIRE(symbology.vecValues[0].symbol.color != symbology.vecValues[1].symbol.color);

    Cartography::ISymbolSelectorPtr ptrSelector = CSymbologyBuilder::CreateSelector(symbology);
    REQUIRE(ptrSelector->GetSymbolSelectorID() == Cartography::UniqueValueSymbolSelectorID);
    REQUIRE(ColorOf(ptrSelector->GetSymbolByFeature(Row("B", 0.))) == symbology.vecValues[1].symbol.color);
    REQUIRE(ColorOf(ptrSelector->GetSymbolByFeature(Row("C", 0.))) == Display::Color(1, 1, 1));
    symbology.bDrawOther = false;
    REQUIRE(CSymbologyBuilder::CreateSelector(symbology)->GetSymbolByFeature(Row("C", 0.)) == nullptr);

    SSymbology ranges;
    ranges.selector = SelectorRanges;
    ranges.sField = "VALUE";
    ranges.vecRanges = CSymbologyBuilder::MakeRanges(0., 10., 5, base);
    ranges.otherSymbol = base;
    REQUIRE(ranges.vecRanges.size() == 5);
    REQUIRE(ranges.vecRanges[1].dFrom == 2.);
    REQUIRE(ranges.vecRanges[4].dTo == 10.);
    REQUIRE(ranges.vecRanges[0].sLabel == "0 - 2");
    REQUIRE(ranges.vecRanges[0].symbol.color != ranges.vecRanges[4].symbol.color);   // light .. dark

    Cartography::ISymbolSelectorPtr ptrRanges = CSymbologyBuilder::CreateSelector(ranges);
    REQUIRE(ColorOf(ptrRanges->GetSymbolByFeature(Row("x", 9.))) == ranges.vecRanges[4].symbol.color);

    REQUIRE(CSymbologyBuilder::MakeRanges(5., 5., 4, base).size() == 1);   // all values the same
    REQUIRE_THROWS(CSymbologyBuilder::MakeRanges(0., 1., 0, base));

    ranges.sField.clear();
    REQUIRE_THROWS(CSymbologyBuilder::CreateSelector(ranges));
}

TEST_CASE("Project: values of a field and a layer with symbology", "[symbology][project]")
{
    std::string sShp = CreateShapefile();
    STableInfo info = CMapProject::GetShapefileInfo(sShp);
    REQUIRE(GeometryKindOf(info.shapeType) == GeometryPolygon);
    REQUIRE(info.vecFields.size() == 3);   // NAME, VALUE and the FID
    auto field = [&](const std::string& sName) -> const SFieldInfo&
    {
        for(size_t i = 0; i < info.vecFields.size(); ++i)
            if(info.vecFields[i].sName == sName)
                return info.vecFields[i];
        FAIL("no field " << sName);
        return info.vecFields[0];
    };
    REQUIRE(field("NAME").bText);
    REQUIRE_FALSE(field("NAME").bNumeric);
    REQUIRE(field("VALUE").bNumeric);

    SDataSource source;
    source.sPath = sShp;

    bool bTruncated = true;
    std::vector<CommonLib::CVariant> vecValues = CMapProject::GetUniqueValues(source, "NAME", 100, &bTruncated);
    REQUIRE_FALSE(bTruncated);
    REQUIRE(vecValues.size() == 2);
    REQUIRE(CSymbologyBuilder::ValueToText(vecValues[0]) == "A");
    REQUIRE(CSymbologyBuilder::ValueToText(vecValues[1]) == "B");

    CMapProject::GetUniqueValues(source, "VALUE", 2, &bTruncated);
    REQUIRE(bTruncated);

    double dMin = 0., dMax = 0.;
    REQUIRE(CMapProject::GetValueRange(source, "VALUE", dMin, dMax));
    REQUIRE(dMin == 1.5);
    REQUIRE(dMax == 20.);
    REQUIRE_THROWS(CMapProject::GetValueRange(source, "NO_FIELD", dMin, dMax));

    SLayerParams params;
    params.ptrSymbology = std::make_shared<SSymbology>();
    params.ptrSymbology->selector = SelectorUniqueValues;
    params.ptrSymbology->sField = "NAME";
    params.ptrSymbology->vecValues = CSymbologyBuilder::MakeUniqueItems(vecValues, CSymbolFactory::CreateDefault(SymbolLineFill, Display::Color(0, 0, 0)));
    params.ptrSymbology->otherSymbol = CSymbolFactory::CreateDefault(SymbolSimpleFill, Display::Color(0, 0, 0));
    params.annotation.sField = "NAME";

    CMapProject project;
    Cartography::ILayerPtr ptrLayer = project.AddShapefile(sShp, params);
    Cartography::IFeatureLayerPtr ptrFeatureLayer = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(ptrLayer);
    REQUIRE(ptrFeatureLayer != nullptr);
    Cartography::ISymbolSelectorPtr ptrSelector = ptrFeatureLayer->GetRenderer(0)->GetSymbolSelector();
    REQUIRE(ptrSelector->GetSymbolSelectorID() == Cartography::UniqueValueSymbolSelectorID);
    REQUIRE(std::dynamic_pointer_cast<Display::CLineFillSymbol>(ptrSelector->GetSymbolByFeature(Row("A", 0.))) != nullptr);
    REQUIRE(ptrFeatureLayer->GetAnnoFieldName() == "NAME");

    // no symbology - the default symbol
    Cartography::IFeatureLayerPtr ptrDefault = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(project.AddShapefile(sShp));
    REQUIRE(ptrDefault->GetRenderer(0)->GetSymbolSelector()->GetSymbolSelectorID() == Cartography::SimpleSymbolSelectorID);
}
