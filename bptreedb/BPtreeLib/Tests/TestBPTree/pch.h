#pragma once

#include <iostream>
#include "../../../../CommonLib/CommonLib.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../BPSet.h"
#include "../../BPMap.h"
#include "../../BPTreeInnerNode.h"
#include "../../BPTreeLeafNode.h"
#include "../../BPTreeMapLeafNode.h"
#include "../../EmptyEncoder.h"
#include "../../BaseDIffEncoder.h"
#include "../../BaseNodeCompressor.h"
#include "../../BaseLeafNodeSetCompressor.h"
#include "../../../StorageLib/Storage.h"
#include "../../../StorageLib/FilePager.h"
#include "../../../StorageLib/FileStorage.h"
#include "../../../../CommonLib/log/logger.h"
#include "../../../../CommonLib/log/ConsoleLogWriter.h"
#include "../../../../CommonLib/log/LogInfo.h"
#include "../../../../CommonLib/perf/PerfCount.h"



template<class _Ty>
struct comp
{
    bool LE(const _Ty& _Left, const _Ty& _Right) const
    {
        return (_Left < _Right);
    }
    bool EQ(const _Ty& _Left, const _Ty& _Right) const
    {
        return (_Left == _Right);
    }
    bool operator() (const _Ty& _Left, const _Ty& _Right) const
    {
        return (_Left < _Right);
    }
    /*bool MR(const _Ty& _Left, const _Ty& _Right){
    return (_Left > _Right);
    }*/
};
