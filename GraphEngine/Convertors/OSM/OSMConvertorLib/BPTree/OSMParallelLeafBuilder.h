#pragma once

// CParallelLeafBuilder: the leaves of the node tree encoded by several threads.

#include "OSMNodeQueue.h"
#include "OSMNodeTreeBuilder.h"
#include "OSMNodeLeafEncoder.h"

#include "../../../../../bptreedb/StorageLib/FilePager.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // The leaves of the node tree built by several threads:
            //   - the sorted nodes are collected into chunks (nChunkNodes), every chunk is a job;
            //   - a worker encodes the leaves of its chunk into page images (CNodeLeafEncoder: every leaf as full as the page
            //     allows, the next leaf starts where the previous one is cut; the last leaf of a chunk is partly filled);
            //   - the calling thread writes the pages in the chunk order: the addresses are allocated in sequence, the
            //     next / prev links are put into the pages, the first keys of the leaves are collected (the inner levels:
            //     CNodeTreeBuilder::BuildInner).
            // The page image: bool leaf, int64 next, int64 prev, the compressor data (the format of TNodeLeafCompressor).
            class CParallelLeafBuilder
            {
            public:
                typedef CNodeTreeBuilder::TLevelEntry TLevelEntry;

                // nPagePayload - the payload of a page of the tree (CNodeTreeBuilder::NodePagePayload)
                CParallelLeafBuilder(bptreedb::TCompressorParamsBasePtr ptrParams, uint32_t nPagePayload, int nThreads, uint32_t nChunkNodes,
                                     std::shared_ptr<bptreedb::storage::CFilePager> ptrPager);
                ~CParallelLeafBuilder();

                // the calling thread: the next batch of the sorted nodes (taken, the batch is empty after it)
                void Add(TBatch& batch);
                // all the leaves are written
                void Finish();
                // the workers stop (Finish, an error / canceled)
                void Stop();

                std::vector<TLevelEntry>& Level() { return m_level; }
                int      Threads() const { return m_nThreads; }
                uint64_t Leaves() const { return m_nLeaves; }   // m_level becomes the inner levels (BuildInner)
                double   EncodeSeconds() const { return m_nEncodeNs.load() / 1e9; }
                double   WriteSeconds() const { return m_dWriteSeconds; }
                double   WaitSeconds() const { return m_dWaitSeconds; }

            private:
                struct SChunk
                {
                    uint64_t nSeq = 0;
                    std::vector<TBatch> batches;
                };
                struct SLeaves
                {
                    std::vector<byte_t>  pages;      // the page images, m_nPayload bytes each
                    std::vector<int64_t> firstKeys;
                };

                CParallelLeafBuilder(const CParallelLeafBuilder&);
                CParallelLeafBuilder& operator=(const CParallelLeafBuilder&);

                void Submit();
                // writes the leaves of the next chunk (in the order) if they are encoded; bWait - waits for them
                bool WriteNext(bool bWait);
                void WriteLeaves(const SLeaves& leaves);
                void WorkerLoop();
                void Encode(SChunk& chunk, SLeaves& leaves, CNodeLeafEncoder& encoder, std::vector<int64_t>& keys, std::vector<SOSMPointE7>& points);

            private:
                bptreedb::TCompressorParamsBasePtr m_ptrParams;
                uint32_t m_nPayload;
                uint32_t m_nLeafPage = 0;
                uint32_t m_nMaxCount = 0;
                uint32_t m_nChunkNodes;
                int      m_nThreads = 1;
                uint64_t m_nMaxInFlight = 2;
                std::shared_ptr<bptreedb::storage::CFilePager> m_ptrPager;

                // the calling thread
                std::vector<TBatch> m_current;
                uint64_t m_nCurrentNodes = 0;
                uint64_t m_nSubmitted = 0;
                uint64_t m_nWritten = 0;
                bptreedb::storage::IFilePagePtr m_ptrPending;   // the last leaf: its next link is set by the next one
                int64_t  m_nPendingAddr = -1;
                std::vector<TLevelEntry> m_level;
                uint64_t m_nLeaves = 0;
                double   m_dWriteSeconds = 0.;
                double   m_dWaitSeconds = 0.;

                std::vector<std::thread> m_threads;
                std::mutex m_mutex;
                std::condition_variable m_cvJobs;
                std::condition_variable m_cvResults;
                std::deque<SChunk> m_jobs;
                std::map<uint64_t, SLeaves> m_results;
                bool m_bStop = false;
                std::exception_ptr m_error;
                std::atomic<uint64_t> m_nEncodeNs{0};
            };
        }
    }
}
