#include "OSMSplitFormat.h"
#include "../../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace GraphEngine {
    namespace Convertors {

        namespace OSMSplit
        {
            const char* MetaFile = "osm_split.xml";
            const char* StringsFile = "strings.dat";
            const char* NodesFlatFile = "nodes.flat";
            const char* NodesSortedFile = "nodes.sorted";
            const char* NodesData = "nodes.dat";
            const char* NodesIndex = "nodes.idx";
            const char* WaysData = "ways.dat";
            const char* WaysIndex = "ways.idx";
            const char* RelationsData = "relations.dat";
            const char* RelationsIndex = "relations.idx";

            std::string FilePath(const std::string& sFolder, const char* pszName)
            {
                return (std::filesystem::u8path(sFolder) / pszName).u8string();
            }

            std::string FolderOf(const std::string& sMetaPath)
            {
                return std::filesystem::u8path(sMetaPath).parent_path().u8string();
            }
        }

        namespace
        {
            typedef std::chrono::steady_clock TClock;
            double Since(TClock::time_point start)
            {
                return std::chrono::duration<double>(TClock::now() - start).count();
            }

            const size_t WriteBufferSize = 4 * 1024 * 1024;
        }

        void COSMRecordReader::Fail()
        {
            throw CommonLib::CExcBase("OSM split data: a record is damaged (truncated)");
        }

        // ---------------- COSMDataFile

        COSMDataFile::~COSMDataFile()
        {
            try
            {
                Close();
            }
            catch (...)
            {
            }
        }

        void COSMDataFile::Create(const std::string& sPath)
        {
            Close();
            m_sPath = sPath;
            m_nFileSize = 0;
            try
            {
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(sPath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                     CommonLib::file::smRead, CommonLib::file::oftBinary);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create the file {0}", sPath, exc);
                throw;
            }
            m_writeBuffer.reserve(WriteBufferSize);
        }

        void COSMDataFile::Open(const std::string& sPath)
        {
            Close();
            m_sPath = sPath;
            try
            {
                m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(sPath.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead,
                                                                     CommonLib::file::smRead, CommonLib::file::oftBinary);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open the file {0}", sPath, exc);
                throw;
            }
            m_nFileSize = m_ptrFile->GetFileSize();
            Rewind();
        }

        void COSMDataFile::Close()
        {
            if(!m_ptrFile.get())
                return;
            Flush();
            m_ptrFile->CloseFile();
            m_ptrFile.reset();
        }

        void COSMDataFile::Append(const void* pData, size_t nSize)
        {
            const uint8_t* p = (const uint8_t*)pData;
            if(m_writeBuffer.size() + nSize > WriteBufferSize)
                Flush();
            if(nSize >= WriteBufferSize)
            {
                TClock::time_point start = TClock::now();
                m_ptrFile->SetFilePos64(m_nFileSize, CommonLib::soFromBegin);
                if((size_t)m_ptrFile->Write((const byte_t*)p, nSize) != nSize)
                    throw CommonLib::CExcBase("Failed to write the file {0} (no disk space?)", m_sPath);
                m_nFileSize += nSize;
                m_dIOSeconds += Since(start);
                return;
            }
            m_writeBuffer.insert(m_writeBuffer.end(), p, p + nSize);
        }

        uint64_t COSMDataFile::AppendRecord(const COSMRecordBuffer& record)
        {
            uint64_t nOffset = Size();
            uint8_t len[10];
            size_t n = 0;
            uint64_t nValue = record.Size();
            while(nValue >= 0x80)
            {
                len[n++] = (uint8_t)(nValue | 0x80);
                nValue >>= 7;
            }
            len[n++] = (uint8_t)nValue;
            Append(len, n);
            Append(record.Data().data(), record.Size());
            return nOffset;
        }

        void COSMDataFile::Flush()
        {
            if(m_writeBuffer.empty() || !m_ptrFile.get())
                return;
            TClock::time_point start = TClock::now();
            m_ptrFile->SetFilePos64(m_nFileSize, CommonLib::soFromBegin);
            if((size_t)m_ptrFile->Write((const byte_t*)m_writeBuffer.data(), m_writeBuffer.size()) != m_writeBuffer.size())
                throw CommonLib::CExcBase("Failed to write the file {0} (no disk space?)", m_sPath);
            m_nFileSize += m_writeBuffer.size();
            m_writeBuffer.clear();
            m_dIOSeconds += Since(start);
        }

        void COSMDataFile::Rewind()
        {
            Flush();
            m_readBuffer.clear();
            m_nReadPos = 0;
            m_nFileReadPos = 0;
        }

        // at least nNeed bytes after m_nReadPos (fewer at the end of the file), false - nothing left
        bool COSMDataFile::FillReadBuffer(size_t nNeed)
        {
            size_t nLeft = m_readBuffer.size() - m_nReadPos;
            if(nLeft >= nNeed)
                return true;
            if(m_nFileReadPos >= m_nFileSize)
                return nLeft > 0;

            // the rest moves to the start, the buffer is filled from the file
            if(m_nReadPos > 0)
            {
                m_readBuffer.erase(m_readBuffer.begin(), m_readBuffer.begin() + (std::ptrdiff_t)m_nReadPos);
                m_nReadPos = 0;
            }
            size_t nChunk = (size_t)(std::min)((uint64_t)(std::max)(m_nReadBufferSize, nNeed), m_nFileSize - m_nFileReadPos);
            size_t nOld = m_readBuffer.size();
            m_readBuffer.resize(nOld + nChunk);
            TClock::time_point start = TClock::now();
            m_ptrFile->SetFilePos64(m_nFileReadPos, CommonLib::soFromBegin);
            if((size_t)m_ptrFile->Read((byte_t*)m_readBuffer.data() + nOld, nChunk) != nChunk)
                throw CommonLib::CExcBase("Failed to read the file {0}", m_sPath);
            m_dIOSeconds += Since(start);
            m_nFileReadPos += nChunk;
            m_nBytesRead += nChunk;
            return true;
        }

        bool COSMDataFile::ReadRecord(std::vector<uint8_t>& record, uint64_t* pOffset)
        {
            if(!FillReadBuffer(10))
                return false;
            uint64_t nOffset = m_nFileReadPos - (m_readBuffer.size() - m_nReadPos);
            COSMRecordReader reader(m_readBuffer.data() + m_nReadPos, m_readBuffer.size() - m_nReadPos);
            uint64_t nSize = reader.Varint();
            size_t nHead = 1;
            for(uint64_t n = nSize; n >= 0x80; n >>= 7)
                ++nHead;
            if(!FillReadBuffer(nHead + (size_t)nSize) || m_readBuffer.size() - m_nReadPos < nHead + nSize)
                throw CommonLib::CExcBase("The file {0} is truncated", m_sPath);
            record.assign(m_readBuffer.begin() + (std::ptrdiff_t)(m_nReadPos + nHead), m_readBuffer.begin() + (std::ptrdiff_t)(m_nReadPos + nHead + nSize));
            m_nReadPos += nHead + (size_t)nSize;
            if(pOffset)
                *pOffset = nOffset;
            return true;
        }

        bool COSMDataFile::NextRecord(const uint8_t*& pRecord, size_t& nSize, uint64_t* pOffset)
        {
            if(!FillReadBuffer(10))
                return false;
            uint64_t nOffset = m_nFileReadPos - (m_readBuffer.size() - m_nReadPos);
            COSMRecordReader reader(m_readBuffer.data() + m_nReadPos, m_readBuffer.size() - m_nReadPos);
            uint64_t nPayload = reader.Varint();
            size_t nHead = 1;
            for(uint64_t n = nPayload; n >= 0x80; n >>= 7)
                ++nHead;
            if(!FillReadBuffer(nHead + (size_t)nPayload) || m_readBuffer.size() - m_nReadPos < nHead + nPayload)
                throw CommonLib::CExcBase("The file {0} is truncated", m_sPath);
            pRecord = m_readBuffer.data() + m_nReadPos + nHead;
            nSize = (size_t)nPayload;
            m_nReadPos += nHead + (size_t)nPayload;
            if(pOffset)
                *pOffset = nOffset;
            return true;
        }

        bool COSMDataFile::Read(void* pData, size_t nSize)
        {
            if(!FillReadBuffer(nSize))
                return false;
            if(m_readBuffer.size() - m_nReadPos < nSize)
                throw CommonLib::CExcBase("The file {0} is truncated", m_sPath);
            memcpy(pData, m_readBuffer.data() + m_nReadPos, nSize);
            m_nReadPos += nSize;
            return true;
        }

        void COSMDataFile::ReadAt(uint64_t nPos, void* pData, size_t nSize)
        {
            if(nPos + nSize > m_nFileSize)
                throw CommonLib::CExcBase("The file {0} is truncated", m_sPath);
            TClock::time_point start = TClock::now();
            m_ptrFile->SetFilePos64(nPos, CommonLib::soFromBegin);
            if((size_t)m_ptrFile->Read((byte_t*)pData, nSize) != nSize)
                throw CommonLib::CExcBase("Failed to read the file {0}", m_sPath);
            m_dIOSeconds += Since(start);
            m_nBytesRead += nSize;
        }

        void COSMDataFile::ReadRecordAt(uint64_t nOffset, std::vector<uint8_t>& record)
        {
            // the length (up to 10 bytes), then the payload
            uint8_t head[10];
            size_t nHeadRead = (size_t)(std::min)((uint64_t)sizeof(head), m_nFileSize - (std::min)(nOffset, m_nFileSize));
            ReadAt(nOffset, head, nHeadRead);
            COSMRecordReader reader(head, nHeadRead);
            uint64_t nSize = reader.Varint();
            size_t nHead = 1;
            for(uint64_t n = nSize; n >= 0x80; n >>= 7)
                ++nHead;
            record.resize((size_t)nSize);
            if(nSize)
                ReadAt(nOffset + nHead, record.data(), (size_t)nSize);
        }

        // ---------------- COSMStringTable

        uint32_t COSMStringTable::Index(const char* psz)
        {
            m_sKey.assign(psz ? psz : "");
            auto it = m_mapIndex.find(m_sKey);
            if(it != m_mapIndex.end())
                return it->second;
            uint32_t nIndex = (uint32_t)m_vecStrings.size();
            m_vecStrings.push_back(m_sKey);
            m_mapIndex.emplace(m_sKey, nIndex);
            return nIndex;
        }

        const char* COSMStringTable::Get(uint64_t nIndex) const
        {
            if(nIndex >= m_vecStrings.size())
                throw CommonLib::CExcBase("OSM split data: string {0} isn't in the dictionary", nIndex);
            return m_vecStrings[(size_t)nIndex].c_str();
        }

        void COSMStringTable::Save(const std::string& sPath) const
        {
            COSMDataFile file;
            file.Create(sPath);
            COSMRecordBuffer buffer;
            buffer.PutVarint(m_vecStrings.size());
            for(const std::string& s : m_vecStrings)
                buffer.PutCString(s.c_str());
            file.Append(buffer.Data().data(), buffer.Size());
            file.Close();
        }

        void COSMStringTable::Load(const std::string& sPath)
        {
            COSMDataFile file;
            file.Open(sPath);
            std::vector<uint8_t> data((size_t)file.Size());
            if(!data.empty())
                file.ReadAt(0, data.data(), data.size());
            COSMRecordReader reader(data.data(), data.size());
            uint64_t nCount = reader.Varint();
            m_vecStrings.clear();
            m_mapIndex.clear();
            m_vecStrings.reserve((size_t)nCount);
            for(uint64_t i = 0; i < nCount; ++i)
            {
                m_vecStrings.push_back(reader.CString());
                m_mapIndex.emplace(m_vecStrings.back(), (uint32_t)i);
            }
        }

        // ---------------- index

        void COSMIdIndexWriter::Create(const std::string& sPath)
        {
            m_file.Create(sPath);
            m_nCount = 0;
            m_nLastId = INT64_MIN;
            m_bSorted = true;
        }

        void COSMIdIndexWriter::Add(int64_t nId, uint64_t nOffset)
        {
            if(nId <= m_nLastId)
                m_bSorted = false;
            m_nLastId = nId;
            int64_t entry[2] = {nId, (int64_t)nOffset};
            m_file.Append(entry, sizeof(entry));
            ++m_nCount;
        }

        void COSMIdIndexWriter::Finish()
        {
            m_file.Flush();
            if(!m_bSorted && m_nCount > 0)
            {
                // an unsorted file (an editor's file, usually small): sorted in memory, the last record of an id stays
                std::vector<std::pair<int64_t, uint64_t> > entries((size_t)m_nCount);
                m_file.ReadAt(0, entries.data(), entries.size() * sizeof(entries[0]));
                std::stable_sort(entries.begin(), entries.end(), [](const std::pair<int64_t, uint64_t>& a, const std::pair<int64_t, uint64_t>& b)
                                 { return a.first < b.first; });
                std::vector<std::pair<int64_t, uint64_t> > unique;
                unique.reserve(entries.size());
                for(size_t i = 0; i < entries.size(); ++i)
                {
                    if(!unique.empty() && unique.back().first == entries[i].first)
                        unique.back() = entries[i];
                    else
                        unique.push_back(entries[i]);
                }
                std::string sPath = m_file.Path();
                m_file.Create(sPath);
                m_file.Append(unique.data(), unique.size() * sizeof(unique[0]));
                m_nCount = unique.size();
                m_bSorted = true;
            }
            m_file.Close();
        }

        void COSMIdIndex::Open(const std::string& sPath, size_t nCacheBlocks)
        {
            m_file.Open(sPath);
            m_nCount = m_file.Size() / sizeof(SEntry);
            m_cache.assign((std::max)((size_t)16, nCacheBlocks), SCachedBlock());

            // the first id of every block: one entry per block is read
            uint64_t nBlocks = (m_nCount + BlockSize - 1) / BlockSize;
            m_vecFirstIds.resize((size_t)nBlocks);
            for(uint64_t b = 0; b < nBlocks; ++b)
            {
                SEntry entry;
                m_file.ReadAt(b * BlockSize * sizeof(SEntry), &entry, sizeof(entry));
                m_vecFirstIds[(size_t)b] = entry.nId;
            }
        }

        bool COSMIdIndex::Find(int64_t nId, uint64_t& nOffset)
        {
            if(m_vecFirstIds.empty() || nId < m_vecFirstIds.front())
                return false;
            uint64_t nBlock = (uint64_t)(std::upper_bound(m_vecFirstIds.begin(), m_vecFirstIds.end(), nId) - m_vecFirstIds.begin()) - 1;
            SCachedBlock& cached = m_cache[(size_t)(nBlock % m_cache.size())];
            if(cached.nBlock != nBlock)
            {
                uint64_t nFirst = nBlock * BlockSize;
                size_t nEntries = (size_t)(std::min)((uint64_t)BlockSize, m_nCount - nFirst);
                cached.nBlock = UINT64_MAX;
                cached.entries.resize(nEntries);
                m_file.ReadAt(nFirst * sizeof(SEntry), cached.entries.data(), nEntries * sizeof(SEntry));
                cached.nBlock = nBlock;
            }
            auto it = std::lower_bound(cached.entries.begin(), cached.entries.end(), nId, [](const SEntry& e, int64_t id) { return e.nId < id; });
            if(it == cached.entries.end() || it->nId != nId)
                return false;
            nOffset = it->nOffset;
            return true;
        }

        // ---------------- records

        void COSMSplitCodec::EncodeTags(COSMRecordBuffer& record, COSMStringTable& strings, const readosm_tag* pTags, int nTags)
        {
            record.PutVarint(nTags > 0 ? (uint64_t)nTags : 0);
            for(int i = 0; i < nTags; ++i)
            {
                record.PutVarint(strings.Index(pTags[i].key));
                record.PutCString(pTags[i].value);
            }
        }

        void COSMSplitCodec::DecodeTags(COSMRecordReader& reader, const COSMStringTable& strings, SOSMSplitObject& object)
        {
            uint64_t nTags = reader.Varint();
            object.tags.resize((size_t)nTags);
            for(uint64_t i = 0; i < nTags; ++i)
            {
                const char* pszKey = strings.Get(reader.Varint());
                const char* pszValue = reader.CString();
                object.tags[(size_t)i] = readosm_tag{pszKey, pszValue};
            }
        }

        void COSMSplitCodec::EncodeNode(COSMRecordBuffer& record, COSMStringTable& strings, int64_t nId, int32_t nLon, int32_t nLat,
                                        const readosm_tag* pTags, int nTags)
        {
            record.Clear();
            record.PutZigzag(nId);
            record.PutZigzag(nLon);
            record.PutZigzag(nLat);
            EncodeTags(record, strings, pTags, nTags);
        }

        void COSMSplitCodec::EncodeWayHead(COSMRecordBuffer& record, COSMStringTable& strings, int64_t nId, uint32_t nFlags,
                                           const readosm_tag* pTags, int nTags)
        {
            record.Clear();
            record.PutZigzag(nId);
            record.PutVarint(nFlags);
            EncodeTags(record, strings, pTags, nTags);
        }

        void COSMSplitCodec::EncodePoints(COSMRecordBuffer& record, const int32_t* pLons, const int32_t* pLats, size_t nPoints)
        {
            record.PutVarint(nPoints);
            int64_t nLastLon = 0, nLastLat = 0;
            for(size_t i = 0; i < nPoints; ++i)
            {
                record.PutZigzag(pLons[i] - nLastLon);
                record.PutZigzag(pLats[i] - nLastLat);
                nLastLon = pLons[i];
                nLastLat = pLats[i];
            }
        }

        void COSMSplitCodec::EncodeWay(COSMRecordBuffer& record, COSMStringTable& strings, int64_t nId, uint32_t nFlags,
                                       const readosm_tag* pTags, int nTags, const int32_t* pLons, const int32_t* pLats, size_t nPoints)
        {
            EncodeWayHead(record, strings, nId, nFlags, pTags, nTags);
            EncodePoints(record, pLons, pLats, nPoints);
        }

        void COSMSplitCodec::EncodeRelation(COSMRecordBuffer& record, COSMStringTable& strings, const readosm_relation& relation)
        {
            record.Clear();
            record.PutZigzag(relation.id);
            EncodeTags(record, strings, relation.tags, relation.tag_count);
            int nMembers = relation.member_count > 0 ? relation.member_count : 0;
            record.PutVarint((uint64_t)nMembers);
            int64_t nLastId = 0;
            for(int i = 0; i < nMembers; ++i)
            {
                const readosm_member& member = relation.members[i];
                record.PutByte((uint8_t)OSMMemberTypeChar(member.member_type));
                record.PutZigzag(member.id - nLastId);
                nLastId = member.id;
                record.PutVarint(strings.Index(member.role));
            }
        }

        void COSMSplitCodec::DecodeNode(const uint8_t* pRecord, size_t nSize, const COSMStringTable& strings, SOSMSplitObject& node)
        {
            COSMRecordReader reader(pRecord, nSize);
            node.nId = reader.Zigzag();
            node.nLon = (int32_t)reader.Zigzag();
            node.nLat = (int32_t)reader.Zigzag();
            DecodeTags(reader, strings, node);
        }

        void COSMSplitCodec::DecodeWay(const uint8_t* pRecord, size_t nSize, const COSMStringTable& strings, SOSMSplitObject& way, bool bPoints)
        {
            COSMRecordReader reader(pRecord, nSize);
            way.nId = reader.Zigzag();
            way.nFlags = (uint32_t)reader.Varint();
            DecodeTags(reader, strings, way);
            uint64_t nPoints = reader.Varint();
            way.nPoints = (size_t)nPoints;
            if(!bPoints)
            {
                way.lons.clear();
                way.lats.clear();
                return;
            }
            way.lons.resize((size_t)nPoints);
            way.lats.resize((size_t)nPoints);
            int64_t nLon = 0, nLat = 0;
            for(uint64_t i = 0; i < nPoints; ++i)
            {
                nLon += reader.Zigzag();
                nLat += reader.Zigzag();
                way.lons[(size_t)i] = (int32_t)nLon;
                way.lats[(size_t)i] = (int32_t)nLat;
            }
        }

        void COSMSplitCodec::DecodeRelation(const uint8_t* pRecord, size_t nSize, const COSMStringTable& strings, SOSMSplitObject& relation)
        {
            COSMRecordReader reader(pRecord, nSize);
            relation.nId = reader.Zigzag();
            DecodeTags(reader, strings, relation);
            uint64_t nMembers = reader.Varint();
            relation.members.resize((size_t)nMembers);
            int64_t nId = 0;
            for(uint64_t i = 0; i < nMembers; ++i)
            {
                char cType = (char)reader.Byte();
                nId += reader.Zigzag();
                const char* pszRole = strings.Get(reader.Varint());
                relation.members[(size_t)i] = SOSMMember{cType, nId, pszRole};
            }
        }
    }
}
