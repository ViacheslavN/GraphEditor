#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../../CommonLib/filesystem/File.h"
#include "OSMConvertorLib.h"
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // Coordinates of the OSM nodes by id: the conversion stores them in the first pass (Add), the ways get them
        // in the second one (Get). The coordinates are stored with 1e-7 degree precision (as in OSM itself).
        class IOSMNodeStore
        {
        public:
            // the number of processed nodes, return false to cancel (Finish)
            typedef std::function<bool(uint64_t nNodes)> TProgress;

            IOSMNodeStore() {}
            virtual ~IOSMNodeStore() {}

            virtual void Add(int64_t nId, double dLon, double dLat) = 0;
            // prepares the lookup; Add can't be called after it
            virtual void Finish(TProgress progress = TProgress()) = 0;
            virtual bool IsFinished() const = 0;
            virtual bool Get(int64_t nId, double& dLon, double& dLat) = 0;
            virtual uint64_t Count() const = 0;

            // the store for the settings: Auto - the flat one for the files of a continent / the planet (see FlatNodeStoreFileSize)
            // on a 64-bit build, the sorted one otherwise; nFileSize - the size of the OSM file
            static std::unique_ptr<IOSMNodeStore> Create(eOSMNodeStore type, const std::string& sOSMPath, uint64_t nFileSize,
                                                         const std::string& sTempDir, uint64_t nMemoryBytes);
            static eOSMNodeStore ResolveType(eOSMNodeStore type, const std::string& sOSMPath, uint64_t nFileSize);

            // Auto: the flat store from this size of a .pbf file (an .osm file is about 10 times bigger)
            static const uint64_t FlatNodeStoreFileSize = 8ull * 1024 * 1024 * 1024;
        };

        // Coordinates of the OSM nodes by id, for files of any size (country / planet):
        //  - the nodes are kept in memory while they fit into the memory limit;
        //  - then they go to a temporary file in sorted runs (planet / extracts are sorted by id, so usually
        //    it is one run), Finish merges the runs of an unsorted file;
        //  - the lookup finds the block (4096 nodes) by an in-memory index of the first ids, the blocks are cached (LRU).
        // 16 bytes per node, the coordinates are stored with 1e-7 degree precision (as in OSM itself).
        // Good for extracts: the disk space is proportional to the number of nodes.
        class COSMNodeStore : public IOSMNodeStore
        {
        public:
            COSMNodeStore(const std::string& sTempDir, uint64_t nMemoryBytes);
            ~COSMNodeStore();

            virtual void Add(int64_t nId, double dLon, double dLat) override;
            // sorts / merges, builds the index; Add can't be called after it
            virtual void Finish(TProgress progress = TProgress()) override;
            virtual bool IsFinished() const override { return m_bFinished; }

            virtual bool Get(int64_t nId, double& dLon, double& dLat) override;

            virtual uint64_t Count() const override { return m_nCount; }
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
