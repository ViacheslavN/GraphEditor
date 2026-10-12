#pragma once
#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/filesystem/File.h"
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
        // Lookups of a finished node store for one thread (the parallel ways pass): every object has its own cache
        // and file handle, the objects of one store can be used by different threads at the same time.
        // the reading of a lookup from the disk (the progress details)
        struct SOSMLookupStats
        {
            uint64_t nReads = 0;        // reads from the file
            uint64_t nBytesRead = 0;
            uint64_t nCacheHits = 0;    // ids found in the cached data
            double   dReadSeconds = 0.;

            void Add(const SOSMLookupStats& stats)
            {
                nReads += stats.nReads;
                nBytesRead += stats.nBytesRead;
                nCacheHits += stats.nCacheHits;
                dReadSeconds += stats.dReadSeconds;
            }
        };

        class IOSMNodeLookup
        {
        public:
            virtual ~IOSMNodeLookup() {}
            virtual bool Get(int64_t nId, double& dLon, double& dLat) = 0;
            // the coordinates as stored: 1e-7 degrees (the OSM integers)
            virtual bool GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) = 0;
            // many ids at once, pIds sorted ascending (duplicates allowed): the store is read in the id order,
            // pFound[i] - 1 / 0
            virtual void GetSortedE7(const int64_t* pIds, size_t nCount, int32_t* pLons, int32_t* pLats, uint8_t* pFound)
            {
                for(size_t i = 0; i < nCount; ++i)
                    pFound[i] = GetE7(pIds[i], pLons[i], pLats[i]) ? 1 : 0;
            }
            // the statistics since the last call
            virtual SOSMLookupStats TakeStats() { return SOSMLookupStats(); }
        };

        // The node refs of a batch of ways resolved at once: the ids are sorted, the duplicates removed, the store is read
        // in the id order (GetSortedE7), the coordinates are scattered back to the refs.
        class COSMRefResolver
        {
        public:
            // the refs [nStart, nStart + nCount) of pRefs are resolved; Add for every range, then Resolve
            // (fnContinue is checked after every ResolveChunk ids, false - throws "canceled")
            void Clear();
            void Add(const int64_t* pRefs, uint32_t nStart, uint32_t nCount);
            void Resolve(IOSMNodeLookup& lookup, size_t nTotalRefs, const std::function<bool()>& fnContinue = std::function<bool()>());
            static const size_t ResolveChunk = 1024;

            // the ref by its index in pRefs (only the added ranges)
            bool Get(size_t nRef, int32_t& nLon, int32_t& nLat) const
            {
                if(!m_refFound[nRef])
                    return false;
                nLon = m_refLons[nRef];
                nLat = m_refLats[nRef];
                return true;
            }
            uint64_t RefCount() const { return m_pairs.size(); }
            uint64_t UniqueCount() const { return m_ids.size(); }

        private:
            std::vector<std::pair<int64_t, uint32_t> > m_pairs;   // id, ref index
            std::vector<int64_t> m_ids;
            std::vector<int32_t> m_lons, m_lats;
            std::vector<uint8_t> m_found;
            std::vector<int32_t> m_refLons, m_refLats;
            std::vector<uint8_t> m_refFound;
        };

        // the writing of a node store (the progress details)
        struct SOSMStoreStats
        {
            std::string sType;              // "flat", "sorted"
            std::string sPath;              // the temporary file, empty - in memory (yet)
            uint64_t    nBytesWritten = 0;
            double      dIOSeconds = 0.;    // writing (and merging) the file
            double      dWaitSeconds = 0.;  // Add waits for the background writer (the disk is slower than the reading)
        };

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
            virtual SOSMStoreStats GetStats() const = 0;
            // checked while Add waits for the disk (the background writer of the flat store), false - Add throws "canceled"
            virtual void SetContinue(std::function<bool()> fnContinue) { (void)fnContinue; }
            // after Finish; nCacheBytes - the memory of the lookup for the cached blocks (a store on the disk)
            virtual std::unique_ptr<IOSMNodeLookup> CreateLookup(uint64_t nCacheBytes) const = 0;

            // the store for the settings: Auto - the flat one for the files of a continent / the planet (see FlatNodeStoreFileSize)
            // on a 64-bit build, the sorted one otherwise; nFileSize - the size of the OSM file
            // nFlatBufferBytes - the write buffer of the flat store, 0 - COSMFlatNodeStore::DefaultBufferBytes
            static std::unique_ptr<IOSMNodeStore> Create(eOSMNodeStore type, const std::string& sOSMPath, uint64_t nFileSize,
                                                         const std::string& sTempDir, uint64_t nMemoryBytes, uint64_t nFlatBufferBytes = 0);
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
            // nExpectedNodes - the buffer is reserved for so many nodes (up to the memory limit), 0 - unknown
            COSMNodeStore(const std::string& sTempDir, uint64_t nMemoryBytes, uint64_t nExpectedNodes = 0);
            ~COSMNodeStore();

            virtual void Add(int64_t nId, double dLon, double dLat) override;
            // sorts / merges, builds the index; Add can't be called after it
            virtual void Finish(TProgress progress = TProgress()) override;
            virtual bool IsFinished() const override { return m_bFinished; }

            virtual bool Get(int64_t nId, double& dLon, double& dLat) override;

            virtual uint64_t Count() const override { return m_nCount; }
            virtual SOSMStoreStats GetStats() const override
            {
                SOSMStoreStats stats;
                stats.sType = "sorted";
                stats.sPath = m_sFilePath;
                stats.nBytesWritten = m_nBytesWritten;
                stats.dIOSeconds = m_dIOSeconds;
                return stats;
            }
            virtual std::unique_ptr<IOSMNodeLookup> CreateLookup(uint64_t nCacheBytes) const override;
            bool     IsInFile() const { return m_ptrFile.get() != nullptr; }
            uint64_t GetCacheHits() const { return m_nHits; }
            uint64_t GetCacheMisses() const { return m_nMisses; }

            static const uint32_t BlockSize = 4096;   // nodes

        private:
            class CLookup;
            friend class CLookup;
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
            static void Read(CommonLib::file::TFilePtr ptrFile, uint64_t nPos, SNode* pNodes, size_t nCount);
            static void Write(CommonLib::file::TFilePtr ptrFile, uint64_t nPos, const SNode* pNodes, size_t nCount);
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
            uint64_t    m_nBytesWritten = 0;
            double      m_dIOSeconds = 0.;
        };
    }
}
