#pragma once

// CNodeQueue: the nodes of an OSM file read by the reading thread and handed to the loading thread in batches.

#include "../OSMReader.h"
#include "OSMBPTreeSettings.h"
#include "OSMPointE7.h"
#include "OSMTagWriter.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            struct SNode
            {
                int64_t     id;
                SOSMPointE7 point;
            };
            typedef std::vector<SNode> TBatch;

            // The reading thread (Read, IOSMReadHandler): the nodes of the file in batches. The nodes in the id order go to
            // the queue, the others aside (inserted after the tree is built). The loading thread takes the batches (Pop).
            // The tags of the nodes go to the tag thread (SetTagWriter, CTagWriter::Push) in batches of nTagBatchBytes.
            class CNodeQueue : public IOSMReadHandler
            {
            public:
                CNodeQueue(const SOSMBPTreeSettings& settings, Display::ITrackCancelPtr ptrCancel);

                // the tags of the nodes go there (before Read), null - no tags
                void SetTagWriter(CTagWriter* pTagWriter) { m_pTagWriter = pTagWriter; }

                // the reading thread: the whole file, then the queue is done (the error is kept: RethrowError)
                void Read(const std::string& sPath);
                // the loading thread: the next batch, false - the file is read (or the reading is stopped)
                bool Pop(TBatch& batch, double& dWaitSeconds);
                // the loading thread: the reading stops (an error / canceled)
                void Stop();
                // after the reading thread is joined
                void RethrowError();
                bool Continue() const { return !m_ptrCancel.get() || m_ptrCancel->Continue(); }

                // IOSMReadHandler
                virtual bool OnNode(const readosm_node& node) override;
                virtual bool OnWay(const readosm_way&) override { return true; }
                virtual bool OnRelation(const readosm_relation&) override { return true; }
                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal) override;
                virtual void OnReaderStats(const SOSMReaderStats& stats) override;

                SOSMReaderStats ReaderStats();
                uint64_t Read() const { return m_nRead.load(std::memory_order_relaxed); }
                uint64_t BytesRead() const { return m_nBytesRead.load(); }
                uint64_t BytesTotal() const { return m_nBytesTotal.load(); }
                uint64_t UnsortedCount() const { return m_nUnsorted.load(std::memory_order_relaxed); }
                // after the reading thread is joined
                std::vector<SNode>& Unsorted() { return m_unsorted; }

            private:
                // the current batch to the queue (waits while the queue is full)
                bool Push();
                // the current tags to the tag thread
                bool PushTags();

            private:
                const SOSMBPTreeSettings& m_settings;
                Display::ITrackCancelPtr m_ptrCancel;

                // the reading thread
                TBatch   m_current;
                int64_t  m_nLastId = (std::numeric_limits<int64_t>::min)();
                std::vector<SNode> m_unsorted;
                CTagWriter* m_pTagWriter = nullptr;
                STagBatch m_tags;

                std::mutex m_mutex;
                std::condition_variable m_cvLoad;   // a batch is queued / the reading is done
                std::condition_variable m_cvRead;   // a batch is taken / stop
                std::deque<TBatch> m_queue;
                bool m_bDone = false;
                std::atomic<bool> m_bStop{false};   // set under m_mutex (the waits), read by OnProgress without it
                std::exception_ptr m_error;

                std::atomic<uint64_t> m_nRead{0};
                std::atomic<uint64_t> m_nUnsorted{0};
                std::atomic<uint64_t> m_nBytesRead{0};
                std::atomic<uint64_t> m_nBytesTotal{0};
                std::mutex m_statsMutex;
                SOSMReaderStats m_readerStats;
            };
        }
    }
}
