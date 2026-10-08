#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        // value of a field normalized for comparison: numbers by value whatever the type, texts as UTF-8
        struct SValueKey
        {
            enum eKind
            {
                KindNull = 0,
                KindNumber = 1,
                KindText = 2,
                KindOther = 3   // shape, blob - never match
            };

            eKind       kind = KindNull;
            double      dNumber = 0.;
            std::string sText;

            bool operator<(const SValueKey& key) const;
            bool operator==(const SValueKey& key) const;
        };

        class CSymbolSelectorUtils
        {
        public:
            static SValueKey MakeKey(const CommonLib::CVariant& value);
            static SValueKey MakeKey(GeoDatabase::IRowPtr ptrRow, int32_t nColumn);

            // numeric value: numbers, bool, numeric text; false - null or not a number
            static bool ToDouble(GeoDatabase::IRowPtr ptrRow, int32_t nColumn, double& dValue);

            // column of the field in the row, nCachedIndex keeps the found index (-1 - search again)
            static int32_t FindColumn(GeoDatabase::IRowPtr ptrRow, const std::string& sField, int32_t& nCachedIndex);

            static void SaveValue(CommonLib::ISerializeObjPtr pObj, const CommonLib::CVariant& value);
            static CommonLib::CVariant LoadValue(CommonLib::ISerializeObjPtr pObj);

            static void SaveSymbol(CommonLib::ISerializeObjPtr pObj, const std::string& sName, Display::ISymbolPtr ptrSymbol);
            static Display::ISymbolPtr LoadSymbol(CommonLib::ISerializeObjPtr pObj, const std::string& sName);
        };

    }
}
