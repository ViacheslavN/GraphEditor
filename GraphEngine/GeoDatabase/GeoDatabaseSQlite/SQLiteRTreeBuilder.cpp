#include "SQLiteRTreeBuilder.h"
#include "../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <queue>
#include <random>

namespace GraphEngine {
    namespace GeoDatabase {

        namespace
        {
            const uint32_t BytesPerCell = 8 + 4 * 4;   // id + 2D box of floats
            const uint32_t MaxRTreeCells = 51;         // RTREE_MAXCELLS of SQLite

            // ---------------- external sort: in memory up to the limit, then sorted runs in a temporary file merged by Next

            const size_t MinMergeChunkBytes = 4 * 1024 * 1024;

            template<class T, class TLess>
            class TExternalSorter
            {
            public:
                TExternalSorter(const std::string& sTempDir, uint64_t nMemoryBytes) :
                        m_sTempDir(sTempDir), m_nMaxEntries((size_t)(std::max)((uint64_t)1024, nMemoryBytes / sizeof(T))), m_nFileEntries(0), m_bMerging(false)
                {}

                ~TExternalSorter()
                {
                    DeleteFile();
                }

                void Add(const T& entry)
                {
                    if(m_buffer.size() >= m_nMaxEntries)
                        SpillRun();
                    m_buffer.push_back(entry);
                }

                // after the last Add
                void StartReading()
                {
                    std::sort(m_buffer.begin(), m_buffer.end(), TLess());
                    if(m_vecRuns.empty())
                    {
                        m_nPos = 0;
                        return;
                    }
                    if(!m_buffer.empty())
                        SpillRun();

                    // k-way merge, every run is read in chunks: big chunks, every refill of a run is a seek
                    // (HDD: ~10 ms), so 4 MB at least whatever the number of runs
                    m_bMerging = true;
                    const size_t nMinChunk = (std::max)((size_t)256, MinMergeChunkBytes / sizeof(T));
                    size_t nChunk = (std::max)(nMinChunk, m_nMaxEntries / (m_vecRuns.size() + 1));
                    m_readers.resize(m_vecRuns.size());
                    for(size_t i = 0; i < m_vecRuns.size(); ++i)
                    {
                        SRunReader& reader = m_readers[i];
                        reader.nNext = m_vecRuns[i].first;
                        reader.nEnd = m_vecRuns[i].first + m_vecRuns[i].second;
                        reader.nChunk = nChunk;
                        if(Fill(reader))
                            m_heap.push(SHeapItem{reader.buffer[0], i});
                    }
                }

                bool Next(T& entry)
                {
                    if(!m_bMerging)
                    {
                        if(m_nPos >= m_buffer.size())
                            return false;
                        entry = m_buffer[m_nPos++];
                        return true;
                    }
                    if(m_heap.empty())
                        return false;
                    SHeapItem item = m_heap.top();
                    m_heap.pop();
                    entry = item.entry;
                    SRunReader& reader = m_readers[item.nRun];
                    if(++reader.nPos < reader.buffer.size() || Fill(reader))
                        m_heap.push(SHeapItem{reader.buffer[reader.nPos], item.nRun});
                    return true;
                }

            private:
                struct SRunReader
                {
                    uint64_t nNext = 0;   // the next entry of the run in the file
                    uint64_t nEnd = 0;
                    size_t   nChunk = 0;
                    std::vector<T> buffer;
                    size_t   nPos = 0;
                };

                struct SHeapItem
                {
                    T      entry;
                    size_t nRun;
                    bool operator<(const SHeapItem& other) const { return TLess()(other.entry, entry); }   // the smallest on the top
                };

                bool Fill(SRunReader& reader)
                {
                    reader.nPos = 0;
                    reader.buffer.clear();
                    if(reader.nNext >= reader.nEnd)
                        return false;
                    size_t nCount = (size_t)(std::min)((uint64_t)reader.nChunk, reader.nEnd - reader.nNext);
                    reader.buffer.resize(nCount);
                    m_ptrFile->SetFilePos64(reader.nNext * sizeof(T), CommonLib::soFromBegin);
                    if(m_ptrFile->Read((byte_t*)reader.buffer.data(), nCount * sizeof(T)) != (std::streamsize)(nCount * sizeof(T)))
                        throw CommonLib::CExcBase("R-tree builder: failed to read the temporary file {0}", m_sFilePath);
                    reader.nNext += nCount;
                    return true;
                }

