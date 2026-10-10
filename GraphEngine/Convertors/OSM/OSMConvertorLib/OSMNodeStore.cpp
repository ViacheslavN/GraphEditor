#include "OSMNodeStore.h"
#include "../../../CommonLib/exception/exc_base.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <queue>
#include <random>

namespace GraphEngine {
    namespace Convertors {

        namespace
        {
            const double CoordScale = 1e7;
        }

        COSMNodeStore::COSMNodeStore(const std::string& sTempDir, uint64_t nMemoryBytes) :
                m_sTempDir(sTempDir), m_nFileNodes(0), m_nCount(0), m_nLastId((std::numeric_limits<int64_t>::min)()),
                m_bSorted(true), m_bFinished(false), m_pLastBlock(nullptr), m_nLastBlock(0), m_nHits(0), m_nMisses(0)
        {
            m_nMemoryNodes = (std::max)((uint64_t)BlockSize, nMemoryBytes / sizeof(SNode));
            m_nCacheBlocks = (size_t)(std::max)((uint64_t)16, nMemoryBytes / (sizeof(SNode) * BlockSize));
            if(m_sTempDir.empty())
            {
                std::error_code ec;
                m_sTempDir = std::filesystem::temp_directory_path(ec).u8string();
            }
        }

        COSMNodeStore::~COSMNodeStore()
        {
            DeleteTempFile(m_ptrFile, m_sFilePath);
        }

        std::string COSMNodeStore::TempFileName(const char* pszSuffix) const
        {
            std::random_device rd;
            std::filesystem::path path = std::filesystem::u8path(m_sTempDir);
            path /= "graphengine_osm_" + std::to_string(rd()) + std::to_string(rd()) + pszSuffix;
            return path.u8string();
        }

        CommonLib::file::TFilePtr COSMNodeStore::CreateTempFile(std::string& sPath) const
        {
            sPath = TempFileName("_nodes.bin");
            try
            {
                return CommonLib::file::CFileCreator::OpenFileA(sPath.c_str(), CommonLib::file::ofmCreateAlways, CommonLib::file::aeReadWrite,
                                                                CommonLib::file::smNoMode, CommonLib::file::oftBinary);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Node store: failed to create the temporary file {0}", sPath, exc);
                throw;
            }
        }

        void COSMNodeStore::DeleteTempFile(CommonLib::file::TFilePtr& ptrFile, const std::string& sPath)
        {
            if(!ptrFile.get())
                return;
            try
            {
                ptrFile->CloseFile();
            }
            catch (...)
            {
            }
            ptrFile.reset();
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(sPath), ec);
        }

        void COSMNodeStore::Write(CommonLib::file::TFilePtr ptrFile, uint64_t nPos, const SNode* pNodes, size_t nCount)
        {
            if(!nCount)
                return;
            ptrFile->SetFilePos64(nPos * sizeof(SNode), CommonLib::soFromBegin);
            size_t nBytes = nCount * sizeof(SNode);
            if((size_t)ptrFile->Write((const byte_t*)pNodes, nBytes) != nBytes)
                throw CommonLib::CExcBase("Node store: failed to write the temporary file (no disk space?)");
        }

        void COSMNodeStore::Read(CommonLib::file::TFilePtr ptrFile, uint64_t nPos, SNode* pNodes, size_t nCount)
        {
            if(!nCount)
                return;
            ptrFile->SetFilePos64(nPos * sizeof(SNode), CommonLib::soFromBegin);
            size_t nBytes = nCount * sizeof(SNode);
            if((size_t)ptrFile->Read((byte_t*)pNodes, nBytes) != nBytes)
                throw CommonLib::CExcBase("Node store: failed to read the temporary file");
        }

        void COSMNodeStore::Add(int64_t nId, double dLon, double dLat)
        {
            if(m_bFinished)
                throw CommonLib::CExcBase("Node store: a node is added after Finish");

            SNode node;
            node.id = nId;
            node.lon = (int32_t)std::lround(dLon * CoordScale);
            node.lat = (int32_t)std::lround(dLat * CoordScale);

            if(nId <= m_nLastId)
                m_bSorted = false;
            m_nLastId = nId;

            m_buffer.push_back(node);
            ++m_nCount;
            if(m_buffer.size() >= m_nMemoryNodes)
                SpillBuffer();
        }

