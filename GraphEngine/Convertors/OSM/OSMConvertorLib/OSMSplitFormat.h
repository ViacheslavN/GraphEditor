#pragma once
#include "OSMConvertorLib.h"
#include "OSMFeatureWriter.h"
#include "readosm/readosm.h"
#include "../../../../CommonLib/filesystem/File.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// The split OSM data: a folder with the objects of an OSM file in separate files, read without the .pbf / .osm file.
//
//   osm_split.xml   the metadata: the source file, the counts, the bounds, the analyzed map (layers, columns,
//                   key / value hierarchy) and the symbols of the layers (Cartography feature layers without a table)
//   strings.dat     the dictionary of the tag keys and the member roles: varint count, then the strings (0 terminated)
//   nodes.flat      the coordinates of all the nodes by id (COSMFlatNodeStore: 8 bytes per id, sparse) - the planet /
//                   continents (the flat node store), optional
//   nodes.sorted    or the coordinates of all the nodes sorted by id: {int64 id, int32 lon, int32 lat} (1e-7 degree),
//                   16 bytes per node, binary search - extracts (the sorted node store), optional
//   nodes.dat       the nodes with tags        nodes.idx
//   ways.dat        all the ways (coordinates) ways.idx
//   relations.dat   all the relations          relations.idx
//
// .dat - records in the file order: varint payload length, payload (little endian varints, zigzag for signed values):
//   node:      zigzag id, zigzag lon, zigzag lat (1e-7 degree), tags
//   way:       zigzag id, varint flags (Closed: the first node ref is the last one, MissingNodes), tags,
//              varint point count, points: zigzag delta lon, zigzag delta lat (1e-7 degree, from 0 / the previous point)
//   relation:  zigzag id, tags, varint member count, members: byte type ('n', 'w', 'r'), zigzag delta id (from the previous
//              member), varint role (strings.dat)
//   tags:      varint count, tags: varint key (strings.dat), value (0 terminated)
// .idx - the records by id: {int64 id, uint64 offset of the record in .dat} sorted by id, 16 bytes per entry.
// The ways keep the coordinates of their nodes (the missing ones are skipped): the export needs no node lookups.

namespace GraphEngine {
    namespace Convertors {

        namespace OSMSplit
        {
            const int Version = 1;
            extern const char* MetaFile;
            extern const char* StringsFile;
            extern const char* NodesFlatFile;
            extern const char* NodesSortedFile;
            extern const char* NodesData;
            extern const char* NodesIndex;
            extern const char* WaysData;
            extern const char* WaysIndex;
            extern const char* RelationsData;
            extern const char* RelationsIndex;

            enum eWayFlags
            {
                WayClosed       = 1,
                WayMissingNodes = 2
            };

            // folder / name (UTF-8)
            std::string FilePath(const std::string& sFolder, const char* pszName);
            // the folder of the metadata file
            std::string FolderOf(const std::string& sMetaPath);
        }

        // a record being encoded
        class COSMRecordBuffer
        {
        public:
            void Clear() { m_data.clear(); }
            void PutVarint(uint64_t nValue)
            {
                while(nValue >= 0x80)
                {
                    m_data.push_back((uint8_t)(nValue | 0x80));
                    nValue >>= 7;
                }
                m_data.push_back((uint8_t)nValue);
            }
            void PutZigzag(int64_t nValue) { PutVarint(((uint64_t)nValue << 1) ^ (uint64_t)(nValue >> 63)); }
            void PutByte(uint8_t nValue) { m_data.push_back(nValue); }
            void PutCString(const char* psz)
            {
                if(psz)
                    m_data.insert(m_data.end(), psz, psz + strlen(psz));
                m_data.push_back(0);
            }
            void PutBytes(const uint8_t* p, size_t n) { m_data.insert(m_data.end(), p, p + n); }
            const std::vector<uint8_t>& Data() const { return m_data; }
            size_t Size() const { return m_data.size(); }

        private:
            std::vector<uint8_t> m_data;
        };

