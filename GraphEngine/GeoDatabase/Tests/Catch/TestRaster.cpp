#include "TestCommon.h"
#include <catch2/matchers/catch_matchers_string.hpp>
#include "../../GeoDatabaseRaster/RasterWorkspace.h"
#include "../../GeoDatabaseRaster/RasterDataset.h"
#include "../../GeoDatabaseRaster/RasterCursor.h"
#include "../../GeoDatabaseRaster/RasterBlock.h"
#include "../../GeoDatabaseRaster/RasterSpatialFilter.h"
#include "../../GeoDatabaseRaster/TIFFReader.h"
#include "../../GeoDatabaseRaster/TIFF/TIFF.h"

extern "C" {
#include "tiffio.h"
}

#include <fstream>
#include <functional>

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

namespace
{
    struct STiffParams
    {
        uint32_t width = 300;
        uint32_t height = 200;
        uint16_t bitsPerSample = 8;
        uint16_t samples = 3;
        uint16_t sampleFormat = SAMPLEFORMAT_UINT;
        uint16_t photometric = PHOTOMETRIC_RGB;
        uint16_t planar = PLANARCONFIG_CONTIG;
        uint16_t compression = COMPRESSION_LZW;
        uint32_t tileSize = 0;       // 0 - strips
        uint32_t rowsPerStrip = 16;
        bool geoTiff = false;        // origin (1000, 2000), pixel 10 x 10, EPSG:32633
    };

    // value of sample s at (x, y), truncated to the sample size by the writer
    uint32_t TestValue(uint32_t x, uint32_t y, uint32_t s)
    {
        return x * 7 + y * 13 + s * 101;
    }

    void SetSample(uint8_t* pPixel, const STiffParams& p, uint32_t x, uint32_t y, uint32_t s, uint32_t sampleInChunk)
    {
        uint32_t v = TestValue(x, y, s);
        switch(p.bitsPerSample)
        {
            case 8: pPixel[sampleInChunk] = uint8_t(v); break;
            case 16: reinterpret_cast<uint16_t*>(pPixel)[sampleInChunk] = uint16_t(v); break;
            case 32:
                if(p.sampleFormat == SAMPLEFORMAT_IEEEFP)
                    reinterpret_cast<float*>(pPixel)[sampleInChunk] = float(v) * 0.5f;
                else
                    reinterpret_cast<uint32_t*>(pPixel)[sampleInChunk] = v;
                break;
            default: break;
        }
    }

