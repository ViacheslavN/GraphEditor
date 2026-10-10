#pragma once
#include "OSMGeometry.h"
#include "../../../CommonLib/filesystem/File.h"
#include <unordered_map>

namespace GraphEngine {
    namespace Convertors {

        // Geometry of the ways which are members of multipolygons (needed after all the ways are read).
        // The points are kept in memory up to the limit, then appended to a temporary file.
        class COSMWayStore
        {
        public:
            COSMWayStore(const std::string& sTempDir, uint64_t nMemoryBytes);
            ~COSMWayStore();

            void Add(int64_t nId, const TOSMPoints& points);
            bool Get(int64_t nId, TOSMPoints& points);
            size_t Count() const { return m_index.size(); }

        private:
            COSMWayStore(const COSMWayStore&);
            COSMWayStore& operator=(const COSMWayStore&);

            void Flush();

            struct SEntry
            {
                uint64_t nOffset;    // points: in the file (bInFile) or in the memory buffer
                uint32_t nCount;
                bool     bInFile;
            };

        private:
            std::string m_sTempDir;
            uint64_t    m_nMemoryPoints;
            TOSMPoints  m_buffer;
            std::unordered_map<int64_t, SEntry> m_index;
            std::vector<int64_t> m_vecBuffered;   // ways in the memory buffer
            CommonLib::file::TFilePtr m_ptrFile;
            std::string m_sFilePath;
            uint64_t    m_nFilePoints;
        };
    }
}
