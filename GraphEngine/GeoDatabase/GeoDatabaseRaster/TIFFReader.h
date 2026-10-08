#pragma once

#include "RasterReader.h"

#include <cstdarg>
#include <deque>
#include <unordered_map>

typedef struct tiff TIFF;

namespace GraphEngine
{
    namespace GeoDatabase {

        // TIFF / GeoTIFF reader on top of libtiff (ThirdParty/tiff).
        // - strips and tiles, chunky (contig) and planar (separate) layouts
        // - 1/2/4 bit samples are unpacked to one byte per sample (values 0..2^bits-1)
        // - 8/16/32/64 bit unsigned/signed/float samples are returned as stored (host byte order)
        // - paletted images are expanded to RGB (3 x uint8)
        // - JPEG-in-TIFF YCbCr is decoded to RGB by libjpeg
        // - anything else libtiff can decode only through its RGBA interface (non-JPEG YCbCr, LogLuv, CIELab,
        //   unusual bit depths) is returned as RGBA (4 x uint8)
        // - geo-referencing: GeoTIFF tags (ModelTransformation, or ModelPixelScale + ModelTiepoint,
        //   PixelIsPoint aware), EPSG code from the GeoKey directory; world file (.tfw/.tifw/.wld) as fallback
        class CTIFFReader : public IRasterReader
        {
        public:
            CTIFFReader();
            virtual ~CTIFFReader();

            CTIFFReader(const CTIFFReader&) = delete;
            CTIFFReader& operator=(const CTIFFReader&) = delete;

            virtual void Open(const std::string& sFilePath);
            virtual IRasterReaderPtr Clone() const;

            virtual int GetWidth() const;
            virtual int GetHeight() const;
            virtual int GetBandCount() const;
            virtual eRasterPixelType GetPixelType() const;
            virtual int GetSourceBandCount() const;
            virtual eRasterPixelType GetSourcePixelType() const;
            virtual const SRasterGeoInfo& GetGeoInfo() const;

            virtual void ReadWindow(int col, int row, int width, int height, int step, void* pDst);

            uint16_t GetPhotometric() const {return m_photometric;}
            uint16_t GetCompression() const {return m_compression;}
            bool IsTiled() const {return m_bTiled;}

            static bool IsTIFFFile(const std::string& sFilePath);

        private:
            enum eReadMode
            {
                rmNative,   // samples as stored
                rmPalette,  // index -> RGB
                rmRGBA      // libtiff RGBA interface
            };

            void Close();
            void Parse();
            void ReadGeoInfo();
            bool ReadGeoTiffTags();
            bool ReadWorldFile();
            const uint8_t* GetChunk(uint32_t nChunk, uint32_t chunkX0, uint32_t chunkY0);
            void ThrowError(const std::string& sMsg) const;

            static int ErrorHandler(TIFF* tif, void* pUserData, const char* module, const char* fmt, va_list ap);
            static int WarningHandler(TIFF* tif, void* pUserData, const char* module, const char* fmt, va_list ap);

        private:
            TIFF* m_pTif;
            std::string m_sFilePath;
            std::string m_sLastError;

            uint32_t m_width;
            uint32_t m_height;
            uint16_t m_bitsPerSample;
            uint16_t m_samplesPerPixel;
            uint16_t m_photometric;
            uint16_t m_planarConfig;
            uint16_t m_sampleFormat;
            uint16_t m_orientation;
            uint16_t m_compression;
            bool m_bTiled;

            uint32_t m_chunkWidth;      // tile width or image width
            uint32_t m_chunkHeight;     // tile length or rows per strip
            size_t m_chunkRowBytes;     // bytes of one decoded row of a chunk
            size_t m_chunkBytes;

            eReadMode m_readMode;
            eRasterPixelType m_sourcePixelType;
            eRasterPixelType m_pixelType;
            int m_bandCount;
            std::vector<uint8_t> m_palette;  // RGB triples

            SRasterGeoInfo m_geoInfo;

            // decoded chunk cache
            typedef std::unordered_map<uint32_t, std::vector<uint8_t> > TChunkMap;
            TChunkMap m_chunks;
            std::deque<uint32_t> m_chunkOrder;
            size_t m_cacheBytes;
            size_t m_maxCacheBytes;
        };
    }
}
