#include "TIFFReader.h"
#include "RasterBlock.h"
#include "../../CommonLib/filesystem/File.h"

extern "C" {
#include "tiffio.h"
}

#include <algorithm>
#include <cstdio>
#include <climits>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <sstream>
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

            const size_t kBaseCacheBytes = 32 * 1024 * 1024;
            const size_t kMaxCacheBytes = 256 * 1024 * 1024;

            // JPEG-in-TIFF YCbCr: libjpeg converts to RGB (also makes libtiff report RGB-sized strips/tiles);
            // a pseudo tag, it is set again for every handle / directory
            void SetJpegColorMode(const TIFFLib::TIFFPtr& ptrTif)
            {
                uint16_t compression = COMPRESSION_NONE, photometric = 0, planar = PLANARCONFIG_CONTIG;
                ptrTif->GetFieldDefaulted(TIFFTAG_COMPRESSION, compression);
                ptrTif->GetFieldDefaulted(TIFFTAG_PLANARCONFIG, planar);
                if(compression == COMPRESSION_JPEG && ptrTif->GetField(TIFFTAG_PHOTOMETRIC, photometric) &&
                   photometric == PHOTOMETRIC_YCBCR && planar == PLANARCONFIG_CONTIG)
                    ptrTif->SetFieldInt(TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
            }

            // the text of a small file (a world file) through the CommonLib file, false - no file
            bool ReadTextFile(const std::string& sPathUtf8, std::string& sText)
            {
                try
                {
                    CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFileA(sPathUtf8.c_str(), CommonLib::file::ofmOpenExisting,
                                                                                                CommonLib::file::arRead, CommonLib::file::smRead,
                                                                                                CommonLib::file::oftBinary);
                    uint64_t nSize = ptrFile->GetFileSize();
                    if(nSize > 64 * 1024)
                        return false;
                    sText.assign((size_t)nSize, '\0');
                    size_t nDone = 0;
                    while(nDone < sText.size())
                    {
                        std::streamsize n = ptrFile->Read((byte_t*)&sText[nDone], sText.size() - nDone);
                        if(n <= 0)
                            break;
                        nDone += (size_t)n;
                    }
                    sText.resize(nDone);
                    ptrFile->CloseFile();
                    return true;
                }
                catch (std::exception&)
                {
                    return false;
                }
            }

            std::filesystem::path ToPath(const std::string& sUtf8)
            {
                return std::filesystem::u8path(sUtf8);
            }
        }

        CTIFFReader::CTIFFReader() :
            m_width(0)
            , m_height(0)
            , m_bitsPerSample(0)
            , m_samplesPerPixel(0)
            , m_photometric(0)
            , m_planarConfig(PLANARCONFIG_CONTIG)
            , m_sampleFormat(SAMPLEFORMAT_UINT)
            , m_orientation(ORIENTATION_TOPLEFT)
            , m_compression(COMPRESSION_NONE)
            , m_readMode(rmNative)
            , m_sourcePixelType(RasterPixelTypeUnknown)
            , m_pixelType(RasterPixelTypeUnknown)
            , m_bandCount(0)
            , m_cacheBytes(0)
            , m_maxCacheBytes(kBaseCacheBytes)
            , m_nDecodedChunks(0)
            , m_bNoData(false)
            , m_dNoData(0.)
        {

        }

        CTIFFReader::~CTIFFReader()
        {
            Close();
        }

        void CTIFFReader::Close()
        {
            m_levels.clear();       // the overview handles are closed by their CTIFF
            m_ptrTif.reset();
            m_chunks.clear();
            m_chunkOrder.clear();
            m_cacheBytes = 0;
        }

        void CTIFFReader::ThrowError(const std::string& sMsg) const
        {
            if(m_ptrTif.get())
                m_ptrTif->ThrowError(sMsg);
            throw CommonLib::CExcBase("{0}, file: {1}", sMsg, m_sFilePath);
        }

        bool CTIFFReader::IsTIFFFile(const std::string& sFilePath)
        {
            return TIFFLib::CTIFF::IsTIFFFile(sFilePath);
        }

        void CTIFFReader::Open(const std::string& sFilePath)
        {
            Close();
            m_sFilePath = sFilePath;
            m_ptrTif = OpenHandle();    // throws

            Parse();
            ScanOverviews();
            ReadGeoInfo();

            // GDAL_NODATA: the value as text ("-32767", "nan")
            m_bNoData = false;
            std::string sNoData;
            if(m_ptrTif->GetField(TIFFTAG_GDAL_NODATA, sNoData) && !sNoData.empty())
            {
                char* pszEnd = nullptr;
                double dValue = strtod(sNoData.c_str(), &pszEnd);
                if(pszEnd != sNoData.c_str())
                {
                    m_dNoData = dValue;
                    m_bNoData = true;
                }
            }
        }

        bool CTIFFReader::GetNoDataValue(double& dValue) const
        {
            if(!m_bNoData)
                return false;
            dValue = m_dNoData;
            return true;
        }

        TIFFLib::TIFFPtr CTIFFReader::OpenHandle() const
        {
            // "O": the offsets / sizes of the strips and tiles are read when they are needed, not the whole arrays
            // (379 220 tiles of a 32 GB DEM: 6 MB per directory)
            return TIFFLib::CTIFF::Open(m_sFilePath, "rO");
        }

        void CTIFFReader::InitLevel(const TIFFLib::TIFFPtr& ptrTif, SLevel& level)
        {
            SetJpegColorMode(ptrTif);
            level.bTiled = ptrTif->IsTiled();
            if(level.bTiled)
            {
                if(!ptrTif->GetField(TIFFTAG_TILEWIDTH, level.chunkWidth) || !ptrTif->GetField(TIFFTAG_TILELENGTH, level.chunkHeight)
                   || level.chunkWidth == 0 || level.chunkHeight == 0)
                    ThrowError("Invalid TIFF tile size");
                level.chunkRowBytes = (size_t)ptrTif->GetTileRowSize();
                level.chunkBytes = (size_t)ptrTif->GetTileSize();
            }
            else
            {
                uint32_t rowsPerStrip = 0;
                ptrTif->GetFieldDefaulted(TIFFTAG_ROWSPERSTRIP, rowsPerStrip);
                level.chunkWidth = level.width;
                level.chunkHeight = std::max<uint32_t>(1, (std::min)(rowsPerStrip, level.height));
                level.chunkRowBytes = (size_t)ptrTif->GetScanlineSize();
                level.chunkBytes = (size_t)ptrTif->GetVStripSize(level.chunkHeight);
            }

            if(m_readMode == rmRGBA)
            {
                level.chunkRowBytes = size_t(level.chunkWidth) * 4;
                level.chunkBytes = level.chunkRowBytes * level.chunkHeight;
            }

            if(level.chunkBytes == 0 || level.chunkRowBytes == 0)
                ThrowError("Invalid TIFF strip/tile size");
        }

        void CTIFFReader::ScanOverviews()
        {
            // the samples are delivered as stored only: an overview must have the same layout as the full image
            if(m_readMode != rmNative)
                return;

            try
            {
                TIFFLib::TIFFPtr ptrScan = OpenHandle();
                uint16_t photometric = 0;
                ptrScan->GetFieldDefaulted(TIFFTAG_PHOTOMETRIC, photometric);
                const uint32_t nDirectories = ptrScan->GetDirectoryCount();
                for(uint32_t d = 1; d < nDirectories && d < 64; ++d)
                {
                    ptrScan->SetDirectory(d);

                    // a reduced resolution image, not a mask
                    uint32_t subfileType = 0;
                    ptrScan->GetFieldDefaulted(TIFFTAG_SUBFILETYPE, subfileType);
                    if(!(subfileType & FILETYPE_REDUCEDIMAGE) || (subfileType & FILETYPE_MASK))
                        continue;

                    SLevel level;
                    uint16_t bits = 0, samples = 0, format = 0, planar = 0, levelPhotometric = 0;
                    if(!ptrScan->GetField(TIFFTAG_IMAGEWIDTH, level.width) || !ptrScan->GetField(TIFFTAG_IMAGELENGTH, level.height))
                        continue;
                    ptrScan->GetFieldDefaulted(TIFFTAG_BITSPERSAMPLE, bits);
                    ptrScan->GetFieldDefaulted(TIFFTAG_SAMPLESPERPIXEL, samples);
                    ptrScan->GetFieldDefaulted(TIFFTAG_SAMPLEFORMAT, format);
                    ptrScan->GetFieldDefaulted(TIFFTAG_PLANARCONFIG, planar);
                    ptrScan->GetFieldDefaulted(TIFFTAG_PHOTOMETRIC, levelPhotometric);
                    if(bits != m_bitsPerSample || samples != m_samplesPerPixel || format != m_sampleFormat || planar != m_planarConfig ||
                       levelPhotometric != photometric)
                        continue;
                    if(level.width == 0 || level.height == 0 || level.width >= m_width || level.height >= m_height)
                        continue;

                    level.nDirectory = (uint16_t)d;
                    InitLevel(ptrScan, level);
                    m_levels.push_back(level);
                }
            }
            catch (std::exception&)
            {
                // a damaged overview: the full resolution image is still read
                m_levels.resize(1);
            }

            // from the biggest one
            std::sort(m_levels.begin() + 1, m_levels.end(), [](const SLevel& a, const SLevel& b) { return a.width > b.width; });
        }

        TIFFLib::TIFFPtr CTIFFReader::LevelHandle(int nLevel)
        {
            SLevel& level = m_levels[nLevel];
            if(nLevel == 0)
                return m_ptrTif;
            if(!level.ptrTif.get())
            {
                TIFFLib::TIFFPtr ptrTif = OpenHandle();
                ptrTif->SetDirectory(level.nDirectory);
                SetJpegColorMode(ptrTif);
                level.ptrTif = ptrTif;
            }
            return level.ptrTif;
        }

        int CTIFFReader::SelectLevel(int step) const
        {
            // the smallest overview with a pixel not bigger than the step (1% for the rounding of the overview sizes)
            int nBest = 0;
            for(size_t i = 1; i < m_levels.size(); ++i)
            {
                double factor = (std::min)(double(m_width) / m_levels[i].width, double(m_height) / m_levels[i].height);
                if(factor <= step * 1.01)
                    nBest = (int)i;
            }
            return nBest;
        }

        IRasterReaderPtr CTIFFReader::Clone() const
        {
            std::shared_ptr<CTIFFReader> ptrReader = std::make_shared<CTIFFReader>();
            ptrReader->Open(m_sFilePath);
            return ptrReader;
        }

        void CTIFFReader::Parse()
        {
            if(!m_ptrTif->GetField(TIFFTAG_IMAGEWIDTH, m_width) || !m_ptrTif->GetField(TIFFTAG_IMAGELENGTH, m_height))
                ThrowError("TIFF has no image size");
            if(m_width == 0 || m_height == 0 || m_width > INT32_MAX || m_height > INT32_MAX)
                ThrowError("Unsupported TIFF image size");

            m_ptrTif->GetFieldDefaulted(TIFFTAG_BITSPERSAMPLE, m_bitsPerSample);
            m_ptrTif->GetFieldDefaulted(TIFFTAG_SAMPLESPERPIXEL, m_samplesPerPixel);
            m_ptrTif->GetFieldDefaulted(TIFFTAG_PLANARCONFIG, m_planarConfig);
            m_ptrTif->GetFieldDefaulted(TIFFTAG_SAMPLEFORMAT, m_sampleFormat);
            m_ptrTif->GetFieldDefaulted(TIFFTAG_ORIENTATION, m_orientation);
            m_ptrTif->GetFieldDefaulted(TIFFTAG_COMPRESSION, m_compression);
            if(!m_ptrTif->GetField(TIFFTAG_PHOTOMETRIC, m_photometric))
                m_photometric = m_samplesPerPixel >= 3 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK;
            if(m_samplesPerPixel == 0)
                ThrowError("TIFF has no samples");

            // JPEG-in-TIFF YCbCr: let libjpeg convert to RGB (also makes libtiff report RGB-sized strips/tiles)
            if(m_compression == COMPRESSION_JPEG && m_photometric == PHOTOMETRIC_YCBCR && m_planarConfig == PLANARCONFIG_CONTIG)
            {
                SetJpegColorMode(m_ptrTif);
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
                const uint16_t* pRed = nullptr;
                const uint16_t* pGreen = nullptr;
                const uint16_t* pBlue = nullptr;
                if(!m_ptrTif->GetColorMap(pRed, pGreen, pBlue))
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
                std::string sMsg;
                if(!m_ptrTif->IsRGBAImageOK(sMsg))
                    throw CommonLib::CExcBase("Unsupported TIFF format, file: {0}, {1}", m_sFilePath, sMsg);

                m_readMode = rmRGBA;
                m_pixelType = RasterPixelTypeUChar;
                m_bandCount = 4;
            }

            // chunk (strip/tile) geometry of the full resolution image
            SLevel level;
            level.ptrTif = m_ptrTif;
            level.width = m_width;
            level.height = m_height;
            InitLevel(m_ptrTif, level);
            m_levels.assign(1, level);
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
            std::vector<double> transform;
            bool bTransform = false;
            if(m_ptrTif->GetField(kTagModelTransformation, transform) && transform.size() >= 16)
            {
                const double* pData = transform.data();
                // [a b 0 d; e f 0 h; ...]  X = a*col + b*row + d, Y = e*col + f*row + h (rotation terms b, e ignored)
                m_geoInfo.pixelSizeX = pData[0];
                m_geoInfo.pixelSizeY = pData[5];
                m_geoInfo.originX = pData[3];
                m_geoInfo.originY = pData[7];
                bTransform = m_geoInfo.pixelSizeX != 0. && m_geoInfo.pixelSizeY != 0.;
            }

            if(!bTransform)
            {
                std::vector<double> scale, tie;
                if(!m_ptrTif->GetField(kTagModelPixelScale, scale) || scale.size() < 2)
                    return false;
                if(!m_ptrTif->GetField(kTagModelTiepoint, tie) || tie.size() < 6)
                    return false;
                const double* pScale = scale.data();
                const double* pTie = tie.data();
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
            std::vector<uint16_t> keys;
            if(m_ptrTif->GetField(kTagGeoKeyDirectory, keys) && keys.size() >= 4)
            {
                const uint32_t keyCount = (uint32_t)keys.size();
                const uint16_t* pKeys = keys.data();
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

                std::string sText;
                if(!ReadTextFile(candidate.u8string(), sText))
                    continue;
                std::istringstream file(sText);
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

        const uint8_t* CTIFFReader::GetChunk(int nLevel, uint32_t nChunk, uint32_t chunkX0, uint32_t chunkY0)
        {
            const uint64_t nKey = (uint64_t(nLevel) << 32) | nChunk;
            TChunkMap::iterator it = m_chunks.find(nKey);
            if(it != m_chunks.end())
                return it->second.data();

            const SLevel& level = m_levels[nLevel];
            while(!m_chunkOrder.empty() && m_cacheBytes + level.chunkBytes > m_maxCacheBytes)
            {
                TChunkMap::iterator old = m_chunks.find(m_chunkOrder.front());
                if(old != m_chunks.end())
                {
                    m_cacheBytes -= old->second.size();
                    m_chunks.erase(old);
                }
                m_chunkOrder.pop_front();
            }

            TIFFLib::TIFFPtr ptrTif = LevelHandle(nLevel);
            std::vector<uint8_t> data(level.chunkBytes, 0);

            if(m_readMode == rmRGBA)
            {
                if(level.bTiled)
                    ptrTif->ReadRGBATile(chunkX0, chunkY0, reinterpret_cast<uint32_t*>(data.data()));
                else
                    ptrTif->ReadRGBAStrip(chunkY0, reinterpret_cast<uint32_t*>(data.data()));
            }
            else if(level.bTiled)
                ptrTif->ReadEncodedTile(nChunk, data.data(), data.size());
            else
                ptrTif->ReadEncodedStrip(nChunk, data.data(), data.size());
            ++m_nDecodedChunks;

            m_cacheBytes += data.size();
            m_chunkOrder.push_back(nKey);
            std::vector<uint8_t>& stored = m_chunks[nKey];
            stored.swap(data);
            return stored.data();
        }

        void CTIFFReader::ReadWindow(int col, int row, int width, int height, int step, void* pDst)
        {
            if(!m_ptrTif.get())
                throw CommonLib::CExcBase("TIFF reader is not opened");
            if(step < 1)
                step = 1;
            if(width <= 0 || height <= 0)
                return;
            if(col < 0 || row < 0 || int64_t(col) + int64_t(width - 1) * step >= int64_t(m_width)
               || int64_t(row) + int64_t(height - 1) * step >= int64_t(m_height))
                throw CommonLib::CExcBase("TIFF read window is out of the image, file: {0}", m_sFilePath);

            // the overview for the step: the pixels are taken from it by the position of their centers
            const int nLevel = SelectLevel(step);
            const SLevel& level = m_levels[nLevel];
            TIFFLib::TIFFPtr ptrTif = LevelHandle(nLevel);
            auto toLevel = [](uint32_t v, uint32_t full, uint32_t reduced) -> uint32_t
            {
                if(reduced == full)
                    return v;
                uint64_t lv = (uint64_t(v) * 2 + 1) * reduced / (uint64_t(full) * 2);
                return (uint32_t)(std::min)(lv, uint64_t(reduced - 1));
            };
            m_vecColumns.resize(size_t(width));
            for(int c = 0; c < width; ++c)
                m_vecColumns[c] = toLevel(uint32_t(col + c * step), m_width, level.width);

            // the chunks of one output row stay in the cache for the next rows
            const bool bSeparateLayout = m_planarConfig == PLANARCONFIG_SEPARATE && m_readMode != rmRGBA;
            uint64_t nRowChunks = uint64_t(m_vecColumns.back() - m_vecColumns.front()) / level.chunkWidth + 2;
            uint64_t nNeeded = nRowChunks * level.chunkBytes * (bSeparateLayout ? m_samplesPerPixel : 1) * 2;
            m_maxCacheBytes = (size_t)(std::max)((uint64_t)kBaseCacheBytes, (std::min)(nNeeded, (uint64_t)kMaxCacheBytes));

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
                const uint32_t y = toLevel(uint32_t(row + r * step), m_height, level.height);
                const uint32_t fileY = bBottomUp ? level.height - 1 - y : y;
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
                        const uint32_t x = m_vecColumns[c];
                        if(!bHasChunk || x < chunkX0 || x >= chunkX0 + level.chunkWidth)
                        {
                            uint32_t nChunk = level.bTiled ? ptrTif->ComputeTile(x, fileY, 0, (uint16_t)plane)
                                                           : ptrTif->ComputeStrip(fileY, (uint16_t)plane);
                            chunkX0 = level.bTiled ? (x / level.chunkWidth) * level.chunkWidth : 0;
                            chunkY0 = (fileY / level.chunkHeight) * level.chunkHeight;
                            pChunk = GetChunk(nLevel, nChunk, chunkX0, chunkY0);

                            uint32_t rowInChunk = fileY - chunkY0;
                            if(m_readMode == rmRGBA)
                            {
                                // RGBA buffers are bottom-up inside the strip/tile
                                uint32_t rowsInChunk = level.bTiled ? level.chunkHeight : (std::min)(level.chunkHeight, level.height - chunkY0);
                                if(!bBottomUp)
                                    rowInChunk = rowsInChunk - 1 - rowInChunk;
                            }
                            pRow = pChunk + size_t(rowInChunk) * level.chunkRowBytes;
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
