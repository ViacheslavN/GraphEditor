#include "TestCommon.h"

#include "../../Display/Display.h"
#include "../../Symbols/SymbolBase.h"
#include "../../Symbols/SymbolsLoader.h"
#include "../../Symbols/SimpleLineSymbol.h"
#include "../../Symbols/SimpleFillSymbol.h"
#include "../../Symbols/SimpleMarketSymbol.h"
#include "../../Symbols/LineTemplate.h"
#include "../../Symbols/HashLineSymbol.h"
#include "../../Symbols/MarkerLineSymbol.h"
#include "../../Symbols/LineFillSymbol.h"
#include "../../Symbols/MarkerFillSymbol.h"
#include "../../Symbols/PictureFillSymbol.h"
#include "../../Symbols/PictureMarkerSymbol.h"
#include "../../Symbols/CharacterMarkerSymbol.h"
#include "../../Symbols/MultiLayerSymbol.h"
#include "../../Symbols/SymbolBitmapUtils.h"
#include "../../Symbols/ArrowMarkerSymbol.h"
#include "../../../CommonLib/Serialize/SerializeXML.h"
#include "../../../CommonLib/xml/XMLNode.h"

using namespace display_test;
using namespace GraphEngine::Display;
using Catch::Matchers::WithinAbs;

namespace
{
    const GRect Window(0, 0, 200, 200);
    const double PixelsPerMm = 96. / 25.4;

    double Mm(double pixels) { return pixels / PixelsPerMm; }   // symbol size which gives these device pixels

    struct STestDisplay
    {
        IGraphicsPtr ptrGraphics;
        IDisplayPtr ptrDisplay;

        STestDisplay()
        {
            auto ptrTrans = std::make_shared<CDisplayTransformation2D>(96., CommonLib::UnitsMeters, Window, ScaleOneMeterPerPixel());
            ptrTrans->SetDeviceClipRect(Window);
            CommonLib::GisXYPoint center = {100., 100.};
            ptrTrans->SetMapPos(center, ptrTrans->GetScale());

            ptrGraphics = IGraphics::CreateCGraphicsAgg(200, 200, false);
            ptrDisplay = std::make_shared<CDisplay>(ptrTrans);
            ptrDisplay->StartDrawing(ptrGraphics);
            ptrGraphics->Erase(Color(255, 255, 255));
        }

        ~STestDisplay()
        {
            ptrDisplay->FinishDrawing();
        }

        Color Pixel(int x, int y) const { return ptrGraphics->GetPixel(x, y); }
        bool IsWhite(int x, int y) const { return Pixel(x, y).GetRGB() == Color(255, 255, 255).GetRGB(); }
        // bilinear filtering of the bitmaps may change a color component by 1-2
        bool IsColor(int x, int y, const Color& color) const
        {
            Color c = Pixel(x, y);
            return std::abs((int)c.GetR() - (int)color.GetR()) <= 3 && std::abs((int)c.GetG() - (int)color.GetG()) <= 3 &&
                   std::abs((int)c.GetB() - (int)color.GetB()) <= 3;
        }

        // pixels which are not white in the rect
        int CountPainted(int x0, int y0, int x1, int y1) const
        {
            int n = 0;
            for(int x = x0; x <= x1; ++x)
                for(int y = y0; y <= y1; ++y)
                    if(!IsWhite(x, y))
                        ++n;
            return n;
        }
    };

    struct SDrawCall
    {
        std::vector<GPoint> vecPoints;
        double dAngle;
    };

    // records DrawGeometryEx calls, works as a line or a marker
    template<class I>
    class CRecordingSymbol : public CSymbolBase<I>
    {
    public:
        std::vector<SDrawCall> vecCalls;
        std::vector<std::string>* pLog = nullptr;
        std::string sName;
        int nPrepare = 0;
        Color color = Color(0, 0, 0);

        virtual void DrawGeometryEx(IDisplayPtr, const GPoint* points, const int* polyCounts, int polyCount)
        {
            SDrawCall call;
            int nTotal = 0;
            for(int i = 0; i < polyCount; ++i)
                nTotal += polyCounts[i];
            call.vecPoints.assign(points, points + nTotal);
            call.dAngle = GetAngleValue();
            vecCalls.push_back(call);
            if(pLog)
                pLog->push_back(sName);
        }
        virtual void QueryBoundaryRectEx(IDisplayPtr, const GPoint*, const int*, int, GRect&) const {}
        virtual void Prepare(IDisplayPtr) { ++nPrepare; }
        virtual void DrawDirectly(IDisplayPtr d, const GPoint* p, const int* c, int n) { DrawGeometryEx(d, p, c, n); }
        virtual double GetAngleValue() const { return 0.; }
    };

    class CRecordingLine : public CRecordingSymbol<ILineSymbol>
    {
    public:
        virtual Color  GetColor() const { return color; }
        virtual void   SetColor(const Color &c) { color = c; }
        virtual double GetWidth() const { return 1.; }
        virtual void   SetWidth(double) {}
    };