        // a record being decoded (throws on a truncated record)
        class COSMRecordReader
        {
        public:
            COSMRecordReader(const uint8_t* p, size_t n) : m_p(p), m_end(p + n) {}
            uint64_t Varint()
            {
                uint64_t nValue = 0;
                for(int nShift = 0; nShift < 64; nShift += 7)
                {
                    if(m_p >= m_end)
                        Fail();
                    uint8_t b = *m_p++;
                    nValue |= (uint64_t)(b & 0x7f) << nShift;
                    if(!(b & 0x80))
                        return nValue;
                }
                Fail();
                return 0;
            }
            int64_t Zigzag() { uint64_t n = Varint(); return (int64_t)(n >> 1) ^ -(int64_t)(n & 1); }
            uint8_t Byte() { if(m_p >= m_end) Fail(); return *m_p++; }
            // a 0 terminated string inside the record
            const char* CString()
            {
                const char* psz = (const char*)m_p;
                while(m_p < m_end && *m_p)
                    ++m_p;
                if(m_p >= m_end)
                    Fail();
                ++m_p;
                return psz;
            }
            bool AtEnd() const { return m_p >= m_end; }

        private:
            static void Fail();

        private:
            const uint8_t* m_p;
            const uint8_t* m_end;
        };

        // A file of the split data (explicit path, kept): appended through a buffer, read sequentially through a buffer
        // or at a position. The time of the disk operations is measured (the progress details).
        class COSMDataFile
        {
        public:
            COSMDataFile() {}
            ~COSMDataFile();

            void Create(const std::string& sPath);
            void Open(const std::string& sPath);    // for reading
            void Close();

            void     Append(const void* pData, size_t nSize);
            // a record: varint length + payload; returns its offset
            uint64_t AppendRecord(const COSMRecordBuffer& record);
            void     Flush();
            uint64_t Size() const { return m_nFileSize + m_writeBuffer.size(); }

            // sequential reading of the records from the beginning (Rewind), false - the end
            void Rewind();
            bool ReadRecord(std::vector<uint8_t>& record, uint64_t* pOffset = nullptr);
            // the same without a copy: the payload stays in the read buffer until the next read
            bool NextRecord(const uint8_t*& pRecord, size_t& nSize, uint64_t* pOffset = nullptr);
            // a record at its offset
            void ReadRecordAt(uint64_t nOffset, std::vector<uint8_t>& record);
            void ReadAt(uint64_t nPos, void* pData, size_t nSize);
            // sequential reading of raw data (after Rewind / Open), false - the end of the file
            bool Read(void* pData, size_t nSize);
            // the buffer of the sequential reading (bigger: fewer seeks when several files are read at once)
            void SetReadBufferSize(size_t nSize) { m_nReadBufferSize = nSize; }

            const std::string& Path() const { return m_sPath; }
            uint64_t BytesRead() const { return m_nBytesRead; }
            double   IOSeconds() const { return m_dIOSeconds; }   // reading and writing

        private:
            COSMDataFile(const COSMDataFile&);
            COSMDataFile& operator=(const COSMDataFile&);
            bool FillReadBuffer(size_t nNeed);

        private:
            std::string m_sPath;
            CommonLib::file::TFilePtr m_ptrFile;
            std::vector<uint8_t> m_writeBuffer;
            uint64_t    m_nFileSize = 0;
            std::vector<uint8_t> m_readBuffer;
            size_t      m_nReadPos = 0;          // in m_readBuffer
            uint64_t    m_nFileReadPos = 0;      // the file position after m_readBuffer
            uint64_t    m_nBytesRead = 0;
            double      m_dIOSeconds = 0.;
            size_t      m_nReadBufferSize = 4 * 1024 * 1024;
        };

        // the dictionary of the tag keys and the member roles (strings.dat)
        class COSMStringTable
        {
        public:
            uint32_t Index(const char* psz);
            const char* Get(uint64_t nIndex) const;
            size_t Count() const { return m_vecStrings.size(); }
            void Save(const std::string& sPath) const;
            void Load(const std::string& sPath);

        private:
            std::vector<std::string> m_vecStrings;
            std::unordered_map<std::string, uint32_t> m_mapIndex;
            std::string m_sKey;
        };

        // .idx being written: the ids come in the file order (sorted for planet / extracts), an unsorted index is sorted
        // by Finish in memory
        class COSMIdIndexWriter
        {
        public:
            void Create(const std::string& sPath);
            void Add(int64_t nId, uint64_t nOffset);
            void Finish();
            uint64_t Count() const { return m_nCount; }
            double IOSeconds() const { return m_file.IOSeconds(); }

