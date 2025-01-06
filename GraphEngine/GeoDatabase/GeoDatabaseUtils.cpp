#include "GeoDatabaseUtils.h"
#include "../CommonLib/str/convertToString.h"

namespace GraphEngine {
    namespace GeoDatabase {

        std::any CGeoDatabaseUtils::GetVariantFromString(eDataTypes type, const std::string& sData)
        {
            switch(type)
            {
                case dtInteger8:
                    return CommonLib::str_utils::AStr2Int8(sData);
                    break;
                case dtUInteger8:
                    return (uint8_t )CommonLib::str_utils::AStr2Int8(sData);
                    break;
                case dtInteger16:
                    return CommonLib::str_utils::AStr2Int16(sData);
                    break;
                case dtUInteger16:
                    return (uint16_t )CommonLib::str_utils::AStr2Int16(sData);
                    break;
                case dtInteger32:
                    return CommonLib::str_utils::AStr2Int32(sData);
                    break;
                case dtUInteger32:
                    return (uint32_t )CommonLib::str_utils::AStr2Int32(sData);
                    break;
                case dtFloat:
                    return CommonLib::str_utils::AStr2Float(sData);
                    break;
                case dtDouble:
                    return CommonLib::str_utils::AStr2Double(sData);
                    break;
                case dtString:
                {
                    return sData;
                }
                    break;
                case dtInteger64:
                {
                    return CommonLib::str_utils::AStr2Int64(sData);
                }
                    break;
                case dtUInteger64:
                {
                    return (uint64_t )CommonLib::str_utils::AStr2Int64(sData);
                }
                    break;

            }

            return std::any();
        }

    }
}