    class CRecordingMarker : public CRecordingSymbol<IMarkerSymbol>
    {
    public:
        double dAngle = 0.;
        double dSize = 1.;
        virtual double GetAngleValue() const { return dAngle; }
        virtual double GetAngle() const { return dAngle; }
        virtual void   SetAngle(double a) { dAngle = a; }
        virtual Color  GetColor() const { return color; }
        virtual void   SetColor(const Color &c) { color = c; }
        virtual double GetSize() const { return dSize; }
        virtual void   SetSize(double s) { dSize = s; }
        virtual double GetXOffset() const { return 0.; }
        virtual void   SetXOffset(double) {}
        virtual double GetYOffset() const { return 0.; }
        virtual void   SetYOffset(double) {}
        virtual bool   GetIgnoreRotation() const { return false; }
        virtual void   SetIgnoreRotation(bool) {}
    };

    CommonLib::ISerializeObjPtr CreateSerializeRoot()
    {
        CommonLib::xml::IXMLNodePtr ptrNode = std::make_shared<CommonLib::xml::CXMLNode>(CommonLib::xml::IXMLNodePtr(), "root");
        return std::make_shared<CommonLib::CSerializeObjXML>(ptrNode);
    }

    template<class T>
    std::shared_ptr<T> SaveLoad(ISymbolPtr ptrSymbol)
    {
        CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
        ptrSymbol->Save(ptrRoot);
        ISymbolPtr ptrLoaded = CSymbolsLoader::LoadSymbol(ptrRoot);
        REQUIRE(ptrLoaded != nullptr);
        REQUIRE(ptrLoaded->GetSymbolID() == ptrSymbol->GetSymbolID());
        std::shared_ptr<T> ptrResult = std::dynamic_pointer_cast<T>(ptrLoaded);
        REQUIRE(ptrResult != nullptr);
        return ptrResult;
    }

    // square polygon 50..150 on the device
    const GPoint TestSquare[5] = {GPoint(50, 50), GPoint(150, 50), GPoint(150, 150), GPoint(50, 150), GPoint(50, 50)};
    const int TestSquareCount = 5;
}

// ---------------- line template

TEST_CASE("Line template: a mark every interval at its middle", "[symbols][template]")
{
    CLineTemplate lineTemplate(10.);
    lineTemplate.Prepare(IDisplayTransformationPtr(), false);   // no transformation - interval is in device units
    REQUIRE(lineTemplate.IsValid());

    GPoint line[2] = {GPoint(0, 0), GPoint(100, 0)};
    int nCount = 2;
    std::vector<GPoint> vecMarks;
    lineTemplate.ForEachMark(line, &nCount, 1, 0, [&](const GPoint& pt, double dAngle)
    {
        vecMarks.push_back(pt);
        REQUIRE_THAT(dAngle, WithinAbs(0., 1e-9));
    });

    REQUIRE(vecMarks.size() == 10);
    REQUIRE_THAT(vecMarks.front().x, WithinAbs(5., 1e-9));
    REQUIRE_THAT(vecMarks.back().x, WithinAbs(95., 1e-9));
}

TEST_CASE("Line template: pattern, angle, offset and parts", "[symbols][template]")
{
    CLineTemplate lineTemplate(10.);
    lineTemplate.AddPatternElement(1., 1.);   // mark 10, gap 10
    lineTemplate.Prepare(IDisplayTransformationPtr(), false);

    // vertical line down the screen: angle 90, the left side of the direction is +x
    GPoint lines[4] = {GPoint(0, 0), GPoint(0, 100), GPoint(50, 0), GPoint(50, 30)};
    int counts[2] = {2, 2};
    std::vector<GPoint> vecMarks;
    std::vector<double> vecAngles;
    lineTemplate.ForEachMark(lines, counts, 2, 3, [&](const GPoint& pt, double dAngle)
    {
        vecMarks.push_back(pt);
        vecAngles.push_back(dAngle);
    });

    // part 1: 5, 25, 45, 65, 85; part 2 (the pattern restarts): 5, 25
    REQUIRE(vecMarks.size() == 7);
    REQUIRE_THAT(vecMarks[1].y, WithinAbs(25., 1e-9));
    REQUIRE_THAT(vecMarks[0].x, WithinAbs(3., 1e-9));
    REQUIRE_THAT(vecAngles[0], WithinAbs(90., 1e-9));
    REQUIRE_THAT(vecMarks[5].x, WithinAbs(53., 1e-9));
    REQUIRE_THAT(vecMarks[5].y, WithinAbs(5., 1e-9));
}