                void SpillRun()
                {
                    std::sort(m_buffer.begin(), m_buffer.end(), TLess());
                    if(!m_ptrFile.get())
                    {
                        std::random_device rd;
                        std::filesystem::path path = m_sTempDir.empty() ? std::filesystem::temp_directory_path() : std::filesystem::u8path(m_sTempDir);
                        path /= "graphengine_rtree_" + std::to_string(rd()) + std::to_string(rd()) + ".bin";
                        m_sFilePath = path.u8string();
                        m_ptrFile = CommonLib::file::CFileCreator::OpenFileA(m_sFilePath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                             CommonLib::file::smNoMode, CommonLib::file::oftBinary);
                    }
                    m_ptrFile->SetFilePos64(m_nFileEntries * sizeof(T), CommonLib::soFromBegin);
                    size_t nBytes = m_buffer.size() * sizeof(T);
                    if(m_ptrFile->Write((const byte_t*)m_buffer.data(), nBytes) != (std::streamsize)nBytes)
                        throw CommonLib::CExcBase("R-tree builder: failed to write the temporary file {0}", m_sFilePath);
                    m_vecRuns.push_back(std::make_pair(m_nFileEntries, (uint64_t)m_buffer.size()));
                    m_nFileEntries += m_buffer.size();
                    m_buffer.clear();
                }

                void DeleteFile()
                {
                    if(!m_ptrFile.get())
                        return;
                    try
                    {
                        m_ptrFile->CloseFile();
                    }
                    catch (...)
                    {
                    }
                    m_ptrFile.reset();
                    std::error_code ec;
                    std::filesystem::remove(std::filesystem::u8path(m_sFilePath), ec);
                }

            private:
                std::string m_sTempDir;
                size_t      m_nMaxEntries;
                std::vector<T> m_buffer;
                size_t      m_nPos = 0;
                CommonLib::file::TFilePtr m_ptrFile;
                std::string m_sFilePath;
                std::vector<std::pair<uint64_t, uint64_t> > m_vecRuns;
                uint64_t    m_nFileEntries;
                bool        m_bMerging;
                std::vector<SRunReader> m_readers;
                std::priority_queue<SHeapItem> m_heap;
            };

            typedef CSQLiteRTreeBuilder::SEntry SEntry;

            struct SEntryLess
            {
                bool operator()(const SEntry& a, const SEntry& b) const
                {
                    return a.nKey < b.nKey || (a.nKey == b.nKey && a.nId < b.nId);
                }
            };

            // rowid -> leaf node (the <index>_rowid table is written in the rowid order)
            struct SRowNode
            {
                int64_t nRowId;
                int64_t nNode;
            };

            struct SRowNodeLess
            {
                bool operator()(const SRowNode& a, const SRowNode& b) const { return a.nRowId < b.nRowId; }
            };

            typedef TExternalSorter<SEntry, SEntryLess> TEntrySorter;

            // the R-tree keeps 32-bit floats: the box is rounded outwards so it still contains the shape
            float RoundDown(double d)
            {
                float f = (float)d;
                if((double)f > d)
                    f = std::nextafter(f, -INFINITY);
                return f;
            }

            float RoundUp(double d)
            {
                float f = (float)d;
                if((double)f < d)
                    f = std::nextafter(f, INFINITY);
                return f;
            }

            // a float as an unsigned integer with the same order
            uint32_t SortableBits(float f)
            {
                uint32_t u;
                memcpy(&u, &f, sizeof(u));
                return (u & 0x80000000u) ? ~u : (u | 0x80000000u);
            }