    std::string WriteTiff(const std::string& sName, const STiffParams& p, const std::function<void(TIFF*)>& extra = nullptr)
    {
        std::filesystem::path dir = TestDir() / "raster";
        std::filesystem::create_directories(dir);
        std::string sPath = (dir / sName).string();

        TIFF* tif = TIFFOpen(sPath.c_str(), "w");
        REQUIRE(tif != nullptr);

        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, p.width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, p.height);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, p.bitsPerSample);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, p.samples);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, p.sampleFormat);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, p.photometric);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, p.planar);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, p.compression);
        if(p.compression == COMPRESSION_JPEG)
        {
            TIFFSetField(tif, TIFFTAG_JPEGQUALITY, 95);
            if(p.photometric == PHOTOMETRIC_YCBCR)
                TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }

        if(p.geoTiff)
        {
            static const TIFFFieldInfo fields[] = {
                {33550, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelPixelScaleTag"},
                {33922, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelTiepointTag"},
                {34735, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SHORT, FIELD_CUSTOM, 1, 1, "GeoKeyDirectoryTag"},
            };
            TIFFMergeFieldInfo(tif, fields, 3);
            double scale[3] = {10., 10., 0.};
            double tie[6] = {0., 0., 0., 1000., 2000., 0.};
            uint16_t keys[] = {1, 1, 0, 3,
                               1024, 0, 1, 1,       // projected
                               1025, 0, 1, 1,       // PixelIsArea
                               3072, 0, 1, 32633};  // WGS 84 / UTM 33N
            TIFFSetField(tif, 33550, 3, scale);
            TIFFSetField(tif, 33922, 6, tie);
            TIFFSetField(tif, 34735, (uint32_t)(sizeof(keys) / sizeof(keys[0])), keys);
        }

        if(extra)
            extra(tif);

        const bool bSeparate = p.planar == PLANARCONFIG_SEPARATE;
        const uint32_t planes = bSeparate ? p.samples : 1;
        const uint32_t samplesInChunk = bSeparate ? 1 : p.samples;
        const uint32_t sampleBytes = p.bitsPerSample / 8;

        if(p.tileSize)
        {
            TIFFSetField(tif, TIFFTAG_TILEWIDTH, p.tileSize);
            TIFFSetField(tif, TIFFTAG_TILELENGTH, p.tileSize);
            std::vector<uint8_t> tile((size_t)TIFFTileSize(tif), 0);
            for(uint32_t plane = 0; plane < planes; ++plane)
                for(uint32_t ty = 0; ty < p.height; ty += p.tileSize)
                    for(uint32_t tx = 0; tx < p.width; tx += p.tileSize)
                    {
                        std::fill(tile.begin(), tile.end(), 0);
                        for(uint32_t y = 0; y < p.tileSize; ++y)
                            for(uint32_t x = 0; x < p.tileSize; ++x)
                                for(uint32_t s = 0; s < samplesInChunk; ++s)
                                {
                                    uint8_t* pPixel = tile.data() + (size_t(y) * p.tileSize + x) * samplesInChunk * sampleBytes;
                                    SetSample(pPixel, p, tx + x, ty + y, bSeparate ? plane : s, s);
                                }
                        REQUIRE(TIFFWriteTile(tif, tile.data(), tx, ty, 0, (uint16_t)plane) >= 0);
                    }
        }
        else
        {
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, p.rowsPerStrip);
            std::vector<uint8_t> line((size_t)TIFFScanlineSize(tif), 0);
            for(uint32_t plane = 0; plane < planes; ++plane)
                for(uint32_t y = 0; y < p.height; ++y)
                {
                    for(uint32_t x = 0; x < p.width; ++x)
                        for(uint32_t s = 0; s < samplesInChunk; ++s)
                            SetSample(line.data() + size_t(x) * samplesInChunk * sampleBytes, p, x, y, bSeparate ? plane : s, s);
                    REQUIRE(TIFFWriteScanline(tif, line.data(), y, (uint16_t)plane) >= 0);
                }
        }

        TIFFClose(tif);
        return sPath;
    }

    // reads the whole dataset through the cursor into one pixel-interleaved buffer
    std::vector<uint8_t> ReadAll(IRasterDatasetPtr ptrDataset, std::shared_ptr<CRasterSpatialFilter> ptrFilter, int& width, int& height, int& pixelSize, int& blocks)
    {
        IRasterCursorPtr ptrCursor = ptrDataset->Search(ptrFilter);
        CRasterCursor* pCursor = dynamic_cast<CRasterCursor*>(ptrCursor.get());
        REQUIRE(pCursor != nullptr);

        const int step = pCursor->GetPixelStep();
        width = (pCursor->GetWindowWidth() + step - 1) / step;
        height = (pCursor->GetWindowHeight() + step - 1) / step;
        pixelSize = CRasterBlock::GetSampleSize(ptrDataset->GetPixelType()) * ptrDataset->GetBandCount();

        std::vector<uint8_t> image(size_t(width) * height * pixelSize, 0);
        std::vector<uint8_t> written(size_t(width) * height, 0);

        CRasterBlockPtr ptrBlock = std::make_shared<CRasterBlock>();
        blocks = 0;
        while(ptrCursor->Next(ptrBlock))
        {
            ++blocks;
            REQUIRE(ptrBlock->GetPixelSize() == pixelSize);
            REQUIRE(ptrBlock->GetExtent() != nullptr);
            int outX = (ptrBlock->GetSourceCol() - pCursor->GetWindowCol()) / step;
            int outY = (ptrBlock->GetSourceRow() - pCursor->GetWindowRow()) / step;
            for(int y = 0; y < ptrBlock->GetHeight(); ++y)
            {
                memcpy(image.data() + (size_t(outY + y) * width + outX) * pixelSize,
                       static_cast<uint8_t*>(ptrBlock->GetData()) + size_t(y) * ptrBlock->GetRowSize(), ptrBlock->GetRowSize());
                for(int x = 0; x < ptrBlock->GetWidth(); ++x)
                {
                    REQUIRE(written[size_t(outY + y) * width + outX + x] == 0); // blocks don't overlap
                    written[size_t(outY + y) * width + outX + x] = 1;
                }
            }
        }
        REQUIRE(blocks == pCursor->GetBlockCount());
        REQUIRE(std::find(written.begin(), written.end(), 0) == written.end()); // and cover the window
        return image;
    }

    IRasterDatasetPtr OpenRaster(const std::string& sPath)
    {
        std::filesystem::path path(sPath);
        IWorkspacePtr ptrWks = CRasterWorkspace::Open("raster", path.parent_path().string().c_str(), CommonLib::CGuid::CreateNew());
        IRasterWorkspacePtr ptrRasterWks = std::dynamic_pointer_cast<IRasterWorkspace>(ptrWks);
        REQUIRE(ptrRasterWks != nullptr);
        REQUIRE(ptrWks->GetWorkspaceType() == wtRaster);
        return ptrRasterWks->OpenRasterDataset(path.filename().string());
    }

    template<class T>
    void CheckImage(const std::vector<uint8_t>& image, int width, int height, int bands, int col0, int row0, int step, double scale = 1.)
    {
        const T* pData = reinterpret_cast<const T*>(image.data());
        for(int y = 0; y < height; ++y)
            for(int x = 0; x < width; ++x)
                for(int s = 0; s < bands; ++s)
                {
                    T expected = T(T(TestValue(col0 + x * step, row0 + y * step, s)) * scale);
                    if(pData[(size_t(y) * width + x) * bands + s] != expected)
                    {
                        INFO("x=" << x << " y=" << y << " band=" << s);
                        REQUIRE(pData[(size_t(y) * width + x) * bands + s] == expected);
                    }
                }
    }
}

