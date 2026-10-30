#include "OSMParallelLeafBuilder.h"
#include "OSMBPTreeUtils.h"
#include "../../../../../CommonLib/stream/FixedMemoryStream.h"

#include <algorithm>
#include <cstring>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            CParallelLeafBuilder::CParallelLeafBuilder(bptreedb::TCompressorParamsBasePtr ptrParams, uint32_t nPagePayload, int nThreads,
                                                       uint32_t nChunkNodes, std::shared_ptr<bptreedb::storage::CFilePager> ptrPager) :
                    m_ptrParams(ptrParams), m_nPayload(nPagePayload), m_nChunkNodes((std::max)(1024u, nChunkNodes)), m_ptrPager(ptrPager)
            {
                m_nLeafPage = TNodeLeafCompressor::LeafPageSize(nPagePayload);
                m_nMaxCount = 8 * m_nLeafPage;      // as TBaseNodeCompressor
                bptreedb::TCompressorParamsPtr ptrKey = ptrParams.get() ? ptrParams->GetCompressParams(bptreedb::eLeafKey) : bptreedb::TCompressorParamsPtr();
                if (ptrKey.get())
                    m_nMaxCount = (uint32_t)ptrKey->GetIntParam("MaxCount", (int32_t)m_nMaxCount);
                m_nThreads = (std::max)(1, nThreads);
                m_nMaxInFlight = (uint64_t)m_nThreads * 2;
                for (int i = 0; i < m_nThreads; ++i)
                    m_threads.emplace_back([this]() { WorkerLoop(); });
            }

            CParallelLeafBuilder::~CParallelLeafBuilder()
            {
                Stop();
            }

            void CParallelLeafBuilder::Add(TBatch& batch)
            {
                m_nCurrentNodes += batch.size();
                m_current.emplace_back(std::move(batch));
                batch = TBatch();
                if (m_nCurrentNodes >= m_nChunkNodes)
                    Submit();
                while (WriteNext(false))
                    ;
            }

            void CParallelLeafBuilder::Finish()
            {
                if (m_nCurrentNodes != 0)
                    Submit();
                while (m_nWritten < m_nSubmitted)
                    WriteNext(true);
                if (m_ptrPending.get())
                {
                    auto start = TClock::now();
                    m_ptrPager->SavePage(m_ptrPending);
                    m_ptrPending.reset();
                    m_dWriteSeconds += Since(start);
                }
                Stop();
            }

            void CParallelLeafBuilder::Stop()
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bStop = true;
                }
                m_cvJobs.notify_all();
                m_cvResults.notify_all();
                for (std::thread& thread : m_threads)
                    if (thread.joinable())
                        thread.join();
            }

            void CParallelLeafBuilder::Submit()
            {
                // the number of the chunks in work / waiting for the writing is limited (the memory)
                while (m_nSubmitted - m_nWritten >= m_nMaxInFlight)
                    WriteNext(true);
                SChunk chunk;
                chunk.nSeq = m_nSubmitted++;
                chunk.batches.swap(m_current);
                m_nCurrentNodes = 0;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_jobs.emplace_back(std::move(chunk));
                }
                m_cvJobs.notify_one();
            }

            bool CParallelLeafBuilder::WriteNext(bool bWait)
            {
                if (m_nWritten == m_nSubmitted)
                    return false;
                SLeaves leaves;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    auto ready = [this]() { return m_error || m_results.count(m_nWritten) != 0; };
                    if (!ready())
                    {
                        if (!bWait)
                            return false;
                        auto start = TClock::now();
                        m_cvResults.wait(lock, ready);
                        m_dWaitSeconds += Since(start);
                    }
                    if (m_error)
                        std::rethrow_exception(m_error);
                    auto it = m_results.find(m_nWritten);
                    leaves = std::move(it->second);
                    m_results.erase(it);
                }
                ++m_nWritten;
                m_cvJobs.notify_all();

                auto start = TClock::now();
                WriteLeaves(leaves);
                m_dWriteSeconds += Since(start);
                return true;
            }

            void CParallelLeafBuilder::WriteLeaves(const SLeaves& leaves)
            {
                for (size_t i = 0; i < leaves.firstKeys.size(); ++i)
                {
                    int64_t nAddr = m_ptrPager->GetNewFilePageAddr();
                    int64_t nPrev = -1;
                    if (m_ptrPending.get())
                    {
                        memcpy(m_ptrPending->GetPayLoadData() + sizeof(bool), &nAddr, sizeof(int64_t));   // next
                        m_ptrPager->SavePage(m_ptrPending);
                        nPrev = m_nPendingAddr;
                    }
                    bptreedb::storage::IFilePagePtr ptrPage = m_ptrPager->GetNewPage(nAddr);
                    if (ptrPage->GetSize() != m_nPayload)
                        throw CommonLib::CExcBase("the page payload {0} isn't the one of the tree {1}", ptrPage->GetSize(), m_nPayload);
                    memcpy(ptrPage->GetPayLoadData(), &leaves.pages[i * m_nPayload], m_nPayload);
                    memcpy(ptrPage->GetPayLoadData() + sizeof(bool) + sizeof(int64_t), &nPrev, sizeof(int64_t));   // prev
                    m_ptrPending = ptrPage;
                    m_nPendingAddr = nAddr;
                    m_level.emplace_back(leaves.firstKeys[i], nAddr);
                    ++m_nLeaves;
                }
            }

            void CParallelLeafBuilder::WorkerLoop()
            {
                try
                {
                    CNodeLeafEncoder encoder(bptreedb::TAllocsSetPtr(), m_ptrParams);
                    std::vector<int64_t> keys;
                    std::vector<SOSMPointE7> points;
                    for (;;)
                    {
                        SChunk chunk;
                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            m_cvJobs.wait(lock, [this]() { return m_bStop || !m_jobs.empty(); });
                            if (m_bStop)
                                return;
                            chunk = std::move(m_jobs.front());
                            m_jobs.pop_front();
                        }

                        auto start = TClock::now();
                        SLeaves leaves;
                        Encode(chunk, leaves, encoder, keys, points);
                        m_nEncodeNs += NanosecondsSince(start);

                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            m_results.emplace(chunk.nSeq, std::move(leaves));
                        }
                        m_cvResults.notify_all();
                    }
                }
                catch (...)
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        if (!m_error)
                            m_error = std::current_exception();
                    }
                    m_cvResults.notify_all();
                }
            }

            void CParallelLeafBuilder::Encode(SChunk& chunk, SLeaves& leaves, CNodeLeafEncoder& encoder, std::vector<int64_t>& keys,
                                              std::vector<SOSMPointE7>& points)
            {
                keys.clear();
                points.clear();
                for (TBatch& batch : chunk.batches)
                {
                    for (const SNode& node : batch)
                    {
                        keys.push_back(node.id);
                        points.push_back(node.point);
                    }
                    TBatch().swap(batch);
                }

                const int64_t nNoLink = -1;
                uint32_t n = (uint32_t)keys.size();
                uint32_t nPos = 0;
                while (nPos < n)
                {
                    const int64_t* pKeys = keys.data() + nPos;
                    const SOSMPointE7* pPoints = points.data() + nPos;
                    uint32_t nFit = encoder.FindFit(pKeys, pPoints, n - nPos, m_nLeafPage, m_nMaxCount);

                    size_t nOffset = leaves.pages.size();
                    leaves.pages.resize(nOffset + m_nPayload, 0);
                    byte_t* pPage = &leaves.pages[nOffset];
                    bool bLeaf = true;
                    memcpy(pPage, &bLeaf, sizeof(bool));
                    memcpy(pPage + sizeof(bool), &nNoLink, sizeof(int64_t));
                    memcpy(pPage + sizeof(bool) + sizeof(int64_t), &nNoLink, sizeof(int64_t));

                    CommonLib::CFxMemoryWriteStream stream;
                    stream.AttachBuffer(pPage + sizeof(bool) + 2 * sizeof(int64_t), m_nLeafPage);
                    encoder.Encode(pKeys, pPoints, &stream);

                    leaves.firstKeys.push_back(pKeys[0]);
                    nPos += nFit;
                }
            }
        }
    }
}