            // the position on the Hilbert curve over the 2^32 x 2^32 grid
            uint64_t HilbertIndex(uint32_t x, uint32_t y)
            {
                uint64_t d = 0;
                for(uint32_t s = 0x80000000u; s > 0; s >>= 1)
                {
                    uint32_t rx = (x & s) ? 1 : 0;
                    uint32_t ry = (y & s) ? 1 : 0;
                    d += (uint64_t)s * (uint64_t)s * (uint64_t)((3 * rx) ^ ry);
                    if(ry == 0)
                    {
                        if(rx == 1)
                        {
                            x = ~x;
                            y = ~y;
                        }
                        std::swap(x, y);
                    }
                }
                return d;
            }

            void PutInt16(byte_t* p, uint32_t v)
            {
                p[0] = (byte_t)(v >> 8);
                p[1] = (byte_t)v;
            }

            void PutInt64(byte_t* p, int64_t v)
            {
                for(int i = 7; i >= 0; --i)
                {
                    p[i] = (byte_t)v;
                    v = (int64_t)((uint64_t)v >> 8);
                }
            }

            void PutFloat(byte_t* p, float f)
            {
                uint32_t u;
                memcpy(&u, &f, sizeof(u));
                p[0] = (byte_t)(u >> 24);
                p[1] = (byte_t)(u >> 16);
                p[2] = (byte_t)(u >> 8);
                p[3] = (byte_t)u;
            }

            struct SBox
            {
                float v[4];   // minX, maxX, minY, maxY

                void Init(const float* box) { memcpy(v, box, sizeof(v)); }
                void Expand(const float* box)
                {
                    v[0] = (std::min)(v[0], box[0]);
                    v[1] = (std::max)(v[1], box[1]);
                    v[2] = (std::min)(v[2], box[2]);
                    v[3] = (std::max)(v[3], box[3]);
                }
            };

            std::string Quote(const std::string& sName)
            {
                std::string s = "\"";
                for(char c : sName)
                {
                    if(c == '"')
                        s += '"';
                    s += c;
                }
                return s + "\"";
            }

            // a node blob: depth (root only), number of cells, cells (id, box), padded with zeros to the node size
            class CNodeWriter
            {
            public:
                CNodeWriter(CommonLib::database::IDatabasePtr ptrDatabase, const std::string& sIndexName, uint32_t nNodeSize) :
                        m_nNodeSize(nNodeSize), m_blob(nNodeSize, 0)
                {
                    m_ptrNode = ptrDatabase->PrepareQuery(("INSERT INTO " + Quote(sIndexName + "_node") + "(nodeno, data) VALUES(?, ?)").c_str());
                    m_ptrParent = ptrDatabase->PrepareQuery(("INSERT INTO " + Quote(sIndexName + "_parent") + "(nodeno, parentnode) VALUES(?, ?)").c_str());
                }

                void Begin()
                {
                    std::fill(m_blob.begin(), m_blob.end(), (byte_t)0);
                    m_nCells = 0;
                }

                void AddCell(int64_t nId, const float* box)
                {
                    byte_t* p = m_blob.data() + 4 + m_nCells * BytesPerCell;
                    PutInt64(p, nId);
                    for(int i = 0; i < 4; ++i)
                        PutFloat(p + 8 + 4 * i, box[i]);
                    ++m_nCells;
                }

                void Write(int64_t nNode, uint32_t nDepth, int64_t nParent)
                {
                    PutInt16(m_blob.data(), nDepth);
                    PutInt16(m_blob.data() + 2, m_nCells);
                    m_ptrNode->BindInt64(1, nNode);
                    m_ptrNode->BindBlob(2, m_blob.data(), (int32_t)m_blob.size(), false);
                    m_ptrNode->Next();
                    m_ptrNode->Reset();
                    if(nParent > 0)
                    {
                        m_ptrParent->BindInt64(1, nNode);
                        m_ptrParent->BindInt64(2, nParent);
                        m_ptrParent->Next();
                        m_ptrParent->Reset();
                    }
                }

            private:
                uint32_t m_nNodeSize;
                std::vector<byte_t> m_blob;
                uint32_t m_nCells = 0;
                CommonLib::database::IStatmentPtr m_ptrNode;
                CommonLib::database::IStatmentPtr m_ptrParent;
            };
        }