TEST_CASE("Line template: polyline corners and invalid templates", "[symbols][template]")
{
    CLineTemplate lineTemplate(10.);
    lineTemplate.Prepare(IDisplayTransformationPtr(), false);

    GPoint line[3] = {GPoint(0, 0), GPoint(10, 0), GPoint(10, 10)};
    int nCount = 3;
    std::vector<double> vecAngles;
    lineTemplate.ForEachMark(line, &nCount, 1, 0, [&](const GPoint&, double dAngle) { vecAngles.push_back(dAngle); });
    REQUIRE(vecAngles.size() == 2);
    REQUIRE_THAT(vecAngles[0], WithinAbs(0., 1e-9));
    REQUIRE_THAT(vecAngles[1], WithinAbs(90., 1e-9));

    CLineTemplate zero(0.);
    zero.Prepare(IDisplayTransformationPtr(), false);
    REQUIRE_FALSE(zero.IsValid());

    CLineTemplate empty(10.);
    empty.AddPatternElement(0., 0.);
    empty.Prepare(IDisplayTransformationPtr(), false);
    REQUIRE_FALSE(empty.IsValid());
}

// ---------------- hash / marker line

TEST_CASE("Hash line draws perpendicular hashes along the line", "[symbols][hashline]")
{
    STestDisplay display;
    std::shared_ptr<CRecordingLine> ptrHash = std::make_shared<CRecordingLine>();
    CHashLineSymbol symbol(ptrHash, Mm(10.), Mm(20.));   // hash 10 px, every 20 px

    symbol.Prepare(display.ptrDisplay);
    REQUIRE(ptrHash->nPrepare == 1);

    GPoint line[2] = {GPoint(0, 100), GPoint(100, 100)};
    int nCount = 2;
    symbol.DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);

    REQUIRE(ptrHash->vecCalls.size() == 5);
    const SDrawCall& first = ptrHash->vecCalls[0];
    REQUIRE(first.vecPoints.size() == 2);
    REQUIRE_THAT(first.vecPoints[0].x, WithinAbs(10., 0.01));
    REQUIRE_THAT(first.vecPoints[1].x, WithinAbs(10., 0.01));
    REQUIRE_THAT(std::fabs(first.vecPoints[1].y - first.vecPoints[0].y), WithinAbs(10., 0.01));
}

TEST_CASE("Hash line draws pixels and saves / loads", "[symbols][hashline][serialize]")
{
    STestDisplay display;
    std::shared_ptr<CHashLineSymbol> ptrSymbol = std::make_shared<CHashLineSymbol>(
        std::make_shared<CSimpleLineSymbol>(Color(255, 0, 0), 2., SimpleLineStyleSolid), Mm(10.), Mm(20.));
    ptrSymbol->SetAngle(60.);
    ptrSymbol->SetOffset(Mm(2.));
    ptrSymbol->GetTemplate()->AddPatternElement(1., 2.);

    ptrSymbol->Prepare(display.ptrDisplay);
    GPoint line[2] = {GPoint(0, 100), GPoint(200, 100)};
    int nCount = 2;
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);
    REQUIRE(display.CountPainted(0, 90, 199, 110) > 0);

    std::shared_ptr<CHashLineSymbol> ptrLoaded = SaveLoad<CHashLineSymbol>(ptrSymbol);
    REQUIRE_THAT(ptrLoaded->GetAngle(), WithinAbs(60., 1e-9));
    REQUIRE_THAT(ptrLoaded->GetWidth(), WithinAbs(Mm(10.), 1e-9));
    REQUIRE_THAT(ptrLoaded->GetOffset(), WithinAbs(Mm(2.), 1e-9));
    REQUIRE(ptrLoaded->GetHashSymbol() != nullptr);
    REQUIRE(ptrLoaded->GetColor() == Color(255, 0, 0));
    REQUIRE(ptrLoaded->GetTemplate()->GetPatternElementCount() == 1);
    REQUIRE_THAT(ptrLoaded->GetTemplate()->GetInterval(), WithinAbs(Mm(20.), 1e-9));
}

TEST_CASE("Marker line rotates the markers by the line direction", "[symbols][markerline]")
{
    STestDisplay display;
    std::shared_ptr<CRecordingMarker> ptrMarker = std::make_shared<CRecordingMarker>();
    ptrMarker->SetAngle(10.);
    CMarkerLineSymbol symbol(ptrMarker, Mm(25.));

    symbol.Prepare(display.ptrDisplay);
    GPoint line[2] = {GPoint(100, 0), GPoint(100, 100)};   // down the screen - 90 degrees
    int nCount = 2;
    symbol.DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);

    REQUIRE(ptrMarker->vecCalls.size() == 4);
    REQUIRE(ptrMarker->vecCalls[0].vecPoints.size() == 1);
    REQUIRE_THAT(ptrMarker->vecCalls[0].vecPoints[0].y, WithinAbs(12.5, 0.01));
    REQUIRE_THAT(ptrMarker->vecCalls[0].dAngle, WithinAbs(100., 1e-9));
    REQUIRE_THAT(ptrMarker->GetAngle(), WithinAbs(10., 1e-9));   // restored
}

