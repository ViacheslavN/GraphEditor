#include "SQLRowFilter.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include "../../../CommonLib/variant/VariantVisitor.h"

namespace GraphEngine {
    namespace GeoDatabase {

        using SQLFilter::CSQLValue;

        namespace
        {
            bool EqualNoCase(const std::string& a, const char* b, size_t nLenB)
            {
                if(a.size() != nLenB)
                    return false;
                for(size_t i = 0; i < nLenB; ++i)
                {
                    char ca = a[i], cb = b[i];
                    if(ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
                    if(cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
                    if(ca != cb)
                        return false;
                }
                return true;
            }

            bool EqualNoCase(const std::string& a, const std::string& b)
            {
                return EqualNoCase(a, b.c_str(), b.size());
            }

            // the column is prefix.name: "prefix.name" as one string without building it
            bool IsQualifiedName(const std::string& sColumn, const std::string& sPrefix, const std::string& sName)
            {
                return sColumn.size() == sPrefix.size() + 1 + sName.size() && sColumn[sPrefix.size()] == '.' &&
                       EqualNoCase(sColumn.substr(0, sPrefix.size()), sPrefix) &&
                       EqualNoCase(sName, sColumn.c_str() + sPrefix.size() + 1, sName.size());
            }

            template<class T>
            bool GetInteger(const CommonLib::CVariant& value, CSQLValue& result)
            {
                const T* pValue = value.GetPtr<T>();
                if(!pValue)
                    return false;
                result = CSQLValue::MakeInteger((int64_t)*pValue);
                return true;
            }

            template<class T>
            bool GetReal(const CommonLib::CVariant& value, CSQLValue& result)
            {
                const T* pValue = value.GetPtr<T>();
                if(!pValue)
                    return false;
                result = CSQLValue::MakeReal((double)*pValue);
                return true;
            }
        }

        // the values of the expression fields from one row
        class CSQLRowFilter::CRowValues : public SQLFilter::IFieldValues
        {
        public:
            CRowValues(const CSQLRowFilter& filter, IRow* pRow) : m_filter(filter), m_pRow(pRow) {}

            virtual CSQLValue GetFieldValue(size_t nField) const
            {
                return CSQLRowFilter::GetColumnValue(m_pRow, m_filter.FindColumn(m_pRow, nField));
            }

        private:
            const CSQLRowFilter& m_filter;
            IRow*                m_pRow;
        };

        CSQLRowFilter::CSQLRowFilter()
        {

        }

        CSQLRowFilter::CSQLRowFilter(const std::string& sExpr)
        {
            SetSQLExpression(sExpr);
        }

        CSQLRowFilter::~CSQLRowFilter()
        {

        }

        ISQLRowFilterPtr CSQLRowFilter::Create(const std::string& sExpr)
        {
            return std::make_shared<CSQLRowFilter>(sExpr);
        }

        bool CSQLRowFilter::EvaluateRow(IRowPtr ptrRow) const
        {
            if(!m_ptrExpr.get())
                return true;
            if(!ptrRow.get())
                return false;

            try
            {
                CRowValues values(*this, ptrRow.get());
                // NULL (unknown) doesn't pass, as in WHERE
                return m_ptrExpr->EvaluateTruth(values) == SQLFilter::eTruth::True;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("SQLRowFilter: failed to evaluate {0}", m_sExpr, exc);
                throw;
            }
        }

        const std::string& CSQLRowFilter::GetSQLExpression() const
        {
            return m_sExpr;
        }

        void CSQLRowFilter::SetSQLExpression(const std::string& sExpr)
        {
            try
            {
                SQLFilter::CSQLExprPtr ptrExpr;
                std::vector<SQLFilter::SFieldRef> vecFields;

                bool bEmpty = sExpr.find_first_not_of(" \t\r\n") == std::string::npos;
                if(!bEmpty)
                {
                    SQLFilter::CSQLExprDriver driver;
                    ptrExpr = driver.Parse(sExpr);
                    vecFields = driver.GetFields();
                }

                // the filter changes only if the expression is right
                m_sExpr = sExpr;
                m_ptrExpr = ptrExpr;
                m_vecFields.swap(vecFields);
                m_vecColumns.assign(m_vecFields.size(), -1);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("SQLRowFilter: wrong expression {0}", sExpr, exc);
                throw;
            }
        }

        int CSQLRowFilter::GetFieldCount() const
        {
            return (int)m_vecFields.size();
        }

        const std::string& CSQLRowFilter::GetFieldName(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecFields.size())
                throw CommonLib::CExcBase("SQLRowFilter: field index out of range: {0}", nIndex);
            return m_vecFields[nIndex].sName;
        }

