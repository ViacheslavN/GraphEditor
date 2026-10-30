#pragma once

// CTagWriter: the tag tree (OSMTagTreeTypes.h) written by its own thread from an asynchronous queue
// (the tags of the nodes: <name>.osmtags, of the ways: <name>.osmwaytags).

#include "OSMTagTreeTypes.h"
#include "OSMBPTreeSettings.h"
#include "../readosm/readosm.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the tags of some nodes: the encoded tags of node i are data[offsets[i], offsets[i + 1]) (the last one - to the end)
            struct STagBatch
            {
                std::vector<int64_t>  ids;
                std::vector<uint32_t> offsets;
                std::string           data;
                uint64_t              nTags = 0;

                // the tags of an object (key US value RS ...)
                void Add(int64_t nId, int nTagCount, const readosm_tag* pTags);
                void Add(const readosm_node& node) { Add(node.id, node.tag_count, node.tags); }
                void Add(const readosm_way& way) { Add(way.id, way.tag_count, way.tags); }
                void Add(const readosm_relation& relation) { Add(relation.id, relation.tag_count, relation.tags); }
                size_t Count() const { return ids.size(); }
                size_t Size(size_t i) const { return (i + 1 < offsets.size() ? offsets[i + 1] : data.size()) - offsets[i]; }
                void Clear() { ids.clear(); offsets.clear(); data.clear(); nTags = 0; }
            };

            // The thread of the tag tree: its own file (storage, pager, tree), the tree is filled by the tree interface:
            //   - TBPMap::BulkLoad from the queue (the ids ascending, a leaf is cut where the page is full);
            //   - the nodes out of the id order are kept aside and inserted (insert) when the queue is done.
            // The reading thread hands the tags over (Push, waits while the queue is full); the loading thread ends it
            // (Finish) when the file is read.
            class CTagWriter
            {
            public:
                CTagWriter(const SOSMBPTreeSettings& settings, const std::string& sTagFile);
                ~CTagWriter();

                // the reading thread: the batch is taken (empty after it); false - the writer stopped (an error / Stop)
                bool Push(STagBatch& batch);
                // no more tags: the tree is finished, the file is written; the error of the thread is thrown
                void Finish();
                // the thread stops (canceled / an error elsewhere), the file is incomplete
                void Stop();

                const std::string& FilePath() const { return m_sFile; }
                const SOSMBPTreeTagHeader& Header() const { return m_header; }   // after Finish
                uint64_t Nodes() const { return m_nNodes.load(); }               // taken by the tree so far
                uint64_t RawBytes() const { return m_nRawBytes.load(); }
                uint64_t Unsorted() const { return m_nUnsorted.load(); }
                uint64_t Duplicates() const { return m_nDuplicates; }            // after Finish
                double   WorkSeconds() const { return m_nWorkNs.load() / 1e9; }  // the thread without the waiting for tags
                double   WaitSeconds() const { return m_nWaitNs.load() / 1e9; }  // the thread waits for tags

            private:
                class CSource;
                friend class CSource;

                CTagWriter(const CTagWriter&);
                CTagWriter& operator=(const CTagWriter&);

                void Run();
                // the tag thread: the next batch, false - the queue is done / stopped
                bool Pop(STagBatch& batch);

            private:
                SOSMBPTreeSettings m_settings;
                std::string m_sFile;
                SOSMBPTreeTagHeader m_header;

                std::thread m_thread;
                std::mutex m_mutex;
                std::condition_variable m_cvPush;   // a batch is taken / stop
                std::condition_variable m_cvPop;    // a batch is queued / the input is done / stop
                std::deque<STagBatch> m_queue;
                bool m_bInputDone = false;
                bool m_bStop = false;
                bool m_bFinished = false;           // the thread ended (done or an error)
                std::exception_ptr m_error;

                std::atomic<uint64_t> m_nNodes{0};
                std::atomic<uint64_t> m_nRawBytes{0};
                std::atomic<uint64_t> m_nUnsorted{0};
                std::atomic<uint64_t> m_nWorkNs{0};
                std::atomic<uint64_t> m_nWaitNs{0};
                uint64_t m_nDuplicates = 0;
            };
        }
    }
}
