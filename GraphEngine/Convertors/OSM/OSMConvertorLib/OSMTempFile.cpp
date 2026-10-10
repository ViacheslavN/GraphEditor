#include "OSMTempFile.h"
#include "../../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <random>

namespace GraphEngine {
    namespace Convertors {

        COSMTempFile::COSMTempFile(const std::string& sTempDir, const char* pszSuffix, size_t nBufferSize) :
                m_sTempDir(sTempDir), m_sSuffix(pszSuffix ? pszSuffix : ".bin"), m_nBufferSize((std::max)((size_t)4096, nBufferSize)),
                m_nFileSize(0), m_nReadPos(0), m_nFileReadPos(0)
        {
            if(m_sTempDir.empty())
            {
                std::error_code ec;
                m_sTempDir = std::filesystem::temp_directory_path(ec).u8string();
            }
        }

        COSMTempFile::~COSMTempFile()
        {
            Close();
        }

        void COSMTempFile::Open()
        {
            if(m_ptrFile.get())
                return;
            std::random_device rd;
            std::filesystem::path path = std::filesystem::u8path(m_sTempDir);
            path /= "graphengine_osm_" + std::to_string(rd()) + std::to_string(rd()) + m_sSuffix;
            m_sPath = path.u8string();
            try
            {
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(m_sPath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                     CommonLib::file::smNoMode, CommonLib::file::oftBinary);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create the temporary file {0}", m_sPath, exc);
                throw;
            }
        }

        void COSMTempFile::Close()
        {
            if(!m_ptrFile.get())
                return;
            try
            {
                m_ptrFile->CloseFile();
            }
            catch (...)
            {
            }
            m_ptrFile.reset();
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(m_sPath), ec);
        }

        void COSMTempFile::Write(const void* pData, size_t nSize)
        {
            const uint8_t* p = (const uint8_t*)pData;
            if(m_writeBuffer.size() + nSize > m_nBufferSize)
                Flush();
            if(nSize >= m_nBufferSize)
            {
                Open();
                m_ptrFile->SetFilePos64(m_nFileSize, CommonLib::soFromBegin);
                if((size_t)m_ptrFile->Write((const byte_t*)p, nSize) != nSize)
                    throw CommonLib::CExcBase("Failed to write the temporary file {0} (no disk space?)", m_sPath);
                m_nFileSize += nSize;
                return;
            }
            m_writeBuffer.insert(m_writeBuffer.end(), p, p + nSize);
        }

        void COSMTempFile::Flush()
        {
            if(m_writeBuffer.empty())
                return;
            Open();
            m_ptrFile->SetFilePos64(m_nFileSize, CommonLib::soFromBegin);
            if((size_t)m_ptrFile->Write((const byte_t*)m_writeBuffer.data(), m_writeBuffer.size()) != m_writeBuffer.size())
                throw CommonLib::CExcBase("Failed to write the temporary file {0} (no disk space?)", m_sPath);
            m_nFileSize += m_writeBuffer.size();
            m_writeBuffer.clear();
        }

        void COSMTempFile::Rewind()
        {
            Flush();
            m_readBuffer.clear();
            m_nReadPos = 0;
            m_nFileReadPos = 0;
        }

        bool COSMTempFile::Read(void* pData, size_t nSize)
        {
            uint8_t* p = (uint8_t*)pData;
            size_t nDone = 0;
            while(nDone < nSize)
            {
                if(m_nReadPos >= m_readBuffer.size())
                {
                    if(m_nFileReadPos >= m_nFileSize)
                    {
                        if(nDone == 0)
                            return false;
                        throw CommonLib::CExcBase("The temporary file {0} is truncated", m_sPath);
                    }
                    size_t nChunk = (size_t)(std::min)((uint64_t)m_nBufferSize, m_nFileSize - m_nFileReadPos);
                    m_readBuffer.resize(nChunk);
                    m_ptrFile->SetFilePos64(m_nFileReadPos, CommonLib::soFromBegin);
                    if((size_t)m_ptrFile->Read((byte_t*)m_readBuffer.data(), nChunk) != nChunk)
                        throw CommonLib::CExcBase("Failed to read the temporary file {0}", m_sPath);
                    m_nFileReadPos += nChunk;
                    m_nReadPos = 0;
                }
                size_t nCopy = (std::min)(nSize - nDone, m_readBuffer.size() - m_nReadPos);
                memcpy(p + nDone, m_readBuffer.data() + m_nReadPos, nCopy);
                m_nReadPos += nCopy;
                nDone += nCopy;
            }
            return true;
        }

        void COSMTempFile::ReadAt(uint64_t nPos, void* pData, size_t nSize)
        {
            if(nSize == 0)
                return;
            Flush();
            if(nPos + nSize > m_nFileSize)
                throw CommonLib::CExcBase("The temporary file {0}: no data at {1}", m_sPath, nPos);
            m_ptrFile->SetFilePos64(nPos, CommonLib::soFromBegin);
            if((size_t)m_ptrFile->Read((byte_t*)pData, nSize) != nSize)
                throw CommonLib::CExcBase("Failed to read the temporary file {0}", m_sPath);
        }

        void COSMTempFile::WriteString(const std::string& sValue)
        {
            WriteValue((uint32_t)sValue.size());
            if(!sValue.empty())
                Write(sValue.data(), sValue.size());
        }

        bool COSMTempFile::ReadString(std::string& sValue)
        {
            uint32_t nSize = 0;
            if(!ReadValue(nSize))
                return false;
            sValue.resize(nSize);
            if(nSize && !Read(&sValue[0], nSize))
                throw CommonLib::CExcBase("The temporary file {0} is truncated", m_sPath);
            return true;
        }
    }
}
