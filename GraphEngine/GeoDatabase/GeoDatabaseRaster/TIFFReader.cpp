#include "TIFFReader.h"
#include "RasterBlock.h"

extern "C" {
#include "tiffio.h"
}

#include <algorithm>
#include <cstdio>
#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace GraphEngine
{
    namespace GeoDatabase {

        namespace
        {
            // GeoTIFF tags (libtiff doesn't know them, they are registered through a tag extender)
            const uint32_t kTagModelPixelScale    = 33550;
            const uint32_t kTagModelTiepoint      = 33922;
            const uint32_t kTagModelTransformation = 34264;
            const uint32_t kTagGeoKeyDirectory    = 34735;
            const uint32_t kTagGeoDoubleParams    = 34736;
            const uint32_t kTagGeoAsciiParams     = 34737;

            // GeoKeys
            const uint16_t kKeyGTModelType        = 1024;
            const uint16_t kKeyGTRasterType       = 1025;
            const uint16_t kKeyGeographicType     = 2048;
            const uint16_t kKeyProjectedCSType    = 3072;

            const uint16_t kModelTypeProjected    = 1;
            const uint16_t kModelTypeGeographic   = 2;
            const uint16_t kRasterPixelIsPoint    = 2;
            const uint16_t kUserDefined           = 32767;

            TIFFFieldInfo g_GeoTiffFields[] = {
                {kTagModelPixelScale,     TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelPixelScaleTag"},
                {kTagModelTiepoint,       TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelTiepointTag"},
                {kTagModelTransformation, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelTransformationTag"},
                {kTagGeoKeyDirectory,     TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SHORT,  FIELD_CUSTOM, 1, 1, "GeoKeyDirectoryTag"},
                {kTagGeoDoubleParams,     TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "GeoDoubleParamsTag"},
                {kTagGeoAsciiParams,      TIFF_VARIABLE,  TIFF_VARIABLE,  TIFF_ASCII,  FIELD_CUSTOM, 1, 0, "GeoASCIIParamsTag"},
            };

            TIFFExtendProc g_ParentExtender = nullptr;

            void GeoTiffTagExtender(TIFF* tif)
            {
                TIFFMergeFieldInfo(tif, g_GeoTiffFields, sizeof(g_GeoTiffFields) / sizeof(g_GeoTiffFields[0]));
                if(g_ParentExtender)
                    g_ParentExtender(tif);
            }

            void RegisterGeoTiffTags()
            {
                static std::once_flag flag;
                std::call_once(flag, []() { g_ParentExtender = TIFFSetTagExtender(GeoTiffTagExtender); });
            }

            std::filesystem::path ToPath(const std::string& sUtf8)
            {
                return std::filesystem::u8path(sUtf8);
            }

            std::string FormatMsg(const char* module, const char* fmt, va_list ap)
            {
                char buf[1024];
                vsnprintf(buf, sizeof(buf), fmt, ap);
                std::string msg;
                if(module && *module)
                {
                    msg = module;
                    msg += ": ";
                }
                msg += buf;
                return msg;
            }
        }

        CTIFFReader::CTIFFReader() :
            m_pTif(nullptr)
            , m_width(0)
            , m_height(0)
            , m_bitsPerSample(0)
            , m_samplesPerPixel(0)
            , m_photometric(0)
            , m_planarConfig(PLANARCONFIG_CONTIG)
            , m_sampleFormat(SAMPLEFORMAT_UINT)
            , m_orientation(ORIENTATION_TOPLEFT)
            , m_compression(COMPRESSION_NONE)
            , m_bTiled(false)
            , m_chunkWidth(0)
            , m_chunkHeight(0)
            , m_chunkRowBytes(0)
            , m_chunkBytes(0)
            , m_readMode(rmNative)
            , m_sourcePixelType(RasterPixelTypeUnknown)
            , m_pixelType(RasterPixelTypeUnknown)
            , m_bandCount(0)
            , m_cacheBytes(0)
            , m_maxCacheBytes(32 * 1024 * 1024)
        {

        }

        CTIFFReader::~CTIFFReader()
        {
            Close();
        }

        void CTIFFReader::Close()
        {
            if(m_pTif)
                TIFFClose(m_pTif);
            m_pTif = nullptr;
            m_chunks.clear();
            m_chunkOrder.clear();
            m_cacheBytes = 0;
        }

        int CTIFFReader::ErrorHandler(TIFF* /*tif*/, void* pUserData, const char* module, const char* fmt, va_list ap)
        {
            CTIFFReader* pReader = static_cast<CTIFFReader*>(pUserData);
            if(pReader)
                pReader->m_sLastError = FormatMsg(module, fmt, ap);
            return 1; // handled, don't call the global (stderr) handler
        }

        int CTIFFReader::WarningHandler(TIFF* /*tif*/, void* /*pUserData*/, const char* /*module*/, const char* /*fmt*/, va_list /*ap*/)
        {
            return 1; // warnings (unknown tags etc.) are ignored
        }

        void CTIFFReader::ThrowError(const std::string& sMsg) const
        {
            if(m_sLastError.empty())
                throw CommonLib::CExcBase("{0}, file: {1}", sMsg, m_sFilePath);
            throw CommonLib::CExcBase("{0}, file: {1}, libtiff: {2}", sMsg, m_sFilePath, m_sLastError);
        }

        bool CTIFFReader::IsTIFFFile(const std::string& sFilePath)
        {
            std::ifstream file(ToPath(sFilePath), std::ios::binary);
            if(!file)
                return false;

            unsigned char hdr[4] = {0, 0, 0, 0};
            file.read(reinterpret_cast<char*>(hdr), 4);
            if(file.gcount() != 4)
                return false;

            // classic TIFF (42) and BigTIFF (43), little and big endian
            if(hdr[0] == 'I' && hdr[1] == 'I' && hdr[3] == 0 && (hdr[2] == 42 || hdr[2] == 43))
                return true;
            if(hdr[0] == 'M' && hdr[1] == 'M' && hdr[2] == 0 && (hdr[3] == 42 || hdr[3] == 43))
                return true;
            return false;
        }

        void CTIFFReader::Open(const std::string& sFilePath)
        {
            Close();
            m_sFilePath = sFilePath;
            m_sLastError.clear();

            RegisterGeoTiffTags();

            TIFFOpenOptions* pOptions = TIFFOpenOptionsAlloc();
            TIFFOpenOptionsSetErrorHandlerExtR(pOptions, &CTIFFReader::ErrorHandler, this);
            TIFFOpenOptionsSetWarningHandlerExtR(pOptions, &CTIFFReader::WarningHandler, this);
#ifdef _WIN32
            std::wstring sWidePath = ToPath(sFilePath).wstring();
            m_pTif = TIFFOpenWExt(sWidePath.c_str(), "r", pOptions);
#else
            m_pTif = TIFFOpenExt(sFilePath.c_str(), "r", pOptions);
#endif
            TIFFOpenOptionsFree(pOptions);

            if(!m_pTif)
                ThrowError("Failed to open TIFF");

            Parse();
            ReadGeoInfo();
        }

        IRasterReaderPtr CTIFFReader::Clone() const
        {
            std::shared_ptr<CTIFFReader> ptrReader = std::make_shared<CTIFFReader>();
            ptrReader->Open(m_sFilePath);
            return ptrReader;
        }

        void CTIFFReader::Parse()
        {
            if(!TIFFGetField(m_pTif, TIFFTAG_IMAGEWIDTH, &m_width) || !TIFFGetField(m_pTif, TIFFTAG_IMAGELENGTH, &m_height))
                ThrowError("TIFF has no image size");
            if(m_width == 0 || m_height == 0 || m_width > INT32_MAX || m_height > INT32_MAX)
                ThrowError("Unsupported TIFF image size");

            TIFFGetFieldDefaulted(m_pTif, TIFFTAG_BITSPERSAMPLE, &m_bitsPerSample);
            TIFFGetFieldDefaulted(m_pTif, TIFFTAG_SAMPLESPERPIXEL, &m_samplesPerPixel);
            TIFFGetFieldDefaulted(m_pTif, TIFFTAG_PLANARCONFIG, &m_planarConfig);
            TIFFGetFieldDefaulted(m_pTif, TIFFTAG_SAMPLEFORMAT, &m_sampleFormat);
            TIFFGetFieldDefaulted(m_pTif, TIFFTAG_ORIENTATION, &m_orientation);
            TIFFGetFieldDefaulted(m_pTif, TIFFTAG_COMPRESSION, &m_compression);
            if(!TIFFGetField(m_pTif, TIFFTAG_PHOTOMETRIC, &m_photometric))
                m_photometric = m_samplesPerPixel >= 3 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK;
            if(m_samplesPerPixel == 0)
                ThrowError("TIFF has no samples");

            // JPEG-in-TIFF YCbCr: let libjpeg convert to RGB (also makes libtiff report RGB-sized strips/tiles)
            if(m_compression == COMPRESSION_JPEG && m_photometric == PHOTOMETRIC_YCBCR && m_planarConfig == PLANARCONFIG_CONTIG)
            {
                TIFFSetField(m_pTif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
                m_photometric = PHOTOMETRIC_RGB;
            }

            // source pixel type
            m_sourcePixelType = RasterPixelTypeUnknown;
            switch(m_bitsPerSample)
            {
                case 1: m_sourcePixelType = RasterPixelType1Bit; break;
                case 2: m_sourcePixelType = RasterPixelType2Bits; break;
                case 4: m_sourcePixelType = RasterPixelType4Bits; break;
                case 8:
                    m_sourcePixelType = m_sampleFormat == SAMPLEFORMAT_INT ? RasterPixelTypeChar : RasterPixelTypeUChar;
                    break;
                case 16:
                    if(m_sampleFormat != SAMPLEFORMAT_IEEEFP)
                        m_sourcePixelType = m_sampleFormat == SAMPLEFORMAT_INT ? RasterPixelTypeShort : RasterPixelTypeUShort;
                    break;
                case 32:
                    if(m_sampleFormat == SAMPLEFORMAT_IEEEFP)
                        m_sourcePixelType = RasterPixelTypeFloat;
                    else
                        m_sourcePixelType = m_sampleFormat == SAMPLEFORMAT_INT ? RasterPixelTypeLong : RasterPixelTypeULong;
                    break;
                case 64:
                    if(m_sampleFormat == SAMPLEFORMAT_IEEEFP)
                        m_sourcePixelType = RasterPixelTypeDouble;
                    else
                        m_sourcePixelType = m_sampleFormat == SAMPLEFORMAT_INT ? RasterPixelTypeLongLong : RasterPixelTypeULongLong;
                    break;
                default:
                    break;
            }
            if(m_sampleFormat == SAMPLEFORMAT_COMPLEXINT || m_sampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP)
                m_sourcePixelType = RasterPixelTypeUnknown;

            // read mode
            bool bNeedsRGBA = m_photometric == PHOTOMETRIC_YCBCR
                    || m_photometric == PHOTOMETRIC_CIELAB || m_photometric == PHOTOMETRIC_ICCLAB || m_photometric == PHOTOMETRIC_ITULAB
                    || m_photometric == PHOTOMETRIC_LOGL || m_photometric == PHOTOMETRIC_LOGLUV
                    || m_compression == COMPRESSION_OJPEG;

            if(!bNeedsRGBA && m_photometric == PHOTOMETRIC_PALETTE && m_samplesPerPixel == 1 && m_bitsPerSample <= 16
               && m_sourcePixelType != RasterPixelTypeUnknown)
            {
                uint16_t* pRed = nullptr;
                uint16_t* pGreen = nullptr;
                uint16_t* pBlue = nullptr;
                if(!TIFFGetField(m_pTif, TIFFTAG_COLORMAP, &pRed, &pGreen, &pBlue))
                    ThrowError("Paletted TIFF has no color map");

                const size_t count = size_t(1) << m_bitsPerSample;
                // some writers store 8-bit color maps
                bool b8Bit = true;
                for(size_t i = 0; i < count && b8Bit; ++i)
                    b8Bit = pRed[i] < 256 && pGreen[i] < 256 && pBlue[i] < 256;
                const int shift = b8Bit ? 0 : 8;

                m_palette.resize(count * 3);
                for(size_t i = 0; i < count; ++i)
                {
                    m_palette[i * 3 + 0] = uint8_t(pRed[i] >> shift);
                    m_palette[i * 3 + 1] = uint8_t(pGreen[i] >> shift);
                    m_palette[i * 3 + 2] = uint8_t(pBlue[i] >> shift);
                }

                m_readMode = rmPalette;
                m_pixelType = RasterPixelTypeUChar;
                m_bandCount = 3;
            }
            else if(!bNeedsRGBA && m_sourcePixelType != RasterPixelTypeUnknown)
            {
                m_readMode = rmNative;
                m_pixelType = m_bitsPerSample < 8 ? RasterPixelTypeUChar : m_sourcePixelType;
                m_bandCount = m_samplesPerPixel;
            }
            else
            {
                char szMsg[1024] = {0};
                if(!TIFFRGBAImageOK(m_pTif, szMsg))
                    throw CommonLib::CExcBase("Unsupported TIFF format, file: {0}, {1}", m_sFilePath, std::string(szMsg));

                m_readMode = rmRGBA;
                m_pixelType = RasterPixelTypeUChar;
                m_bandCount = 4;
            }

            // chunk (strip/tile) geometry
            m_bTiled = TIFFIsTiled(m_pTif) != 0;
            if(m_bTiled)
            {
                if(!TIFFGetField(m_pTif, TIFFTAG_TILEWIDTH, &m_chunkWidth) || !TIFFGetField(m_pTif, TIFFTAG_TILELENGTH, &m_chunkHeight)
                   || m_chunkWidth == 0 || m_chunkHeight == 0)
                    ThrowError("Invalid TIFF tile size");
                m_chunkRowBytes = (size_t)TIFFTileRowSize64(m_pTif);
                m_chunkBytes = (size_t)TIFFTileSize64(m_pTif);
            }
            else
            {
                uint32_t rowsPerStrip = 0;
                TIFFGetFieldDefaulted(m_pTif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
                m_chunkWidth = m_width;
                m_chunkHeight = std::max<uint32_t>(1, (std::min)(rowsPerStrip, m_height));
                m_chunkRowBytes = (size_t)TIFFScanlineSize64(m_pTif);
                m_chunkBytes = (size_t)TIFFVStripSize64(m_pTif, m_chunkHeight);
            }

            if(m_readMode == rmRGBA)
            {
                m_chunkRowBytes = size_t(m_chunkWidth) * 4;
                m_chunkBytes = m_chunkRowBytes * m_chunkHeight;
            }

            if(m_chunkBytes == 0 || m_chunkRowBytes == 0)
                ThrowError("Invalid TIFF strip/tile size");
        }

        void CTIFFReader::ReadGeoInfo()
        {
            m_geoInfo = SRasterGeoInfo();
            if(ReadGeoTiffTags())
                return;
            if(ReadWorldFile())
                return;

            // not geo-referenced: pixel space, y up (image is upright in map units)
            m_geoInfo.bGeoreferenced = false;
            m_geoInfo.originX = 0.;
            m_geoInfo.originY = m_height;
            m_geoInfo.pixelSizeX = 1.;
            m_geoInfo.pixelSizeY = -1.;
        }

        bool CTIFFReader::ReadGeoTiffTags()
        {
            uint32_t count = 0;
            double* pData = nullptr;

            bool bTransform = false;
            if(TIFFGetField(m_pTif, kTagModelTransformation, &count, &pData) && count >= 16 && pData)
            {
                // [a b 0 d; e f 0 h; ...]  X = a*col + b*row + d, Y = e*col + f*row + h (rotation terms b, e ignored)
                m_geoInfo.pixelSizeX = pData[0];
                m_geoInfo.pixelSizeY = pData[5];
                m_geoInfo.originX = pData[3];
                m_geoInfo.originY = pData[7];
                bTransform = m_geoInfo.pixelSizeX != 0. && m_geoInfo.pixelSizeY != 0.;
            }

            if(!bTransform)
            {
                uint32_t scaleCount = 0;
                double* pScale = nullptr;
                uint32_t tieCount = 0;
                double* pTie = nullptr;
                if(!TIFFGetField(m_pTif, kTagModelPixelScale, &scaleCount, &pScale) || scaleCount < 2 || !pScale)
                    return false;
                if(!TIFFGetField(m_pTif, kTagModelTiepoint, &tieCount, &pTie) || tieCount < 6 || !pTie)
                    return false;
                if(pScale[0] == 0. || pScale[1] == 0.)
                    return false;

                // tiepoint (I, J, K) -> (X, Y, Z)
                m_geoInfo.pixelSizeX = pScale[0];
                m_geoInfo.pixelSizeY = -pScale[1];
                m_geoInfo.originX = pTie[3] - pTie[0] * m_geoInfo.pixelSizeX;
                m_geoInfo.originY = pTie[4] - pTie[1] * m_geoInfo.pixelSizeY;
            }

            m_geoInfo.bGeoreferenced = true;
            m_geoInfo.sSource = "geotiff";

            // GeoKey directory: header {version, revision, minor, numKeys}, then {keyId, tagLocation, count, value}
            uint32_t keyCount = 0;
            uint16_t* pKeys = nullptr;
            if(TIFFGetField(m_pTif, kTagGeoKeyDirectory, &keyCount, &pKeys) && keyCount >= 4 && pKeys)
            {
                uint16_t modelType = 0;
                uint16_t rasterType = 0;
                uint16_t geographicType = 0;
                uint16_t projectedType = 0;

                uint32_t numKeys = std::min<uint32_t>(pKeys[3], (keyCount - 4) / 4);
                for(uint32_t i = 0; i < numKeys; ++i)
                {
                    const uint16_t* pEntry = pKeys + 4 + i * 4;
                    if(pEntry[1] != 0) // value is stored in another tag - not needed for the keys below
                        continue;
                    switch(pEntry[0])
                    {
                        case kKeyGTModelType: modelType = pEntry[3]; break;
                        case kKeyGTRasterType: rasterType = pEntry[3]; break;
                        case kKeyGeographicType: geographicType = pEntry[3]; break;
                        case kKeyProjectedCSType: projectedType = pEntry[3]; break;
                        default: break;
                    }
                }

                auto isCode = [](uint16_t code) { return code != 0 && code != kUserDefined; };
                if(modelType == kModelTypeGeographic && isCode(geographicType))
                    m_geoInfo.epsgCode = geographicType;
                else if(isCode(projectedType))
                    m_geoInfo.epsgCode = projectedType;
                else if(isCode(geographicType))
                    m_geoInfo.epsgCode = geographicType;

                // PixelIsPoint: the tie point refers to the pixel center
                if(rasterType == kRasterPixelIsPoint && !bTransform)
                {
                    m_geoInfo.originX -= m_geoInfo.pixelSizeX * 0.5;
                    m_geoInfo.originY -= m_geoInfo.pixelSizeY * 0.5;
                }
                (void)kModelTypeProjected;
            }

            return true;
        }

        bool CTIFFReader::ReadWorldFile()
        {
            std::filesystem::path path = ToPath(m_sFilePath);
            std::filesystem::path base = path;
            base.replace_extension();

            std::vector<std::filesystem::path> candidates;
            candidates.push_back(std::filesystem::path(base).concat(".tfw"));
            candidates.push_back(std::filesystem::path(base).concat(".TFW"));
            candidates.push_back(std::filesystem::path(path).concat("w"));   // .tifw
            candidates.push_back(std::filesystem::path(base).concat(".tifw"));
            candidates.push_back(std::filesystem::path(base).concat(".wld"));
            candidates.push_back(std::filesystem::path(base).concat(".WLD"));

            for(const std::filesystem::path& candidate : candidates)
            {
                std::error_code ec;
                if(!std::filesystem::is_regular_file(candidate, ec))
                    continue;

                std::ifstream file(candidate);
                double v[6];
                int n = 0;
                while(n < 6 && (file >> v[n]))
                    ++n;
                if(n < 6 || v[0] == 0. || v[3] == 0.)
                    continue;

                // A (x size), D (rotation), B (rotation), E (y size), C, F (center of the upper-left pixel)
                m_geoInfo.pixelSizeX = v[0];
                m_geoInfo.pixelSizeY = v[3];
                m_geoInfo.originX = v[4] - v[0] * 0.5;
                m_geoInfo.originY = v[5] - v[3] * 0.5;
                m_geoInfo.bGeoreferenced = true;
                m_geoInfo.sSource = "worldfile";
                return true;
            }

            return false;
        }

        int CTIFFReader::GetWidth() const
        {
            return (int)m_width;
        }

        int CTIFFReader::GetHeight() const
        {
            return (int)m_height;
        }

        int CTIFFReader::GetBandCount() const
        {
            return m_bandCount;
        }

        eRasterPixelType CTIFFReader::GetPixelType() const
        {
            return m_pixelType;
        }

        int CTIFFReader::GetSourceBandCount() const
        {
            return m_samplesPerPixel;
        }

        eRasterPixelType CTIFFReader::GetSourcePixelType() const
        {
            return m_sourcePixelType;
        }

        const SRasterGeoInfo& CTIFFReader::GetGeoInfo() const
        {
            return m_geoInfo;
        }

        const uint8_t* CTIFFReader::GetChunk(uint32_t nChunk, uint32_t chunkX0, uint32_t chunkY0)
        {
            TChunkMap::iterator it = m_chunks.find(nChunk);
            if(it != m_chunks.end())
                return it->second.data();

            while(!m_chunkOrder.empty() && m_cacheBytes + m_chunkBytes > m_maxCacheBytes)
            {
                TChunkMap::iterator old = m_chunks.find(m_chunkOrder.front());
                if(old != m_chunks.end())
                {
                    m_cacheBytes -= old->second.size();
                    m_chunks.erase(old);
                }
                m_chunkOrder.pop_front();
            }

            std::vector<uint8_t> data(m_chunkBytes, 0);
            m_sLastError.clear();

            if(m_readMode == rmRGBA)
            {
                int ok = m_bTiled ? TIFFReadRGBATile(m_pTif, chunkX0, chunkY0, reinterpret_cast<uint32_t*>(data.data()))
                                  : TIFFReadRGBAStrip(m_pTif, chunkY0, reinterpret_cast<uint32_t*>(data.data()));
                if(!ok)
                    ThrowError("Failed to read TIFF data");
            }
            else
            {
                tmsize_t res = m_bTiled ? TIFFReadEncodedTile(m_pTif, nChunk, data.data(), (tmsize_t)data.size())
                                        : TIFFReadEncodedStrip(m_pTif, nChunk, data.data(), (tmsize_t)data.size());
                if(res < 0)
                    ThrowError("Failed to read TIFF data");
            }

            m_cacheBytes += data.size();
            m_chunkOrder.push_back(nChunk);
            std::vector<uint8_t>& stored = m_chunks[nChunk];
            stored.swap(data);
            return stored.data();
        }

        void CTIFFReader::ReadWindow(int col, int row, int width, int height, int step, void* pDst)
        {
            if(!m_pTif)
                throw CommonLib::CExcBase("TIFF reader is not opened");
            if(step < 1)
                step = 1;
            if(width <= 0 || height <= 0)
                return;
            if(col < 0 || row < 0 || int64_t(col) + int64_t(width - 1) * step >= int64_t(m_width)
               || int64_t(row) + int64_t(height - 1) * step >= int64_t(m_height))
                throw CommonLib::CExcBase("TIFF read window is out of the image, file: {0}", m_sFilePath);

            const int outSampleBytes = CRasterBlock::GetSampleSize(m_pixelType);
            const size_t outPixelBytes = size_t(outSampleBytes) * m_bandCount;
            const bool bSeparate = m_planarConfig == PLANARCONFIG_SEPARATE && m_readMode != rmRGBA;
            const int planes = bSeparate ? m_samplesPerPixel : 1;
            const int samplesInChunk = bSeparate ? 1 : m_samplesPerPixel;
            const int srcSampleBytes = m_bitsPerSample >= 8 ? m_bitsPerSample / 8 : 1;
            const uint32_t bitMask = m_bitsPerSample < 8 ? (1u << m_bitsPerSample) - 1 : 0;
            const bool bBottomUp = m_orientation == ORIENTATION_BOTLEFT || m_orientation == ORIENTATION_BOTRIGHT;

            uint8_t* pOutRows = static_cast<uint8_t*>(pDst);

            for(int r = 0; r < height; ++r)
            {
                const uint32_t y = uint32_t(row + r * step);
                const uint32_t fileY = bBottomUp ? m_height - 1 - y : y;
                uint8_t* pOutRow = pOutRows + size_t(r) * width * outPixelBytes;

                for(int plane = 0; plane < planes; ++plane)
                {
                    const uint8_t* pChunk = nullptr;
                    const uint8_t* pRow = nullptr;
                    uint32_t chunkX0 = 0;
                    uint32_t chunkY0 = 0;
                    bool bHasChunk = false;

                    for(int c = 0; c < width; ++c)
                    {
                        const uint32_t x = uint32_t(col + c * step);
                        if(!bHasChunk || x < chunkX0 || x >= chunkX0 + m_chunkWidth)
                        {
                            uint32_t nChunk = m_bTiled ? TIFFComputeTile(m_pTif, x, fileY, 0, (uint16_t)plane)
                                                       : TIFFComputeStrip(m_pTif, fileY, (uint16_t)plane);
                            chunkX0 = m_bTiled ? (x / m_chunkWidth) * m_chunkWidth : 0;
                            chunkY0 = (fileY / m_chunkHeight) * m_chunkHeight;
                            pChunk = GetChunk(nChunk, chunkX0, chunkY0);

                            uint32_t rowInChunk = fileY - chunkY0;
                            if(m_readMode == rmRGBA)
                            {
                                // RGBA buffers are bottom-up inside the strip/tile
                                uint32_t rowsInChunk = m_bTiled ? m_chunkHeight : (std::min)(m_chunkHeight, m_height - chunkY0);
                                if(!bBottomUp)
                                    rowInChunk = rowsInChunk - 1 - rowInChunk;
                            }
                            pRow = pChunk + size_t(rowInChunk) * m_chunkRowBytes;
                            bHasChunk = true;
                        }

                        uint8_t* pOut = pOutRow + size_t(c) * outPixelBytes;
                        const uint32_t xInChunk = x - chunkX0;

                        switch(m_readMode)
                        {
                            case rmRGBA:
                            {
                                uint32_t abgr = reinterpret_cast<const uint32_t*>(pRow)[xInChunk];
                                pOut[0] = (uint8_t)TIFFGetR(abgr);
                                pOut[1] = (uint8_t)TIFFGetG(abgr);
                                pOut[2] = (uint8_t)TIFFGetB(abgr);
                                pOut[3] = (uint8_t)TIFFGetA(abgr);
                                break;
                            }
                            case rmPalette:
                            {
                                uint32_t index;
                                if(m_bitsPerSample == 8)
                                    index = pRow[xInChunk];
                                else if(m_bitsPerSample == 16)
                                {
                                    uint16_t v;
                                    memcpy(&v, pRow + size_t(xInChunk) * 2, 2);
                                    index = v;
                                }
                                else
                                {
                                    size_t bitPos = size_t(xInChunk) * m_bitsPerSample;
                                    index = (pRow[bitPos >> 3] >> (8 - m_bitsPerSample - (bitPos & 7))) & bitMask;
                                }
                                memcpy(pOut, &m_palette[size_t(index) * 3], 3);
                                break;
                            }
                            case rmNative:
                            {
                                if(m_bitsPerSample >= 8)
                                {
                                    const uint8_t* pSrc = pRow + size_t(xInChunk) * samplesInChunk * srcSampleBytes;
                                    if(bSeparate)
                                        memcpy(pOut + size_t(plane) * srcSampleBytes, pSrc, srcSampleBytes);
                                    else
                                        memcpy(pOut, pSrc, outPixelBytes);
                                }
                                else
                                {
                                    for(int s = 0; s < samplesInChunk; ++s)
                                    {
                                        size_t bitPos = (size_t(xInChunk) * samplesInChunk + s) * m_bitsPerSample;
                                        uint8_t v = uint8_t((pRow[bitPos >> 3] >> (8 - m_bitsPerSample - (bitPos & 7))) & bitMask);
                                        pOut[bSeparate ? plane : s] = v;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}
