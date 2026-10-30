#pragma once

// Small helpers of the OSM -> B+ tree conversion.

#include <chrono>
#include <cstdint>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            typedef std::chrono::steady_clock TClock;

            inline double Since(TClock::time_point start)
            {
                return std::chrono::duration<double>(TClock::now() - start).count();
            }

            inline uint64_t NanosecondsSince(TClock::time_point start)
            {
                return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(TClock::now() - start).count();
            }

            // the physical memory of the computer (8 GB if unknown)
            uint64_t PhysicalMemory();
        }
    }
}
