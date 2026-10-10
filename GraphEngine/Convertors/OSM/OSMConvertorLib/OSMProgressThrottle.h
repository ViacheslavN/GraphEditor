#pragma once
#include <chrono>

namespace GraphEngine {
    namespace Convertors {

        // Limits how often the progress is sent to IProgressUpdater: at most once per interval (1 s). The stage changes
        // and the end of a pass are sent at once (Reset); the cancel is still checked on every step.
        class COSMProgressThrottle
        {
        public:
            explicit COSMProgressThrottle(std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) :
                    m_interval(interval), m_last(std::chrono::steady_clock::now())
            {}

            // the interval passed since the last report: true and the next interval starts
            bool IsTime()
            {
                auto now = std::chrono::steady_clock::now();
                if(now - m_last < m_interval)
                    return false;
                m_last = now;
                return true;
            }

            // a report was sent now (a stage change)
            void Reset()
            {
                m_last = std::chrono::steady_clock::now();
            }

        private:
            std::chrono::milliseconds m_interval;
            std::chrono::steady_clock::time_point m_last;
        };
    }
}
