#pragma once

// CReaderThread: the thread reading the OSM file into CNodeQueue, stopped and joined on any exit.

#include "OSMNodeQueue.h"

#include <string>
#include <thread>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class CReaderThread
            {
            public:
                CReaderThread(CNodeQueue& queue, const std::string& sPath) : m_queue(queue)
                {
                    m_thread = std::thread([this, sPath]() { m_queue.Read(sPath); });
                }
                ~CReaderThread() { Join(true); }

                // bStop - the reading stops (the queue isn't taken any more)
                void Join(bool bStop)
                {
                    if (!m_thread.joinable())
                        return;
                    if (bStop)
                        m_queue.Stop();
                    m_thread.join();
                }

            private:
                CReaderThread(const CReaderThread&);
                CReaderThread& operator=(const CReaderThread&);

            private:
                CNodeQueue& m_queue;
                std::thread m_thread;
            };
        }
    }
}
