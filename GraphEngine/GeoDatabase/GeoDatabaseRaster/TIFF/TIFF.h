#pragma once

#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/filesystem/File.h"

#include <cstdarg>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

typedef struct tiff TIFF;

namespace GraphEngine {
    namespace GeoDatabase {
        namespace TIFFLib {

            class CTIFF;
            typedef std::shared_ptr<CTIFF> TIFFPtr;

            // C++ wrapper of a libtiff handle (struct tiff):
            //  - the file is read / written through the CommonLib file (IFile, 64-bit positions, UTF-8 paths) with the
            //    libtiff client procedures, the handle is closed by the destructor;
            //  - the libtiff errors of the handle are kept (GetLastError) and the failures throw CExcBase with them;
            //  - the GeoTIFF and GDAL tags are registered (they are read as typed fields, not as anonymous ones).
            // Not thread safe: one handle per thread (a reader per cursor).
            class CTIFF
            {
            public:
                // sMode - libtiff mode: "r", "rO" (the strip / tile offsets are loaded on demand: big files), "w", "w8" (BigTIFF)
                static TIFFPtr Open(const std::string& sFilePathUtf8, const char* pszMode = "rO");
                static TIFFPtr Create(const std::string& sFilePathUtf8, bool bBigTIFF = false);
                // the file starts with a TIFF / BigTIFF header (little and big endian)
                static bool IsTIFFFile(const std::string& sFilePathUtf8);

                ~CTIFF();
                CTIFF(const CTIFF&) = delete;
                CTIFF& operator=(const CTIFF&) = delete;

                void Close();
                bool IsOpen() const { return m_pTif != nullptr; }
                const std::string& GetFilePath() const { return m_sFilePath; }
                // the last libtiff error message of the handle, empty - none
                const std::string& GetLastTiffError() const { return m_sLastError; }
                void ClearLastError() { m_sLastError.clear(); }
                // throws "sMsg, file: ..., libtiff: <the last error>"
                [[noreturn]] void ThrowError(const std::string& sMsg) const;

                // fields (TIFFGetField / TIFFGetFieldDefaulted), false - the field isn't set
                bool GetField(uint32_t tag, uint16_t& value) const;
                bool GetField(uint32_t tag, uint32_t& value) const;
                bool GetField(uint32_t tag, std::string& value) const;            // ASCII
                bool GetField(uint32_t tag, std::vector<double>& values) const;   // a counted array of doubles
                bool GetField(uint32_t tag, std::vector<uint16_t>& values) const; // a counted array of shorts
                bool GetFieldDefaulted(uint32_t tag, uint16_t& value) const;
                bool GetFieldDefaulted(uint32_t tag, uint32_t& value) const;
                // the color map: 2^bitsPerSample entries per channel
                bool GetColorMap(const uint16_t*& pRed, const uint16_t*& pGreen, const uint16_t*& pBlue) const;

                bool SetField(uint32_t tag, uint16_t value);
                bool SetField(uint32_t tag, uint32_t value);
                bool SetFieldInt(uint32_t tag, int value);      // pseudo tags (JPEGCOLORMODE, JPEGQUALITY)
                bool SetField(uint32_t tag, const std::string& value);
                bool SetField(uint32_t tag, const std::vector<double>& values);
                bool SetField(uint32_t tag, const std::vector<uint16_t>& values);

                // directories (images): 0 - the first one
                uint32_t GetDirectoryCount() const;
                uint32_t GetCurrentDirectoryIndex() const;
                void     SetDirectory(uint32_t nDirectory);    // throws
                void     WriteDirectory();                     // writing: finishes the current image, throws

                // strips / tiles of the current directory
                bool     IsTiled() const;
                uint64_t GetTileSize() const;
                uint64_t GetTileRowSize() const;
                uint64_t GetScanlineSize() const;
                uint64_t GetVStripSize(uint32_t nRows) const;
                uint32_t ComputeTile(uint32_t x, uint32_t y, uint32_t z, uint16_t sample) const;
                uint32_t ComputeStrip(uint32_t row, uint16_t sample) const;

                // decoding, throw on an error
                void ReadEncodedTile(uint32_t nTile, void* pBuffer, size_t nSize);
                void ReadEncodedStrip(uint32_t nStrip, void* pBuffer, size_t nSize);
                // ABGR pixels (TIFFGetR ...), bottom-up rows
                void ReadRGBATile(uint32_t x, uint32_t y, uint32_t* pBuffer);
                void ReadRGBAStrip(uint32_t row, uint32_t* pBuffer);
                // the image can be decoded by the RGBA interface, sMessage - why not
                bool IsRGBAImageOK(std::string& sMessage) const;

                // encoding (writing)
                void WriteTile(const void* pBuffer, uint32_t x, uint32_t y, uint32_t z, uint16_t sample);
                void WriteScanline(const void* pBuffer, uint32_t row, uint16_t sample);

                // the GeoTIFF / GDAL tags, called by Open / Create (once for the process)
                static void RegisterTags();

            private:
                CTIFF();
                void OpenHandle(const std::string& sFilePathUtf8, const char* pszMode, bool bWrite);

                // the libtiff client procedures and the error handlers (TIFF.cpp, with the libtiff types)
                struct SProcs;
                friend struct SProcs;

            private:
                TIFF*       m_pTif;
                CommonLib::file::TFilePtr m_ptrFile;
                std::string m_sFilePath;
                std::string m_sLastError;
            };
        }
    }
}
