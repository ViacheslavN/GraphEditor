#include "OSMNodeQueue.h"
#include "OSMBPTreeUtils.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            CNodeQueue::CNodeQueue(const SOSMBPTreeSettings& settings, Display::ITrackCancelPtr ptrCancel) :
                    m_settings(settings), m_ptrCancel(ptrCancel)
            {
                m_current.reserve(m_settings.nBatchNodes);
            }

            void CNodeQueue::Read(const std::string& sPath)
            {
                try
                {
                    COSMReader::Read(sPath, *this, COSMReader::ReadNodes, m_settings.nThreads);
                    if (!m_current.empty())
                        Push();
                    if (m_tags.Count() != 0)
                        PushTags();
                }
                catch (...)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_error = std::current_exception();
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bDone = true;
                }
                m_cvLoad.notify_all();
            }

            bool CNodeQueue::Pop(TBatch& batch, double& dWaitSeconds)
            {
                auto start = TClock::now();
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvLoad.wait(lock, [this]() { return !m_queue.empty() || m_bDone || m_bStop; });
                dWaitSeconds += Since(start);
                if (m_queue.empty() || m_bStop)
                    return false;
                batch.swap(m_queue.front());
                m_queue.pop_front();
                lock.unlock();
                m_cvRead.notify_one();
                return true;
            }

            void CNodeQueue::Stop()
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bStop = true;
                }
                m_cvRead.notify_all();
                m_cvLoad.notify_all();
            }

            void CNodeQueue::RethrowError()
            {
                if (m_error)
                    std::rethrow_exception(m_error);
            }

            bool CNodeQueue::OnNode(const readosm_node& node)
            {
                SNode item;
                item.id = node.id;
                item.point = SOSMPointE7::FromDegrees(node.latitude, node.longitude);
                m_nRead.fetch_add(1, std::memory_order_relaxed);

                if (m_pTagWriter && node.tag_count > 0)
                {
                    m_tags.Add(node);
                    if (m_tags.data.size() >= m_settings.nTagBatchBytes && !PushTags())
                        return false;
                }

                if (item.id > m_nLastId)
                {
                    m_nLastId = item.id;
                    m_current.push_back(item);
                    if (m_current.size() >= m_settings.nBatchNodes)
                        return Push();
                }
                else
                {
                    m_unsorted.push_back(item);
                    m_nUnsorted.fetch_add(1, std::memory_order_relaxed);
                }
                return true;
            }

            bool CNodeQueue::OnProgress(uint64_t nBytesRead, uint64_t nBytesTotal)
            {
                m_nBytesRead = nBytesRead;
                m_nBytesTotal = nBytesTotal;
                if (m_bStop)
                    return false;
                return Continue();
            }

            void CNodeQueue::OnReaderStats(const SOSMReaderStats& stats)
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_readerStats = stats;
            }

            SOSMReaderStats CNodeQueue::ReaderStats()
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                return m_readerStats;
            }

            bool CNodeQueue::PushTags()
            {
                // false: the tag thread stopped (canceled / its error is thrown by CTagWriter::Finish), the reading stops
                return m_pTagWriter->Push(m_tags);
            }

            bool CNodeQueue::Push()
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvRead.wait(lock, [this]() { return m_queue.size() < m_settings.nQueueBatches || m_bStop; });
                if (m_bStop)
                    return false;
                m_queue.emplace_back();
                m_queue.back().swap(m_current);
                lock.unlock();
                m_cvLoad.notify_one();
                m_current.clear();
                m_current.reserve(m_settings.nBatchNodes);
                return Continue();
            }
        }
    }
}
