#include "OSMWayReader.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            CWayReader::CWayReader(const SOSMBPTreeSettings& settings, CWayResolver& resolver, CTagWriter* pTags, Display::ITrackCancelPtr ptrCancel) :
                    m_settings(settings), m_resolver(resolver), m_pTags(pTags), m_ptrCancel(ptrCancel)
            {
            }

            void CWayReader::Read(const std::string& sPath)
            {
                try
                {
                    if (COSMReader::Read(sPath, *this, COSMReader::ReadWays, m_settings.nThreads))
                    {
                        if (m_batch.Count() != 0)
                            SubmitBatch();
                        if (m_tags.Count() != 0)
                            PushTags();
                    }
                }
                catch (...)
                {
                    m_error = std::current_exception();
                }
                m_resolver.InputDone();
            }

            void CWayReader::RethrowError()
            {
                if (m_error)
                    std::rethrow_exception(m_error);
            }

            bool CWayReader::OnWay(const readosm_way& way)
            {
                m_nRead.fetch_add(1, std::memory_order_relaxed);
                m_batch.Add(way);
                if (m_pTags && way.tag_count > 0)
                {
                    m_tags.Add(way);
                    if (m_tags.data.size() >= m_settings.nTagBatchBytes && !PushTags())
                        return false;
                }
                if (m_batch.refs.size() >= m_settings.nWayBatchRefs)
                    return SubmitBatch();
                return true;
            }

            bool CWayReader::SubmitBatch()
            {
                // false: the resolver stopped (canceled / an error, thrown by the writer), the reading stops
                return m_resolver.Submit(m_batch);
            }

            bool CWayReader::PushTags()
            {
                return m_pTags->Push(m_tags);
            }

            bool CWayReader::OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
            {
                m_nBytesRead = nBytesRead;
                m_nBytesTotal = nBytesTotal;
                if (m_bStop)
                    return false;
                return !m_ptrCancel.get() || m_ptrCancel->Continue();
            }

            void CWayReader::OnReaderStats(const SOSMReaderStats& stats)
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_readerStats = stats;
            }

            SOSMReaderStats CWayReader::ReaderStats()
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                return m_readerStats;
            }
        }
    }
}
