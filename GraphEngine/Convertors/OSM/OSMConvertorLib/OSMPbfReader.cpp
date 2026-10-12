#include "OSMPbfReader.h"
#include "../../../../CommonLib/filesystem/File.h"
#include <zlib.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            enum eBlobTypes : uint8_t
            {
                TypeNodes     = 1,
                TypeWays      = 2,
                TypeRelations = 4
            };

            const uint32_t MaxBlobHeaderSize = 64 * 1024;          // the PBF format limits
            const uint32_t MaxBlobSize = 32 * 1024 * 1024;

            // ---------------- the index of the blocks of a file (kept for the process)

            struct SBlobInfo
            {
                uint64_t nOffset;   // of the length of the blob header
                uint32_t nSize;     // length + header + blob
                uint8_t  types;     // eBlobTypes of the objects, 0 - the header block
            };

            struct SFileIndex
            {
                uint64_t nFileSize = 0;
                int64_t  nWriteTime = 0;
                std::vector<SBlobInfo> blobs;
            };

            std::mutex g_indexMutex;
            std::map<std::string, std::shared_ptr<SFileIndex> > g_indexes;

            // ---------------- protobuf

            class CPbf
            {
            public:
                CPbf() : m_p(nullptr), m_end(nullptr) {}
                CPbf(const uint8_t* p, size_t n) : m_p(p), m_end(p + n) {}

                bool Empty() const { return m_p >= m_end; }

                bool Next(uint32_t& nField, uint32_t& nWire)
                {
                    if(m_p >= m_end)
                        return false;
                    uint64_t nKey = Varint();
                    nField = (uint32_t)(nKey >> 3);
                    nWire = (uint32_t)(nKey & 7);
                    return true;
                }

                uint64_t Varint()
                {
                    uint64_t v = 0;
                    for(uint32_t nShift = 0; nShift < 64; nShift += 7)
                    {
                        if(m_p >= m_end)
                            Fail();
                        uint8_t b = *m_p++;
                        v |= (uint64_t)(b & 0x7F) << nShift;
                        if((b & 0x80) == 0)
                            return v;
                    }
                    Fail();
                    return 0;
                }

                int64_t SVarint()
                {
                    uint64_t u = Varint();
                    return (int64_t)(u >> 1) ^ -(int64_t)(u & 1);
                }

                CPbf Bytes()
                {
                    uint64_t n = Varint();
                    if(n > (uint64_t)(m_end - m_p))
                        Fail();
                    CPbf sub(m_p, (size_t)n);
                    m_p += n;
                    return sub;
                }

                void Skip(uint32_t nWire)
                {
                    switch(nWire)
                    {
                        case 0: Varint(); break;
                        case 1: Advance(8); break;
                        case 2: Bytes(); break;
                        case 5: Advance(4); break;
                        default: Fail();
                    }
                }

                const uint8_t* Data() const { return m_p; }
                size_t Size() const { return (size_t)(m_end - m_p); }

            private:
                void Advance(size_t n)
                {
                    if(n > (size_t)(m_end - m_p))
                        Fail();
                    m_p += n;
                }

                static void Fail()
                {
                    throw CommonLib::CExcBase("PBF data are damaged");
                }

            private:
                const uint8_t* m_p;
                const uint8_t* m_end;
            };

            // ---------------- a block: the blob read from the file and the objects decoded from it (the arrays are reused)

            enum eObjectKind : uint8_t
            {
                KindNode,
                KindWay,
                KindRelation
            };

            struct SSpan
            {
                uint32_t nStart;
                uint32_t nCount;
            };

            struct SBlock
            {
                // input
                SBlobInfo            info;
                bool                 bData = false;     // OSMData, otherwise OSMHeader
                std::vector<uint8_t> blob;              // the Blob message
                int                  nObjects = 0;      // the requested object types (eBlobTypes)

                // output
                std::vector<uint8_t>     inflated;
                std::vector<char>        strings;
                std::vector<uint32_t>    stringOffsets;
                std::vector<const char*> table;
                std::vector<CPbf>        groups;
                std::vector<readosm_tag> tags;

                std::vector<long long>   nodeIds;
                std::vector<double>      nodeLat;
                std::vector<double>      nodeLon;
                std::vector<SSpan>       nodeTags;

                std::vector<long long>   wayIds;
                std::vector<SSpan>       wayTags;
                std::vector<SSpan>       wayRefs;
                std::vector<long long>   refs;

                std::vector<long long>   relIds;
                std::vector<SSpan>       relTags;
                std::vector<SSpan>       relMembers;
                std::vector<readosm_member> members;

                std::vector<std::pair<eObjectKind, uint32_t> > segments;   // the objects in the file order
                uint8_t            types = 0;
                std::exception_ptr ptrError;
                bool               bDone = false;
                double             dDecodeSeconds = 0.;   // set by the decoding thread

                void Reset()
                {
                    blob.clear();
                    strings.clear();
                    stringOffsets.clear();
                    table.clear();
                    groups.clear();
                    tags.clear();
                    nodeIds.clear();
                    nodeLat.clear();
                    nodeLon.clear();
                    nodeTags.clear();
                    wayIds.clear();
                    wayTags.clear();
                    wayRefs.clear();
                    refs.clear();
                    relIds.clear();
                    relTags.clear();
                    relMembers.clear();
                    members.clear();
                    segments.clear();
                    types = 0;
                    ptrError = nullptr;
                    bDone = false;
                }

                const char* String(uint64_t nIndex) const
                {
                    if(nIndex >= table.size())
                        throw CommonLib::CExcBase("PBF data are damaged: string {0} of {1}", nIndex, table.size());
                    return table[(size_t)nIndex];
                }

                void AddTags(CPbf keys, CPbf vals)
                {
                    while(!keys.Empty())
                    {
                        uint64_t k = keys.Varint();
                        if(vals.Empty())
                            throw CommonLib::CExcBase("PBF data are damaged: a key without a value");
                        uint64_t v = vals.Varint();
                        tags.push_back(readosm_tag{String(k), String(v)});
                    }
                }

                void AddSegment(eObjectKind kind, uint32_t nCount)
                {
                    if(nCount == 0)
                        return;
                    if(!segments.empty() && segments.back().first == kind)
                        segments.back().second += nCount;
                    else
                        segments.push_back(std::make_pair(kind, nCount));
                }

                void Decode();
                void DecodeGroup(CPbf group, int64_t nGranularity, int64_t nLatOffset, int64_t nLonOffset);
            };

            void SBlock::Decode()
            {
                // Blob: raw / zlib_data
                CPbf blobMsg(blob.data(), blob.size());
                CPbf data;
                bool bZlib = false;
                uint64_t nRawSize = 0;
                uint32_t nField, nWire;
                while(blobMsg.Next(nField, nWire))
                {
                    switch(nField)
                    {
                        case 1: data = blobMsg.Bytes(); bZlib = false; break;
                        case 2: nRawSize = blobMsg.Varint(); break;
                        case 3: data = blobMsg.Bytes(); bZlib = true; break;
                        case 4: throw CommonLib::CExcBase("LZMA compressed PBF blocks aren't supported");
                        case 6: throw CommonLib::CExcBase("BZIP2 compressed PBF blocks aren't supported");
                        case 7: throw CommonLib::CExcBase("LZ4 compressed PBF blocks aren't supported");
                        case 8: throw CommonLib::CExcBase("ZSTD compressed PBF blocks aren't supported");
                        default: blobMsg.Skip(nWire); break;
                    }
                }

                const uint8_t* pData = data.Data();
                size_t nSize = data.Size();
                if(bZlib)
                {
                    if(nRawSize == 0 || nRawSize > MaxBlobSize)
                        throw CommonLib::CExcBase("PBF block: wrong raw size {0}", nRawSize);
                    inflated.resize((size_t)nRawSize);
                    uLongf nInflated = (uLongf)nRawSize;
                    int nRet = uncompress(inflated.data(), &nInflated, pData, (uLong)nSize);
                    if(nRet != Z_OK || nInflated != nRawSize)
                        throw CommonLib::CExcBase("PBF block: failed to inflate ({0})", nRet);
                    pData = inflated.data();
                    nSize = (size_t)nInflated;
                }

                if(!bData)
                    return;   // OSMHeader: the bounds and the required features aren't needed

                // PrimitiveBlock: the groups are decoded after the granularity and the offsets (they come after the groups)
                CPbf block(pData, nSize);
                CPbf stringTable;
                int64_t nGranularity = 100, nLatOffset = 0, nLonOffset = 0;
                while(block.Next(nField, nWire))
                {
                    switch(nField)
                    {
                        case 1: stringTable = block.Bytes(); break;
                        case 2: groups.push_back(block.Bytes()); break;
                        case 17: nGranularity = (int64_t)block.Varint(); break;
                        case 19: nLatOffset = (int64_t)block.Varint(); break;
                        case 20: nLonOffset = (int64_t)block.Varint(); break;
                        default: block.Skip(nWire); break;
                    }
                }

                // the object types of the block (for the index), the requested ones are decoded
                bool bDecode = false;
                for(CPbf group : groups)
                {
                    while(group.Next(nField, nWire))
                    {
                        uint8_t type = (nField == 1 || nField == 2) ? TypeNodes : (nField == 3 ? TypeWays : (nField == 4 ? TypeRelations : 0));
                        types |= type;
                        bDecode = bDecode || (type & nObjects) != 0;
                        group.Skip(nWire);
                    }
                }
                if(!bDecode)
                    return;

                // the strings with terminating zeros
                while(stringTable.Next(nField, nWire))
                {
                    if(nField != 1)
                    {
                        stringTable.Skip(nWire);
                        continue;
                    }
                    CPbf s = stringTable.Bytes();
                    stringOffsets.push_back((uint32_t)strings.size());
                    strings.insert(strings.end(), (const char*)s.Data(), (const char*)s.Data() + s.Size());
                    strings.push_back(0);
                }
                table.resize(stringOffsets.size());
                for(size_t i = 0; i < stringOffsets.size(); ++i)
                    table[i] = strings.data() + stringOffsets[i];

                if(nGranularity <= 0)
                    throw CommonLib::CExcBase("PBF block: wrong granularity {0}", nGranularity);
                for(const CPbf& group : groups)
                    DecodeGroup(group, nGranularity, nLatOffset, nLonOffset);
            }

            void SBlock::DecodeGroup(CPbf group, int64_t nGranularity, int64_t nLatOffset, int64_t nLonOffset)
            {
                const double dScale = 1e-9;
                uint32_t nField, nWire;
                while(group.Next(nField, nWire))
                {
                    if(nField == 2 && (nObjects & TypeNodes))
                    {
                        // DenseNodes: delta coded ids and coordinates, the tags of all the nodes in one array (0 - the end of a node)
                        CPbf dense = group.Bytes(), ids, lats, lons, keysVals;
                        uint32_t f, w;
                        while(dense.Next(f, w))
                        {
                            switch(f)
                            {
                                case 1: ids = dense.Bytes(); break;
                                case 8: lats = dense.Bytes(); break;
                                case 9: lons = dense.Bytes(); break;
                                case 10: keysVals = dense.Bytes(); break;
                                default: dense.Skip(w); break;   // DenseInfo
                            }
                        }
                        int64_t nId = 0, nLat = 0, nLon = 0;
                        uint32_t nCount = 0;
                        while(!ids.Empty())
                        {
                            nId += ids.SVarint();
                            nLat += lats.SVarint();
                            nLon += lons.SVarint();
                            nodeIds.push_back(nId);
                            nodeLat.push_back(dScale * (double)(nLatOffset + nGranularity * nLat));
                            nodeLon.push_back(dScale * (double)(nLonOffset + nGranularity * nLon));
                            SSpan span = {(uint32_t)tags.size(), 0};
                            if(!keysVals.Empty())
                            {
                                for(;;)
                                {
                                    uint64_t k = keysVals.Varint();
                                    if(k == 0)
                                        break;
                                    uint64_t v = keysVals.Varint();
                                    tags.push_back(readosm_tag{String(k), String(v)});
                                }
                            }
                            span.nCount = (uint32_t)tags.size() - span.nStart;
                            nodeTags.push_back(span);
                            ++nCount;
                        }
                        AddSegment(KindNode, nCount);
                    }
                    else if(nField == 1 && (nObjects & TypeNodes))
                    {
                        CPbf node = group.Bytes(), keys, vals;
                        int64_t nId = 0, nLat = 0, nLon = 0;
                        uint32_t f, w;
                        while(node.Next(f, w))
                        {
                            switch(f)
                            {
                                case 1: nId = node.SVarint(); break;
                                case 2: keys = node.Bytes(); break;
                                case 3: vals = node.Bytes(); break;
                                case 8: nLat = node.SVarint(); break;
                                case 9: nLon = node.SVarint(); break;
                                default: node.Skip(w); break;
                            }
                        }
                        nodeIds.push_back(nId);
                        nodeLat.push_back(dScale * (double)(nLatOffset + nGranularity * nLat));
                        nodeLon.push_back(dScale * (double)(nLonOffset + nGranularity * nLon));
                        SSpan span = {(uint32_t)tags.size(), 0};
                        AddTags(keys, vals);
                        span.nCount = (uint32_t)tags.size() - span.nStart;
                        nodeTags.push_back(span);
                        AddSegment(KindNode, 1);
                    }
                    else if(nField == 3 && (nObjects & TypeWays))
                    {
                        CPbf way = group.Bytes(), keys, vals, wayRefsMsg;
                        int64_t nId = 0;
                        uint32_t f, w;
                        while(way.Next(f, w))
                        {
                            switch(f)
                            {
                                case 1: nId = (int64_t)way.Varint(); break;
                                case 2: keys = way.Bytes(); break;
                                case 3: vals = way.Bytes(); break;
                                case 8: wayRefsMsg = way.Bytes(); break;
                                default: way.Skip(w); break;
                            }
                        }
                        wayIds.push_back(nId);
                        SSpan tagSpan = {(uint32_t)tags.size(), 0};
                        AddTags(keys, vals);
                        tagSpan.nCount = (uint32_t)tags.size() - tagSpan.nStart;
                        wayTags.push_back(tagSpan);

                        SSpan refSpan = {(uint32_t)refs.size(), 0};
                        int64_t nRef = 0;
                        while(!wayRefsMsg.Empty())
                        {
                            nRef += wayRefsMsg.SVarint();
                            refs.push_back(nRef);
                        }
                        refSpan.nCount = (uint32_t)refs.size() - refSpan.nStart;
                        wayRefs.push_back(refSpan);
                        AddSegment(KindWay, 1);
                    }
                    else if(nField == 4 && (nObjects & TypeRelations))
                    {
                        CPbf rel = group.Bytes(), keys, vals, roles, memIds, memTypes;
                        int64_t nId = 0;
                        uint32_t f, w;
                        while(rel.Next(f, w))
                        {
                            switch(f)
                            {
                                case 1: nId = (int64_t)rel.Varint(); break;
                                case 2: keys = rel.Bytes(); break;
                                case 3: vals = rel.Bytes(); break;
                                case 8: roles = rel.Bytes(); break;
                                case 9: memIds = rel.Bytes(); break;
                                case 10: memTypes = rel.Bytes(); break;
                                default: rel.Skip(w); break;
                            }
                        }
                        relIds.push_back(nId);
                        SSpan tagSpan = {(uint32_t)tags.size(), 0};
                        AddTags(keys, vals);
                        tagSpan.nCount = (uint32_t)tags.size() - tagSpan.nStart;
                        relTags.push_back(tagSpan);

                        SSpan memberSpan = {(uint32_t)members.size(), 0};
                        int64_t nMemId = 0;
                        while(!memIds.Empty())
                        {
                            nMemId += memIds.SVarint();
                            uint64_t nType = memTypes.Empty() ? 0 : memTypes.Varint();
                            uint64_t nRole = roles.Empty() ? 0 : roles.Varint();
                            int nMemberType = nType == 1 ? READOSM_MEMBER_WAY : (nType == 2 ? READOSM_MEMBER_RELATION : READOSM_MEMBER_NODE);
                            members.push_back(readosm_member{nMemberType, nMemId, String(nRole)});
                        }
                        memberSpan.nCount = (uint32_t)members.size() - memberSpan.nStart;
                        relMembers.push_back(memberSpan);
                        AddSegment(KindRelation, 1);
                    }
                    else
                        group.Skip(nWire);
                }
            }

            // ---------------- the decoding threads

            class CDecoderPool
            {
            public:
                explicit CDecoderPool(int nThreads) : m_bStop(false)
                {
                    for(int i = 0; i < nThreads; ++i)
                        m_threads.emplace_back([this]() { Run(); });
                }

                ~CDecoderPool()
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_bStop = true;
                    }
                    m_cvQueue.notify_all();
                    for(std::thread& thread : m_threads)
                        thread.join();
                }

                void Submit(SBlock* pBlock)
                {
                    if(m_threads.empty())
                    {
                        DecodeSafe(*pBlock);
                        pBlock->bDone = true;
                        return;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_queue.push_back(pBlock);
                    }
                    m_cvQueue.notify_one();
                }

                void Wait(SBlock* pBlock)
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cvDone.wait(lock, [pBlock]() { return pBlock->bDone; });
                }

            private:
                static void DecodeSafe(SBlock& block)
                {
                    auto start = std::chrono::steady_clock::now();
                    try
                    {
                        block.Decode();
                    }
                    catch (...)
                    {
                        block.ptrError = std::current_exception();
                    }
                    block.dDecodeSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                }

                void Run()
                {
                    for(;;)
                    {
                        SBlock* pBlock = nullptr;
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            m_cvQueue.wait(lock, [this]() { return m_bStop || !m_queue.empty(); });
                            if(m_bStop)
                                return;
                            pBlock = m_queue.front();
                            m_queue.pop_front();
                        }
                        DecodeSafe(*pBlock);
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            pBlock->bDone = true;
                        }
                        m_cvDone.notify_all();
                    }
                }

            private:
                std::vector<std::thread> m_threads;
                std::deque<SBlock*>      m_queue;
                std::mutex               m_mutex;
                std::condition_variable  m_cvQueue;
                std::condition_variable  m_cvDone;
                bool                     m_bStop;
            };

            // ---------------- the file

            // the file through the CommonLib wrapper (UTF-8 name, 64-bit positions)
            class CPbfFile
            {
            public:
                explicit CPbfFile(const std::string& sPathUtf8) : m_sPath(sPathUtf8), m_nPos(0)
                {
                    try
                    {
                        m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(sPathUtf8.c_str(), CommonLib::file::ofmOpenExisting, CommonLib::file::arRead,
                                                                             CommonLib::file::smRead, CommonLib::file::oftBinary);
                        m_nSize = m_ptrFile->GetFileSize();
                    }
                    catch (std::exception& exc)
                    {
                        CommonLib::CExcBase::RegenExcT("Failed to open OSM file {0}", sPathUtf8, exc);
                        throw;
                    }
                }

                ~CPbfFile()
                {
                    try
                    {
                        m_ptrFile->CloseFile();
                    }
                    catch (...)
                    {
                    }
                }

                uint64_t Size() const { return m_nSize; }

                void Seek(uint64_t nOffset)
                {
                    if(nOffset > m_nSize)
                        throw CommonLib::CExcBase("OSM file {0} is changed: no block at {1}", m_sPath, nOffset);
                    m_ptrFile->SetFilePos64(nOffset, CommonLib::soFromBegin);
                    m_nPos = nOffset;
                }

                // false - the end of the file (only before the first byte when bEndAllowed)
                bool ReadExact(void* pBuf, size_t nSize, bool bEndAllowed = false)
                {
                    if(bEndAllowed && m_nPos >= m_nSize)
                        return false;
                    size_t nRead = 0;
                    while(nRead < nSize)
                    {
                        std::streamsize n = m_ptrFile->Read((byte_t*)pBuf + nRead, nSize - nRead);
                        if(n <= 0)
                            throw CommonLib::CExcBase("Failed to read OSM file {0}: unexpected end of the file", m_sPath);
                        nRead += (size_t)n;
                    }
                    m_nPos += nSize;
                    return true;
                }

                // the next blob into the block, false - the end of the file
                bool ReadBlob(SBlock& block, uint64_t nOffset)
                {
                    uint8_t lenBytes[4];
                    if(!ReadExact(lenBytes, 4, true))
                        return false;
                    uint32_t nHeaderSize = ((uint32_t)lenBytes[0] << 24) | ((uint32_t)lenBytes[1] << 16) | ((uint32_t)lenBytes[2] << 8) | lenBytes[3];
                    if(nHeaderSize == 0 || nHeaderSize > MaxBlobHeaderSize)
                        throw CommonLib::CExcBase("OSM file {0} isn't a PBF file or is damaged: blob header size {1}", m_sPath, nHeaderSize);

                    m_header.resize(nHeaderSize);
                    ReadExact(m_header.data(), nHeaderSize);
                    CPbf header(m_header.data(), nHeaderSize);
                    std::string sType;
                    uint64_t nDataSize = 0;
                    uint32_t nField, nWire;
                    while(header.Next(nField, nWire))
                    {
                        if(nField == 1)
                        {
                            CPbf s = header.Bytes();
                            sType.assign((const char*)s.Data(), s.Size());
                        }
                        else if(nField == 3)
                            nDataSize = header.Varint();
                        else
                            header.Skip(nWire);
                    }
                    if(nDataSize > MaxBlobSize)
                        throw CommonLib::CExcBase("OSM file {0} is damaged: blob size {1}", m_sPath, nDataSize);

                    block.blob.resize((size_t)nDataSize);
                    if(nDataSize)
                        ReadExact(block.blob.data(), (size_t)nDataSize);
                    block.bData = sType == "OSMData";
                    block.info.nOffset = nOffset;
                    block.info.nSize = (uint32_t)(4 + nHeaderSize + nDataSize);
                    block.info.types = 0;
                    return true;
                }

            private:
                std::string m_sPath;
                CommonLib::file::TFilePtr m_ptrFile;
                uint64_t    m_nSize = 0;
                uint64_t    m_nPos;
                std::vector<uint8_t> m_header;
            };

            bool FileStamp(const std::string& sPath, uint64_t& nSize, int64_t& nTime)
            {
                std::error_code ec;
                std::filesystem::path path = std::filesystem::u8path(sPath);
                nSize = (uint64_t)std::filesystem::file_size(path, ec);
                if(ec)
                    return false;
                nTime = (int64_t)std::filesystem::last_write_time(path, ec).time_since_epoch().count();
                return !ec;
            }

            // dispatches the decoded objects of a block in the file order, false - the handler stopped
            bool Dispatch(const SBlock& block, IOSMReadHandler& handler)
            {
                size_t iNode = 0, iWay = 0, iRel = 0;
                for(const auto& segment : block.segments)
                {
                    for(uint32_t i = 0; i < segment.second; ++i)
                    {
                        switch(segment.first)
                        {
                            case KindNode:
                            {
                                const SSpan& tags = block.nodeTags[iNode];
                                readosm_node node = {block.nodeIds[iNode], block.nodeLat[iNode], block.nodeLon[iNode], 0, 0, nullptr, 0, nullptr,
                                                     (int)tags.nCount, tags.nCount ? &block.tags[tags.nStart] : nullptr};
                                ++iNode;
                                if(!handler.OnNode(node))
                                    return false;
                                break;
                            }
                            case KindWay:
                            {
                                const SSpan& tags = block.wayTags[iWay];
                                const SSpan& refs = block.wayRefs[iWay];
                                readosm_way way = {block.wayIds[iWay], 0, 0, nullptr, 0, nullptr,
                                                   (int)refs.nCount, refs.nCount ? &block.refs[refs.nStart] : nullptr,
                                                   (int)tags.nCount, tags.nCount ? &block.tags[tags.nStart] : nullptr};
                                ++iWay;
                                if(!handler.OnWay(way))
                                    return false;
                                break;
                            }
                            case KindRelation:
                            {
                                const SSpan& tags = block.relTags[iRel];
                                const SSpan& members = block.relMembers[iRel];
                                readosm_relation relation = {block.relIds[iRel], 0, 0, nullptr, 0, nullptr,
                                                             (int)members.nCount, members.nCount ? &block.members[members.nStart] : nullptr,
                                                             (int)tags.nCount, tags.nCount ? &block.tags[tags.nStart] : nullptr};
                                ++iRel;
                                if(!handler.OnRelation(relation))
                                    return false;
                                break;
                            }
                        }
                    }
                }
                return true;
            }
        }

        bool COSMPbfReader::Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects, int nThreads)
        {
            uint8_t wanted = (uint8_t)(((nObjects & COSMReader::ReadNodes) ? TypeNodes : 0) | ((nObjects & COSMReader::ReadWays) ? TypeWays : 0) |
                                       ((nObjects & COSMReader::ReadRelations) ? TypeRelations : 0));
            if(wanted == 0)
                return true;

            // the index of the file from an earlier pass: only the blocks with the requested objects are read
            uint64_t nFileSize = 0;
            int64_t nWriteTime = 0;
            bool bStamp = FileStamp(sPath, nFileSize, nWriteTime);
            std::shared_ptr<SFileIndex> ptrIndex;
            if(bStamp)
            {
                std::lock_guard<std::mutex> lock(g_indexMutex);
                auto it = g_indexes.find(sPath);
                if(it != g_indexes.end() && it->second->nFileSize == nFileSize && it->second->nWriteTime == nWriteTime)
                    ptrIndex = it->second;
            }
            std::shared_ptr<SFileIndex> ptrNewIndex;
            if(!ptrIndex.get() && bStamp)
            {
                ptrNewIndex = std::make_shared<SFileIndex>();
                ptrNewIndex->nFileSize = nFileSize;
                ptrNewIndex->nWriteTime = nWriteTime;
            }

            if(nThreads <= 0)
            {
                unsigned int nCores = std::thread::hardware_concurrency();
                nThreads = nCores > 1 ? (int)nCores - 1 : 1;   // the calling thread reads the file and runs the handler
            }
            nThreads = (std::min)(nThreads, 32);
            const size_t nMaxInFlight = (size_t)nThreads * 4 + 2;

            CPbfFile file(sPath);
            // the blocks live longer than the pool: a worker can still decode one when an exception unwinds the stack
            std::vector<std::unique_ptr<SBlock> > vecBlocks;
            std::vector<SBlock*> vecFree;
            std::deque<SBlock*> inFlight;
            CDecoderPool pool(nThreads);

            // the progress: the processed blocks of the data the pass reads
            uint64_t nBytesTotal = file.Size();
            if(ptrIndex.get())
            {
                nBytesTotal = 0;
                for(const SBlobInfo& info : ptrIndex->blobs)
                {
                    if(info.types & wanted)
                        nBytesTotal += info.nSize;
                }
            }
            uint64_t nBytesDone = 0;

            // where the time of the reading thread goes (SOSMReaderStats)
            SOSMReaderStats stats;
            stats.nThreads = nThreads;
            typedef std::chrono::steady_clock TClock;
            auto seconds = [](TClock::time_point start) { return std::chrono::duration<double>(TClock::now() - start).count(); };

            size_t nNextIndexed = 0;     // the next blob of the index
            uint64_t nOffset = 0;        // the next blob of the file (sequential reading)
            auto readNext = [&](SBlock& block) -> bool
            {
                if(ptrIndex.get())
                {
                    while(nNextIndexed < ptrIndex->blobs.size() && (ptrIndex->blobs[nNextIndexed].types & wanted) == 0)
                        ++nNextIndexed;
                    if(nNextIndexed >= ptrIndex->blobs.size())
                        return false;
                    const SBlobInfo& info = ptrIndex->blobs[nNextIndexed++];
                    file.Seek(info.nOffset);
                    if(!file.ReadBlob(block, info.nOffset))
                        throw CommonLib::CExcBase("OSM file {0} is changed: no block at {1}", sPath, info.nOffset);
                    return true;
                }
                if(!file.ReadBlob(block, nOffset))
                    return false;
                nOffset += block.info.nSize;
                return true;
            };

            bool bEnd = false;
            try
            {
                for(;;)
                {
                    while(!bEnd && inFlight.size() < nMaxInFlight)
                    {
                        SBlock* pBlock = nullptr;
                        if(!vecFree.empty())
                        {
                            pBlock = vecFree.back();
                            vecFree.pop_back();
                        }
                        else
                        {
                            vecBlocks.push_back(std::unique_ptr<SBlock>(new SBlock()));
                            pBlock = vecBlocks.back().get();
                        }
                        pBlock->Reset();
                        pBlock->nObjects = wanted;
                        TClock::time_point readStart = TClock::now();
                        bool bRead = readNext(*pBlock);
                        stats.dReadSeconds += seconds(readStart);
                        if(!bRead)
                        {
                            vecFree.push_back(pBlock);
                            bEnd = true;
                            break;
                        }
                        stats.nBytesRead += pBlock->info.nSize;
                        pool.Submit(pBlock);
                        inFlight.push_back(pBlock);
                    }
                    if(inFlight.empty())
                        break;

                    SBlock* pBlock = inFlight.front();
                    TClock::time_point waitStart = TClock::now();
                    pool.Wait(pBlock);
                    stats.dWaitSeconds += seconds(waitStart);
                    stats.dDecodeSeconds += pBlock->dDecodeSeconds;
                    inFlight.pop_front();
                    if(pBlock->ptrError)
                        std::rethrow_exception(pBlock->ptrError);

                    if(ptrNewIndex.get())
                    {
                        SBlobInfo info = pBlock->info;
                        info.types = pBlock->types;
                        ptrNewIndex->blobs.push_back(info);
                    }

                    TClock::time_point dispatchStart = TClock::now();
                    bool bContinue = Dispatch(*pBlock, handler);
                    stats.dDispatchSeconds += seconds(dispatchStart);
                    ++stats.nBlocks;
                    nBytesDone += pBlock->info.nSize;
                    vecFree.push_back(pBlock);
                    if(bContinue)
                    {
                        handler.OnReaderStats(stats);
                        bContinue = handler.OnProgress((std::min)(nBytesDone, nBytesTotal), nBytesTotal);
                    }
                    if(!bContinue)
                    {
                        for(SBlock* pWait : inFlight)
                            pool.Wait(pWait);
                        return false;
                    }
                }
            }
            catch (std::exception& exc)
            {
                for(SBlock* pWait : inFlight)
                    pool.Wait(pWait);
                CommonLib::CExcBase::RegenExcT("Failed to read OSM file {0}", sPath, exc);
                throw;
            }
            catch (...)
            {
                for(SBlock* pWait : inFlight)
                    pool.Wait(pWait);
                throw;
            }

            // the whole file is read: the index is kept for the next passes
            if(ptrNewIndex.get())
            {
                std::lock_guard<std::mutex> lock(g_indexMutex);
                g_indexes[sPath] = ptrNewIndex;
            }
            return true;
        }

        size_t COSMPbfReader::GetIndexedBlockCount(const std::string& sPath)
        {
            std::lock_guard<std::mutex> lock(g_indexMutex);
            auto it = g_indexes.find(sPath);
            return it != g_indexes.end() ? it->second->blobs.size() : 0;
        }

        void COSMPbfReader::ClearIndex()
        {
            std::lock_guard<std::mutex> lock(g_indexMutex);
            g_indexes.clear();
        }
    }
}
