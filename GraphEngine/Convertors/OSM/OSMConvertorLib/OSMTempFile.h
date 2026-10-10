#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/filesystem/File.h"
#include <cstdint>
#include <string>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // A temporary file of the conversion (CommonLib file): appended through a buffer, read sequentially through
        // a buffer or at a position. The file is created at the first write to the disk and deleted by the destructor.
        class COSMTempFile
        {
        public:
            // sTempDir - UTF-8, empty - the system temporary folder; pszSuffix - the end of the file name ("_ways.bin")
            COSMTempFile(const std::string& sTempDir, const char* pszSuffix, size_t nBufferSize = 1024 * 1024);
            ~COSMTempFile();

            void     Write(const void* pData, size_t nSize);
            // the written size (with the buffer)
            uint64_t Size() const { return m_nFileSize + m_writeBuffer.size(); }

            // sequential reading from the beginning (the buffered writes go to the file)
            void Rewind();
            // false at the end of the file, throws if the file ends inside the data
            bool Read(void* pData, size_t nSize);

            // reads at a position (the buffered writes go to the file)
            void ReadAt(uint64_t nPos, void* pData, size_t nSize);

            void Flush();
            const std::string& Path() const { return m_sPath; }

            template<class T> void WriteValue(const T& value) { Write(&value, sizeof(T)); }
            template<class T> bool ReadValue(T& value) { return Read(&value, sizeof(T)); }
            void WriteString(const std::string& sValue);
            bool ReadString(std::string& sValue);

        private:
            COSMTempFile(const COSMTempFile&);
            COSMTempFile& operator=(const COSMTempFile&);

            void Open();
            void Close();

        private:
            std::string m_sTempDir;
            std::string m_sSuffix;
            std::string m_sPath;
            CommonLib::file::TFilePtr m_ptrFile;
            size_t      m_nBufferSize;
            std::vector<uint8_t> m_writeBuffer;
            uint64_t    m_nFileSize;
            std::vector<uint8_t> m_readBuffer;
            size_t      m_nReadPos;         // in m_readBuffer
            uint64_t    m_nFileReadPos;     // the file position after m_readBuffer
        };
    }
}