        void COSMNodeStore::SpillBuffer()
        {
            if(m_buffer.empty())
                return;

            if(!m_ptrFile.get())
                m_ptrFile = CreateTempFile(m_sFilePath);

            // every run is sorted, a sorted file is one long run
            if(!std::is_sorted(m_buffer.begin(), m_buffer.end(), [](const SNode& a, const SNode& b) { return a.id < b.id; }))
                std::sort(m_buffer.begin(), m_buffer.end(), [](const SNode& a, const SNode& b) { return a.id < b.id; });

            m_vecRuns.push_back(m_nFileNodes);
            Write(m_ptrFile, m_nFileNodes, m_buffer.data(), m_buffer.size());
            m_nFileNodes += m_buffer.size();
            m_buffer.clear();
        }

        void COSMNodeStore::IndexNode(const SNode& node, uint64_t nIndex)
        {
            if(nIndex % BlockSize == 0)
                m_vecBlockFirstIds.push_back(node.id);
        }

        void COSMNodeStore::Finish(TProgress progress)
        {
            if(m_bFinished)
                return;

            if(!m_ptrFile.get())
            {
                // everything is in memory
                if(!m_bSorted)
                {
                    std::stable_sort(m_buffer.begin(), m_buffer.end(), [](const SNode& a, const SNode& b) { return a.id < b.id; });
                    // the same node twice: the last one stays
                    std::vector<SNode> unique;
                    unique.reserve(m_buffer.size());
                    for(size_t i = 0; i < m_buffer.size(); ++i)
                    {
                        if(!unique.empty() && unique.back().id == m_buffer[i].id)
                            unique.back() = m_buffer[i];
                        else
                            unique.push_back(m_buffer[i]);
                    }
                    m_buffer.swap(unique);
                }
                m_buffer.shrink_to_fit();
                m_bFinished = true;
                return;
            }

            SpillBuffer();
            std::vector<SNode>().swap(m_buffer);

            if(m_bSorted)
            {
                // the runs are one sorted sequence: only the index of the blocks
                m_vecBlockFirstIds.clear();
                SNode first;
                for(uint64_t nPos = 0; nPos < m_nFileNodes; nPos += BlockSize)
                {
                    Read(m_ptrFile, nPos, &first, 1);
                    m_vecBlockFirstIds.push_back(first.id);
                }
            }
            else
                Merge(progress);

            m_bFinished = true;
        }

        void COSMNodeStore::Merge(TProgress progress)
        {
            // k-way merge of the sorted runs into a new file
            std::string sMergedPath;
            CommonLib::file::TFilePtr ptrMerged = CreateTempFile(sMergedPath);

            try
            {
                struct SRun
                {
                    uint64_t nPos;     // next node in the file
                    uint64_t nEnd;
                    std::vector<SNode> buffer;
                    size_t   nBufferPos;
                };

                size_t nRuns = m_vecRuns.size();
                // the memory is shared by the read buffers of the runs
                size_t nRunBuffer = (size_t)(std::max)((uint64_t)1024, m_nMemoryNodes / (nRuns + 1));
                std::vector<SRun> runs(nRuns);
                for(size_t i = 0; i < nRuns; ++i)
                {
                    runs[i].nPos = m_vecRuns[i];
                    runs[i].nEnd = i + 1 < nRuns ? m_vecRuns[i + 1] : m_nFileNodes;
                    runs[i].nBufferPos = 0;
                }

                auto fill = [&](SRun& run) -> bool
                {
                    if(run.nBufferPos < run.buffer.size())
                        return true;
                    if(run.nPos >= run.nEnd)
                        return false;
                    size_t nRead = (size_t)(std::min)((uint64_t)nRunBuffer, run.nEnd - run.nPos);
                    run.buffer.resize(nRead);
                    Read(m_ptrFile, run.nPos, run.buffer.data(), nRead);
                    run.nPos += nRead;
                    run.nBufferPos = 0;
                    return true;
                };

                // (id, run): the later run wins for the same id (the node met last in the file)
                typedef std::pair<int64_t, size_t> TItem;
                auto cmp = [](const TItem& a, const TItem& b) { return a.first > b.first || (a.first == b.first && a.second < b.second); };
                std::priority_queue<TItem, std::vector<TItem>, decltype(cmp)> queue(cmp);
                for(size_t i = 0; i < nRuns; ++i)
                {
                    if(fill(runs[i]))
                        queue.push(TItem(runs[i].buffer[0].id, i));
                }

                std::vector<SNode> out;
                out.reserve(nRunBuffer);
                uint64_t nOut = 0;
                bool bHasLast = false;
                SNode last = SNode();
                m_vecBlockFirstIds.clear();

                uint64_t nWritten = 0;
                auto flush = [&]()
                {
                    Write(ptrMerged, nWritten, out.data(), out.size());
                    nWritten += out.size();
                    out.clear();
                };

                uint64_t nProcessed = 0;
                while(!queue.empty())
                {
                    TItem item = queue.top();
                    queue.pop();
                    SRun& run = runs[item.second];
                    SNode node = run.buffer[run.nBufferPos++];

                    if(bHasLast && last.id == node.id)
                        last = node;   // duplicate id: the later one
                    else
                    {
                        if(bHasLast)
                        {
                            IndexNode(last, nOut++);
                            out.push_back(last);
                            if(out.size() >= nRunBuffer)
                                flush();
                        }
                        last = node;
                        bHasLast = true;
                    }

                    if(fill(run))
                        queue.push(TItem(run.buffer[run.nBufferPos].id, item.second));

                    if(progress && (++nProcessed % 1000000) == 0 && !progress(nProcessed))
                        throw CommonLib::CExcBase("Canceled");
                }

                if(bHasLast)
                {
                    IndexNode(last, nOut++);
                    out.push_back(last);
                }
                flush();

                DeleteTempFile(m_ptrFile, m_sFilePath);
                m_ptrFile = ptrMerged;
                m_sFilePath = sMergedPath;
                m_nFileNodes = nOut;
                m_vecRuns.assign(1, 0);
            }
            catch (...)
            {
                DeleteTempFile(ptrMerged, sMergedPath);
                throw;
            }
        }

