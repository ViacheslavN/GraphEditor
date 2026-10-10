#include "OSMFlatNodeStore.h"
#include "../../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const double CoordScale = 1e7;
            const int64_t LatBias = 1000000000;         // -90..90 degrees -> 100 000 000..1 900 000 000, 0 is free for "no node"
            const size_t BufferRecords = 1024 * 1024;   // 8 MB
        }

        COSMFlatNodeStore::COSMFlatNodeStore(const std::string& sTempDir) :
                m_sTempDir(sTempDir), m_nBufferStart(0), m_nFileRecords(0), m_nCount(0), m_bFinished(false)
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
            try
            {
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(m_sFilePath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                     CommonLib::file::smNoMode, CommonLib::file::oftBinary);
                // NTFS: the gaps between the ids aren't written to the disk (a file system without sparse files fills them with zeros)
                CommonLib::file::CFileMapping::SetSparse(*m_ptrFile);
            }
            catch (std::exception& exc)
            {
                Close();
                CommonLib::CExcBase::RegenExc("Flat node store: failed to create the temporary file {0}", m_sFilePath, exc);
                throw;
            }
            m_buffer.reserve(BufferRecords);
        }

        COSMFlatNodeStore::~COSMFlatNodeStore()
        {
            Close();
        }

        void COSMFlatNodeStore::Close()
        {
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
            if(!m_sFilePath.empty())
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

            uint64_t nRecord = (uint64_t)nId;
            if(!m_buffer.empty())
            {
                uint64_t nEnd = m_nBufferStart + m_buffer.size();
                if(nRecord >= m_nBufferStart && nRecord < nEnd)
                {
                    m_buffer[(size_t)(nRecord - m_nBufferStart)] = record;   // a duplicate id
                    return;
                }
                // the next id of a sorted file: the small gap is filled with empty records, otherwise the buffer is written
                // and a new run starts (the gap stays a hole of the sparse file)
                if(nRecord < nEnd || nRecord - nEnd > MaxGap || m_buffer.size() + (nRecord - nEnd) >= BufferRecords)
                    FlushBuffer();
                else
                    m_buffer.resize(m_buffer.size() + (size_t)(nRecord - nEnd), SRecord{0, 0});
            }
            if(m_buffer.empty())
                m_nBufferStart = nRecord;
            m_buffer.push_back(record);
        }

        void COSMFlatNodeStore::FlushBuffer()
        {
            if(m_buffer.empty())
                return;
            m_ptrFile->SetFilePos64(m_nBufferStart * sizeof(SRecord), CommonLib::soFromBegin);
            size_t nBytes = m_buffer.size() * sizeof(SRecord);
            if((size_t)m_ptrFile->Write((const byte_t*)m_buffer.data(), nBytes) != nBytes)
                throw CommonLib::CExcBase("Flat node store: failed to write the temporary file (no disk space?)");
            m_nFileRecords = (std::max)(m_nFileRecords, m_nBufferStart + m_buffer.size());
            m_buffer.clear();
        }

        void COSMFlatNodeStore::Finish(TProgress progress)
        {
            if(m_bFinished)
                return;
            try
            {
                FlushBuffer();
                std::vector<SRecord>().swap(m_buffer);
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

        namespace
        {
            class CFlatLookup : public IOSMNodeLookup
            {
            public:
                explicit CFlatLookup(const COSMFlatNodeStore& store) : m_store(store) {}

                virtual bool Get(int64_t nId, double& dLon, double& dLat) override
                {
                    int32_t nLon = 0, nLat = 0;
                    if(!m_store.GetE7(nId, nLon, nLat))
                        return false;
                    dLon = nLon / CoordScale;
                    dLat = nLat / CoordScale;
                    return true;
                }

                virtual bool GetE7(int64_t nId, int32_t& nLon, int32_t& nLat) override
                {
                    return m_store.GetE7(nId, nLon, nLat);
                }

            private:
                const COSMFlatNodeStore& m_store;
            };
        }

        std::unique_ptr<IOSMNodeLookup> COSMFlatNodeStore::CreateLookup(uint64_t /*nCacheBytes*/) const
        {
            if(!m_bFinished)
                throw CommonLib::CExcBase("Flat node store: Finish isn't called");
            return std::unique_ptr<IOSMNodeLookup>(new CFlatLookup(*this));
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
    }
}