TEST_CASE("Marker line draws pixels and saves / loads", "[symbols][markerline][serialize]")
{
    STestDisplay display;
    std::shared_ptr<CSimpleMarketSymbol> ptrMarker = std::make_shared<CSimpleMarketSymbol>();
    ptrMarker->SetStyle(SimpleMarkerStyleSquare);
    ptrMarker->SetColor(Color(0, 0, 255));
    ptrMarker->SetSize(Mm(6.));
    ptrMarker->SetOutline(false);

    std::shared_ptr<CMarkerLineSymbol> ptrSymbol = std::make_shared<CMarkerLineSymbol>(ptrMarker, Mm(20.));
    ptrSymbol->Prepare(display.ptrDisplay);
    GPoint line[2] = {GPoint(0, 100), GPoint(200, 100)};
    int nCount = 2;
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);

    REQUIRE_FALSE(display.IsWhite(10, 100));   // marker at 10
    REQUIRE(display.IsWhite(20, 100));         // between the markers

    std::shared_ptr<CMarkerLineSymbol> ptrLoaded = SaveLoad<CMarkerLineSymbol>(ptrSymbol);
    REQUIRE(std::dynamic_pointer_cast<CSimpleMarketSymbol>(ptrLoaded->GetMarkerSymbol()) != nullptr);
    REQUIRE_THAT(ptrLoaded->GetWidth(), WithinAbs(Mm(6.), 1e-9));
}

// ---------------- fills

TEST_CASE("Line fill draws lines only inside the polygon", "[symbols][linefill]")
{
    STestDisplay display;
    std::shared_ptr<CLineFillSymbol> ptrSymbol = std::make_shared<CLineFillSymbol>(
        std::make_shared<CSimpleLineSymbol>(Color(0, 0, 0), 1., SimpleLineStyleSolid), 0., Mm(10.));

    ptrSymbol->Prepare(display.ptrDisplay);
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, TestSquare, &TestSquareCount, 1);

    // horizontal lines every 10 px: a vertical column inside crosses about 10 of them
    int nPainted = display.CountPainted(100, 52, 100, 148);
    REQUIRE(nPainted >= 8);
    REQUIRE(nPainted < 60);

    // nothing outside the polygon
    REQUIRE(display.CountPainted(0, 0, 45, 199) == 0);
    REQUIRE(display.CountPainted(155, 0, 199, 199) == 0);
    REQUIRE(display.CountPainted(0, 0, 199, 45) == 0);

    std::shared_ptr<CLineFillSymbol> ptrLoaded = SaveLoad<CLineFillSymbol>(ptrSymbol);
    REQUIRE_THAT(ptrLoaded->GetSeparation(), WithinAbs(Mm(10.), 1e-9));
    REQUIRE_THAT(ptrLoaded->GetAngle(), WithinAbs(0., 1e-9));
    REQUIRE(ptrLoaded->GetLineSymbol() != nullptr);
}

TEST_CASE("Line fill lines are aligned to the map, not to the polygon", "[symbols][linefill]")
{
    STestDisplay display;
    std::shared_ptr<CRecordingLine> ptrLine = std::make_shared<CRecordingLine>();
    CLineFillSymbol symbol(ptrLine, 0., Mm(10.));
    symbol.Prepare(display.ptrDisplay);

    symbol.DrawGeometryEx(display.ptrDisplay, TestSquare, &TestSquareCount, 1);
    std::vector<double> vecFirst;
    for(size_t i = 0; i < ptrLine->vecCalls.size(); ++i)
        vecFirst.push_back(ptrLine->vecCalls[i].vecPoints[0].y);

    // the same polygon moved by 3 pixels gets the lines at the same device positions
    GPoint moved[5];
    for(int i = 0; i < 5; ++i)
        moved[i] = GPoint(TestSquare[i].x, TestSquare[i].y + 3);
    ptrLine->vecCalls.clear();
    symbol.DrawGeometryEx(display.ptrDisplay, moved, &TestSquareCount, 1);

    REQUIRE(vecFirst.size() > 2);
    double dSeparation = vecFirst[1] - vecFirst[0];   // ~10 px
    REQUIRE_THAT(dSeparation, WithinAbs(10., 0.01));
    REQUIRE_FALSE(ptrLine->vecCalls.empty());
    for(size_t i = 0; i < ptrLine->vecCalls.size(); ++i)
    {
        double dLines = (ptrLine->vecCalls[i].vecPoints[0].y - vecFirst[0]) / dSeparation;
        REQUIRE_THAT(dLines, WithinAbs(std::round(dLines), 1e-6));   // on the same grid
    }
}

