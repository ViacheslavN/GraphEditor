#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/filesystem/File.h"
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // Coordinates of the OSM nodes by id, for files of any size (country / planet):
        //  - the nodes are kept in memory while they fit into the memory limit;
        //  - then they go to a temporary file in sorted runs (planet / extracts are sorted by id, so usually
        //    it is one run), Finish merges the runs of an unsorted file;
        //  - the lookup finds the block (4096 nodes) by an in-memory index of the first ids, the blocks are cached (LRU).
        // 16 bytes per node, the coordinates are stored with 1e-7 degree precision (as in OSM itself).
        class COSMNodeStore
        {
        public:
            // the number of processed nodes, return false to cancel (Finish of an unsorted file)
            typedef std::function<bool(uint64_t nNodes)> TProgress;

            COSMNodeStore(const std::string& sTempDir, uint64_t nMemoryBytes);
            ~COSMNodeStore();

            void Add(int64_t nId, double dLon, double dLat);
            // sorts / merges, builds the index; Add can't be called after it
            void Finish(TProgress progress = TProgress());
            bool IsFinished() const { return m_bFinished; }

            bool Get(int64_t nId, double& dLon, double& dLat);

            uint64_t Count() const { return m_nCount; }
            bool     IsInFile() const { return m_ptrFile.get() != nullptr; }
            uint64_t GetCacheHits() const { return m_nHits; }
            uint64_t GetCacheMisses() const { return m_nMisses; }

            static const uint32_t BlockSize = 4096;   // nodes

        private:
            COSMNodeStore(const COSMNodeStore&);
            COSMNodeStore& operator=(const COSMNodeStore&);

#pragma pack(push, 4)
            struct SNode
            {
                int64_t id;
                int32_t lon;
                int32_t lat;
            };
#pragma pack(pop)

            struct SBlock
            {
                uint64_t           nBlock;
                std::vector<SNode> nodes;
            };

            void SpillBuffer();
            void Merge(TProgress progress);
            void Read(CommonLib::file::TFilePtr ptrFile, uint64_t nPos, SNode* pNodes, size_t nCount);
            void Write(CommonLib::file::TFilePtr ptrFile, uint64_t nPos, const SNode* pNodes, size_t nCount);
            CommonLib::file::TFilePtr CreateTempFile(std::string& sPath) const;
            static void DeleteTempFile(CommonLib::file::TFilePtr& ptrFile, const std::string& sPath);
            void IndexNode(const SNode& node, uint64_t nIndex);
            const std::vector<SNode>* GetBlock(uint64_t nBlock);
            static bool Find(const SNode* pNodes, size_t nCount, int64_t nId, double& dLon, double& dLat);
            std::string TempFileName(const char* pszSuffix) const;

        private:
            std::string m_sTempDir;
            uint64_t    m_nMemoryNodes;     // nodes kept in memory
            std::vector<SNode> m_buffer;    // all the nodes (memory mode) / the run being collected
            CommonLib::file::TFilePtr m_ptrFile;
            std::string m_sFilePath;        // UTF-8
            std::vector<uint64_t> m_vecRuns;  // start of every run in the file (nodes)
            uint64_t    m_nFileNodes;
            uint64_t    m_nCount;
            int64_t     m_nLastId;
            bool        m_bSorted;
            bool        m_bFinished;

            std::vector<int64_t> m_vecBlockFirstIds;
            std::list<SBlock>    m_cache;      // the most recently used first
            std::unordered_map<uint64_t, std::list<SBlock>::iterator> m_mapCache;
            size_t      m_nCacheBlocks;
            const std::vector<SNode>* m_pLastBlock;
            uint64_t    m_nLastBlock;
            uint64_t    m_nHits;
            uint64_t    m_nMisses;
        };
    }
}
