#include "TestCommon.h"
#include "../../drawer/MapDrawer.h"
#include "../../layers/RasterLayer.h"
#include "../../legend/Legend.h"
#include "../../renders/Raster/RasterRGBRenderer.h"
#include "../../renders/Raster/RasterStretchRenderer.h"
#include "../../../GeoDatabase/GeoDatabaseRaster/RasterWorkspace.h"
#include "../../../GeoDatabase/WorkspaceHolder.h"

extern "C" {
#include "tiffio.h"
}

#include <filesystem>
#include <fstream>
#include <chrono>
#include <functional>
#include <thread>

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    std::filesystem::path RasterDir()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineCartographyTests" / "raster";
        std::filesystem::create_directories(dir);
        return dir;
    }

    // width x height raster, map extent (0,0)-(width,height): pixel (x, y) covers map x..x+1, height-y-1..height-y
    std::string WriteTiff(const std::string& name, uint32_t width, uint32_t height, uint16_t samples, uint16_t bits,
                          const std::function<void(uint32_t x, uint32_t y, uint8_t* pPixel)>& pixel)
    {
        std::string path = (RasterDir() / name).string();
        TIFF* tif = TIFFOpen(path.c_str(), "w");
        REQUIRE(tif != nullptr);
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, samples >= 3 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 16);

        std::vector<uint8_t> line((size_t)TIFFScanlineSize(tif), 0);
        const size_t pixelSize = size_t(samples) * bits / 8;
        for(uint32_t y = 0; y < height; ++y)
        {
            for(uint32_t x = 0; x < width; ++x)
                pixel(x, y, line.data() + x * pixelSize);
            REQUIRE(TIFFWriteScanline(tif, line.data(), y, 0) >= 0);
        }
        TIFFClose(tif);

        // world file: pixel size 1, upper-left pixel center (0.5, height - 0.5)
        std::filesystem::path tfw = RasterDir() / name;
        tfw.replace_extension(".tfw");
        std::ofstream file(tfw.string());
        file << "1\n0\n0\n-1\n0.5\n" << (height - 0.5) << "\n";
        return path;
    }

    // 100 x 100: red top-left, green top-right, blue bottom-left, black bottom-right
    std::string QuadrantsTiff()
    {
        static std::string path = WriteTiff("quadrants.tif", 100, 100, 3, 8, [](uint32_t x, uint32_t y, uint8_t* p) {
            p[0] = p[1] = p[2] = 0;
            if(y < 50)
                p[x < 50 ? 0 : 1] = 255;
            else if(x < 50)
                p[2] = 255;
        });
        return path;
    }

    // 200 x 50 16-bit gradient: value = 1000 + x * 10
    std::string GradientTiff()
    {
        static std::string path = WriteTiff("gradient16.tif", 200, 50, 1, 16, [](uint32_t x, uint32_t, uint8_t* p) {
            uint16_t v = uint16_t(1000 + x * 10);
            memcpy(p, &v, 2);
        });
        return path;
    }

    GeoDatabase::IRasterWorkspacePtr OpenRasterWorkspace()
    {
        static GeoDatabase::IRasterWorkspacePtr ptrWks;
        if(!ptrWks.get())
        {
            ptrWks = std::dynamic_pointer_cast<GeoDatabase::IRasterWorkspace>(
                    GeoDatabase::CRasterWorkspace::Open("raster", RasterDir().string().c_str(), CommonLib::CGuid::CreateNew()));
            GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWks);
        }
        return ptrWks;
    }

    // the drawer has no callbacks: poll it until n drawings are finished and nothing is drawn now
    bool WaitDrawn(const CMapDrawer& drawer, uint64_t n)
    {
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while(drawer.GetDrawCounter() < n || drawer.IsDrawing())
        {
            if(std::chrono::steady_clock::now() > end)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    IMapPtr CreateRasterMap(std::shared_ptr<CRasterLayer> ptrLayer)
    {
        IMapPtr ptrMap = std::make_shared<CMap>();
        ptrMap->SetSpatialReference(ptrLayer->GetRasterDataset()->GetSpatialReference());
        ptrMap->GetLayers()->AddLayer(ptrLayer);
        return ptrMap;
    }

    Display::Color PixelAt(CMapDrawer& drawer, double x, double y)
    {
        CommonLib::GisXYPoint mapPt = {x, y};
        Display::GPoint devPt;
        drawer.GetTransformation()->MapToDevice(&mapPt, &devPt, 1);
        return drawer.GetOutGraphics()->GetPixel(devPt.x, devPt.y);
    }

    bool NearRGB(const Display::Color& c, int r, int g, int b, int tolerance = 2)
    {
        return std::abs(c.GetR() - r) <= tolerance && std::abs(c.GetG() - g) <= tolerance && std::abs(c.GetB() - b) <= tolerance;
    }

    void CheckQuadrants(CMapDrawer& drawer)
    {
        REQUIRE(NearRGB(PixelAt(drawer, 25, 75), 255, 0, 0));   // top-left
        REQUIRE(NearRGB(PixelAt(drawer, 75, 75), 0, 255, 0));   // top-right
        REQUIRE(NearRGB(PixelAt(drawer, 25, 25), 0, 0, 255));   // bottom-left
        REQUIRE(NearRGB(PixelAt(drawer, 75, 25), 0, 0, 0));     // bottom-right
    }

    std::shared_ptr<CRasterLayer> CreateQuadrantsLayer()
    {
        std::shared_ptr<CRasterLayer> ptrLayer = std::make_shared<CRasterLayer>(OpenRasterWorkspace()->OpenRasterDataset("quadrants.tif"));
        ptrLayer->SetVisible(true);
        return ptrLayer;
    }
}

TEST_CASE("Raster layer: extent and default renderers", "[cartography][raster]")
{
    QuadrantsTiff();
    GradientTiff();

    std::shared_ptr<CRasterLayer> ptrLayer = CreateQuadrantsLayer();
    REQUIRE(ptrLayer->IsValid());
    REQUIRE(ptrLayer->GetLayerTypeID() == RasterLayerID);
    REQUIRE(ptrLayer->GetName() == "quadrants.tif");
    const CommonLib::bbox& bb = ptrLayer->GetExtent()->GetBoundingBox();
    REQUIRE(bb.xMin == 0.);
    REQUIRE(bb.xMax == 100.);
    REQUIRE(bb.yMax == 100.);
    REQUIRE(ptrLayer->GetRenderer()->GetRasterRendererID() == RasterRGBRendererID);

    CRasterLayer gradient(OpenRasterWorkspace()->OpenRasterDataset("gradient16.tif"));
    REQUIRE(gradient.GetRenderer()->GetRasterRendererID() == RasterStretchRendererID);

    REQUIRE_FALSE(CRasterLayer().IsValid());
}

TEST_CASE("Raster layer: RGB drawing keeps the orientation, rotation and 3D view", "[cartography][raster][drawer]")
{
    QuadrantsTiff();
    IMapPtr ptrMap = CreateRasterMap(CreateQuadrantsLayer());

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(300, 200, true);
    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.GetLastError().empty());
    CheckQuadrants(drawer);

    // outside of the raster - map background
    const CommonLib::bbox& fitted = drawer.GetTransformation()->GetFittedBounds();
    REQUIRE(fitted.xMin < -10.);
    REQUIRE(NearRGB(PixelAt(drawer, -5, 50), 255, 255, 255));

    drawer.SetRotation(30.);
    REQUIRE(WaitDrawn(drawer, 2));
    REQUIRE(drawer.GetLastError().empty());
    CheckQuadrants(drawer);

    drawer.SetRotation(0.);
    REQUIRE(WaitDrawn(drawer, 3));
    drawer.SetTilt(45.);
    drawer.Set3DMode(true);
    REQUIRE(WaitDrawn(drawer, 4));
    REQUIRE(drawer.Is3DMode());
    REQUIRE(drawer.GetLastError().empty());
    CheckQuadrants(drawer);
}

