#include "SQLValue.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace GraphEngine {
    namespace GeoDatabase {
        namespace SQLFilter {

            namespace
            {
                bool IsSpace(char c)
                {
                    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
                }

                bool IsDigit(char c)
                {
                    return c >= '0' && c <= '9';
                }

                // the number at the start of pText: [spaces][+-]digits[.digits][e[+-]digits]; nLen - its length (0 - not a number)
                CSQLValue ScanNumber(const char* pText, size_t& nLen)
                {
                    const char* p = pText;
                    while(IsSpace(*p))
                        ++p;

                    const char* pStart = p;
                    if(*p == '+' || *p == '-')
                        ++p;

                    bool bDigits = false;
                    bool bReal = false;
                    while(IsDigit(*p))
                    {
                        ++p;
                        bDigits = true;
                    }
                    if(*p == '.')
                    {
                        const char* pDot = p++;
                        bool bFraction = false;
                        while(IsDigit(*p))
                        {
                            ++p;
                            bFraction = true;
                        }
                        if(!bDigits && !bFraction)
                            p = pDot;
                        else
                            bReal = true;
                        bDigits = bDigits || bFraction;
                    }
                    if(!bDigits)
                    {
                        nLen = 0;
                        return CSQLValue();
                    }
                    if(*p == 'e' || *p == 'E')
                    {
                        const char* pExp = p++;
                        if(*p == '+' || *p == '-')
                            ++p;
                        if(IsDigit(*p))
                        {
                            while(IsDigit(*p))
                                ++p;
                            bReal = true;
                        }
                        else
                            p = pExp;
                    }

                    nLen = (size_t)(p - pText);
                    std::string sNumber(pStart, p);
                    if(!bReal)
                    {
                        errno = 0;
                        long long n = strtoll(sNumber.c_str(), nullptr, 10);
                        if(errno != ERANGE)
                            return CSQLValue::MakeInteger((int64_t)n);
                    }
                    return CSQLValue::MakeReal(strtod(sNumber.c_str(), nullptr));
                }

                int CompareNumbers(const CSQLValue& a, const CSQLValue& b)
                {
                    if(a.GetType() == CSQLValue::Integer && b.GetType() == CSQLValue::Integer)
                        return a.GetInteger() < b.GetInteger() ? -1 : (a.GetInteger() > b.GetInteger() ? 1 : 0);

                    double da = a.ToReal();
                    double db = b.ToReal();
                    return da < db ? -1 : (da > db ? 1 : 0);
                }
            }

            CSQLValue CSQLValue::MakeInteger(int64_t nValue)
            {
                CSQLValue value;
                value.m_type = Integer;
                value.m_nInteger = nValue;
                return value;
            }

            CSQLValue CSQLValue::MakeReal(double dValue)
            {
                if(std::isnan(dValue))
                    return CSQLValue();   // as SQLite: NaN is NULL

                CSQLValue value;
                value.m_type = Real;
                value.m_dReal = dValue;
                return value;
            }

            CSQLValue CSQLValue::MakeText(std::string sValue)
            {
                CSQLValue value;
                value.m_type = Text;
                value.m_sText = std::move(sValue);
                return value;
            }

            CSQLValue CSQLValue::FromTruth(eTruth truth)
            {
                if(truth == eTruth::Unknown)
                    return CSQLValue();
                return MakeBool(truth == eTruth::True);
            }

            CSQLValue CSQLValue::ToNumeric() const
            {
                switch(m_type)
                {
                    case Integer:
                    case Real:
                    case Null:
                        return *this;
                    case Text:
                    {
                        size_t nLen = 0;
                        CSQLValue value = ScanNumber(m_sText.c_str(), nLen);
                        return nLen == 0 ? MakeInteger(0) : value;
                    }
                }
                return CSQLValue();
            }

            double CSQLValue::ToReal() const
            {
                switch(m_type)
                {
                    case Integer: return (double)m_nInteger;
                    case Real:    return m_dReal;
                    case Text:    return ToNumeric().ToReal();
                    default:      return 0.;
                }
            }

            int64_t CSQLValue::ToInteger() const
            {
                switch(m_type)
                {
                    case Integer: return m_nInteger;
                    case Real:
                        if(m_dReal >= 9223372036854775807.0)
                            return INT64_MAX;
                        if(m_dReal <= -9223372036854775808.0)
                            return INT64_MIN;
                        return (int64_t)m_dReal;
                    case Text:    return ToNumeric().ToInteger();
                    default:      return 0;
                }
            }

            std::string CSQLValue::ToText() const
            {
                char szBuf[64];
                switch(m_type)
                {
                    case Integer:
                        snprintf(szBuf, sizeof(szBuf), "%lld", (long long)m_nInteger);
                        return szBuf;
                    case Real:
                    {
                        snprintf(szBuf, sizeof(szBuf), "%.15g", m_dReal);
                        std::string s = szBuf;
                        // 3.0 -> "3.0" as SQLite, not "3"
                        if(s.find_first_of(".eEni") == std::string::npos)
                            s += ".0";
                        return s;
                    }
                    case Text:
                        return m_sText;
                    default:
                        return std::string();
                }
            }

            eTruth CSQLValue::ToTruth() const
            {
                switch(m_type)
                {
                    case Integer: return m_nInteger != 0 ? eTruth::True : eTruth::False;
                    case Real:    return m_dReal != 0. ? eTruth::True : eTruth::False;
                    case Text:    return ToNumeric().ToTruth();
                    default:      return eTruth::Unknown;
                }
            }

            bool CSQLValue::ParseNumber(const std::string& sText, CSQLValue& value)
            {
                size_t nLen = 0;
                CSQLValue number = ScanNumber(sText.c_str(), nLen);
                if(nLen == 0)
                    return false;

                for(size_t i = nLen; i < sText.size(); ++i)
                    if(!IsSpace(sText[i]))
                        return false;

                value = number;
                return true;
            }

            bool CompareValues(const CSQLValue& a, const CSQLValue& b, int& nResult)
            {
                if(a.IsNull() || b.IsNull())
                    return false;

                if(a.IsNumeric() && b.IsNumeric())
                {
                    nResult = CompareNumbers(a, b);
                    return true;
                }

                if(a.GetType() == CSQLValue::Text && b.GetType() == CSQLValue::Text)
                {
                    int n = a.GetText().compare(b.GetText());
                    nResult = n < 0 ? -1 : (n > 0 ? 1 : 0);
                    return true;
                }

                // a number and a text: '10' = 10, otherwise numbers go before texts
                const CSQLValue& text = a.IsNumeric() ? b : a;
                CSQLValue number;
                if(CSQLValue::ParseNumber(text.GetText(), number))
                {
                    nResult = a.IsNumeric() ? CompareNumbers(a, number) : CompareNumbers(number, b);
                    return true;
                }

                nResult = a.IsNumeric() ? -1 : 1;
                return true;
            }

            eTruth TruthNot(eTruth a)
            {
                if(a == eTruth::Unknown)
                    return a;
                return a == eTruth::True ? eTruth::False : eTruth::True;
            }

            eTruth TruthAnd(eTruth a, eTruth b)
            {
                if(a == eTruth::False || b == eTruth::False)
                    return eTruth::False;
                if(a == eTruth::Unknown || b == eTruth::Unknown)
                    return eTruth::Unknown;
                return eTruth::True;
            }

            eTruth TruthOr(eTruth a, eTruth b)
            {
                if(a == eTruth::True || b == eTruth::True)
                    return eTruth::True;
                if(a == eTruth::Unknown || b == eTruth::Unknown)
                    return eTruth::Unknown;
                return eTruth::False;
            }
        }
    }
}