        CSQLiteRTreeBuilder::CSQLiteRTreeBuilder(const std::string& sTempDir, uint64_t nMemoryBytes) :
                m_sTempDir(sTempDir), m_nMemoryBytes(nMemoryBytes), m_nCount(0)
        {
            m_ptrSorter = std::make_shared<TEntrySorter>(sTempDir, nMemoryBytes);
        }

        CSQLiteRTreeBuilder::~CSQLiteRTreeBuilder()
        {
        }

        uint32_t CSQLiteRTreeBuilder::MaxCellsForNodeSize(uint32_t nNodeSize)
        {
            return nNodeSize > 4 ? (nNodeSize - 4) / BytesPerCell : 0;
        }

        void CSQLiteRTreeBuilder::Add(int64_t nRowId, const CommonLib::bbox& box)
        {
            if(box.type != CommonLib::bbox_type_normal)
                return;

            SEntry entry;
            entry.nId = nRowId;
            entry.box[0] = RoundDown(box.xMin);
            entry.box[1] = RoundUp(box.xMax);
            entry.box[2] = RoundDown(box.yMin);
            entry.box[3] = RoundUp(box.yMax);
            float cx = 0.5f * entry.box[0] + 0.5f * entry.box[1];
            float cy = 0.5f * entry.box[2] + 0.5f * entry.box[3];
            entry.nKey = HilbertIndex(SortableBits(cx), SortableBits(cy));
            static_cast<TEntrySorter*>(m_ptrSorter.get())->Add(entry);
            ++m_nCount;
        }