TEST_CASE("Raster: GeoTIFF RGB strips, georeference and full read", "[geodatabase][raster]")
{
    STiffParams p;
    p.geoTiff = true;
    std::string sPath = WriteTiff("rgb_strips.tif", p);

    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    CRasterDataset* pDataset = dynamic_cast<CRasterDataset*>(ptrDataset.get());
    REQUIRE(pDataset != nullptr);

    REQUIRE(ptrDataset->GetDatasetType() == dtTypeRaster);
    REQUIRE(ptrDataset->GetDatasetName() == "rgb_strips.tif");
    double dNoDataNone = 0.;
    REQUIRE_FALSE(ptrDataset->GetNoDataValue(dNoDataNone));
    REQUIRE(pDataset->GetWidth() == 300);
    REQUIRE(pDataset->GetHeight() == 200);
    REQUIRE(ptrDataset->GetBandCount() == 3);
    REQUIRE(ptrDataset->GetPixelType() == RasterPixelTypeUChar);
    REQUIRE(ptrDataset->GetResolution() == 10.);
    REQUIRE(pDataset->IsGeoreferenced());
    REQUIRE(pDataset->GetGeoInfo().epsgCode == 32633);

    const CommonLib::bbox& bb = pDataset->GetExtent()->GetBoundingBox();
    REQUIRE(bb.xMin == 1000.);
    REQUIRE(bb.xMax == 4000.);
    REQUIRE(bb.yMax == 2000.);
    REQUIRE(bb.yMin == 0.);
    REQUIRE(ptrDataset->GetSpatialReference() != nullptr);

    std::shared_ptr<CRasterSpatialFilter> ptrFilter = std::make_shared<CRasterSpatialFilter>();
    ptrFilter->SetBlockSize(128, 64);
    int width, height, pixelSize, blocks;
    std::vector<uint8_t> image = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    REQUIRE(width == 300);
    REQUIRE(height == 200);
    REQUIRE(blocks == 3 * 4);
    CheckImage<uint8_t>(image, width, height, 3, 0, 0, 1);
}

TEST_CASE("Raster: spatial filter selects the pixel window", "[geodatabase][raster]")
{
    STiffParams p;
    p.geoTiff = true;
    std::string sPath = WriteTiff("rgb_strips.tif", p);
    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);

    // x 1505..2095 -> cols 50..110, y 1795..1405 -> rows 20..60
    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 1505.; bb.xMax = 2095.;
    bb.yMin = 1405.; bb.yMax = 1795.;
    std::shared_ptr<CRasterSpatialFilter> ptrFilter = std::make_shared<CRasterSpatialFilter>(bb);
    ptrFilter->SetBlockSize(32, 32);

    IRasterCursorPtr ptrCursor = ptrDataset->Search(ptrFilter);
    CRasterCursor* pCursor = dynamic_cast<CRasterCursor*>(ptrCursor.get());
    REQUIRE(pCursor->GetWindowCol() == 50);
    REQUIRE(pCursor->GetWindowRow() == 20);
    REQUIRE(pCursor->GetWindowWidth() == 60);
    REQUIRE(pCursor->GetWindowHeight() == 40);

    CRasterBlockPtr ptrBlock = std::make_shared<CRasterBlock>();
    REQUIRE(ptrCursor->Next(ptrBlock));
    const CommonLib::bbox& blockBB = ptrBlock->GetExtent()->GetBoundingBox();
    REQUIRE(blockBB.xMin == 1500.);
    REQUIRE(blockBB.xMax == 1820.);
    REQUIRE(blockBB.yMax == 1800.);
    REQUIRE(blockBB.yMin == 1480.);

    int width, height, pixelSize, blocks;
    std::vector<uint8_t> image = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    REQUIRE(blocks == 2 * 2);
    CheckImage<uint8_t>(image, width, height, 3, 50, 20, 1);

    // reset restarts the iteration
    ptrCursor->Reset();
    int n = 0;
    while(ptrCursor->Next(ptrBlock))
        ++n;
    REQUIRE(n == 4);

    // outside of the raster
    bb.xMin = -500.; bb.xMax = -100.;
    ptrFilter->SetBB(bb);
    ptrCursor = ptrDataset->Search(ptrFilter);
    REQUIRE_FALSE(ptrCursor->Next(ptrBlock));
}