TEST_CASE("Marker fill draws markers only inside the polygon", "[symbols][markerfill]")
{
    STestDisplay display;
    std::shared_ptr<CSimpleMarketSymbol> ptrMarker = std::make_shared<CSimpleMarketSymbol>();
    ptrMarker->SetStyle(SimpleMarkerStyleSquare);
    ptrMarker->SetColor(Color(0, 128, 0));
    ptrMarker->SetSize(Mm(4.));
    ptrMarker->SetOutline(false);

    std::shared_ptr<CMarkerFillSymbol> ptrSymbol = std::make_shared<CMarkerFillSymbol>(ptrMarker, Mm(10.), Mm(10.));
    ptrSymbol->Prepare(display.ptrDisplay);
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, TestSquare, &TestSquareCount, 1);

    REQUIRE(display.CountPainted(50, 50, 150, 150) > 50);
    REQUIRE(display.CountPainted(0, 0, 45, 199) == 0);
    REQUIRE(display.CountPainted(155, 0, 199, 199) == 0);

    ptrSymbol->SetStyle(MarkerFillStyleRandom);
    std::shared_ptr<CMarkerFillSymbol> ptrLoaded = SaveLoad<CMarkerFillSymbol>(ptrSymbol);
    REQUIRE(ptrLoaded->GetStyle() == MarkerFillStyleRandom);
    REQUIRE_THAT(ptrLoaded->GetXSeparation(), WithinAbs(Mm(10.), 1e-9));
    REQUIRE(ptrLoaded->GetMarkerSymbol() != nullptr);
}

TEST_CASE("Marker fill random style is stable", "[symbols][markerfill]")
{
    STestDisplay display;
    std::shared_ptr<CRecordingMarker> ptrMarker = std::make_shared<CRecordingMarker>();
    CMarkerFillSymbol symbol(ptrMarker, Mm(10.), Mm(10.));
    symbol.SetStyle(MarkerFillStyleRandom);
    symbol.Prepare(display.ptrDisplay);

    symbol.DrawGeometryEx(display.ptrDisplay, TestSquare, &TestSquareCount, 1);
    std::vector<SDrawCall> vecFirst = ptrMarker->vecCalls;
    ptrMarker->vecCalls.clear();
    symbol.DrawGeometryEx(display.ptrDisplay, TestSquare, &TestSquareCount, 1);

    REQUIRE(vecFirst.size() > 50);
    REQUIRE(vecFirst.size() == ptrMarker->vecCalls.size());
    bool bShifted = false;
    for(size_t i = 0; i < vecFirst.size(); ++i)
    {
        REQUIRE(vecFirst[i].vecPoints[0] == ptrMarker->vecCalls[i].vecPoints[0]);
        if(std::fabs(std::fmod(vecFirst[i].vecPoints[0].x - 100., 10.)) > 1e-6)
            bShifted = true;
    }
    REQUIRE(bShifted);
}

TEST_CASE("Picture fill tiles the bitmap inside the polygon", "[symbols][picturefill]")
{
    STestDisplay display;
    BitmapPtr ptrBitmap = CSymbolBitmapUtils::CreateSolid(4, 4, Color(255, 0, 0));
    std::shared_ptr<CPictureFillSymbol> ptrSymbol = std::make_shared<CPictureFillSymbol>(ptrBitmap);

    ptrSymbol->Prepare(display.ptrDisplay);
    REQUIRE(ptrSymbol->GetDrawBitmap() != nullptr);
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, TestSquare, &TestSquareCount, 1);

    REQUIRE(display.IsColor(100, 100, Color(255, 0, 0)));
    REQUIRE(display.IsColor(60, 140, Color(255, 0, 0)));
    REQUIRE(display.IsWhite(20, 20));
    REQUIRE(display.IsWhite(180, 100));

    ptrSymbol->SetBackgroundColor(Color(0, 255, 0));
    std::shared_ptr<CPictureFillSymbol> ptrLoaded = SaveLoad<CPictureFillSymbol>(ptrSymbol);
    REQUIRE(ptrLoaded->GetBitmap() != nullptr);
    REQUIRE(ptrLoaded->GetBitmap()->Width() == 4);
    REQUIRE(ptrLoaded->GetBackgroundColor() == Color(0, 255, 0));
}

TEST_CASE("Symbol bitmaps: mono colors and transparency color", "[symbols][bitmap]")
{
    // 1 bpp 8x1: bits 10100000
    BitmapPtr ptrMono = std::make_shared<CBitmap>(8, 1, BitmapFormatType1bpp);
    ptrMono->Bits()[0] = 0xA0;
    BitmapPtr ptrArgb = CSymbolBitmapUtils::ToARGB(ptrMono, Color(255, 0, 0), Color(0, 0, 255), Color(Color::Black, Color::Transparent));
    REQUIRE(ptrArgb->Type() == BitmapFormatType32bppARGB);
    REQUIRE(ptrArgb->Pixel(0, 0) == Color(0, 0, 255));   // set bit - background
    REQUIRE(ptrArgb->Pixel(0, 1) == Color(255, 0, 0));   // clear bit - foreground
    REQUIRE(ptrArgb->Pixel(0, 2) == Color(0, 0, 255));

    // white is the transparency color, background is transparent
    BitmapPtr ptrSolid = CSymbolBitmapUtils::CreateSolid(2, 2, Color(255, 255, 255));
    BitmapPtr ptrTransparent = CSymbolBitmapUtils::ToARGB(ptrSolid, Color(0, 0, 0), Color(Color::White, Color::Transparent), Color(255, 255, 255));
    REQUIRE(ptrTransparent->Pixel(1, 1).GetA() == Color::Transparent);

    // ... and with a background color it is replaced by it
    BitmapPtr ptrReplaced = CSymbolBitmapUtils::ToARGB(ptrSolid, Color(0, 0, 0), Color(0, 255, 0), Color(255, 255, 255));
    REQUIRE(ptrReplaced->Pixel(0, 0) == Color(0, 255, 0));
}