        private:
            COSMDataFile m_file;
            uint64_t     m_nCount = 0;
            int64_t      m_nLastId = INT64_MIN;
            bool         m_bSorted = true;
        };

        // .idx being read: the first ids of the blocks in memory, the blocks are cached
        class COSMIdIndex
        {
        public:
            void Open(const std::string& sPath, size_t nCacheBlocks = 1024);
            bool Find(int64_t nId, uint64_t& nOffset);
            uint64_t Count() const { return m_nCount; }
            double IOSeconds() const { return m_file.IOSeconds(); }

            static const uint32_t BlockSize = 4096;   // entries

        private:
#pragma pack(push, 4)
            struct SEntry
            {
                int64_t  nId;
                uint64_t nOffset;
            };
#pragma pack(pop)
            struct SCachedBlock
            {
                uint64_t nBlock = UINT64_MAX;
                std::vector<SEntry> entries;
            };

        private:
            COSMDataFile m_file;
            uint64_t     m_nCount = 0;
            std::vector<int64_t> m_vecFirstIds;
            std::vector<SCachedBlock> m_cache;   // direct mapped by the block number
        };

        // a decoded object (the strings point into the record and the string table)
        struct SOSMSplitObject
        {
            int64_t  nId = 0;
            int32_t  nLon = 0, nLat = 0;           // node, 1e-7 degree
            uint32_t nFlags = 0;                   // way: OSMSplit::eWayFlags
            size_t   nPoints = 0;                  // way: the point count (also when the points aren't decoded)
            std::vector<readosm_tag> tags;
            std::vector<int32_t> lons, lats;       // way points, 1e-7 degree
            std::vector<SOSMMember> members;       // relation

            COSMTags Tags() const { return COSMTags(tags.empty() ? nullptr : tags.data(), (int)tags.size()); }
        };

        class COSMSplitCodec
        {
        public:
            static void EncodeNode(COSMRecordBuffer& record, COSMStringTable& strings, int64_t nId, int32_t nLon, int32_t nLat,
                                   const readosm_tag* pTags, int nTags);
            static void EncodeWay(COSMRecordBuffer& record, COSMStringTable& strings, int64_t nId, uint32_t nFlags,
                                  const readosm_tag* pTags, int nTags, const int32_t* pLons, const int32_t* pLats, size_t nPoints);
            // a way in two parts: the points can be encoded by another thread (EncodePoints), the head needs the string table
            static void EncodeWayHead(COSMRecordBuffer& record, COSMStringTable& strings, int64_t nId, uint32_t nFlags,
                                      const readosm_tag* pTags, int nTags);
            static void EncodePoints(COSMRecordBuffer& record, const int32_t* pLons, const int32_t* pLats, size_t nPoints);
            static void EncodeRelation(COSMRecordBuffer& record, COSMStringTable& strings, const readosm_relation& relation);

            static void DecodeNode(const std::vector<uint8_t>& record, const COSMStringTable& strings, SOSMSplitObject& node)
            {
                DecodeNode(record.data(), record.size(), strings, node);
            }
            static void DecodeWay(const std::vector<uint8_t>& record, const COSMStringTable& strings, SOSMSplitObject& way)
            {
                DecodeWay(record.data(), record.size(), strings, way);
            }
            static void DecodeRelation(const std::vector<uint8_t>& record, const COSMStringTable& strings, SOSMSplitObject& relation)
            {
                DecodeRelation(record.data(), record.size(), strings, relation);
            }
            // the strings of the object point into the record: it must live while the object is used
            static void DecodeNode(const uint8_t* pRecord, size_t nSize, const COSMStringTable& strings, SOSMSplitObject& node);
            // bPoints false: the id, the flags, the tags and nPoints only (lons / lats are left empty)
            static void DecodeWay(const uint8_t* pRecord, size_t nSize, const COSMStringTable& strings, SOSMSplitObject& way, bool bPoints = true);
            static void DecodeRelation(const uint8_t* pRecord, size_t nSize, const COSMStringTable& strings, SOSMSplitObject& relation);

        private:
            static void EncodeTags(COSMRecordBuffer& record, COSMStringTable& strings, const readosm_tag* pTags, int nTags);
            static void DecodeTags(COSMRecordReader& reader, const COSMStringTable& strings, SOSMSplitObject& object);
        };
    }
}
