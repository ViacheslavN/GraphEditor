#include "GeoDatabase.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CGeoDatabaseUtils
        {
        public:
           static std::any GetVariantFromString(eDataTypes type, const std::string& sData);
        };
    }
}