TEST_CASE("Raster: tiled 16-bit Deflate/LZW with world file and pixel step", "[geodatabase][raster]")
{
    STiffParams p;
    p.width = 520;
    p.height = 300;
    p.bitsPerSample = 16;
    p.samples = 1;
    p.photometric = PHOTOMETRIC_MINISBLACK;
    p.tileSize = 256;
#ifdef ZIP_SUPPORT
    p.compression = COMPRESSION_ADOBE_DEFLATE;
#else
    p.compression = COMPRESSION_LZW;
#endif
    std::string sPath = WriteTiff("gray16_tiled.tif", p);
    {
        std::ofstream tfw((TestDir() / "raster" / "gray16_tiled.tfw").string());
        tfw << "2.0\n0.0\n0.0\n-2.0\n101.0\n599.0\n";   // upper-left pixel center (101, 599)
    }

    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    CRasterDataset* pDataset = dynamic_cast<CRasterDataset*>(ptrDataset.get());
    REQUIRE(ptrDataset->GetPixelType() == RasterPixelTypeUShort);
    REQUIRE(ptrDataset->GetBandCount() == 1);
    REQUIRE(pDataset->GetGeoInfo().sSource == "worldfile");
    REQUIRE(pDataset->GetExtent()->GetBoundingBox().xMin == 100.);
    REQUIRE(pDataset->GetExtent()->GetBoundingBox().yMax == 600.);

    std::shared_ptr<CRasterSpatialFilter> ptrFilter = std::make_shared<CRasterSpatialFilter>();
    int width, height, pixelSize, blocks;
    std::vector<uint8_t> image = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    REQUIRE(width == 520);
    CheckImage<uint16_t>(image, width, height, 1, 0, 0, 1);

    ptrFilter->SetPixelStep(3);
    ptrFilter->SetBlockSize(100, 50);
    image = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    REQUIRE(width == 174);
    REQUIRE(height == 100);
    CheckImage<uint16_t>(image, width, height, 1, 0, 0, 3);
}

TEST_CASE("Raster: planar separate float32", "[geodatabase][raster]")
{
    STiffParams p;
    p.width = 70;
    p.height = 45;
    p.bitsPerSample = 32;
    p.samples = 2;
    p.sampleFormat = SAMPLEFORMAT_IEEEFP;
    p.photometric = PHOTOMETRIC_MINISBLACK;
    p.planar = PLANARCONFIG_SEPARATE;
    p.compression = COMPRESSION_NONE;
    p.rowsPerStrip = 7;
    std::string sPath = WriteTiff("float_planar.tif", p, [](TIFF* tif) {
        uint16_t extra = EXTRASAMPLE_UNSPECIFIED;
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, &extra);
    });

    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    REQUIRE(ptrDataset->GetPixelType() == RasterPixelTypeFloat);
    REQUIRE(ptrDataset->GetBandCount() == 2);
    REQUIRE_FALSE(dynamic_cast<CRasterDataset*>(ptrDataset.get())->IsGeoreferenced());

    int width, height, pixelSize, blocks;
    std::shared_ptr<CRasterSpatialFilter> ptrFilter = std::make_shared<CRasterSpatialFilter>();
    ptrFilter->SetBlockSize(16, 16);
    std::vector<uint8_t> image = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    CheckImage<float>(image, width, height, 2, 0, 0, 1, 0.5);
}

