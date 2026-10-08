#include "TestCommon.h"
#include "../../GeoDatabaseRaster/RasterWorkspace.h"
#include "../../GeoDatabaseRaster/RasterDataset.h"
#include "../../GeoDatabaseRaster/RasterCursor.h"
#include "../../GeoDatabaseRaster/RasterBlock.h"
#include "../../GeoDatabaseRaster/RasterSpatialFilter.h"

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
