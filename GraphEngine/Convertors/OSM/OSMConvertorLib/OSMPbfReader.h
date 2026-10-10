#pragma once
// Reader of .osm.pbf files (replaces readosm for PBF):
//  - the blocks are inflated and decoded by a pool of threads, the handler gets the objects in the file order
//    on the calling thread;
//  - no allocation per object: the block arrays are reused, the metadata (version, user, timestamp) isn't decoded;
//  - an index of the blocks (offset, size, object types) is built while a file is read and kept for the process:
//    the next passes over the same file read only the blocks with the requested objects
//    (the ways pass skips the node blocks, most of the file).
// The objects come to IOSMReadHandler as readosm structures (the metadata fields are empty).

#include "OSMReader.h"
#include <memory>

namespace GraphEngine {
    namespace Convertors {

        class COSMPbfReader
        {
        public:
            // sPath - UTF-8; nObjects - COSMReader::eObjects; nThreads - the decoding threads, 0 - by the number of the processors;
            // returns false if the handler stopped the reading; throws on an IO / format error
            static bool Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects, int nThreads = 0);

            // the number of blocks of the file in the index (0 - not indexed yet), for the tests
            static size_t GetIndexedBlockCount(const std::string& sPath);
            static void   ClearIndex();
        };
    }
}
