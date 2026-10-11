#pragma once
#include "OSMConvertorLib.h"
#include "readosm/readosm.h"

namespace GraphEngine {
    namespace Convertors {

        // the work of the reading of a .pbf file so far (one Read call)
        struct SOSMReaderStats
        {
            int      nThreads = 0;          // the decoding threads
            uint64_t nBlocks = 0;           // the blocks handed to the handler
            uint64_t nBytesRead = 0;        // read from the file
            double   dReadSeconds = 0.;     // reading the file on the reading thread (the disk)
            double   dWaitSeconds = 0.;     // the reading thread waits for the decoding threads
            double   dDispatchSeconds = 0.; // the handler (the objects) on the reading thread
            double   dDecodeSeconds = 0.;   // inflating / decoding, summed over the decoding threads
        };

        // receives the objects of an OSM file, returns false to stop the reading
        class IOSMReadHandler
        {
        public:
            IOSMReadHandler(){}
            virtual ~IOSMReadHandler(){}
            virtual bool OnNode(const readosm_node& node) = 0;
            virtual bool OnWay(const readosm_way& way) = 0;
            virtual bool OnRelation(const readosm_relation& relation) = 0;
            // .pbf: after every block, nBytesRead of nBytesTotal (the file or the blocks with the requested objects) are processed;
            // returns false to stop the reading
            virtual bool OnProgress(uint64_t /*nBytesRead*/, uint64_t /*nBytesTotal*/) { return true; }
            // .pbf: before every OnProgress
            virtual void OnReaderStats(const SOSMReaderStats& /*stats*/) {}
        };

        // the items of the reading (SOSMProgressItem): the file, the decoding, the waiting, the handler
        void AddReaderItems(std::vector<SOSMProgressItem>& vecItems, const SOSMReaderStats& stats, const std::string& sPath);

        // Reads .osm with readosm (expat), .pbf with COSMPbfReader (multithreaded), by the file suffix.
        // Exceptions of the handler are passed through readosm (C code) and rethrown.
        class COSMReader
        {
        public:
            enum eObjects
            {
                ReadNodes     = 1,
                ReadWays      = 2,
                ReadRelations = 4,
                ReadAll       = 7
            };

            // returns false if the handler stopped the reading
            // sPath - UTF-8; nThreads - the decoding threads of a .pbf file, 0 - by the number of the processors
            static bool Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects = ReadAll, int nThreads = 0);

            static std::string ErrorText(int nCode);
        };
    }
}