TEST_CASE("Raster layer: zoom in reads the full resolution window", "[cartography][raster][drawer]")
{
    QuadrantsTiff();
    IMapPtr ptrMap = CreateRasterMap(CreateQuadrantsLayer());

    CMapDrawer drawer;
    drawer.SetMap(ptrMap);
    drawer.SetSize(200, 200, true);
    REQUIRE(WaitDrawn(drawer, 1));

    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 45; bb.xMax = 55; bb.yMin = 45; bb.yMax = 55;
    drawer.ZoomIn(bb);
    REQUIRE(WaitDrawn(drawer, 2));
    REQUIRE(drawer.GetLastError().empty());

    // the quadrant border is exactly at 50
    REQUIRE(NearRGB(PixelAt(drawer, 49.5, 50.5), 255, 0, 0));
    REQUIRE(NearRGB(PixelAt(drawer, 50.5, 50.5), 0, 255, 0));
    REQUIRE(NearRGB(PixelAt(drawer, 49.5, 49.5), 0, 0, 255));
    REQUIRE(NearRGB(PixelAt(drawer, 50.5, 49.5), 0, 0, 0));
}

TEST_CASE("Raster layer: transparency and background values", "[cartography][raster][drawer]")
{
    QuadrantsTiff();
    std::shared_ptr<CRasterLayer> ptrLayer = CreateQuadrantsLayer();
    CRasterRGBRendererPtr ptrRenderer = std::dynamic_pointer_cast<CRasterRGBRenderer>(ptrLayer->GetRenderer());
    REQUIRE(ptrRenderer != nullptr);
    ptrRenderer->SetBackgroundValues({0., 0., 0.});   // black -> transparent
    ptrRenderer->SetDisplayBackground(false);
    ptrRenderer->SetTransparency(50);

    CMapDrawer drawer;
    drawer.SetMap(CreateRasterMap(ptrLayer));
    drawer.SetSize(300, 200, true);
    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.GetLastError().empty());

    REQUIRE(NearRGB(PixelAt(drawer, 25, 75), 255, 127, 127, 3));  // red over white, 50%
    REQUIRE(NearRGB(PixelAt(drawer, 75, 25), 255, 255, 255));     // background value - map background
}

