#include "OSMBPTreeUtils.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            uint64_t PhysicalMemory()
            {
#ifdef _WIN32
                MEMORYSTATUSEX status;
                status.dwLength = sizeof(status);
                if (::GlobalMemoryStatusEx(&status))
                    return status.ullTotalPhys;
                return 8ull * 1024 * 1024 * 1024;
#else
                long nPages = sysconf(_SC_PHYS_PAGES);
                long nPageSize = sysconf(_SC_PAGE_SIZE);
                return nPages > 0 && nPageSize > 0 ? (uint64_t)nPages * (uint64_t)nPageSize : 8ull * 1024 * 1024 * 1024;
#endif
            }
        }
    }
}
