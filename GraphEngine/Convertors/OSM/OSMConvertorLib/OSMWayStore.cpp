#include "OSMWayStore.h"
#include <filesystem>
#include <random>

namespace GraphEngine {
    namespace Convertors {

        COSMWayStore::COSMWayStore(const std::string& sTempDir, uint64_t nMemoryBytes) : m_sTempDir(sTempDir), m_nFilePoints(0)
        {
            m_nMemoryPoints = (std::max)((uint64_t)65536, nMemoryBytes / sizeof(CommonLib::GisXYPoint));
            if(m_sTempDir.empty())
            {
                std::error_code ec;
                m_sTempDir = std::filesystem::temp_directory_path(ec).u8string();
            }
        }

        COSMWayStore::~COSMWayStore()
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
            std::filesystem::remove(std::filesystem::u8path(m_sFilePath), ec);
        }

        void COSMWayStore::Flush()
        {
            if(m_buffer.empty())
                return;

            if(!m_ptrFile.get())
            {
                std::random_device rd;
                std::filesystem::path path = std::filesystem::u8path(m_sTempDir);
                path /= "graphengine_osm_" + std::to_string(rd()) + std::to_string(rd()) + "_ways.bin";
                m_sFilePath = path.u8string();
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(m_sFilePath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                     CommonLib::file::smNoMode, CommonLib::file::oftBinary);
            }

            m_ptrFile->SetFilePos64(m_nFilePoints * sizeof(CommonLib::GisXYPoint), CommonLib::soFromBegin);
            size_t nBytes = m_buffer.size() * sizeof(CommonLib::GisXYPoint);
            if((size_t)m_ptrFile->Write((const byte_t*)m_buffer.data(), nBytes) != nBytes)
                throw CommonLib::CExcBase("Way store: failed to write the temporary file (no disk space?)");

            // the entries of the buffer move to the file
            for(size_t i = 0; i < m_vecBuffered.size(); ++i)
            {
                SEntry& entry = m_index[m_vecBuffered[i]];
                entry.nOffset += m_nFilePoints;
                entry.bInFile = true;
            }
            m_vecBuffered.clear();
            m_nFilePoints += m_buffer.size();
            m_buffer.clear();
        }

        void COSMWayStore::Add(int64_t nId, const TOSMPoints& points)
        {
            if(points.empty() || m_index.find(nId) != m_index.end())
                return;

            if(m_buffer.size() + points.size() > m_nMemoryPoints)
                Flush();

            SEntry entry;
            entry.nOffset = m_buffer.size();
            entry.nCount = (uint32_t)points.size();
            entry.bInFile = false;
            m_buffer.insert(m_buffer.end(), points.begin(), points.end());
            m_index[nId] = entry;
            m_vecBuffered.push_back(nId);
        }

        bool COSMWayStore::Get(int64_t nId, TOSMPoints& points)
        {
            auto it = m_index.find(nId);
            if(it == m_index.end())
                return false;

            const SEntry& entry = it->second;
            if(!entry.bInFile)
            {
                points.assign(m_buffer.begin() + (size_t)entry.nOffset, m_buffer.begin() + (size_t)(entry.nOffset + entry.nCount));
                return true;
            }

            points.resize(entry.nCount);
            m_ptrFile->SetFilePos64(entry.nOffset * sizeof(CommonLib::GisXYPoint), CommonLib::soFromBegin);
            size_t nBytes = entry.nCount * sizeof(CommonLib::GisXYPoint);
            if((size_t)m_ptrFile->Read((byte_t*)points.data(), nBytes) != nBytes)
                throw CommonLib::CExcBase("Way store: failed to read the temporary file");
            return true;
        }
    }
}
