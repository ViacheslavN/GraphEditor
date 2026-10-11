#pragma once
#include "OSMNodeStore.h"
#include "../../../CommonLib/filesystem/FileMapping.h"
#include <atomic>
#include <functional>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        // Node coordinates in a file indexed by the node id (the planet / continents):
        //  - 8 bytes per id: the longitude and the latitude in 1e-7 degrees, an empty record (zeros) - no node;
        //  - the file is as long as the biggest id (~13 billion ids now: ~100 GB), it is sparse, the ids which aren't
        //    in the OSM file take no disk space (but a 4 KB page of 512 ids is stored if one of them is used: an extract
        //    of a small area is better in COSMNodeStore);
        //  - Add writes sequentially (a sorted file) through a buffer; Get reads the memory mapped file; the lookups of the threads
        //    (CreateLookup) read ranges of the file by their own handles into their caches: a sorted request of many ids
        //    (GetSortedE7) is a few big reads instead of a page fault per id;
        //  - the negative ids (objects of an editor not uploaded yet) are kept in memory.
        // Needs a 64-bit build (the file is mapped by 1 GB segments which stay mapped).
        class COSMFlatNodeStore : public IOSMNodeStore
        {
        public:
            // nBufferBytes - the write buffer (SOSMConvertSettings::nNodeWriteBufferMB): Windows windows of ids, one is filled
            // by Add while the others are written by a background thread (each window is flushed to the disk)
            static const uint64_t DefaultBufferBytes = 64ull * 1024 * 1024;
            static const size_t   Windows = 4;

            explicit COSMFlatNodeStore(const std::string& sTempDir, uint64_t nBufferBytes = DefaultBufferBytes);
            // a file at the path, kept after the store is closed (the split data: nodes.flat)
            COSMFlatNodeStore(const std::string& sFilePath, bool bKeepFile, uint64_t nBufferBytes = DefaultBufferBytes);
            virtual ~COSMFlatNodeStore();

            virtual void Add(int64_t nId, double dLon, double dLat) override;
            virtual void Finish(TProgress progress = TProgress()) override;
            virtual bool IsFinished() const override { return m_bFinished; }
            virtual bool Get(int64_t nId, double& dLon, double& dLat) override;
            virtual uint64_t Count() const override { return m_nCount; }
            virtual void SetContinue(std::function<bool()> fnContinue) override { m_fnContinue = fnContinue; }
            virtual SOSMStoreStats GetStats() const override
            {
                SOSMStoreStats stats;
                stats.sType = "flat";
                stats.sPath = m_sFilePath;
                stats.nBytesWritten = m_nBytesWritten.load();
                stats.dIOSeconds = m_nIONs.load() / 1e9;
                stats.dWaitSeconds = m_nWaitNs.load() / 1e9;
                return stats;
            }
            // the segments are mapped by Finish: the lookups only read the mapped memory
            virtual std::unique_ptr<IOSMNodeLookup> CreateLookup(uint64_t nCacheBytes) const override;
            bool GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) const;

#pragma pack(push, 4)
            // a record of the file
            struct SRecord
            {
                int32_t  lon;
                uint32_t lat;    // latitude + LatBias, 0 - no node
            };
