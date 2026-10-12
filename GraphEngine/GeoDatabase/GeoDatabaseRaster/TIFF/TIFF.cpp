#include "TIFF.h"
#include "../../../../CommonLib/exception/exc_base.h"

extern "C" {
#include "tiffio.h"
}

#include <cstdio>
#include <mutex>

namespace GraphEngine {
    namespace GeoDatabase {
        namespace TIFFLib {

            namespace
            {
                // GeoTIFF tags (libtiff doesn't know them) and the GDAL NoData value
                TIFFFieldInfo g_Fields[] = {
                    {33550, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelPixelScaleTag"},
                    {33922, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelTiepointTag"},
                    {34264, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "ModelTransformationTag"},
                    {34735, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SHORT,  FIELD_CUSTOM, 1, 1, "GeoKeyDirectoryTag"},
                    {34736, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "GeoDoubleParamsTag"},
                    {34737, TIFF_VARIABLE,  TIFF_VARIABLE,  TIFF_ASCII,  FIELD_CUSTOM, 1, 0, "GeoASCIIParamsTag"},
                    {TIFFTAG_GDAL_NODATA, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_ASCII, FIELD_CUSTOM, 1, 0, "GDALNoDataValue"},
                };

                TIFFExtendProc g_ParentExtender = nullptr;

                void TagExtender(TIFF* pTif)
                {
                    TIFFMergeFieldInfo(pTif, g_Fields, sizeof(g_Fields) / sizeof(g_Fields[0]));
                    if(g_ParentExtender)
                        g_ParentExtender(pTif);
                }

                std::string FormatTiffMessage(const char* pszModule, const char* pszFormat, va_list ap)
                {
                    char buf[1024];
                    vsnprintf(buf, sizeof(buf), pszFormat, ap);
                    std::string sMsg;
                    if(pszModule && *pszModule)
                    {
                        sMsg = pszModule;
                        sMsg += ": ";
                    }
                    return sMsg + buf;
                }

                CTIFF* Self(void* pHandle)
                {
                    return static_cast<CTIFF*>(pHandle);
                }
            }

            // ---------------- client procedures (C callbacks: no exceptions through libtiff)

            struct CTIFF::SProcs
            {

            static tmsize_t ReadProc(thandle_t pHandle, void* pBuffer, tmsize_t nSize)
            {
                try
                {
                    CTIFF* pSelf = Self(pHandle);
                    tmsize_t nDone = 0;
                    while(nDone < nSize)
                    {
                        std::streamsize n = pSelf->m_ptrFile->Read((byte_t*)pBuffer + nDone, (size_t)(nSize - nDone));
                        if(n <= 0)
                            break;     // the end of the file
                        nDone += (tmsize_t)n;
                    }
                    return nDone;
                }
                catch (std::exception& exc)
                {
                    Self(pHandle)->m_sLastError = exc.what();
                    return -1;
                }
            }

            static tmsize_t WriteProc(thandle_t pHandle, void* pBuffer, tmsize_t nSize)
            {
                try
                {
                    CTIFF* pSelf = Self(pHandle);
                    tmsize_t nDone = 0;
                    while(nDone < nSize)
                    {
                        std::streamsize n = pSelf->m_ptrFile->Write((const byte_t*)pBuffer + nDone, (size_t)(nSize - nDone));
                        if(n <= 0)
                            return -1;    // no disk space
                        nDone += (tmsize_t)n;
                    }
                    return nDone;
                }
                catch (std::exception& exc)
                {
                    Self(pHandle)->m_sLastError = exc.what();
                    return -1;
                }
            }

            static toff_t SeekProc(thandle_t pHandle, toff_t nOffset, int nWhence)
            {
                try
                {
                    CommonLib::file::TFilePtr& ptrFile = Self(pHandle)->m_ptrFile;
                    // the offset of SEEK_CUR / SEEK_END is signed (two's complement in the unsigned value)
                    toff_t nPos = nOffset;
                    if(nWhence == SEEK_CUR)
                        nPos = ptrFile->GetFilePos() + nOffset;
                    else if(nWhence == SEEK_END)
                        nPos = ptrFile->GetFileSize() + nOffset;
                    ptrFile->SetFilePos64(nPos, CommonLib::soFromBegin);
                    return ptrFile->GetFilePos();
                }
                catch (std::exception& exc)
                {
                    Self(pHandle)->m_sLastError = exc.what();
                    return (toff_t)-1;
                }
            }

            static int CloseProc(thandle_t /*pHandle*/)
            {
                return 0;    // the file is closed by Close
            }

            static toff_t SizeProc(thandle_t pHandle)
            {
                try
                {
                    return Self(pHandle)->m_ptrFile->GetFileSize();
                }
                catch (std::exception& exc)
                {
                    Self(pHandle)->m_sLastError = exc.what();
                    return 0;
                }
            }

            static int MapProc(thandle_t /*pHandle*/, void** /*ppBase*/, toff_t* /*pSize*/)
            {
                return 0;    // no memory mapping: the strips / tiles are read (a 32 GB file isn't mapped as a whole)
            }

            static void UnmapProc(thandle_t /*pHandle*/, void* /*pBase*/, toff_t /*nSize*/)
            {

            }

            static int ErrorHandler(TIFF* /*pTif*/, void* pUserData, const char* pszModule, const char* pszFormat, va_list ap)
            {
                if(CTIFF* pSelf = static_cast<CTIFF*>(pUserData))
                    pSelf->m_sLastError = FormatTiffMessage(pszModule, pszFormat, ap);
                return 1;    // handled, not to stderr
            }

            static int WarningHandler(TIFF* /*pTif*/, void* /*pUserData*/, const char* /*pszModule*/, const char* /*pszFormat*/, va_list /*ap*/)
            {
                return 1;    // the warnings (unknown tags ...) are ignored
            }
            };

            // ---------------- opening

            CTIFF::CTIFF() : m_pTif(nullptr)
            {

            }

            CTIFF::~CTIFF()
            {
                try
                {
                    Close();
                }
                catch (...)
                {
                }
            }

            void CTIFF::RegisterTags()
            {
                static std::once_flag flag;
                std::call_once(flag, []() { g_ParentExtender = TIFFSetTagExtender(TagExtender); });
            }

            TIFFPtr CTIFF::Open(const std::string& sFilePathUtf8, const char* pszMode)
            {
                const char* pszOpenMode = pszMode && *pszMode ? pszMode : "rO";
                bool bWrite = pszOpenMode[0] == 'w' || pszOpenMode[0] == 'a';
                TIFFPtr ptrTif(new CTIFF());
                ptrTif->OpenHandle(sFilePathUtf8, pszOpenMode, bWrite);
                return ptrTif;
            }

            TIFFPtr CTIFF::Create(const std::string& sFilePathUtf8, bool bBigTIFF)
            {
                return Open(sFilePathUtf8, bBigTIFF ? "w8" : "w");
            }

            void CTIFF::OpenHandle(const std::string& sFilePathUtf8, const char* pszMode, bool bWrite)
            {
                RegisterTags();
                m_sFilePath = sFilePathUtf8;
                m_sLastError.clear();

                try
                {
                    // libtiff reads the file it writes (the directories), the write mode opens it for reading too
                    m_ptrFile = bWrite ?
                            CommonLib::file::CFileCreator::OpenFileA(sFilePathUtf8.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                     CommonLib::file::smNoMode, CommonLib::file::oftBinary) :
                            CommonLib::file::CFileCreator::OpenFileA(sFilePathUtf8.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead,
                                                                     CommonLib::file::smRead, CommonLib::file::oftBinary);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExc("Failed to open TIFF {0}", sFilePathUtf8, exc);
                    throw;
                }

                TIFFOpenOptions* pOptions = TIFFOpenOptionsAlloc();
                TIFFOpenOptionsSetErrorHandlerExtR(pOptions, &SProcs::ErrorHandler, this);
                TIFFOpenOptionsSetWarningHandlerExtR(pOptions, &SProcs::WarningHandler, this);
                m_pTif = TIFFClientOpenExt(sFilePathUtf8.c_str(), pszMode, (thandle_t)this,
                                           &SProcs::ReadProc, &SProcs::WriteProc, &SProcs::SeekProc, &SProcs::CloseProc,
                                           &SProcs::SizeProc, &SProcs::MapProc, &SProcs::UnmapProc, pOptions);
                TIFFOpenOptionsFree(pOptions);

                if(!m_pTif)
                {
                    try
                    {
                        m_ptrFile->CloseFile();
                    }
                    catch (...)
                    {
                    }
                    m_ptrFile.reset();
                    ThrowError("Failed to open TIFF");
                }
            }

            void CTIFF::Close()
            {
                if(m_pTif)
                {
                    TIFF* pTif = m_pTif;
                    m_pTif = nullptr;
                    TIFFClose(pTif);    // writes the last directory of a new file, calls CloseProc
                }
                if(m_ptrFile.get())
                {
                    CommonLib::file::TFilePtr ptrFile = m_ptrFile;
                    m_ptrFile.reset();
                    if(ptrFile->IsValid())
                        ptrFile->CloseFile();
                }
            }

            bool CTIFF::IsTIFFFile(const std::string& sFilePathUtf8)
            {
                unsigned char hdr[4] = {0, 0, 0, 0};
                try
                {
                    CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFileA(sFilePathUtf8.c_str(), CommonLib::file::ofmOpenExisting,
                                                                                                CommonLib::file::arRead, CommonLib::file::smRead,
                                                                                                CommonLib::file::oftBinary);
                    size_t nRead = 0;
                    while(nRead < 4)
                    {
                        std::streamsize n = ptrFile->Read(hdr + nRead, 4 - nRead);
                        if(n <= 0)
                            break;
                        nRead += (size_t)n;
                    }
                    ptrFile->CloseFile();
                    if(nRead != 4)
                        return false;
                }
                catch (std::exception&)
                {
                    return false;
                }

                // classic TIFF (42) and BigTIFF (43)
                if(hdr[0] == 'I' && hdr[1] == 'I' && hdr[3] == 0 && (hdr[2] == 42 || hdr[2] == 43))
                    return true;
                return hdr[0] == 'M' && hdr[1] == 'M' && hdr[2] == 0 && (hdr[3] == 42 || hdr[3] == 43);
            }

            void CTIFF::ThrowError(const std::string& sMsg) const
            {
                if(m_sLastError.empty())
                    throw CommonLib::CExcBase("{0}, file: {1}", sMsg, m_sFilePath);
                throw CommonLib::CExcBase("{0}, file: {1}, libtiff: {2}", sMsg, m_sFilePath, m_sLastError);
            }

            // ---------------- fields

            bool CTIFF::GetField(uint32_t tag, uint16_t& value) const
            {
                return TIFFGetField(m_pTif, tag, &value) != 0;
            }

            bool CTIFF::GetField(uint32_t tag, uint32_t& value) const
            {
                return TIFFGetField(m_pTif, tag, &value) != 0;
            }

            bool CTIFF::GetField(uint32_t tag, std::string& value) const
            {
                char* pszValue = nullptr;
                if(!TIFFGetField(m_pTif, tag, &pszValue) || !pszValue)
                    return false;
                value = pszValue;
                return true;
            }

            bool CTIFF::GetField(uint32_t tag, std::vector<double>& values) const
            {
                uint32_t nCount = 0;
                double* pData = nullptr;
                if(!TIFFGetField(m_pTif, tag, &nCount, &pData) || !pData)
                    return false;
                values.assign(pData, pData + nCount);
                return true;
            }

            bool CTIFF::GetField(uint32_t tag, std::vector<uint16_t>& values) const
            {
                uint32_t nCount = 0;
                uint16_t* pData = nullptr;
                if(!TIFFGetField(m_pTif, tag, &nCount, &pData) || !pData)
                    return false;
                values.assign(pData, pData + nCount);
                return true;
            }

            bool CTIFF::GetFieldDefaulted(uint32_t tag, uint16_t& value) const
            {
                return TIFFGetFieldDefaulted(m_pTif, tag, &value) != 0;
            }

            bool CTIFF::GetFieldDefaulted(uint32_t tag, uint32_t& value) const
            {
                return TIFFGetFieldDefaulted(m_pTif, tag, &value) != 0;
            }

            bool CTIFF::GetColorMap(const uint16_t*& pRed, const uint16_t*& pGreen, const uint16_t*& pBlue) const
            {
                uint16_t* r = nullptr;
                uint16_t* g = nullptr;
                uint16_t* b = nullptr;
                if(!TIFFGetField(m_pTif, TIFFTAG_COLORMAP, &r, &g, &b) || !r || !g || !b)
                    return false;
                pRed = r;
                pGreen = g;
                pBlue = b;
                return true;
            }

            bool CTIFF::SetField(uint32_t tag, uint16_t value)
            {
                return TIFFSetField(m_pTif, tag, value) != 0;
            }

            bool CTIFF::SetField(uint32_t tag, uint32_t value)
            {
                return TIFFSetField(m_pTif, tag, value) != 0;
            }

            bool CTIFF::SetFieldInt(uint32_t tag, int value)
            {
                return TIFFSetField(m_pTif, tag, value) != 0;
            }

            bool CTIFF::SetField(uint32_t tag, const std::string& value)
            {
                return TIFFSetField(m_pTif, tag, value.c_str()) != 0;
            }

            bool CTIFF::SetField(uint32_t tag, const std::vector<double>& values)
            {
                return TIFFSetField(m_pTif, tag, (uint32_t)values.size(), values.data()) != 0;
            }

            bool CTIFF::SetField(uint32_t tag, const std::vector<uint16_t>& values)
            {
                return TIFFSetField(m_pTif, tag, (uint32_t)values.size(), values.data()) != 0;
            }

            // ---------------- directories

            uint32_t CTIFF::GetDirectoryCount() const
            {
                return (uint32_t)TIFFNumberOfDirectories(m_pTif);
            }

            uint32_t CTIFF::GetCurrentDirectoryIndex() const
            {
                return (uint32_t)TIFFCurrentDirectory(m_pTif);
            }

            void CTIFF::SetDirectory(uint32_t nDirectory)
            {
                if(!TIFFSetDirectory(m_pTif, (tdir_t)nDirectory))
                    ThrowError("Failed to read the TIFF directory " + std::to_string(nDirectory));
            }

            void CTIFF::WriteDirectory()
            {
                if(!TIFFWriteDirectory(m_pTif))
                    ThrowError("Failed to write a TIFF directory");
            }

            // ---------------- strips / tiles

            bool CTIFF::IsTiled() const
            {
                return TIFFIsTiled(m_pTif) != 0;
            }

            uint64_t CTIFF::GetTileSize() const
            {
                return TIFFTileSize64(m_pTif);
            }

            uint64_t CTIFF::GetTileRowSize() const
            {
                return TIFFTileRowSize64(m_pTif);
            }

            uint64_t CTIFF::GetScanlineSize() const
            {
                return TIFFScanlineSize64(m_pTif);
            }

            uint64_t CTIFF::GetVStripSize(uint32_t nRows) const
            {
                return TIFFVStripSize64(m_pTif, nRows);
            }

            uint32_t CTIFF::ComputeTile(uint32_t x, uint32_t y, uint32_t z, uint16_t sample) const
            {
                return TIFFComputeTile(m_pTif, x, y, z, sample);
            }

            uint32_t CTIFF::ComputeStrip(uint32_t row, uint16_t sample) const
            {
                return TIFFComputeStrip(m_pTif, row, sample);
            }

            void CTIFF::ReadEncodedTile(uint32_t nTile, void* pBuffer, size_t nSize)
            {
                m_sLastError.clear();
                if(TIFFReadEncodedTile(m_pTif, nTile, pBuffer, (tmsize_t)nSize) < 0)
                    ThrowError("Failed to read TIFF tile " + std::to_string(nTile));
            }

            void CTIFF::ReadEncodedStrip(uint32_t nStrip, void* pBuffer, size_t nSize)
            {
                m_sLastError.clear();
                if(TIFFReadEncodedStrip(m_pTif, nStrip, pBuffer, (tmsize_t)nSize) < 0)
                    ThrowError("Failed to read TIFF strip " + std::to_string(nStrip));
            }

            void CTIFF::ReadRGBATile(uint32_t x, uint32_t y, uint32_t* pBuffer)
            {
                m_sLastError.clear();
                if(!TIFFReadRGBATile(m_pTif, x, y, pBuffer))
                    ThrowError("Failed to read TIFF data");
            }

            void CTIFF::ReadRGBAStrip(uint32_t row, uint32_t* pBuffer)
            {
                m_sLastError.clear();
                if(!TIFFReadRGBAStrip(m_pTif, row, pBuffer))
                    ThrowError("Failed to read TIFF data");
            }

            bool CTIFF::IsRGBAImageOK(std::string& sMessage) const
            {
                char szMsg[1024] = {0};
                bool bOk = TIFFRGBAImageOK(m_pTif, szMsg) != 0;
                sMessage = szMsg;
                return bOk;
            }

            void CTIFF::WriteTile(const void* pBuffer, uint32_t x, uint32_t y, uint32_t z, uint16_t sample)
            {
                if(TIFFWriteTile(m_pTif, const_cast<void*>(pBuffer), x, y, z, sample) < 0)
                    ThrowError("Failed to write a TIFF tile");
            }

            void CTIFF::WriteScanline(const void* pBuffer, uint32_t row, uint16_t sample)
            {
                if(TIFFWriteScanline(m_pTif, const_cast<void*>(pBuffer), row, sample) < 0)
                    ThrowError("Failed to write a TIFF scanline");
            }
        }
    }
}