TEST_CASE("Raster layer: stretch renderer with a color ramp", "[cartography][raster][drawer]")
{
    GradientTiff();
    std::shared_ptr<CRasterLayer> ptrLayer = std::make_shared<CRasterLayer>(OpenRasterWorkspace()->OpenRasterDataset("gradient16.tif"));
    ptrLayer->SetVisible(true);
    CRasterStretchRendererPtr ptrRenderer = std::dynamic_pointer_cast<CRasterStretchRenderer>(ptrLayer->GetRenderer());
    REQUIRE(ptrRenderer != nullptr);
    ptrRenderer->SetStretchType(RasterStretchTypeMinMax);
    ptrRenderer->SetColorRamp(Display::Color(0, 0, 255), Display::Color(255, 0, 0));

    CMapDrawer drawer;
    drawer.SetMap(CreateRasterMap(ptrLayer));
    drawer.SetSize(400, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.GetLastError().empty());

    CRasterStatisticsPtr ptrStats = ptrRenderer->GetStatistics();
    REQUIRE(ptrStats != nullptr);
    REQUIRE(ptrStats->IsCalculated());
    REQUIRE(ptrStats->GetBandStats(0).dMin == 1000.);
    REQUIRE(ptrStats->GetBandStats(0).dMax == 2990.);

    Display::Color left = PixelAt(drawer, 0.5, 25);
    Display::Color middle = PixelAt(drawer, 100.5, 25);
    Display::Color right = PixelAt(drawer, 199.5, 25);
    REQUIRE(NearRGB(left, 0, 0, 255));
    REQUIRE(NearRGB(right, 255, 0, 0));
    REQUIRE(std::abs(middle.GetR() - 128) <= 3);
    REQUIRE(std::abs(middle.GetB() - 127) <= 3);
}

