#pragma once

// CRelationReader: the relations of an OSM file read by the reading thread: the batches of relations go to the resolving
// threads (CRelationResolver), their tags to the tag thread (CTagWriter). The reading thread: CWayReaderThread.

#include "../OSMReader.h"
#include "OSMBPTreeSettings.h"
#include "OSMRelationResolver.h"
#include "OSMTagWriter.h"

#include <atomic>
#include <exception>
#include <mutex>
#include <string>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class CRelationReader : public IOSMReadHandler
            {
            public:
                CRelationReader(const SOSMBPTreeSettings& settings, CRelationResolver& resolver, CTagWriter* pTags, Display::ITrackCancelPtr ptrCancel);

                // the reading thread: all the relations of the file, then the resolver gets no more jobs
                void Read(const std::string& sPath);
                void Stop() { m_bStop = true; }
                void RethrowError();

                // a multipolygon / boundary relation (its shape - a polygon)
                static bool IsArea(const readosm_relation& relation);

                virtual bool OnNode(const readosm_node&) override { return true; }
                virtual bool OnWay(const readosm_way&) override { return true; }
                virtual bool OnRelation(const readosm_relation& relation) override;
                virtual bool OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal) override;
                virtual void OnReaderStats(const SOSMReaderStats& stats) override;

                SOSMReaderStats ReaderStats();
                uint64_t Read() const { return m_nRead.load(std::memory_order_relaxed); }
                uint64_t BytesRead() const { return m_nBytesRead.load(); }
                uint64_t BytesTotal() const { return m_nBytesTotal.load(); }

            private:
                const SOSMBPTreeSettings& m_settings;
                CRelationResolver& m_resolver;
                CTagWriter* m_pTags;
                Display::ITrackCancelPtr m_ptrCancel;

                SRelationBatch m_batch;
                STagBatch m_tags;

                std::atomic<bool> m_bStop{false};
                std::exception_ptr m_error;
                std::atomic<uint64_t> m_nRead{0};
                std::atomic<uint64_t> m_nBytesRead{0};
                std::atomic<uint64_t> m_nBytesTotal{0};
                std::mutex m_statsMutex;
                SOSMReaderStats m_readerStats;
            };
        }
    }
}