// ---------------- markers

TEST_CASE("Picture marker draws the bitmap scaled to the marker size", "[symbols][picturemarker]")
{
    STestDisplay display;
    BitmapPtr ptrBitmap = CSymbolBitmapUtils::CreateSolid(10, 10, Color(0, 0, 255));
    std::shared_ptr<CPictureMarkerSymbol> ptrSymbol = std::make_shared<CPictureMarkerSymbol>(ptrBitmap, Mm(20.));

    ptrSymbol->Prepare(display.ptrDisplay);
    REQUIRE_THAT(ptrSymbol->GetDrawScale(), WithinAbs(2., 0.01));

    GPoint pt(100, 100);
    int nCount = 1;
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, &pt, &nCount, 1);
    REQUIRE(display.IsColor(100, 100, Color(0, 0, 255)));
    REQUIRE(display.IsColor(93, 107, Color(0, 0, 255)));   // inside 20x20
    REQUIRE(display.IsWhite(80, 100));                                        // outside

    GRect rect;
    ptrSymbol->QueryBoundaryRectEx(display.ptrDisplay, &pt, &nCount, 1, rect);
    REQUIRE(rect.xMin < 91);
    REQUIRE(rect.xMax > 109);

    ptrSymbol->SetDrawExact(true);
    ptrSymbol->SetAngle(30.);
    std::shared_ptr<CPictureMarkerSymbol> ptrLoaded = SaveLoad<CPictureMarkerSymbol>(ptrSymbol);
    REQUIRE(ptrLoaded->GetDrawExact());
    REQUIRE_THAT(ptrLoaded->GetAngle(), WithinAbs(30., 1e-9));
    REQUIRE(ptrLoaded->GetBitmap()->Width() == 10);
}

TEST_CASE("Character marker properties and save / load", "[symbols][charactermarker][serialize]")
{
    std::shared_ptr<CCharacterMarkerSymbol> ptrSymbol = std::make_shared<CCharacterMarkerSymbol>("Wingdings", 0x6C, 5., Color(255, 0, 0));
    ptrSymbol->SetAngle(45.);

    std::shared_ptr<CCharacterMarkerSymbol> ptrLoaded = SaveLoad<CCharacterMarkerSymbol>(ptrSymbol);
    REQUIRE(ptrLoaded->GetCharacterIndex() == 0x6C);
    REQUIRE(ptrLoaded->GetFont()->GetFace() == "Wingdings");
    REQUIRE_THAT(ptrLoaded->GetSize(), WithinAbs(5., 1e-9));
    REQUIRE_THAT(ptrLoaded->GetAngle(), WithinAbs(45., 1e-9));
    REQUIRE(ptrLoaded->GetColor() == Color(255, 0, 0));

    ptrLoaded->SetCharacterIndex(0);
    REQUIRE_FALSE(ptrLoaded->CanDraw(CreatePoint(1., 1.)));
}

