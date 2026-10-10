#pragma once
#include "OSMConvertorLib.h"
#include "readosm/readosm.h"

namespace GraphEngine {
    namespace Convertors {

        // receives the objects of an OSM file, returns false to stop the reading
        class IOSMReadHandler
        {
        public:
            IOSMReadHandler(){}
            virtual ~IOSMReadHandler(){}
            virtual bool OnNode(const readosm_node& node) = 0;
            virtual bool OnWay(const readosm_way& way) = 0;
            virtual bool OnRelation(const readosm_relation& relation) = 0;
        };

        // C++ wrapper of readosm: .osm (expat) or .pbf by the file suffix.
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
            static bool Read(const std::string& sPath, IOSMReadHandler& handler, int nObjects = ReadAll);

            static std::string ErrorText(int nCode);
        };
    }
}