TEST_CASE("Raster layer: save / load", "[cartography][raster][serialize]")
{
    QuadrantsTiff();
    std::shared_ptr<CRasterLayer> ptrLayer = CreateQuadrantsLayer();
    ptrLayer->SetName("ortho");
    CRasterRGBRendererPtr ptrRenderer = std::dynamic_pointer_cast<CRasterRGBRenderer>(ptrLayer->GetRenderer());
    ptrRenderer->SetBandIndices(2, 1, 0);
    ptrRenderer->SetTransparency(30);
    ptrRenderer->SetStretchType(RasterStretchTypeStandardDeviation);
    ptrRenderer->SetBackgroundValues({1., 2., 3.});
    ptrRenderer->SetContrast(20);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrLayer->Save(ptrRoot);

    ILayerPtr ptrLoaded = CLayersLoader::LoadLayer(ptrRoot);
    std::shared_ptr<CRasterLayer> ptrRaster = std::dynamic_pointer_cast<CRasterLayer>(ptrLoaded);
    REQUIRE(ptrRaster != nullptr);
    REQUIRE(ptrRaster->GetName() == "ortho");
    REQUIRE(ptrRaster->GetLayerId() == ptrLayer->GetLayerId());
    REQUIRE(ptrRaster->GetRasterDataset() != nullptr);
    REQUIRE(ptrRaster->GetRasterDataset()->GetDatasetName() == "quadrants.tif");

    CRasterRGBRendererPtr ptrLoadedRenderer = std::dynamic_pointer_cast<CRasterRGBRenderer>(ptrRaster->GetRenderer());
    REQUIRE(ptrLoadedRenderer != nullptr);
    REQUIRE(ptrLoadedRenderer->GetRedBandIndex() == 2);
    REQUIRE(ptrLoadedRenderer->GetBlueBandIndex() == 0);
    REQUIRE(ptrLoadedRenderer->GetTransparency() == 30);
    REQUIRE(ptrLoadedRenderer->GetStretchType() == RasterStretchTypeStandardDeviation);
    REQUIRE(ptrLoadedRenderer->GetBackgroundValues() == std::vector<double>{1., 2., 3.});
    REQUIRE(ptrLoadedRenderer->GetContrast() == 20);

    CRasterStretchRenderer stretch;
    stretch.SetBand(0);
    stretch.SetColorRamp(Display::Color(10, 20, 30), Display::Color(200, 100, 50));
    stretch.SetInvert(true);
    CommonLib::ISerializeObjPtr ptrStretchRoot = CreateSerializeRoot();
    stretch.Save(ptrStretchRoot);
    std::shared_ptr<CRasterStretchRenderer> ptrStretch = std::dynamic_pointer_cast<CRasterStretchRenderer>(CLoaderRenderers::LoadRasterRenderer(ptrStretchRoot));
    REQUIRE(ptrStretch != nullptr);
    REQUIRE(ptrStretch->GetInvert());
    REQUIRE(ptrStretch->GetToColor() == Display::Color(200, 100, 50));
    REQUIRE(ptrStretch->GetFromColor() == Display::Color(10, 20, 30));
}

TEST_CASE("Raster layer: percent clip stretch (2 - 98 %) is the default for more than 8 bits", "[cartography][raster][drawer]")
{
    GradientTiff();
    std::shared_ptr<CRasterLayer> ptrLayer = std::make_shared<CRasterLayer>(OpenRasterWorkspace()->OpenRasterDataset("gradient16.tif"));
    ptrLayer->SetVisible(true);
    CRasterStretchRendererPtr ptrRenderer = std::dynamic_pointer_cast<CRasterStretchRenderer>(ptrLayer->GetRenderer());
    REQUIRE(ptrRenderer != nullptr);
    REQUIRE(ptrRenderer->GetStretchType() == RasterStretchTypePercentClip);
    REQUIRE(ptrRenderer->GetClipLowPercent() == 2.);
    REQUIRE(ptrRenderer->GetClipHighPercent() == 98.);

    CMapDrawer drawer;
    drawer.SetMap(CreateRasterMap(ptrLayer));
    drawer.SetSize(400, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));
    REQUIRE(drawer.GetLastError().empty());

    // 200 columns 1000, 1010 ... 2990 of 50 pixels: 2 % - the 4th column, 98 % - the 196th one
    CRasterStatisticsPtr ptrStats = ptrRenderer->GetStatistics();
    REQUIRE(ptrStats->HasHistogram());
    REQUIRE(ptrStats->GetPercentile(0, 0.) == 1000.);
    REQUIRE(ptrStats->GetPercentile(0, 2.) == 1030.);
    REQUIRE(ptrStats->GetPercentile(0, 50.) == 1990.);
    REQUIRE(ptrStats->GetPercentile(0, 98.) == 2950.);
    REQUIRE(ptrStats->GetPercentile(0, 100.) == 2990.);

    // the values below / above are clipped to the ends of the ramp (black - white)
    REQUIRE(NearRGB(PixelAt(drawer, 1.5, 25), 0, 0, 0));
    REQUIRE(NearRGB(PixelAt(drawer, 198.5, 25), 255, 255, 255));
    Display::Color middle = PixelAt(drawer, 100.5, 25);
    REQUIRE(std::abs(middle.GetR() - 128) <= 4);

    // the statistics loaded from a project have no histogram: they are calculated again for the clip
    CRasterStatisticsPtr ptrLoaded = std::make_shared<CRasterStatistics>();
    ptrLoaded->SetBandStats(0, ptrStats->GetBandStats(0));
    CRasterStatistics::SBandStats noHistogram = ptrStats->GetBandStats(0);
    noHistogram.histogram.clear();
    ptrLoaded->SetBandStats(0, noHistogram);
    REQUIRE_FALSE(ptrLoaded->HasHistogram());
    ptrRenderer->SetStatistics(ptrLoaded);
    drawer.SetSize(401, 100, true);
    REQUIRE(WaitDrawn(drawer, 2));
    REQUIRE(ptrRenderer->GetStatistics()->HasHistogram());
}