        const std::string& CSQLRowFilter::GetFieldPrefix(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecFields.size())
                throw CommonLib::CExcBase("SQLRowFilter: field index out of range: {0}", nIndex);
            return m_vecFields[nIndex].sPrefix;
        }

        int32_t CSQLRowFilter::FindColumn(IRow* pRow, size_t nField) const
        {
            const SQLFilter::SFieldRef& field = m_vecFields[nField];
            int32_t nColumns = pRow->ColumnCount();

            // the cached column if the row has the same one there
            int32_t nCached = m_vecColumns[nField];
            if(nCached >= 0 && nCached < nColumns)
            {
                std::string sColumn = pRow->ColumnName(nCached);
                if(field.sPrefix.empty() ? EqualNoCase(sColumn, field.sName) :
                   (IsQualifiedName(sColumn, field.sPrefix, field.sName) || EqualNoCase(sColumn, field.sName)))
                    return nCached;
            }

            int32_t nFound = -1;
            if(!field.sPrefix.empty())
            {
                for(int32_t i = 0; i < nColumns && nFound < 0; ++i)
                    if(IsQualifiedName(pRow->ColumnName(i), field.sPrefix, field.sName))
                        nFound = i;
            }
            for(int32_t i = 0; i < nColumns && nFound < 0; ++i)
                if(EqualNoCase(pRow->ColumnName(i), field.sName))
                    nFound = i;

            if(nFound < 0)
                throw CommonLib::CExcBase("SQLRowFilter: field {0} not found in the row", field.GetFullName());

            m_vecColumns[nField] = nFound;
            return nFound;
        }

        CSQLValue CSQLRowFilter::GetColumnValue(IRow* pRow, int32_t nColumn)
        {
            if(pRow->ColumnIsNull(nColumn))
                return CSQLValue();

            switch(pRow->GetColumnType(nColumn))
            {
                case dtBlob:
                case dtGeometry:
                    return CSQLValue();   // not comparable: NULL
                default:
                    break;
            }

            CommonLib::CVariantPtr ptrValue = pRow->GetValue(nColumn);
            if(!ptrValue.get() || ptrValue->IsNull())
                return CSQLValue();

            const CommonLib::CVariant& value = *ptrValue;
            CSQLValue result;
            if(GetInteger<int64_t>(value, result) || GetInteger<int32_t>(value, result) ||
               GetInteger<int16_t>(value, result) || GetInteger<int8_t>(value, result) ||
               GetInteger<uint32_t>(value, result) || GetInteger<uint16_t>(value, result) ||
               GetInteger<uint8_t>(value, result) || GetInteger<bool>(value, result) ||
               GetReal<double>(value, result) || GetReal<float>(value, result))
                return result;

            if(const uint64_t* pValue = value.GetPtr<uint64_t>())
            {
                if(*pValue > (uint64_t)INT64_MAX)
                    return CSQLValue::MakeReal((double)*pValue);
                return CSQLValue::MakeInteger((int64_t)*pValue);
            }

            if(const CommonLib::astr_t* pText = value.GetPtr<CommonLib::astr_t>())
                return CSQLValue::MakeText(*pText);
            if(const CommonLib::wstr_t* pText = value.GetPtr<CommonLib::wstr_t>())
                return CSQLValue::MakeText(CommonLib::StringEncoding::str_w2utf8_safe(*pText));
            if(value.IsType<CommonLib::CGuid>())
            {
                CommonLib::CStringVisitor visitor;
                value.Accept(visitor);
                return CSQLValue::MakeText(visitor.GetString());
            }

            return CSQLValue();   // a shape, a blob
        }
    }
}
