#pragma once

// The helpers of the progress / the results of COSMBPTreeConvertor.

#include "../OSMConvertorLib.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            inline SOSMProgressItem Item(const char* pszName, const std::string& sInfo, uint64_t nCount, uint64_t nBytes, double dSeconds, bool bThreads = false)
            {
                SOSMProgressItem item;
                item.bThreads = bThreads;
                item.sName = pszName;
                item.sInfo = sInfo;
                item.nCount = nCount;
                item.nBytes = nBytes;
                item.dSeconds = dSeconds;
                return item;
            }

            // 0 - no such file
            inline uint64_t FileSize(const std::string& sPath)
            {
                std::error_code ec;
                uint64_t nSize = std::filesystem::file_size(std::filesystem::u8path(sPath), ec);
                return ec ? 0 : nSize;
            }
        }
    }
}