TEST_CASE("Raster: paletted 4-bit is expanded to RGB", "[geodatabase][raster]")
{
    std::filesystem::path dir = TestDir() / "raster";
    std::filesystem::create_directories(dir);
    std::string sPath = (dir / "palette4.tif").string();

    const uint32_t width = 33;
    const uint32_t height = 10;
    TIFF* tif = TIFFOpen(sPath.c_str(), "w");
    REQUIRE(tif != nullptr);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 4);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_PACKBITS);
    uint16_t red[16], green[16], blue[16];
    for(int i = 0; i < 16; ++i)
    {
        red[i] = uint16_t((i * 16) << 8);
        green[i] = uint16_t((255 - i) << 8);
        blue[i] = uint16_t(i << 8);
    }
    TIFFSetField(tif, TIFFTAG_COLORMAP, red, green, blue);
    std::vector<uint8_t> line((width + 1) / 2, 0);
    for(uint32_t y = 0; y < height; ++y)
    {
        std::fill(line.begin(), line.end(), 0);
        for(uint32_t x = 0; x < width; ++x)
            line[x / 2] |= uint8_t(((x + y) % 16) << ((x % 2) ? 0 : 4));
        REQUIRE(TIFFWriteScanline(tif, line.data(), y, 0) >= 0);
    }
    TIFFClose(tif);

    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    CRasterDataset* pDataset = dynamic_cast<CRasterDataset*>(ptrDataset.get());
    REQUIRE(pDataset->GetSourcePixelType() == RasterPixelType4Bits);
    REQUIRE(ptrDataset->GetPixelType() == RasterPixelTypeUChar);
    REQUIRE(ptrDataset->GetBandCount() == 3);

    int w, h, pixelSize, blocks;
    std::vector<uint8_t> image = ReadAll(ptrDataset, std::make_shared<CRasterSpatialFilter>(), w, h, pixelSize, blocks);
    for(uint32_t y = 0; y < height; ++y)
        for(uint32_t x = 0; x < width; ++x)
        {
            int index = (x + y) % 16;
            const uint8_t* pPixel = image.data() + (size_t(y) * width + x) * 3;
            REQUIRE(pPixel[0] == index * 16);
            REQUIRE(pPixel[1] == 255 - index);
            REQUIRE(pPixel[2] == index);
        }
}

TEST_CASE("Raster: JPEG YCbCr tiled is decoded to RGB", "[geodatabase][raster]")
{
#ifdef JPEG_SUPPORT
    STiffParams p;
    p.width = 64;
    p.height = 48;
    p.photometric = PHOTOMETRIC_YCBCR;
    p.compression = COMPRESSION_JPEG;
    p.tileSize = 16;
    std::string sPath = WriteTiff("jpeg_ycbcr.tif", p);

    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    REQUIRE(ptrDataset->GetPixelType() == RasterPixelTypeUChar);
    REQUIRE(ptrDataset->GetBandCount() == 3);

    int width, height, pixelSize, blocks;
    std::vector<uint8_t> image = ReadAll(ptrDataset, std::make_shared<CRasterSpatialFilter>(), width, height, pixelSize, blocks);
    // lossy: compare with libtiff's own RGBA decoding of the same file
    std::vector<uint32_t> reference(size_t(width) * height, 0);
    TIFF* tif = TIFFOpen(sPath.c_str(), "r");
    REQUIRE(tif != nullptr);
    REQUIRE(TIFFReadRGBAImageOriented(tif, width, height, reference.data(), ORIENTATION_TOPLEFT, 0) == 1);
    TIFFClose(tif);

    int maxDiff = 0;
    for(int y = 0; y < height; ++y)
        for(int x = 0; x < width; ++x)
        {
            uint32_t abgr = reference[size_t(y) * width + x];
            const uint8_t* pPixel = image.data() + (size_t(y) * width + x) * 3;
            maxDiff = std::max(maxDiff, std::abs(int(pPixel[0]) - int(TIFFGetR(abgr))));
            maxDiff = std::max(maxDiff, std::abs(int(pPixel[1]) - int(TIFFGetG(abgr))));
            maxDiff = std::max(maxDiff, std::abs(int(pPixel[2]) - int(TIFFGetB(abgr))));
        }
    REQUIRE(maxDiff <= 1);
#endif
}

TEST_CASE("Raster: bottom-up orientation is flipped", "[geodatabase][raster]")
{
    STiffParams p;
    p.width = 20;
    p.height = 30;
    p.samples = 1;
    p.photometric = PHOTOMETRIC_MINISBLACK;
    p.rowsPerStrip = 4;
    std::string sPath = WriteTiff("botleft.tif", p, [](TIFF* tif) {
        TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    });

    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    int width, height, pixelSize, blocks;
    std::shared_ptr<CRasterSpatialFilter> ptrFilter = std::make_shared<CRasterSpatialFilter>();
    ptrFilter->SetBlockSize(8, 8);
    std::vector<uint8_t> image = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    for(int y = 0; y < height; ++y)
        for(int x = 0; x < width; ++x)
            REQUIRE(image[size_t(y) * width + x] == uint8_t(TestValue(x, height - 1 - y, 0)));
}