TEST_CASE("Arrow marker points along the angle", "[symbols][arrowmarker]")
{
    STestDisplay display;
    std::shared_ptr<CArrowMarkerSymbol> ptrSymbol = std::make_shared<CArrowMarkerSymbol>(Mm(40.), Mm(20.), Color(255, 0, 0));
    REQUIRE_THAT(ptrSymbol->GetSize(), WithinAbs(Mm(40.), 1e-9));

    ptrSymbol->Prepare(display.ptrDisplay);
    GPoint triangle[3];
    ptrSymbol->GetArrowPoints(GPoint(100, 100), triangle);
    REQUIRE_THAT(triangle[0].x, WithinAbs(120., 0.01));   // tip to the right
    REQUIRE_THAT(triangle[0].y, WithinAbs(100., 0.01));
    REQUIRE_THAT(triangle[1].x, WithinAbs(80., 0.01));
    REQUIRE_THAT(std::fabs(triangle[2].y - triangle[1].y), WithinAbs(20., 0.01));

    GPoint pt(100, 100);
    int nCount = 1;
    ptrSymbol->DrawGeometryEx(display.ptrDisplay, &pt, &nCount, 1);
    REQUIRE(display.IsColor(115, 100, Color(255, 0, 0)));   // near the tip
    REQUIRE(display.IsColor(85, 105, Color(255, 0, 0)));    // near the base
    REQUIRE(display.IsWhite(115, 92));                       // beside the tip - narrow there
    REQUIRE(display.IsWhite(125, 100));                      // beyond the tip

    // 90 degrees: the same rotation as the other markers (RotateCoords), the tip goes down the screen
    ptrSymbol->SetAngle(90.);
    ptrSymbol->Prepare(display.ptrDisplay);
    ptrSymbol->GetArrowPoints(GPoint(100, 100), triangle);
    REQUIRE_THAT(triangle[0].x, WithinAbs(100., 0.01));
    REQUIRE_THAT(triangle[0].y, WithinAbs(120., 0.01));

    GRect rect;
    ptrSymbol->QueryBoundaryRectEx(display.ptrDisplay, &pt, &nCount, 1, rect);
    REQUIRE_THAT(rect.yMax, WithinAbs(120., 0.01));
    REQUIRE_THAT(rect.xMax - rect.xMin, WithinAbs(20., 0.01));

    ptrSymbol->SetSize(Mm(20.));   // keeps the proportions
    REQUIRE_THAT(ptrSymbol->GetLength(), WithinAbs(Mm(20.), 1e-9));
    REQUIRE_THAT(ptrSymbol->GetWidth(), WithinAbs(Mm(10.), 1e-9));

    ptrSymbol->SetStyle(ArrowMarkerStylePosition);
    std::shared_ptr<CArrowMarkerSymbol> ptrLoaded = SaveLoad<CArrowMarkerSymbol>(ptrSymbol);
    REQUIRE(ptrLoaded->GetStyle() == ArrowMarkerStylePosition);
    REQUIRE_THAT(ptrLoaded->GetLength(), WithinAbs(Mm(20.), 1e-9));
    REQUIRE_THAT(ptrLoaded->GetWidth(), WithinAbs(Mm(10.), 1e-9));
    REQUIRE_THAT(ptrLoaded->GetAngle(), WithinAbs(90., 1e-9));
    REQUIRE(ptrLoaded->GetColor() == Color(255, 0, 0));
}

TEST_CASE("Marker line with arrows points them along the line", "[symbols][arrowmarker][markerline]")
{
    STestDisplay display;
    std::shared_ptr<CArrowMarkerSymbol> ptrArrow = std::make_shared<CArrowMarkerSymbol>(Mm(10.), Mm(6.), Color(0, 0, 0));
    CMarkerLineSymbol symbol(ptrArrow, Mm(40.));
    symbol.Prepare(display.ptrDisplay);

    GPoint line[2] = {GPoint(100, 0), GPoint(100, 200)};   // down the screen
    int nCount = 2;
    symbol.DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);

    // arrow at y = 20 pointing down: tip at y = 25, base at y = 15
    REQUIRE_FALSE(display.IsWhite(100, 23));
    REQUIRE_FALSE(display.IsWhite(98, 16));
    REQUIRE(display.IsWhite(100, 28));
    REQUIRE_THAT(ptrArrow->GetAngle(), WithinAbs(0., 1e-9));   // restored
}

// ---------------- multi layer

TEST_CASE("Multi layer symbol draws all layers", "[symbols][multilayer]")
{
    STestDisplay display;
    std::vector<std::string> vecLog;
    std::shared_ptr<CRecordingMarker> ptrBottom = std::make_shared<CRecordingMarker>();
    std::shared_ptr<CRecordingMarker> ptrTop = std::make_shared<CRecordingMarker>();
    ptrBottom->pLog = &vecLog; ptrBottom->sName = "bottom"; ptrBottom->SetSize(4.);
    ptrTop->pLog = &vecLog; ptrTop->sName = "top"; ptrTop->SetSize(2.);

    CMultiLayerMarkerSymbol symbol;
    REQUIRE(symbol.AddLayer(ptrBottom) == 0);
    REQUIRE(symbol.AddLayer(ptrTop) == 1);
    REQUIRE_THROWS(symbol.AddLayer(std::make_shared<CSimpleLineSymbol>()));   // not a marker

    symbol.SetSize(8.);   // keeps the proportions
    REQUIRE_THAT(ptrBottom->GetSize(), WithinAbs(8., 1e-9));
    REQUIRE_THAT(ptrTop->GetSize(), WithinAbs(4., 1e-9));

    symbol.Prepare(display.ptrDisplay);
    REQUIRE(ptrBottom->nPrepare == 1);

    GPoint pt(10, 10);
    int nCount = 1;
    symbol.DrawGeometryEx(display.ptrDisplay, &pt, &nCount, 1);
    symbol.DrawGeometryEx(display.ptrDisplay, &pt, &nCount, 1);
    REQUIRE(vecLog == std::vector<std::string>{"bottom", "top", "bottom", "top"});
}