TEST_CASE("Raster layer: brightness, contrast, gamma, saturation and switching the renderer", "[cartography][raster][drawer]")
{
    QuadrantsTiff();
    std::shared_ptr<CRasterLayer> ptrLayer = CreateQuadrantsLayer();
    CRasterRGBRendererPtr ptrRGB = std::dynamic_pointer_cast<CRasterRGBRenderer>(ptrLayer->GetRenderer());
    REQUIRE(ptrRGB != nullptr);
    REQUIRE(ptrRGB->GetGamma() == 1.);

    CMapDrawer drawer;
    drawer.SetMap(CreateRasterMap(ptrLayer));
    drawer.SetSize(300, 200, true);
    REQUIRE(WaitDrawn(drawer, 1));
    CheckQuadrants(drawer);

    // saturation -100: gray, red -> 0.299 * 255
    ptrRGB->SetSaturation(-100);
    drawer.SetSize(301, 200, true);
    REQUIRE(WaitDrawn(drawer, 2));
    REQUIRE(drawer.GetLastError().empty());
    REQUIRE(NearRGB(PixelAt(drawer, 25, 75), 76, 76, 76));
    REQUIRE(NearRGB(PixelAt(drawer, 75, 75), 150, 150, 150));

    // brightness +50 on black: 127.5; gamma 2 lightens it: 255 * (128 / 255) ^ 0.5 = 181
    ptrRGB->SetSaturation(0);
    ptrRGB->SetBrightness(50);
    drawer.SetSize(302, 200, true);
    REQUIRE(WaitDrawn(drawer, 3));
    REQUIRE(NearRGB(PixelAt(drawer, 75, 25), 128, 128, 128));
    ptrRGB->SetGamma(2.);
    drawer.SetSize(303, 200, true);
    REQUIRE(WaitDrawn(drawer, 4));
    REQUIRE(NearRGB(PixelAt(drawer, 75, 25), 181, 181, 181));
    REQUIRE_THROWS(ptrRGB->SetGamma(0.));
    ptrRGB->SetGamma(100.);
    REQUIRE(ptrRGB->GetGamma() == CRasterRendererBase::kMaxGamma);
    ptrRGB->SetGamma(2.);

    // to the stretch renderer: the adjustments go with it, the band is the first one (red: 255 / 0 / 0 / 0)
    IRasterRendererPtr ptrStretch = CRasterLayer::CreateRenderer(RasterStretchRendererID, ptrLayer->GetRasterDataset(), ptrRGB);
    CRasterStretchRendererPtr pStretch = std::dynamic_pointer_cast<CRasterStretchRenderer>(ptrStretch);
    REQUIRE(pStretch != nullptr);
    REQUIRE(pStretch->GetBrightness() == 50);
    REQUIRE(pStretch->GetGamma() == 2.);
    REQUIRE(pStretch->GetStretchType() == ptrRGB->GetStretchType());
    pStretch->SetBrightness(0);
    pStretch->SetGamma(1.);
    ptrLayer->SetRenderer(ptrStretch);
    drawer.SetSize(304, 200, true);
    REQUIRE(WaitDrawn(drawer, 5));
    REQUIRE(drawer.GetLastError().empty());
    REQUIRE(NearRGB(PixelAt(drawer, 25, 75), 255, 255, 255));   // red 255 -> white of the ramp
    REQUIRE(NearRGB(PixelAt(drawer, 75, 75), 0, 0, 0));         // green quadrant, red 0 -> black

    // and back, the save / load of the adjustments
    IRasterRendererPtr ptrBack = CRasterLayer::CreateRenderer(RasterRGBRendererID, ptrLayer->GetRasterDataset(), ptrStretch);
    REQUIRE(ptrBack->GetRasterRendererID() == RasterRGBRendererID);
    REQUIRE_THROWS(CRasterLayer::CreateRenderer(UndefineRasterRendererID, ptrLayer->GetRasterDataset()));

    CRasterRGBRenderer saved;
    saved.SetBrightness(-20);
    saved.SetContrast(30);
    saved.SetGamma(0.5);
    saved.SetSaturation(40);
    CommonLib::ISerializeObjPtr ptrObj = CreateSerializeRoot();
    saved.Save(ptrObj);
    CRasterRGBRenderer loaded;
    loaded.Load(ptrObj);
    REQUIRE(loaded.GetBrightness() == -20);
    REQUIRE(loaded.GetContrast() == 30);
    REQUIRE(loaded.GetGamma() == 0.5);
    REQUIRE(loaded.GetSaturation() == 40);
}