TEST_CASE("Raster: workspace errors and serialization", "[geodatabase][raster]")
{
    std::filesystem::path dir = TestDir() / "raster";
    std::filesystem::create_directories(dir);
    {
        std::ofstream txt((dir / "not_a_raster.tif").string());
        txt << "hello";
    }

    IWorkspacePtr ptrWks = CRasterWorkspace::Open("raster", dir.string().c_str(), CommonLib::CGuid::CreateNew());
    IRasterWorkspacePtr ptrRasterWks = std::dynamic_pointer_cast<IRasterWorkspace>(ptrWks);
    REQUIRE_THROWS(ptrRasterWks->OpenRasterDataset("not_a_raster.tif"));
    REQUIRE_THROWS(ptrRasterWks->OpenRasterDataset("missing.tif"));

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrWks->Save(ptrRoot);
    IWorkspacePtr ptrLoaded = CDatasetLoader::LoadWorkspace(ptrRoot);
    REQUIRE(ptrLoaded->GetWorkspaceType() == wtRaster);
    REQUIRE(ptrLoaded->GetWorkspaceId() == ptrWks->GetWorkspaceId());
}

namespace
{
    // the value of a level: 0 - the full resolution, 1.. - the overviews
    int16_t LevelValue(int nLevel, uint32_t x, uint32_t y)
    {
        return int16_t(nLevel * 1000 + (x % 30) * 10 + y % 10);
    }

    void WriteLevel(TIFF* tif, int nLevel, uint32_t width, uint32_t height, uint32_t subfileType, uint16_t bits = 16)
    {
        TIFFSetField(tif, TIFFTAG_SUBFILETYPE, subfileType);
        if(nLevel == 0)
        {
            static const TIFFFieldInfo noData[] = {{TIFFTAG_GDAL_NODATA, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_ASCII, FIELD_CUSTOM, 1, 0, "GDALNoDataValue"}};
            TIFFMergeFieldInfo(tif, noData, 1);
            TIFFSetField(tif, TIFFTAG_GDAL_NODATA, "-32767");
        }
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_INT);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
        TIFFSetField(tif, TIFFTAG_TILEWIDTH, 64);
        TIFFSetField(tif, TIFFTAG_TILELENGTH, 64);
        std::vector<uint8_t> tile((size_t)TIFFTileSize(tif), 0);
        for(uint32_t ty = 0; ty < height; ty += 64)
            for(uint32_t tx = 0; tx < width; tx += 64)
            {
                for(uint32_t y = 0; y < 64; ++y)
                    for(uint32_t x = 0; x < 64; ++x)
                    {
                        if(bits == 16)
                            reinterpret_cast<int16_t*>(tile.data())[y * 64 + x] = LevelValue(nLevel, tx + x, ty + y);
                        else
                            tile[y * 64 + x] = 7;
                    }
                REQUIRE(TIFFWriteTile(tif, tile.data(), tx, ty, 0, 0) >= 0);
            }
        REQUIRE(TIFFWriteDirectory(tif));
    }
}

