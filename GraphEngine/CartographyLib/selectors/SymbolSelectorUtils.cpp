#include "SymbolSelectorUtils.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/variant/VariantVisitor.h"
#include "../../DisplayLib/Symbols/SymbolsLoader.h"
#include <cstdlib>

namespace GraphEngine {
    namespace Cartography {

        bool SValueKey::operator<(const SValueKey& key) const
        {
            if(kind != key.kind)
                return kind < key.kind;
            if(kind == KindNumber)
                return dNumber < key.dNumber;
            if(kind == KindText)
                return sText < key.sText;
            return false;
        }

        bool SValueKey::operator==(const SValueKey& key) const
        {
            return !(*this < key) && !(key < *this);
        }

        namespace
        {
            template<class T>
            bool GetNumber(const CommonLib::CVariant& value, double& dValue)
            {
                const T* pValue = value.GetPtr<T>();
                if(!pValue)
                    return false;
                dValue = (double)*pValue;
                return true;
            }

            bool VariantToDouble(const CommonLib::CVariant& value, double& dValue)
            {
                return GetNumber<int8_t>(value, dValue) || GetNumber<uint8_t>(value, dValue) ||
                       GetNumber<int16_t>(value, dValue) || GetNumber<uint16_t>(value, dValue) ||
                       GetNumber<int32_t>(value, dValue) || GetNumber<uint32_t>(value, dValue) ||
                       GetNumber<int64_t>(value, dValue) || GetNumber<uint64_t>(value, dValue) ||
                       GetNumber<float>(value, dValue) || GetNumber<double>(value, dValue) ||
                       GetNumber<bool>(value, dValue);
            }
        }

        SValueKey CSymbolSelectorUtils::MakeKey(const CommonLib::CVariant& value)
        {
            SValueKey key;
            if(value.IsNull())
                return key;

            if(VariantToDouble(value, key.dNumber))
            {
                key.kind = SValueKey::KindNumber;
                return key;
            }

            if(value.IsType<CommonLib::astr_t>())
            {
                key.kind = SValueKey::KindText;
                key.sText = value.Get<CommonLib::astr_t>();
            }
            else if(value.IsType<CommonLib::wstr_t>())
            {
                key.kind = SValueKey::KindText;
                key.sText = CommonLib::StringEncoding::str_w2utf8_safe(value.Get<CommonLib::wstr_t>());
            }
            else if(value.IsType<CommonLib::CGuid>())
            {
                CommonLib::CStringVisitor visitor;
                value.Accept(visitor);
                key.kind = SValueKey::KindText;
                key.sText = visitor.GetString();
            }
            else
                key.kind = SValueKey::KindOther;

            return key;
        }

        SValueKey CSymbolSelectorUtils::MakeKey(GeoDatabase::IRowPtr ptrRow, int32_t nColumn)
        {
            if(ptrRow->ColumnIsNull(nColumn))
                return SValueKey();

            CommonLib::CVariantPtr ptrValue = ptrRow->GetValue(nColumn);
            return ptrValue.get() ? MakeKey(*ptrValue) : SValueKey();
        }

        bool CSymbolSelectorUtils::ToDouble(GeoDatabase::IRowPtr ptrRow, int32_t nColumn, double& dValue)
        {
            SValueKey key = MakeKey(ptrRow, nColumn);
            if(key.kind == SValueKey::KindNumber)
            {
                dValue = key.dNumber;
                return true;
            }

            if(key.kind == SValueKey::KindText && !key.sText.empty())
            {
                char* pEnd = nullptr;
                dValue = strtod(key.sText.c_str(), &pEnd);
                return pEnd && *pEnd == 0;
            }
            return false;
        }

        int32_t CSymbolSelectorUtils::FindColumn(GeoDatabase::IRowPtr ptrRow, const std::string& sField, int32_t& nCachedIndex)
        {
            int32_t nColumns = ptrRow->ColumnCount();
            if(nCachedIndex >= 0 && nCachedIndex < nColumns)
                return nCachedIndex;

            nCachedIndex = -1;
            for(int32_t i = 0; i < nColumns; ++i)
            {
                if(ptrRow->ColumnName(i) == sField)
                {
                    nCachedIndex = i;
                    break;
                }
            }

            if(nCachedIndex < 0)
                throw CommonLib::CExcBase("Symbol selector: field {0} not found", sField);
            return nCachedIndex;
        }

        void CSymbolSelectorUtils::SaveValue(CommonLib::ISerializeObjPtr pObj, const CommonLib::CVariant& value)
        {
            if(value.IsNull())
            {
                pObj->AddPropertyString("Kind", "null");
                return;
            }

            if(value.IsType<int8_t>() || value.IsType<int16_t>() || value.IsType<int32_t>() || value.IsType<int64_t>() ||
               value.IsType<uint8_t>() || value.IsType<uint16_t>() || value.IsType<uint32_t>())
            {
                double d = 0.;
                VariantToDouble(value, d);
                pObj->AddPropertyString("Kind", "int");
                pObj->AddPropertyInt64("Value", (int64_t)d);
            }
            else if(value.IsType<uint64_t>())
            {
                pObj->AddPropertyString("Kind", "uint");
                pObj->AddPropertyIntU64("Value", value.Get<uint64_t>());
            }
            else if(value.IsType<bool>())
            {
                pObj->AddPropertyString("Kind", "bool");
                pObj->AddPropertyBool("Value", value.Get<bool>());
            }
            else if(value.IsType<float>() || value.IsType<double>())
            {
                double d = 0.;
                VariantToDouble(value, d);
                pObj->AddPropertyString("Kind", "double");
                pObj->AddPropertyDouble("Value", d);
            }
            else
            {
                SValueKey key = MakeKey(value);
                if(key.kind != SValueKey::KindText)
                    throw CommonLib::CExcBase("Symbol selector: value of this type can't be saved, type: {0}", (int)value.GetTypeID());

                pObj->AddPropertyString("Kind", "text");
                pObj->AddPropertyString("Value", key.sText);
            }
        }

        CommonLib::CVariant CSymbolSelectorUtils::LoadValue(CommonLib::ISerializeObjPtr pObj)
        {
            std::string sKind = pObj->GetPropertyString("Kind", "null");
            if(sKind == "int")
                return CommonLib::CVariant(pObj->GetPropertyInt64("Value", 0));
            if(sKind == "uint")
                return CommonLib::CVariant(pObj->GetPropertyIntU64("Value", 0));
            if(sKind == "bool")
                return CommonLib::CVariant(pObj->GetPropertyBool("Value", false));
            if(sKind == "double")
                return CommonLib::CVariant(pObj->GetPropertyDouble("Value", 0.));
            if(sKind == "text")
                return CommonLib::CVariant(CommonLib::astr_t(pObj->GetPropertyString("Value", std::string())));
            return CommonLib::CVariant();
        }

        void CSymbolSelectorUtils::SaveSymbol(CommonLib::ISerializeObjPtr pObj, const std::string& sName, Display::ISymbolPtr ptrSymbol)
        {
            if(ptrSymbol.get())
                ptrSymbol->Save(pObj->CreateChildNode(sName));
        }

        Display::ISymbolPtr CSymbolSelectorUtils::LoadSymbol(CommonLib::ISerializeObjPtr pObj, const std::string& sName)
        {
            if(!pObj->IsChildExists(sName))
                return Display::ISymbolPtr();
            return Display::CSymbolsLoader::LoadSymbol(pObj->GetChild(sName));
        }

    }
}
