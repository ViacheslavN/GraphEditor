#include "OSMFlatNodeStore.h"
#include <chrono>
#include "../../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <cmath>
#include <filesystem>
#include <random>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const double CoordScale = 1e7;
            const int64_t LatBias = 1000000000;         // -90..90 degrees -> 100 000 000..1 900 000 000, 0 is free for "no node"
            const size_t MaxWriteBytes = 64 * 1024 * 1024;   // one write call
        }

        COSMFlatNodeStore::COSMFlatNodeStore(const std::string& sTempDir, uint64_t nBufferBytes) :
                m_sTempDir(sTempDir), m_nWindowRecords((size_t)(std::max)((uint64_t)1 << 20, nBufferBytes / Windows / sizeof(SRecord))),
                m_nFileRecords(0), m_nCount(0), m_bFinished(false)
        {
            if(sizeof(void*) < 8)
                throw CommonLib::CExcBase("The flat node store needs a 64-bit build, use the sorted node store");

            if(m_sTempDir.empty())
            {
                std::error_code ec;
                m_sTempDir = std::filesystem::temp_directory_path(ec).u8string();
            }

            std::random_device rd;
            std::filesystem::path path = std::filesystem::u8path(m_sTempDir);
            path /= "graphengine_osm_" + std::to_string(rd()) + std::to_string(rd()) + "_flatnodes.bin";
            m_sFilePath = path.u8string();
            Create();
        }

        COSMFlatNodeStore::COSMFlatNodeStore(const std::string& sFilePath, bool bKeepFile, uint64_t nBufferBytes) :
                m_sFilePath(sFilePath), m_nWindowRecords((size_t)(std::max)((uint64_t)1 << 20, nBufferBytes / Windows / sizeof(SRecord))),
                m_nFileRecords(0), m_nCount(0), m_bFinished(false), m_bKeepFile(bKeepFile)
        {
            if(sizeof(void*) < 8)
                throw CommonLib::CExcBase("The flat node store needs a 64-bit build");
            Create();
        }

        void COSMFlatNodeStore::Create()
        {
            try
            {
                // the lookups read the file by their own handles
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(m_sFilePath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                     CommonLib::file::smRead, CommonLib::file::oftBinary);
                // NTFS: the gaps between the ids aren't written to the disk (a file system without sparse files fills them with zeros)
                CommonLib::file::CFileMapping::SetSparse(*m_ptrFile);
            }
            catch (std::exception& exc)
            {
                Close();
                CommonLib::CExcBase::RegenExc("Flat node store: failed to create the temporary file {0}", m_sFilePath, exc);
                throw;
            }
        }

        COSMFlatNodeStore::~COSMFlatNodeStore()
        {
            Close();
        }

        void COSMFlatNodeStore::Close()
        {
            // closed before Finish (canceled, failed): the queued windows are dropped, the writer stops after its current write
            if(!m_bFinished)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_bAbort = true;
                m_queue.clear();
            }
            try
            {
                WaitWriter();
            }
            catch (...)
            {
            }
            m_vecSegments.clear();      // unmapped before the file is closed
            if(m_ptrFile.get())
            {
                try
                {
                    m_ptrFile->CloseFile();
                }
                catch (...)
                {
                }
                m_ptrFile.reset();
            }
            if(!m_sFilePath.empty() && !m_bKeepFile)
            {
                std::error_code ec;
                std::filesystem::remove(std::filesystem::u8path(m_sFilePath), ec);
            }
        }

        COSMFlatNodeStore::SRecord COSMFlatNodeStore::Encode(double dLon, double dLat)
        {
            SRecord record;
            record.lon = (int32_t)std::lround(dLon * CoordScale);
            int64_t nLat = (int64_t)std::lround(dLat * CoordScale) + LatBias;
            record.lat = (uint32_t)(std::max)((int64_t)1, nLat);
            return record;
        }

        void COSMFlatNodeStore::Decode(const SRecord& record, double& dLon, double& dLat)
        {
            dLon = record.lon / CoordScale;
            dLat = ((int64_t)record.lat - LatBias) / CoordScale;
        }

        void COSMFlatNodeStore::Add(int64_t nId, double dLon, double dLat)
        {
            if(m_bFinished)
                throw CommonLib::CExcBase("Flat node store: a node is added after Finish");

            ++m_nCount;
            SRecord record = Encode(dLon, dLat);
            if(nId < 0)
            {
                m_negative[nId] = record;
                return;
            }

            // the window: the nodes of the ids [nStart, nStart + m_nWindowRecords); an id out of it (a sorted file: the next
            // window, an unsorted one: anywhere) - the window goes to the writer, a new one starts at the id
            uint64_t nRecord = (uint64_t)nId;
            if(!m_pCurrent)
            {
                m_vecWindows.push_back(std::unique_ptr<SWindow>(new SWindow()));
                m_pCurrent = m_vecWindows.back().get();
                m_pCurrent->records.assign(m_nWindowRecords, SRecord{0, 0});
                m_writer = std::thread(&COSMFlatNodeStore::WriterLoop, this);
            }
            if(m_pCurrent->nUsed > 0 && (nRecord < m_pCurrent->nStart || nRecord - m_pCurrent->nStart >= m_nWindowRecords))
                FlushBuffer();
            if(m_pCurrent->nUsed == 0)
                m_pCurrent->nStart = nRecord;
            size_t nIndex = (size_t)(nRecord - m_pCurrent->nStart);
            m_pCurrent->records[nIndex] = record;     // a duplicate id: the last one
            if(nIndex + 1 > m_pCurrent->nUsed)
                m_pCurrent->nUsed = nIndex + 1;
        }

        void COSMFlatNodeStore::FlushBuffer()
        {
            if(!m_pCurrent || m_pCurrent->nUsed == 0)
                return;
            m_nFileRecords = (std::max)(m_nFileRecords, m_pCurrent->nStart + m_pCurrent->nUsed);

            std::unique_lock<std::mutex> lock(m_mutex);
            if(m_writeError)
            {
                std::exception_ptr ptrError = m_writeError;
                m_writeError = nullptr;
                std::rethrow_exception(ptrError);
            }
            m_queue.push_back(m_pCurrent);
            m_pCurrent = nullptr;
            m_cvWriter.notify_one();

            // a free window, a new one while there are less than Windows, otherwise the reading waits for the disk
            if(m_free.empty() && m_vecWindows.size() < Windows)
            {
                lock.unlock();
                std::unique_ptr<SWindow> ptrWindow(new SWindow());
                ptrWindow->records.assign(m_nWindowRecords, SRecord{0, 0});
                lock.lock();
                m_vecWindows.push_back(std::move(ptrWindow));
                m_pCurrent = m_vecWindows.back().get();
                return;
            }
            // the wait is checked for the cancel 5 times per second (a window of a hard disk is written seconds)
            auto start = std::chrono::steady_clock::now();
            while(!m_cvFree.wait_for(lock, std::chrono::milliseconds(200), [this]() { return !m_free.empty() || m_writeError; }))
            {
                if(m_fnContinue && !m_fnContinue())
                {
                    m_nWaitNs += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
                    throw CommonLib::CExcBase("OSM conversion is canceled");
                }
            }
            m_nWaitNs += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            if(m_writeError)
            {
                std::exception_ptr ptrError = m_writeError;
                m_writeError = nullptr;
                std::rethrow_exception(ptrError);
            }
            m_pCurrent = m_free.back();
            m_free.pop_back();
        }

        void COSMFlatNodeStore::WriterLoop()
        {
            for(;;)
            {
                SWindow* pWindow = nullptr;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cvWriter.wait(lock, [this]() { return m_bStopWriter || !m_queue.empty(); });
                    if(m_queue.empty())
                        return;     // stopped, everything is written
                    pWindow = m_queue.front();
                    m_queue.pop_front();
                    ++m_nWriting;
                }
                std::exception_ptr ptrError;
                try
                {
                    WriteWindow(*pWindow);
                }
                catch (...)
                {
                    ptrError = std::current_exception();
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    --m_nWriting;
                    if(ptrError && !m_writeError)
                        m_writeError = ptrError;
                    m_free.push_back(pWindow);
                }
                m_cvFree.notify_all();
            }
        }

        void COSMFlatNodeStore::WaitWriter()
        {
            if(m_writer.joinable())
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bStopWriter = true;
                }
                m_cvWriter.notify_all();
                m_writer.join();    // the queue is written first
            }
            if(m_writeError)
            {
                std::exception_ptr ptrError = m_writeError;
                m_writeError = nullptr;
                std::rethrow_exception(ptrError);
            }
        }

        void COSMFlatNodeStore::WriteWindow(SWindow& window)
        {
            auto start = std::chrono::steady_clock::now();
            const std::vector<SRecord>& records = window.records;
            const size_t nUsed = window.nUsed;
            size_t i = 0;
            while(i < nUsed)
            {
                while(i < nUsed && records[i].lat == 0)
                    ++i;
                if(i >= nUsed)
                    break;
                // a run: the nodes with the gaps up to MaxGap between them (a longer gap stays a hole of the sparse file)
                size_t nRunStart = i, nRunEnd = i + 1;
                for(size_t j = i + 1; j < nUsed && j - nRunEnd <= MaxGap; ++j)
                {
                    if(records[j].lat != 0)
                        nRunEnd = j + 1;
                }
                for(size_t nPos = nRunStart; nPos < nRunEnd; )
                {
                    if(m_bAbort)
                        return;     // closed before Finish: the file is deleted anyway
                    size_t nChunk = (std::min)(nRunEnd - nPos, MaxWriteBytes / sizeof(SRecord));
                    size_t nBytes = nChunk * sizeof(SRecord);
                    m_ptrFile->SetFilePos64((window.nStart + nPos) * sizeof(SRecord), CommonLib::soFromBegin);
                    if((size_t)m_ptrFile->Write((const byte_t*)(records.data() + nPos), nBytes) != nBytes)
                        throw CommonLib::CExcBase("Flat node store: failed to write the file {0} (no disk space?)", m_sFilePath);
                    m_nBytesWritten += nBytes;
                    nPos += nChunk;
                }
                i = nRunEnd;
            }
            // the written data go to the disk now: gigabytes of dirty pages in the system cache would be written later
            // at once by the system, stopping the reading of the OSM file meanwhile
            m_ptrFile->Flush();
            // cleared for the next window
            std::fill(window.records.begin(), window.records.begin() + (std::ptrdiff_t)nUsed, SRecord{0, 0});
            window.nUsed = 0;
            m_nIONs += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        }

        void COSMFlatNodeStore::Finish(TProgress progress)
        {
            if(m_bFinished)
                return;
            try
            {
                FlushBuffer();
                WaitWriter();
                m_pCurrent = nullptr;
                m_queue.clear();
                m_free.clear();
                m_vecWindows.clear();   // the memory of the buffer
                m_vecSegments.resize((size_t)((m_nFileRecords + ((uint64_t)1 << SegmentBits) - 1) >> SegmentBits));
                // all the segments are mapped now (address space only): the lookups of the threads don't change the store
                for(size_t i = 0; i < m_vecSegments.size(); ++i)
                    Segment(i);
                m_bFinished = true;
                if(progress)
                    progress(m_nCount);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Flat node store: failed to finish the file {0}", m_sFilePath, exc);
                throw;
            }
        }

        const COSMFlatNodeStore::SRecord* COSMFlatNodeStore::Segment(uint64_t nSegment)
        {
            std::unique_ptr<CommonLib::file::CFileMapping>& ptrSegment = m_vecSegments[(size_t)nSegment];
            if(!ptrSegment.get())
            {
                uint64_t nFirst = nSegment << SegmentBits;
                uint64_t nRecords = (std::min)((uint64_t)1 << SegmentBits, m_nFileRecords - nFirst);
                std::unique_ptr<CommonLib::file::CFileMapping> ptrMapping(new CommonLib::file::CFileMapping());
                ptrMapping->Map(*m_ptrFile, nFirst * sizeof(SRecord), (size_t)(nRecords * sizeof(SRecord)), false);
                ptrSegment = std::move(ptrMapping);
            }
            return (const SRecord*)ptrSegment->Data();
        }

        const COSMFlatNodeStore::SRecord* COSMFlatNodeStore::Find(int64_t nId) const
        {
            if(nId < 0)
            {
                auto it = m_negative.find(nId);
                return it != m_negative.end() ? &it->second : nullptr;
            }
            if((uint64_t)nId >= m_nFileRecords)
                return nullptr;
            const std::unique_ptr<CommonLib::file::CFileMapping>& ptrSegment = m_vecSegments[(size_t)((uint64_t)nId >> SegmentBits)];
            const SRecord& record = ((const SRecord*)ptrSegment->Data())[(size_t)((uint64_t)nId & (((uint64_t)1 << SegmentBits) - 1))];
            return record.lat != 0 ? &record : nullptr;
        }

        bool COSMFlatNodeStore::GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) const
        {
            const SRecord* pRecord = Find(nId);
            if(!pRecord)
                return false;
            nLon = pRecord->lon;
            nLat = (int32_t)((int64_t)pRecord->lat - LatBias);
            return true;
        }

        // A lookup of one thread: its own file handle and a cache of the ranges read from the file.
        // The ids are read in ranges: the ids of a sorted request closer than MergeGap records go to one read (the gaps are
        // read too, it is cheaper than another read), up to MaxRead records, aligned to 4 KB. The ranges stay in the cache
        // (LRU, nCacheBytes): the next requests (the ways of the next batch have the nodes near the same ids) find them there.
        class COSMFlatNodeStore::CLookup : public IOSMNodeLookup
        {
        public:
            // a seek of a hard disk costs as much as reading ~1.5 MB: the ids up to 1 MB apart are read by one read
            static const uint64_t MergeGap = 128 * 1024;    // records, 1 MB
            static const uint64_t MaxRead = 4 * 1024 * 1024;  // records, 32 MB
            static const uint64_t Align = 512;              // records, 4 KB

            CLookup(const COSMFlatNodeStore& store, uint64_t nCacheBytes) : m_store(store),
                    m_nCacheBytes((std::max)((uint64_t)16 * 1024 * 1024, nCacheBytes))
            {
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(store.m_sFilePath.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead,
                                                                     (CommonLib::file::enShareMode)(CommonLib::file::smRead | CommonLib::file::smWrite),
                                                                     CommonLib::file::oftBinary);
            }

            virtual bool Get(int64_t nId, double& dLon, double& dLat) override
            {
                int32_t nLon = 0, nLat = 0;
                if(!GetE7(nId, nLon, nLat))
                    return false;
                dLon = nLon / CoordScale;
                dLat = nLat / CoordScale;
                return true;
            }

            virtual bool GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) override
            {
                uint8_t bFound = 0;
                GetSortedE7(&nId, 1, &nLon, &nLat, &bFound);
                return bFound != 0;
            }

            virtual void GetSortedE7(const int64_t* pIds, size_t nCount, int32_t* pLons, int32_t* pLats, uint8_t* pFound) override
            {
                const uint64_t nFileRecords = m_store.m_nFileRecords;
                size_t i = 0;
                while(i < nCount)
                {
                    int64_t nId = pIds[i];
                    if(nId < 0 || (uint64_t)nId >= nFileRecords)
                    {
                        // the negative ids are in memory, beyond the file - no node
                        pFound[i] = nId < 0 && m_store.GetE7(nId, pLons[i], pLats[i]) ? 1 : 0;
                        ++i;
                        continue;
                    }

                    // a cached range with the id, otherwise a new read of the ids around
                    const SRange* pRange = FindRange((uint64_t)nId);
                    if(pRange)
                        ++m_stats.nCacheHits;
                    else
                        pRange = ReadRange(pIds, nCount, i, nFileRecords);

                    // all the ids of the request in the range
                    while(i < nCount && pIds[i] >= 0 && (uint64_t)pIds[i] >= pRange->nStart && (uint64_t)pIds[i] < pRange->nEnd)
                    {
                        const SRecord& record = pRange->records[(size_t)((uint64_t)pIds[i] - pRange->nStart)];
                        pFound[i] = record.lat != 0 ? 1 : 0;
                        if(record.lat != 0)
                        {
                            pLons[i] = record.lon;
                            pLats[i] = (int32_t)((int64_t)record.lat - LatBias);
                        }
                        ++i;
                    }
                }
            }

            virtual SOSMLookupStats TakeStats() override
            {
                SOSMLookupStats stats = m_stats;
                m_stats = SOSMLookupStats();
                return stats;
            }

        private:
            struct SRange
            {
                uint64_t nStart = 0;
                uint64_t nEnd = 0;
                uint64_t nLastUse = 0;
                std::vector<SRecord> records;
            };

            const SRange* FindRange(uint64_t nRecord)
            {
                auto it = m_ranges.upper_bound(nRecord);
                if(it == m_ranges.begin())
                    return nullptr;
                --it;
                if(nRecord >= it->second.nEnd)
                    return nullptr;
                it->second.nLastUse = ++m_nUse;
                return &it->second;
            }

            // the ids from pIds[i] which are close to each other in one read (up to the next cached range)
            const SRange* ReadRange(const int64_t* pIds, size_t nCount, size_t i, uint64_t nFileRecords)
            {
                uint64_t nStart = (uint64_t)pIds[i] / Align * Align;
                uint64_t nEnd = (uint64_t)pIds[i] + 1;
                for(size_t j = i + 1; j < nCount; ++j)
                {
                    uint64_t nNext = (uint64_t)pIds[j];
                    if(nNext < nEnd)
                        continue;   // a duplicate
                    if(nNext - nEnd > MergeGap || nNext + 1 - nStart > MaxRead)
                        break;
                    nEnd = nNext + 1;
                }
                nEnd = (std::min)((nEnd + Align - 1) / Align * Align, nFileRecords);
                auto itNext = m_ranges.upper_bound(nStart);
                if(itNext != m_ranges.end() && itNext->first < nEnd && itNext->first > (uint64_t)pIds[i])
                    nEnd = itNext->first;     // the rest is cached already

                SRange range;
                range.nStart = nStart;
                range.nEnd = nEnd;
                range.nLastUse = ++m_nUse;
                range.records.resize((size_t)(nEnd - nStart));
                size_t nBytes = range.records.size() * sizeof(SRecord);
                auto start = std::chrono::steady_clock::now();
                m_ptrFile->SetFilePos64(nStart * sizeof(SRecord), CommonLib::soFromBegin);
                if((size_t)m_ptrFile->Read((byte_t*)range.records.data(), nBytes) != nBytes)
                    throw CommonLib::CExcBase("Flat node store: failed to read the file {0}", m_store.m_sFilePath);
                m_stats.dReadSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                ++m_stats.nReads;
                m_stats.nBytesRead += nBytes;

                // the least recently used ranges leave the cache
                m_nCachedBytes += nBytes;
                while(m_nCachedBytes > m_nCacheBytes && !m_ranges.empty())
                {
                    auto itOld = m_ranges.begin();
                    for(auto it = m_ranges.begin(); it != m_ranges.end(); ++it)
                    {
                        if(it->second.nLastUse < itOld->second.nLastUse)
                            itOld = it;
                    }
                    m_nCachedBytes -= itOld->second.records.size() * sizeof(SRecord);
                    m_ranges.erase(itOld);
                }
                auto itOld = m_ranges.find(nStart);
                if(itOld != m_ranges.end())
                {
                    m_nCachedBytes -= itOld->second.records.size() * sizeof(SRecord);
                    m_ranges.erase(itOld);
                }
                return &(m_ranges[nStart] = std::move(range));
            }

        private:
            const COSMFlatNodeStore&   m_store;
            CommonLib::file::TFilePtr  m_ptrFile;
            uint64_t                   m_nCacheBytes;
            uint64_t                   m_nCachedBytes = 0;
            uint64_t                   m_nUse = 0;
            std::map<uint64_t, SRange> m_ranges;   // by the first record
            SOSMLookupStats            m_stats;
        };

        std::unique_ptr<IOSMNodeLookup> COSMFlatNodeStore::CreateLookup(uint64_t nCacheBytes) const
        {
            if(!m_bFinished)
                throw CommonLib::CExcBase("Flat node store: Finish isn't called");
            return std::unique_ptr<IOSMNodeLookup>(new CLookup(*this, nCacheBytes));
        }

        bool COSMFlatNodeStore::Get(int64_t nId, double& dLon, double& dLat)
        {
            if(!m_bFinished)
                throw CommonLib::CExcBase("Flat node store: Get is called before Finish");

            if(nId < 0)
            {
                auto it = m_negative.find(nId);
                if(it == m_negative.end())
                    return false;
                Decode(it->second, dLon, dLat);
                return true;
            }
            if((uint64_t)nId >= m_nFileRecords)
                return false;

            const SRecord& record = Segment((uint64_t)nId >> SegmentBits)[(size_t)((uint64_t)nId & (((uint64_t)1 << SegmentBits) - 1))];
            if(record.lat == 0)
                return false;
            Decode(record, dLon, dLat);
            return true;
        }

        bool COSMFlatNodeStore::DecodeE7(const SRecord& record, int32_t& nLon, int32_t& nLat)
        {
            if(record.lat == 0)
                return false;
            nLon = record.lon;
            nLat = (int32_t)((int64_t)record.lat - LatBias);
            return true;
        }

        // ---------------- COSMNodeSegment

        void COSMNodeSegment::Load(const std::string& sFilePath, uint64_t nStart, uint64_t nCount, const std::function<bool(uint64_t)>& progress)
        {
            typedef COSMFlatNodeStore::SRecord SRecord;
            if(nCount > m_nCapacity)
            {
                m_ptrData.reset();
                m_ptrData.reset(new SRecord[(size_t)nCount]);    // not initialized: read from the file
                m_nCapacity = nCount;
            }
            m_nStart = nStart;
            m_nCount = nCount;

            CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFileA(sFilePath.c_str(), CommonLib::file::ofmOpenExisting,
                    CommonLib::file::arRead, (CommonLib::file::enShareMode)(CommonLib::file::smRead | CommonLib::file::smWrite), CommonLib::file::oftBinary);
            uint64_t nFileRecords = ptrFile->GetFileSize() / sizeof(SRecord);
            uint64_t nInFile = nStart < nFileRecords ? (std::min)(nCount, nFileRecords - nStart) : 0;
            // after the end of the file: no nodes
            if(nInFile < nCount)
                memset(m_ptrData.get() + nInFile, 0, (size_t)(nCount - nInFile) * sizeof(SRecord));

            auto start = std::chrono::steady_clock::now();
            const size_t nChunkRecords = MaxWriteBytes / sizeof(SRecord);
            ptrFile->SetFilePos64(nStart * sizeof(SRecord), CommonLib::soFromBegin);
            for(uint64_t nPos = 0; nPos < nInFile; )
            {
                size_t nChunk = (size_t)(std::min)((uint64_t)nChunkRecords, nInFile - nPos);
                size_t nBytes = nChunk * sizeof(SRecord);
                if((size_t)ptrFile->Read((byte_t*)(m_ptrData.get() + nPos), nBytes) != nBytes)
                    throw CommonLib::CExcBase("Failed to read the node file {0}", sFilePath);
                nPos += nChunk;
                m_nBytesRead += nBytes;
                if(progress && !progress(nPos * sizeof(SRecord)))
                    throw CommonLib::CExcBase("OSM splitting is canceled");
            }
            m_dReadSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }

        void COSMNodeSegment::Free()
        {
            m_ptrData.reset();
            m_nCapacity = m_nCount = 0;
        }
    }
}