TEST_CASE("Raster: overviews are read for a pixel step", "[geodatabase][raster]")
{
    std::filesystem::path dir = TestDir() / "raster";
    std::filesystem::create_directories(dir);
    std::string sPath = (dir / "overviews.tif").string();
    {
        TIFF* tif = TIFFOpen(sPath.c_str(), "w8");    // BigTIFF, as the big COG files
        REQUIRE(tif != nullptr);
        WriteLevel(tif, 0, 1000, 800, 0);
        WriteLevel(tif, 1, 500, 400, FILETYPE_REDUCEDIMAGE);
        WriteLevel(tif, 9, 500, 400, FILETYPE_REDUCEDIMAGE | FILETYPE_MASK, 8);   // a mask: skipped
        WriteLevel(tif, 3, 125, 100, FILETYPE_REDUCEDIMAGE);                       // out of order: sorted
        WriteLevel(tif, 2, 250, 200, FILETYPE_REDUCEDIMAGE);
        WriteLevel(tif, 9, 63, 50, FILETYPE_REDUCEDIMAGE, 8);                      // another pixel type: skipped
        WriteLevel(tif, 9, 300, 300, 0);                                           // not an overview: skipped
        TIFFClose(tif);
    }

    CTIFFReader reader;
    reader.Open(sPath);
    REQUIRE(reader.GetWidth() == 1000);
    REQUIRE(reader.GetLevelCount() == 4);
    REQUIRE(reader.GetLevelWidth(1) == 500);
    REQUIRE(reader.GetLevelWidth(2) == 250);
    REQUIRE(reader.GetLevelWidth(3) == 125);
    REQUIRE(reader.SelectLevel(1) == 0);
    REQUIRE(reader.SelectLevel(2) == 1);
    REQUIRE(reader.SelectLevel(3) == 1);
    REQUIRE(reader.SelectLevel(5) == 2);
    REQUIRE(reader.SelectLevel(1000) == 3);
    double dNoData = 0.;
    REQUIRE(reader.GetNoDataValue(dNoData));
    REQUIRE(dNoData == -32767.);

    // step 1: the full resolution
    std::vector<int16_t> buf(1000 * 800);
    reader.ReadWindow(100, 50, 200, 3, 1, buf.data());
    for(uint32_t r = 0; r < 3; ++r)
        for(uint32_t c = 0; c < 200; ++c)
            REQUIRE(buf[r * 200 + c] == LevelValue(0, 100 + c, 50 + r));

    // step 4: the 250 x 200 overview, the pixel (c * 4, r * 4) is the overview pixel (c, r)
    reader.ReadWindow(0, 0, 250, 200, 4, buf.data());
    for(uint32_t r = 0; r < 200; ++r)
        for(uint32_t c = 0; c < 250; ++c)
            REQUIRE(buf[r * 250 + c] == LevelValue(2, c, r));

    // step 10 from an offset: the 125 x 100 overview (factor 8), by the pixel centers
    uint64_t nDecoded = reader.GetDecodedChunkCount();
    reader.ReadWindow(5, 3, 99, 79, 10, buf.data());
    for(uint32_t r = 0; r < 79; ++r)
        for(uint32_t c = 0; c < 99; ++c)
        {
            uint32_t x = ((5 + c * 10) * 2 + 1) * 125 / 2000;
            uint32_t y = ((3 + r * 10) * 2 + 1) * 100 / 1600;
            REQUIRE(buf[r * 99 + c] == LevelValue(3, x, y));
        }
    REQUIRE(reader.GetDecodedChunkCount() - nDecoded == 4);    // 2 x 2 tiles of the overview

    // the dataset cursor with a step, the extent stays the one of the full resolution
    IRasterDatasetPtr ptrDataset = OpenRaster(sPath);
    REQUIRE(ptrDataset->GetWidth() == 1000);
    REQUIRE(ptrDataset->GetNoDataValue(dNoData));
    std::shared_ptr<CRasterSpatialFilter> ptrFilter = std::make_shared<CRasterSpatialFilter>();
    ptrFilter->SetPixelStep(2);
    int width = 0, height = 0, pixelSize = 0, blocks = 0;
    std::vector<uint8_t> data = ReadAll(ptrDataset, ptrFilter, width, height, pixelSize, blocks);
    REQUIRE(width == 500);
    REQUIRE(height == 400);
    const int16_t* pValues = reinterpret_cast<const int16_t*>(data.data());
    REQUIRE(pValues[0] == LevelValue(1, 0, 0));
    REQUIRE(pValues[399 * 500 + 499] == LevelValue(1, 499, 399));
}