        bool COSMNodeStore::Find(const SNode* pNodes, size_t nCount, int64_t nId, double& dLon, double& dLat)
        {
            const SNode* pEnd = pNodes + nCount;
            const SNode* pFound = std::lower_bound(pNodes, pEnd, nId, [](const SNode& node, int64_t id) { return node.id < id; });
            if(pFound == pEnd || pFound->id != nId)
                return false;
            dLon = pFound->lon / CoordScale;
            dLat = pFound->lat / CoordScale;
            return true;
        }

        const std::vector<COSMNodeStore::SNode>* COSMNodeStore::GetBlock(uint64_t nBlock)
        {
            if(m_pLastBlock && m_nLastBlock == nBlock)
            {
                ++m_nHits;
                return m_pLastBlock;
            }

            auto it = m_mapCache.find(nBlock);
            if(it != m_mapCache.end())
            {
                ++m_nHits;
                m_cache.splice(m_cache.begin(), m_cache, it->second);
            }
            else
            {
                ++m_nMisses;
                if(m_cache.size() >= m_nCacheBlocks)
                {
                    // the least recently used block is reused
                    m_mapCache.erase(m_cache.back().nBlock);
                    m_cache.splice(m_cache.begin(), m_cache, std::prev(m_cache.end()));
                }
                else
                    m_cache.push_front(SBlock());

                SBlock& block = m_cache.front();
                block.nBlock = nBlock;
                uint64_t nStart = nBlock * BlockSize;
                size_t nRead = (size_t)(std::min)((uint64_t)BlockSize, m_nFileNodes - nStart);
                block.nodes.resize(nRead);
                Read(m_ptrFile, nStart, block.nodes.data(), nRead);
                m_mapCache[nBlock] = m_cache.begin();
            }

            m_pLastBlock = &m_cache.front().nodes;
            m_nLastBlock = nBlock;
            return m_pLastBlock;
        }

        bool COSMNodeStore::Get(int64_t nId, double& dLon, double& dLat)
        {
            if(!m_bFinished)
                throw CommonLib::CExcBase("Node store: Finish isn't called");

            if(!m_ptrFile.get())
                return Find(m_buffer.data(), m_buffer.size(), nId, dLon, dLat);

            if(m_vecBlockFirstIds.empty() || nId < m_vecBlockFirstIds[0])
                return false;

            auto it = std::upper_bound(m_vecBlockFirstIds.begin(), m_vecBlockFirstIds.end(), nId);
            uint64_t nBlock = (uint64_t)(it - m_vecBlockFirstIds.begin()) - 1;
            const std::vector<SNode>* pBlock = GetBlock(nBlock);
            return Find(pBlock->data(), pBlock->size(), nId, dLon, dLat);
        }
    }
}