#pragma pack(pop)
            // false - an empty record (no node)
            static bool DecodeE7(const SRecord& record, int32_t& nLon, int32_t& nLat);

            uint64_t FileSize() const { return m_nFileRecords * sizeof(SRecord); }
            uint64_t FileRecords() const { return m_nFileRecords; }
            const std::string& FilePath() const { return m_sFilePath; }

            static const uint32_t SegmentBits = 27;     // 128 M ids, 1 GB per mapped segment
            static const uint32_t MaxGap = 4096;        // empty records between the nodes written with them (a longer gap stays a hole)

        private:
            class CLookup;
            friend class CLookup;
            COSMFlatNodeStore(const COSMFlatNodeStore&);
            COSMFlatNodeStore& operator=(const COSMFlatNodeStore&);

            static SRecord Encode(double dLon, double dLat);
            static void Decode(const SRecord& record, double& dLon, double& dLat);
            struct SWindow
            {
                std::vector<SRecord> records;
                uint64_t nStart = 0;
                size_t   nUsed = 0;     // the last used record + 1, 0 - empty
            };

            // the current window goes to the writer queue, Add continues with a free one (waits if all of them are queued)
            void FlushBuffer();
            // all the queued windows are written, the writer thread stops; the error of the writer is thrown
            void WaitWriter();
            void WriterLoop();
            // the nodes of the window: the runs of records (empty records inside a run up to MaxGap), then the window is cleared
            void WriteWindow(SWindow& window);
            const SRecord* Segment(uint64_t nSegment);
            const SRecord* Find(int64_t nId) const;
            void Close();

        private:
            std::string m_sTempDir;
            std::string m_sFilePath;
            CommonLib::file::TFilePtr m_ptrFile;
            size_t      m_nWindowRecords;
            std::vector<std::unique_ptr<SWindow> > m_vecWindows;   // created when needed, up to Windows
            SWindow*    m_pCurrent = nullptr;   // being filled: the records [nStart, nStart + m_nWindowRecords)
            std::deque<SWindow*>  m_queue;      // to be written, in the order
            std::vector<SWindow*> m_free;
            size_t      m_nWriting = 0;         // the windows taken by the writer
            std::thread m_writer;
            std::mutex  m_mutex;
            std::condition_variable m_cvWriter; // a window is queued / the writer stops
            std::condition_variable m_cvFree;   // a window is written
            bool        m_bStopWriter = false;
            std::exception_ptr m_writeError;
            std::atomic<uint64_t> m_nWaitNs{0};
            std::atomic<bool>     m_bAbort{false};   // Close before Finish (canceled): the queued windows aren't written
            std::function<bool()> m_fnContinue;
            uint64_t    m_nFileRecords;         // the file length in records (the biggest written id + 1)
            uint64_t    m_nCount;
            bool        m_bFinished;
            std::unordered_map<int64_t, SRecord> m_negative;
            std::vector<std::unique_ptr<CommonLib::file::CFileMapping> > m_vecSegments;
            std::atomic<uint64_t> m_nBytesWritten{0};
            std::atomic<uint64_t> m_nIONs{0};
            bool        m_bKeepFile = false;

            void Create();
        };

        // A part of the ids of a flat node file in memory: the records [start, start + count) are read sequentially
        // (the holes of the sparse file are read as zeros, fast), a lookup is one memory access.
        // The memory is kept for the next Load (the parts of the same size).
        class COSMNodeSegment
        {
        public:
            // progress(bytes read) - false cancels
            void Load(const std::string& sFilePath, uint64_t nStart, uint64_t nCount, const std::function<bool(uint64_t)>& progress);
            void Free();

            uint64_t Start() const { return m_nStart; }
            uint64_t End() const { return m_nStart + m_nCount; }
            bool Contains(int64_t nId) const { return nId >= 0 && (uint64_t)nId >= m_nStart && (uint64_t)nId < m_nStart + m_nCount; }
            const COSMFlatNodeStore::SRecord& Record(int64_t nId) const { return m_ptrData[(size_t)((uint64_t)nId - m_nStart)]; }
            bool GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) const
            {
                return Contains(nId) && COSMFlatNodeStore::DecodeE7(Record(nId), nLon, nLat);
            }
            uint64_t BytesRead() const { return m_nBytesRead; }
            double   ReadSeconds() const { return m_dReadSeconds; }

        private:
            std::unique_ptr<COSMFlatNodeStore::SRecord[]> m_ptrData;
            uint64_t m_nCapacity = 0;
            uint64_t m_nStart = 0;
            uint64_t m_nCount = 0;
            uint64_t m_nBytesRead = 0;
            double   m_dReadSeconds = 0.;
        };
    }
}