        void CSQLiteRTreeBuilder::Build(CommonLib::database::IDatabasePtr ptrDatabase, const std::string& sIndexName)
        {
            try
            {
                TEntrySorter& sorter = *static_cast<TEntrySorter*>(m_ptrSorter.get());

                // the node size of the index (the size of its root), the entries it already has
                uint32_t nNodeSize = 0;
                {
                    CommonLib::database::IStatmentPtr ptrStmt = ptrDatabase->PrepareQuery(
                            ("SELECT length(data) FROM " + Quote(sIndexName + "_node") + " WHERE nodeno = 1").c_str());
                    if(ptrStmt->Next())
                        nNodeSize = (uint32_t)ptrStmt->ReadInt64(0);
                }
                uint32_t nMaxCells = MaxCellsForNodeSize(nNodeSize);
                if(nMaxCells < 2)
                    throw CommonLib::CExcBase("the index has a wrong root node ({0} bytes)", nNodeSize);
                nMaxCells = (std::min)(nMaxCells, MaxRTreeCells);
                {
                    CommonLib::database::IStatmentPtr ptrStmt = ptrDatabase->PrepareQuery(
                            ("SELECT rowid, minX, maxX, minY, maxY FROM " + Quote(sIndexName)).c_str());   // rowid - the id column of any name
                    while(ptrStmt->Next())
                    {
                        CommonLib::bbox box;
                        box.type = CommonLib::bbox_type_normal;
                        box.xMin = ptrStmt->ReadDouble(1);
                        box.xMax = ptrStmt->ReadDouble(2);
                        box.yMin = ptrStmt->ReadDouble(3);
                        box.yMax = ptrStmt->ReadDouble(4);
                        Add(ptrStmt->ReadInt64(0), box);
                    }
                }

                for(const char* pszTable : {"_node", "_rowid", "_parent"})
                    ptrDatabase->Execute(("DELETE FROM " + Quote(sIndexName + pszTable)).c_str());

                // the levels: leaves first, the root has one node (number 1), the other nodes are numbered from the top
                std::vector<uint64_t> vecLevelNodes;
                uint64_t nItems = m_nCount;
                do
                {
                    uint64_t nNodes = (std::max)((uint64_t)1, (nItems + nMaxCells - 1) / nMaxCells);
                    vecLevelNodes.push_back(nNodes);
                    nItems = nNodes;
                }
                while(nItems > 1);
                size_t nLevels = vecLevelNodes.size();
                std::vector<int64_t> vecLevelBase(nLevels);
                vecLevelBase[nLevels - 1] = 1;
                int64_t nNextNode = 2;
                for(size_t k = nLevels - 1; k-- > 0;)
                {
                    vecLevelBase[k] = nNextNode;
                    nNextNode += (int64_t)vecLevelNodes[k];
                }
                auto parentOf = [&](size_t nLevel, uint64_t nIndex) -> int64_t
                {
                    return nLevel + 1 < nLevels ? vecLevelBase[nLevel + 1] + (int64_t)(nIndex / nMaxCells) : 0;
                };

                CNodeWriter writer(ptrDatabase, sIndexName, nNodeSize);
                TExternalSorter<SRowNode, SRowNodeLess> rowNodes(m_sTempDir, m_nMemoryBytes / 2);

                // leaves: the entries in the Hilbert order
                sorter.StartReading();
                std::vector<SBox> vecBoxes;          // of the nodes of the last written level
                vecBoxes.reserve((size_t)vecLevelNodes[0]);
                SEntry entry;
                uint64_t nLeaf = 0;
                uint32_t nInLeaf = 0;
                SBox leafBox;
                writer.Begin();
                while(sorter.Next(entry))
                {
                    if(nInLeaf == 0)
                        leafBox.Init(entry.box);
                    else
                        leafBox.Expand(entry.box);
                    writer.AddCell(entry.nId, entry.box);
                    rowNodes.Add(SRowNode{entry.nId, vecLevelBase[0] + (int64_t)nLeaf});
                    if(++nInLeaf == nMaxCells)
                    {
                        writer.Write(vecLevelBase[0] + (int64_t)nLeaf, nLevels == 1 ? 0 : 0, parentOf(0, nLeaf));
                        vecBoxes.push_back(leafBox);
                        writer.Begin();
                        ++nLeaf;
                        nInLeaf = 0;
                    }
                }
                if(nInLeaf > 0 || m_nCount == 0)
                {
                    writer.Write(vecLevelBase[0] + (int64_t)nLeaf, 0, parentOf(0, nLeaf));
                    if(nInLeaf > 0)
                        vecBoxes.push_back(leafBox);
                }

                // the upper levels from the boxes of the level below
                for(size_t k = 1; k < nLevels; ++k)
                {
                    std::vector<SBox> vecLevel;
                    vecLevel.reserve((size_t)vecLevelNodes[k]);
                    for(uint64_t nNode = 0; nNode < vecLevelNodes[k]; ++nNode)
                    {
                        writer.Begin();
                        SBox box;
                        uint64_t nFirst = nNode * nMaxCells;
                        uint64_t nLast = (std::min)((uint64_t)vecBoxes.size(), nFirst + nMaxCells);
                        for(uint64_t i = nFirst; i < nLast; ++i)
                        {
                            writer.AddCell(vecLevelBase[k - 1] + (int64_t)i, vecBoxes[(size_t)i].v);
                            if(i == nFirst)
                                box.Init(vecBoxes[(size_t)i].v);
                            else
                                box.Expand(vecBoxes[(size_t)i].v);
                        }
                        // the depth is stored in the root only
                        writer.Write(vecLevelBase[k] + (int64_t)nNode, k + 1 == nLevels ? (uint32_t)(nLevels - 1) : 0, parentOf(k, nNode));
                        vecLevel.push_back(box);
                    }
                    vecBoxes.swap(vecLevel);
                }

                // rowid -> leaf in the rowid order (sequential inserts into the rowid table)
                rowNodes.StartReading();
                CommonLib::database::IStatmentPtr ptrRowid = ptrDatabase->PrepareQuery(
                        ("INSERT INTO " + Quote(sIndexName + "_rowid") + "(rowid, nodeno) VALUES(?, ?)").c_str());
                SRowNode rowNode;
                while(rowNodes.Next(rowNode))
                {
                    ptrRowid->BindInt64(1, rowNode.nRowId);
                    ptrRowid->BindInt64(2, rowNode.nNode);
                    ptrRowid->Next();
                    ptrRowid->Reset();
                }

                m_ptrSorter = std::make_shared<TEntrySorter>(m_sTempDir, m_nMemoryBytes);
                m_nCount = 0;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExcT("Failed to build the spatial index {0}", sIndexName, exc);
                throw;
            }
        }
    }
}