TEST_CASE("Raster layer: legend of the bands", "[cartography][raster][legend]")
{
    QuadrantsTiff();
    GradientTiff();
    std::shared_ptr<CRasterLayer> ptrRGB = CreateQuadrantsLayer();
    std::vector<ILegendGroupPtr> groups = CLegendUtils::GetLayerLegend(ptrRGB);
    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0]->GetClassCount() == 3);
    REQUIRE(groups[0]->GetClass(0)->GetLabel() == "Red: Band 1");
    REQUIRE(groups[0]->GetClass(1)->GetLabel() == "Green: Band 2");
    REQUIRE(groups[0]->GetClass(2)->GetLabel() == "Blue: Band 3");
    REQUIRE(groups[0]->GetClass(0)->GetSymbol() != nullptr);

    std::dynamic_pointer_cast<CRasterRGBRenderer>(ptrRGB->GetRenderer())->SetBandIndices(2, 2, 2);
    groups = CLegendUtils::GetLayerLegend(ptrRGB);
    REQUIRE(groups[0]->GetClassCount() == 1);
    REQUIRE(groups[0]->GetClass(0)->GetLabel() == "Gray: Band 3");

    // stretch: the band, the values of the ramp ends after the statistics are calculated (the first drawing)
    std::shared_ptr<CRasterLayer> ptrLayer = std::make_shared<CRasterLayer>(OpenRasterWorkspace()->OpenRasterDataset("gradient16.tif"));
    ptrLayer->SetVisible(true);
    groups = CLegendUtils::GetLayerLegend(ptrLayer);
    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0]->GetHeading() == "Band 1");
    REQUIRE(groups[0]->GetClassCount() == 2);
    REQUIRE(groups[0]->GetClass(0)->GetLabel() == "High");
    REQUIRE(groups[0]->GetClass(1)->GetLabel() == "Low");

    CMapDrawer drawer;
    drawer.SetMap(CreateRasterMap(ptrLayer));
    drawer.SetSize(400, 100, true);
    REQUIRE(WaitDrawn(drawer, 1));
    groups = CLegendUtils::GetLayerLegend(ptrLayer);
    REQUIRE(groups[0]->GetClass(0)->GetLabel() == "2950");
    REQUIRE(groups[0]->GetClass(1)->GetLabel() == "1030");
}
