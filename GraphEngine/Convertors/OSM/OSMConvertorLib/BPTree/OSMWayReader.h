#pragma once

// CWayReader: the ways of an OSM file read by the reading thread: the batches of ways go to the resolving threads
// (CWayResolver), their tags to the tag thread (CTagWriter).

#include "../OSMReader.h"
#include "OSMBPTreeSettings.h"
#include "OSMWayResolver.h"
#include "OSMTagWriter.h"

#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class CWayReader : public IOSMReadHandler
            {
            public:
                // pTags - the tag thread of the ways, null - no tags
                CWayReader(const SOSMBPTreeSettings& settings, CWayResolver& resolver, CTagWriter* pTags, Display::ITrackCancelPtr ptrCancel);

                // the reading thread: all the ways of the file, then the resolver gets no more jobs (also on an error: it is kept)
                void Read(const std::string& sPath);
                // the reading stops
                void Stop() { m_bStop = true; }
                // after the reading thread is joined
                void RethrowError();

                // IOSMReadHandler
                virtual bool OnNode(const readosm_node&) override { return true; }
                virtual bool OnWay(const readosm_way& way) override;
                virtual bool OnRelation(const readosm_relation&) override { return true; }
                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal) override;
                virtual void OnReaderStats(const SOSMReaderStats& stats) override;

                SOSMReaderStats ReaderStats();
                uint64_t Read() const { return m_nRead.load(std::memory_order_relaxed); }
                uint64_t BytesRead() const { return m_nBytesRead.load(); }
                uint64_t BytesTotal() const { return m_nBytesTotal.load(); }

            private:
                bool SubmitBatch();
                bool PushTags();

            private:
                const SOSMBPTreeSettings& m_settings;
                CWayResolver& m_resolver;
                CTagWriter* m_pTags;
                Display::ITrackCancelPtr m_ptrCancel;

                // the reading thread
                SWayBatch m_batch;
                STagBatch m_tags;

                std::atomic<bool> m_bStop{false};
                std::exception_ptr m_error;
                std::atomic<uint64_t> m_nRead{0};
                std::atomic<uint64_t> m_nBytesRead{0};
                std::atomic<uint64_t> m_nBytesTotal{0};
                std::mutex m_statsMutex;
                SOSMReaderStats m_readerStats;
            };

            // the reading thread of the ways (CWayReader) / the relations (CRelationReader): on any exit fnStop is called (the
            // reader / resolver / tag thread stop, a blocked reading thread wakes up) and the thread is joined
            class CWayReaderThread
            {
            public:
                template <class TReader>
                CWayReaderThread(TReader& reader, const std::string& sPath, std::function<void()> fnStop) : m_fnStop(fnStop)
                {
                    m_thread = std::thread([&reader, sPath]() { reader.Read(sPath); });
                }
                ~CWayReaderThread() { Join(true); }

                void Join(bool bStop)
                {
                    if (!m_thread.joinable())
                        return;
                    if (bStop && m_fnStop)
                        m_fnStop();
                    m_thread.join();
                }

            private:
                CWayReaderThread(const CWayReaderThread&);
                CWayReaderThread& operator=(const CWayReaderThread&);

            private:
                std::function<void()> m_fnStop;
                std::thread m_thread;
            };
        }
    }
}