TEST_CASE("Raster: CTIFF wrapper writes and reads through the CommonLib file", "[geodatabase][raster]")
{
    std::filesystem::path dir = TestDir() / "raster";
    std::filesystem::create_directories(dir);
    // a non-ASCII name: the UTF-8 path goes to the CommonLib file
    std::string sPath = (dir / std::filesystem::u8path(u8"тайл.tif")).u8string();

    {
        TIFFLib::TIFFPtr ptrTif = TIFFLib::CTIFF::Create(sPath, true);
        REQUIRE(ptrTif->IsOpen());
        REQUIRE(ptrTif->SetField(TIFFTAG_IMAGEWIDTH, (uint32_t)100));
        REQUIRE(ptrTif->SetField(TIFFTAG_IMAGELENGTH, (uint32_t)70));
        REQUIRE(ptrTif->SetField(TIFFTAG_BITSPERSAMPLE, (uint16_t)16));
        REQUIRE(ptrTif->SetField(TIFFTAG_SAMPLESPERPIXEL, (uint16_t)1));
        REQUIRE(ptrTif->SetField(TIFFTAG_SAMPLEFORMAT, (uint16_t)SAMPLEFORMAT_INT));
        REQUIRE(ptrTif->SetField(TIFFTAG_PHOTOMETRIC, (uint16_t)PHOTOMETRIC_MINISBLACK));
        REQUIRE(ptrTif->SetField(TIFFTAG_COMPRESSION, (uint16_t)COMPRESSION_DEFLATE));
        REQUIRE(ptrTif->SetField(TIFFTAG_TILEWIDTH, (uint32_t)64));
        REQUIRE(ptrTif->SetField(TIFFTAG_TILELENGTH, (uint32_t)64));
        REQUIRE(ptrTif->SetField(TIFFTAG_GDAL_NODATA, std::string("-9999")));
        REQUIRE(ptrTif->SetField(33550, std::vector<double>{2., 3., 0.}));
        std::vector<int16_t> tile(64 * 64);
        for(uint32_t ty = 0; ty < 70; ty += 64)
            for(uint32_t tx = 0; tx < 100; tx += 64)
            {
                for(uint32_t y = 0; y < 64; ++y)
                    for(uint32_t x = 0; x < 64; ++x)
                        tile[y * 64 + x] = int16_t((tx + x) - (ty + y) * 3);
                ptrTif->WriteTile(tile.data(), tx, ty, 0, 0);
            }
        ptrTif->Close();
        REQUIRE_FALSE(ptrTif->IsOpen());
    }

    REQUIRE(TIFFLib::CTIFF::IsTIFFFile(sPath));
    TIFFLib::TIFFPtr ptrTif = TIFFLib::CTIFF::Open(sPath);
    uint32_t width = 0;
    uint16_t bits = 0, planar = 0;
    REQUIRE(ptrTif->GetField(TIFFTAG_IMAGEWIDTH, width));
    REQUIRE(width == 100);
    REQUIRE(ptrTif->GetField(TIFFTAG_BITSPERSAMPLE, bits));
    REQUIRE(bits == 16);
    REQUIRE(ptrTif->GetField(TIFFTAG_PLANARCONFIG, planar));
    REQUIRE(planar == PLANARCONFIG_CONTIG);
    uint16_t orientation = 0;
    REQUIRE_FALSE(ptrTif->GetField(TIFFTAG_ORIENTATION, orientation));       // not written
    REQUIRE(ptrTif->GetFieldDefaulted(TIFFTAG_ORIENTATION, orientation));
    REQUIRE(orientation == ORIENTATION_TOPLEFT);
    std::string sNoData;
    REQUIRE(ptrTif->GetField(TIFFTAG_GDAL_NODATA, sNoData));
    REQUIRE(sNoData == "-9999");
    std::vector<double> scale;
    REQUIRE(ptrTif->GetField(33550, scale));
    REQUIRE(scale == std::vector<double>{2., 3., 0.});
    REQUIRE(ptrTif->IsTiled());
    REQUIRE(ptrTif->GetDirectoryCount() == 1);
    REQUIRE(ptrTif->GetTileSize() == 64 * 64 * 2);

    uint32_t nTile = ptrTif->ComputeTile(70, 65, 0, 0);
    REQUIRE(nTile == 3);
    std::vector<int16_t> data(64 * 64);
    ptrTif->ReadEncodedTile(nTile, data.data(), data.size() * 2);
    REQUIRE(data[1 * 64 + 6] == int16_t(70 - 65 * 3));
    REQUIRE_THROWS_WITH(ptrTif->ReadEncodedTile(100, data.data(), data.size() * 2), Catch::Matchers::ContainsSubstring("libtiff"));
    REQUIRE_THROWS(ptrTif->SetDirectory(5));

    // the reader on the same file
    CTIFFReader reader;
    reader.Open(sPath);
    double dNoData = 0.;
    REQUIRE(reader.GetNoDataValue(dNoData));
    REQUIRE(dNoData == -9999.);
    int16_t value = 0;
    reader.ReadWindow(99, 69, 1, 1, 1, &value);
    REQUIRE(value == int16_t(99 - 69 * 3));

    // errors: no file, not a TIFF
    REQUIRE_THROWS(TIFFLib::CTIFF::Open((dir / "missing.tif").u8string()));
    std::string sText = (dir / "not_tiff.tif").u8string();
    {
        std::ofstream out(std::filesystem::u8path(sText), std::ios::binary);
        out << "this is not a TIFF file at all";
    }
    REQUIRE_FALSE(TIFFLib::CTIFF::IsTIFFFile(sText));
    REQUIRE_THROWS_WITH(TIFFLib::CTIFF::Open(sText), Catch::Matchers::ContainsSubstring("Failed to open TIFF"));
}