TEST_CASE("Multi layer symbol with cache draws layer by layer on flush", "[symbols][multilayer]")
{
    STestDisplay display;
    std::vector<std::string> vecLog;
    std::shared_ptr<CRecordingLine> ptrCasing = std::make_shared<CRecordingLine>();
    std::shared_ptr<CRecordingLine> ptrRoad = std::make_shared<CRecordingLine>();
    ptrCasing->pLog = &vecLog; ptrCasing->sName = "casing";
    ptrRoad->pLog = &vecLog; ptrRoad->sName = "road";

    CMultiLayerLineSymbol symbol;
    symbol.AddLayer(ptrCasing);
    symbol.AddLayer(ptrRoad);
    symbol.SetUseCache(true);

    GPoint line[2] = {GPoint(0, 0), GPoint(10, 10)};
    int nCount = 2;
    symbol.DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);
    line[1] = GPoint(20, 20);
    symbol.DrawGeometryEx(display.ptrDisplay, line, &nCount, 1);
    REQUIRE(vecLog.empty());
    REQUIRE(symbol.GetCachedCount() == 2);

    symbol.FlushBuffers(display.ptrDisplay, ITrackCancelPtr());
    REQUIRE(vecLog == std::vector<std::string>{"casing", "casing", "road", "road"});
    REQUIRE(ptrRoad->vecCalls[1].vecPoints[1] == GPoint(20, 20));   // the geometry was copied
    REQUIRE(symbol.GetCachedCount() == 0);
}

TEST_CASE("Multi layer symbols save / load", "[symbols][multilayer][serialize]")
{
    std::shared_ptr<CMultiLayerLineSymbol> ptrLine = std::make_shared<CMultiLayerLineSymbol>();
    ptrLine->AddLayer(std::make_shared<CSimpleLineSymbol>(Color(0, 0, 0), 5., SimpleLineStyleSolid));
    ptrLine->AddLayer(std::make_shared<CSimpleLineSymbol>(Color(255, 255, 0), 3., SimpleLineStyleSolid));
    ptrLine->SetUseCache(true);

    std::shared_ptr<CMultiLayerLineSymbol> ptrLoadedLine = SaveLoad<CMultiLayerLineSymbol>(ptrLine);
    REQUIRE(ptrLoadedLine->GetCount() == 2);
    REQUIRE(ptrLoadedLine->GetUseCache());
    REQUIRE(std::dynamic_pointer_cast<ILineSymbol>(ptrLoadedLine->GetLayer(1))->GetColor() == Color(255, 255, 0));

    ptrLoadedLine->MoveLayer(1, 0);
    REQUIRE(std::dynamic_pointer_cast<ILineSymbol>(ptrLoadedLine->GetLayer(0))->GetColor() == Color(255, 255, 0));
    ptrLoadedLine->DeleteLayer(0);
    REQUIRE(ptrLoadedLine->GetCount() == 1);
    REQUIRE_THROWS(ptrLoadedLine->GetLayer(1));

    std::shared_ptr<CMultiLayerFillSymbol> ptrFill = std::make_shared<CMultiLayerFillSymbol>();
    std::shared_ptr<CSimpleFillSymbol> ptrSolid = std::make_shared<CSimpleFillSymbol>();
    ptrSolid->SetColor(Color(200, 200, 200));
    ptrFill->AddLayer(ptrSolid);
    ptrFill->AddLayer(std::make_shared<CLineFillSymbol>());
    ptrFill->SetOutlineSymbol(std::make_shared<CSimpleLineSymbol>(Color(0, 0, 0), 1., SimpleLineStyleSolid));

    std::shared_ptr<CMultiLayerFillSymbol> ptrLoadedFill = SaveLoad<CMultiLayerFillSymbol>(ptrFill);
    REQUIRE(ptrLoadedFill->GetCount() == 2);
    REQUIRE(std::dynamic_pointer_cast<CLineFillSymbol>(ptrLoadedFill->GetLayer(1)) != nullptr);
    REQUIRE(ptrLoadedFill->GetOutlineSymbol() != nullptr);

    std::shared_ptr<CMultiLayerMarkerSymbol> ptrMarker = std::make_shared<CMultiLayerMarkerSymbol>();
    ptrMarker->AddLayer(std::make_shared<CSimpleMarketSymbol>());
    ptrMarker->AddLayer(std::make_shared<CPictureMarkerSymbol>(CSymbolBitmapUtils::CreateSolid(2, 2, Color(1, 2, 3)), 3.));
    std::shared_ptr<CMultiLayerMarkerSymbol> ptrLoadedMarker = SaveLoad<CMultiLayerMarkerSymbol>(ptrMarker);
    REQUIRE(ptrLoadedMarker->GetCount() == 2);
    REQUIRE(std::dynamic_pointer_cast<CPictureMarkerSymbol>(ptrLoadedMarker->GetLayer(1)) != nullptr);
}
