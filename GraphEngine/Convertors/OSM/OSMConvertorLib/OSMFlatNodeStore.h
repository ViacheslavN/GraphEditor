#pragma once
#include "OSMNodeStore.h"
#include "../../../CommonLib/filesystem/FileMapping.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // Node coordinates in a file indexed by the node id (the planet / continents):
        //  - 8 bytes per id: the longitude and the latitude in 1e-7 degrees, an empty record (zeros) - no node;
        //  - the file is as long as the biggest id (~13 billion ids now: ~100 GB), it is sparse, the ids which aren't
        //    in the OSM file take no disk space (but a 4 KB page of 512 ids is stored if one of them is used: an extract
        //    of a small area is better in COSMNodeStore);
        //  - Add writes sequentially (a sorted file) through a buffer, Get reads the memory mapped file: the system caches
        //    the pages, a lookup is one memory access (no index, no search);
        //  - the negative ids (objects of an editor not uploaded yet) are kept in memory.
        // Needs a 64-bit build (the file is mapped by 1 GB segments which stay mapped).
        class COSMFlatNodeStore : public IOSMNodeStore
        {
        public:
            explicit COSMFlatNodeStore(const std::string& sTempDir);
            virtual ~COSMFlatNodeStore();

            virtual void Add(int64_t nId, double dLon, double dLat) override;
            virtual void Finish(TProgress progress = TProgress()) override;
            virtual bool IsFinished() const override { return m_bFinished; }
            virtual bool Get(int64_t nId, double& dLon, double& dLat) override;
            virtual uint64_t Count() const override { return m_nCount; }
            // the segments are mapped by Finish: the lookups only read the mapped memory
            virtual std::unique_ptr<IOSMNodeLookup> CreateLookup(uint64_t nCacheBytes) const override;
            bool GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) const;

            uint64_t FileSize() const { return m_nFileRecords * sizeof(SRecord); }
            const std::string& FilePath() const { return m_sFilePath; }

            static const uint32_t SegmentBits = 27;     // 128 M ids, 1 GB per mapped segment
            static const uint32_t MaxGap = 4096;        // the gap between ids filled with empty records in the write buffer

        private:
            COSMFlatNodeStore(const COSMFlatNodeStore&);
            COSMFlatNodeStore& operator=(const COSMFlatNodeStore&);

#pragma pack(push, 4)
            struct SRecord
            {
                int32_t  lon;
                uint32_t lat;    // latitude + LatBias, 0 - no node
            };
#pragma pack(pop)

            static SRecord Encode(double dLon, double dLat);
            static void Decode(const SRecord& record, double& dLon, double& dLat);
            void FlushBuffer();
            const SRecord* Segment(uint64_t nSegment);
            const SRecord* Find(int64_t nId) const;
            void Close();

        private:
            std::string m_sTempDir;
            std::string m_sFilePath;
            CommonLib::file::TFilePtr m_ptrFile;
            std::vector<SRecord> m_buffer;      // the records from m_nBufferStart
            uint64_t    m_nBufferStart;
            uint64_t    m_nFileRecords;         // the file length in records (the biggest written id + 1)
            uint64_t    m_nCount;
            bool        m_bFinished;
            std::unordered_map<int64_t, SRecord> m_negative;
            std::vector<std::unique_ptr<CommonLib::file::CFileMapping> > m_vecSegments;
        };
    }
}
