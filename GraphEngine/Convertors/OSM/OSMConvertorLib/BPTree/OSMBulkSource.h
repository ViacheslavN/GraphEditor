#pragma once

// CBulkSource: the source of TBPMap::BulkLoad (the one thread build) - the batches of CNodeQueue.

#include "OSMNodeQueue.h"
#include "OSMBPTreeSettings.h"

#include <functional>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the loading thread: the nodes of the queue one by one, the progress, the header (count, ids, bounds)
            class CBulkSource
            {
            public:
                CBulkSource(CNodeQueue& queue, SOSMBPTreeNodeHeader& header, std::function<void()> fnProgress) :
                        m_queue(queue), m_header(header), m_fnProgress(fnProgress)
                {}

                // TBPMap::BulkLoad
                bool Next(int64_t& nId, SOSMPointE7& point)
                {
                    if (m_nPos == m_batch.size())
                    {
                        m_batch.clear();
                        m_nPos = 0;
                        if (!m_queue.Continue())
                        {
                            m_bCanceled = true;
                            return false;
                        }
                        m_fnProgress();
                        if (!m_queue.Pop(m_batch, m_dWaitSeconds))
                            return false;
                    }
                    const SNode& node = m_batch[m_nPos++];
                    nId = node.id;
                    point = node.point;
                    m_header.Add(node.id, node.point);
                    return true;
                }

                bool IsCanceled() const { return m_bCanceled; }
                double WaitSeconds() const { return m_dWaitSeconds; }

            private:
                CNodeQueue& m_queue;
                SOSMBPTreeNodeHeader& m_header;
                std::function<void()> m_fnProgress;
                TBatch m_batch;
                size_t m_nPos = 0;
                double m_dWaitSeconds = 0.;
                bool   m_bCanceled = false;
            };
        }
    }
}
