#include "SQLExpr.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace GraphEngine {
    namespace GeoDatabase {
        namespace SQLFilter {

            namespace
            {
                // ---- UTF-8: the functions count characters, not bytes (an invalid byte is a character itself)

                size_t Utf8CharLen(const std::string& s, size_t nPos)
                {
                    unsigned char c = (unsigned char)s[nPos];
                    size_t nLen = 1;
                    if(c >= 0xF0)      nLen = 4;
                    else if(c >= 0xE0) nLen = 3;
                    else if(c >= 0xC0) nLen = 2;

                    if(nPos + nLen > s.size())
                        return 1;
                    for(size_t i = 1; i < nLen; ++i)
                        if(((unsigned char)s[nPos + i] & 0xC0) != 0x80)
                            return 1;
                    return nLen;
                }

                // the byte offsets of the characters, plus the end
                std::vector<size_t> Utf8Offsets(const std::string& s)
                {
                    std::vector<size_t> vecOffsets;
                    vecOffsets.reserve(s.size() + 1);
                    for(size_t i = 0; i < s.size(); i += Utf8CharLen(s, i))
                        vecOffsets.push_back(i);
                    vecOffsets.push_back(s.size());
                    return vecOffsets;
                }

                std::vector<uint32_t> Utf8Decode(const std::string& s)
                {
                    std::vector<uint32_t> vecChars;
                    vecChars.reserve(s.size());
                    for(size_t i = 0; i < s.size();)
                    {
                        size_t nLen = Utf8CharLen(s, i);
                        uint32_t c = (unsigned char)s[i];
                        if(nLen == 2)      c &= 0x1F;
                        else if(nLen == 3) c &= 0x0F;
                        else if(nLen == 4) c &= 0x07;
                        for(size_t j = 1; j < nLen; ++j)
                            c = (c << 6) | ((unsigned char)s[i + j] & 0x3F);
                        vecChars.push_back(c);
                        i += nLen;
                    }
                    return vecChars;
                }

                size_t Utf8Length(const std::string& s)
                {
                    size_t n = 0;
                    for(size_t i = 0; i < s.size(); i += Utf8CharLen(s, i))
                        ++n;
                    return n;
                }

                uint32_t FoldAscii(uint32_t c)
                {
                    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
                }

                // ---- arithmetic (as SQLite: integer while it fits, then real; division by zero is NULL)

                const double c_dInt64Limit = 9223372036854775808.0;

                CSQLValue Arithmetic(CBinaryExpr::eOp op, const CSQLValue& left, const CSQLValue& right)
                {
                    if(left.IsNull() || right.IsNull())
                        return CSQLValue();

                    CSQLValue a = left.ToNumeric();
                    CSQLValue b = right.ToNumeric();

                    if(a.GetType() == CSQLValue::Integer && b.GetType() == CSQLValue::Integer)
                    {
                        int64_t x = a.GetInteger();
                        int64_t y = b.GetInteger();
                        switch(op)
                        {
                            case CBinaryExpr::Add:
                                if((y > 0 && x > INT64_MAX - y) || (y < 0 && x < INT64_MIN - y))
                                    return CSQLValue::MakeReal((double)x + (double)y);
                                return CSQLValue::MakeInteger(x + y);
                            case CBinaryExpr::Subtract:
                                if((y < 0 && x > INT64_MAX + y) || (y > 0 && x < INT64_MIN + y))
                                    return CSQLValue::MakeReal((double)x - (double)y);
                                return CSQLValue::MakeInteger(x - y);
                            case CBinaryExpr::Multiply:
                            {
                                double d = (double)x * (double)y;
                                if(std::fabs(d) >= c_dInt64Limit)
                                    return CSQLValue::MakeReal(d);
                                return CSQLValue::MakeInteger(x * y);
                            }
                            case CBinaryExpr::Divide:
                                if(y == 0)
                                    return CSQLValue();
                                if(x == INT64_MIN && y == -1)
                                    return CSQLValue::MakeReal(-(double)x);
                                return CSQLValue::MakeInteger(x / y);
                            case CBinaryExpr::Modulo:
                                if(y == 0)
                                    return CSQLValue();
                                if(y == -1)
                                    return CSQLValue::MakeInteger(0);
                                return CSQLValue::MakeInteger(x % y);
                            default:
                                break;
                        }
                        return CSQLValue();
                    }

                    if(op == CBinaryExpr::Modulo)
                    {
                        // as SQLite: the remainder of the integer parts
                        int64_t y = b.ToInteger();
                        if(y == 0)
                            return CSQLValue();
                        if(y == -1)
                            return CSQLValue::MakeReal(0.);
                        return CSQLValue::MakeReal((double)(a.ToInteger() % y));
                    }

                    double x = a.ToReal();
                    double y = b.ToReal();
                    switch(op)
                    {
                        case CBinaryExpr::Add:      return CSQLValue::MakeReal(x + y);
                        case CBinaryExpr::Subtract: return CSQLValue::MakeReal(x - y);
                        case CBinaryExpr::Multiply: return CSQLValue::MakeReal(x * y);
                        case CBinaryExpr::Divide:
                            if(y == 0.)
                                return CSQLValue();
                            return CSQLValue::MakeReal(x / y);
                        default:
                            break;
                    }
                    return CSQLValue();
                }

                eTruth Comparison(CBinaryExpr::eOp op, const CSQLValue& a, const CSQLValue& b)
                {
                    int n = 0;
                    if(!CompareValues(a, b, n))
                        return eTruth::Unknown;

                    bool bResult = false;
                    switch(op)
                    {
                        case CBinaryExpr::Equal:          bResult = n == 0; break;
                        case CBinaryExpr::NotEqual:       bResult = n != 0; break;
                        case CBinaryExpr::Less:           bResult = n < 0;  break;
                        case CBinaryExpr::LessOrEqual:    bResult = n <= 0; break;
                        case CBinaryExpr::Greater:        bResult = n > 0;  break;
                        case CBinaryExpr::GreaterOrEqual: bResult = n >= 0; break;
                        default: break;
                    }
                    return bResult ? eTruth::True : eTruth::False;
                }

                std::string TrimText(const std::string& s, const std::string& sChars, bool bLeft, bool bRight)
                {
                    std::vector<uint32_t> vecTrim = Utf8Decode(sChars);
                    std::vector<size_t> vecOffsets = Utf8Offsets(s);
                    std::vector<uint32_t> vecText = Utf8Decode(s);

                    auto isTrim = [&](uint32_t c) { return std::find(vecTrim.begin(), vecTrim.end(), c) != vecTrim.end(); };

                    size_t nBegin = 0;
                    size_t nEnd = vecText.size();
                    if(bLeft)
                        while(nBegin < nEnd && isTrim(vecText[nBegin]))
                            ++nBegin;
                    if(bRight)
                        while(nEnd > nBegin && isTrim(vecText[nEnd - 1]))
                            --nEnd;
                    return s.substr(vecOffsets[nBegin], vecOffsets[nEnd] - vecOffsets[nBegin]);
                }

                // SUBSTR of SQLite: 1-based start, a negative start counts from the end, a negative length takes the characters before
                std::string SubstrText(const std::string& s, int64_t p1, int64_t p2, bool bHasLength)
                {
                    std::vector<size_t> vecOffsets = Utf8Offsets(s);
                    int64_t nLen = (int64_t)vecOffsets.size() - 1;

                    if(!bHasLength)
                        p2 = nLen + 1;   // to the end
                    bool bNegLength = p2 < 0;
                    if(bNegLength)
                        p2 = -p2;

                    if(p1 < 0)
                    {
                        p1 += nLen;
                        if(p1 < 0)
                        {
                            p2 += p1;
                            if(p2 < 0)
                                p2 = 0;
                            p1 = 0;
                        }
                    }
                    else if(p1 > 0)
                        --p1;
                    else if(p2 > 0)
                        --p2;

                    if(bNegLength)
                    {
                        p1 -= p2;
                        if(p1 < 0)
                        {
                            p2 += p1;
                            p1 = 0;
                        }
                    }

                    if(p1 >= nLen || p2 <= 0)
                        return std::string();
                    int64_t nEnd = (std::min)(nLen, p1 + p2);
                    return s.substr(vecOffsets[(size_t)p1], vecOffsets[(size_t)nEnd] - vecOffsets[(size_t)p1]);
                }
            }

            // ---- CUnaryExpr

            CSQLValue CUnaryExpr::Evaluate(const IFieldValues& values) const
            {
                CSQLValue value = m_ptrExpr->Evaluate(values);
                switch(m_op)
                {
                    case Not:
                        return CSQLValue::FromTruth(TruthNot(value.ToTruth()));
                    case Plus:
                        return value;   // as SQLite: +x is x itself, a text stays a text
                    case Negate:
                    {
                        if(value.IsNull())
                            return value;
                        CSQLValue number = value.ToNumeric();
                        if(number.GetType() == CSQLValue::Integer)
                        {
                            if(number.GetInteger() == INT64_MIN)
                                return CSQLValue::MakeReal(-(double)number.GetInteger());
                            return CSQLValue::MakeInteger(-number.GetInteger());
                        }
                        return CSQLValue::MakeReal(-number.GetReal());
                    }
                }
                return CSQLValue();
            }

            // ---- CBinaryExpr

            CSQLValue CBinaryExpr::Evaluate(const IFieldValues& values) const
            {
                switch(m_op)
                {
                    case And:
                    {
                        eTruth left = m_ptrLeft->EvaluateTruth(values);
                        if(left == eTruth::False)
                            return CSQLValue::MakeBool(false);
                        return CSQLValue::FromTruth(TruthAnd(left, m_ptrRight->EvaluateTruth(values)));
                    }
                    case Or:
                    {
                        eTruth left = m_ptrLeft->EvaluateTruth(values);
                        if(left == eTruth::True)
                            return CSQLValue::MakeBool(true);
                        return CSQLValue::FromTruth(TruthOr(left, m_ptrRight->EvaluateTruth(values)));
                    }
                    default:
                        break;
                }

                CSQLValue left = m_ptrLeft->Evaluate(values);
                CSQLValue right = m_ptrRight->Evaluate(values);
                switch(m_op)
                {
                    case Add:
                    case Subtract:
                    case Multiply:
                    case Divide:
                    case Modulo:
                        return Arithmetic(m_op, left, right);
                    case Concat:
                        if(left.IsNull() || right.IsNull())
                            return CSQLValue();
                        return CSQLValue::MakeText(left.ToText() + right.ToText());
                    default:
                        return CSQLValue::FromTruth(Comparison(m_op, left, right));
                }
            }

            // ---- CIsNullExpr

            CSQLValue CIsNullExpr::Evaluate(const IFieldValues& values) const
            {
                bool bNull = m_ptrExpr->Evaluate(values).IsNull();
                return CSQLValue::MakeBool(m_bNot ? !bNull : bNull);
            }

            // ---- CLikeExpr

            bool CLikeExpr::Match(const std::string& sText, const std::string& sPattern, int nEscape)
            {
                std::vector<uint32_t> text = Utf8Decode(sText);
                std::vector<uint32_t> pattern = Utf8Decode(sPattern);

                // greedy with backtracking to the last %
                size_t t = 0, p = 0;
                size_t nStarP = std::string::npos, nStarT = 0;
                while(t < text.size())
                {
                    if(p < pattern.size())
                    {
                        uint32_t c = pattern[p];
                        if(nEscape >= 0 && c == (uint32_t)nEscape && p + 1 < pattern.size())
                        {
                            if(FoldAscii(pattern[p + 1]) == FoldAscii(text[t]))
                            {
                                p += 2;
                                ++t;
                                continue;
                            }
                        }
                        else if(c == '%')
                        {
                            nStarP = p++;
                            nStarT = t;
                            continue;
                        }
                        else if(c == '_' || FoldAscii(c) == FoldAscii(text[t]))
                        {
                            ++p;
                            ++t;
                            continue;
                        }
                    }

                    if(nStarP == std::string::npos)
                        return false;
                    p = nStarP + 1;
                    t = ++nStarT;
                }

                while(p < pattern.size() && pattern[p] == '%' && !(nEscape >= 0 && pattern[p] == (uint32_t)nEscape))
                    ++p;
                return p == pattern.size();
            }

            CSQLValue CLikeExpr::Evaluate(const IFieldValues& values) const
            {
                CSQLValue value = m_ptrExpr->Evaluate(values);
                CSQLValue pattern = m_ptrPattern->Evaluate(values);
                if(value.IsNull() || pattern.IsNull())
                    return CSQLValue();

                int nEscape = -1;
                if(m_ptrEscape.get())
                {
                    CSQLValue escape = m_ptrEscape->Evaluate(values);
                    if(escape.IsNull())
                        return CSQLValue();
                    std::vector<uint32_t> vecEscape = Utf8Decode(escape.ToText());
                    if(vecEscape.size() != 1)
                        throw std::runtime_error("ESCAPE expression must be a single character");
                    nEscape = (int)vecEscape[0];
                }

                bool bMatch = Match(value.ToText(), pattern.ToText(), nEscape);
                return CSQLValue::MakeBool(m_bNot ? !bMatch : bMatch);
            }

            // ---- CBetweenExpr

            CSQLValue CBetweenExpr::Evaluate(const IFieldValues& values) const
            {
                CSQLValue value = m_ptrExpr->Evaluate(values);
                eTruth truth = TruthAnd(Comparison(CBinaryExpr::GreaterOrEqual, value, m_ptrLow->Evaluate(values)),
                                        Comparison(CBinaryExpr::LessOrEqual, value, m_ptrHigh->Evaluate(values)));
                return CSQLValue::FromTruth(m_bNot ? TruthNot(truth) : truth);
            }

            // ---- CInExpr

            CSQLValue CInExpr::Evaluate(const IFieldValues& values) const
            {
                CSQLValue value = m_ptrExpr->Evaluate(values);
                if(value.IsNull())
                    return CSQLValue();

                eTruth truth = eTruth::False;
                for(size_t i = 0; i < m_list.size() && truth != eTruth::True; ++i)
                    truth = TruthOr(truth, Comparison(CBinaryExpr::Equal, value, m_list[i]->Evaluate(values)));

                return CSQLValue::FromTruth(m_bNot ? TruthNot(truth) : truth);
            }

            // ---- CCaseExpr

            CSQLValue CCaseExpr::Evaluate(const IFieldValues& values) const
            {
                if(m_ptrOperand.get())
                {
                    CSQLValue operand = m_ptrOperand->Evaluate(values);
                    for(size_t i = 0; i < m_vecWhens.size(); ++i)
                        if(Comparison(CBinaryExpr::Equal, operand, m_vecWhens[i].ptrWhen->Evaluate(values)) == eTruth::True)
                            return m_vecWhens[i].ptrThen->Evaluate(values);
                }
                else
                {
                    for(size_t i = 0; i < m_vecWhens.size(); ++i)
                        if(m_vecWhens[i].ptrWhen->EvaluateTruth(values) == eTruth::True)
                            return m_vecWhens[i].ptrThen->Evaluate(values);
                }

                return m_ptrElse.get() ? m_ptrElse->Evaluate(values) : CSQLValue();
            }

            // ---- CCastExpr

            CSQLValue CCastExpr::Evaluate(const IFieldValues& values) const
            {
                CSQLValue value = m_ptrExpr->Evaluate(values);
                if(value.IsNull())
                    return value;

                switch(m_type)
                {
                    case CSQLValue::Integer: return CSQLValue::MakeInteger(value.ToInteger());
                    case CSQLValue::Real:    return CSQLValue::MakeReal(value.ToReal());
                    case CSQLValue::Text:    return CSQLValue::MakeText(value.ToText());
                    default:                 return CSQLValue();
                }
            }

            // ---- CFunctionExpr

            bool CFunctionExpr::Resolve(const std::string& sName, size_t nArgs, eFunction& func, std::string& sError)
            {
                struct SFunc
                {
                    const char* pszName;
                    eFunction   func;
                    size_t      nMin;
                    size_t      nMax;
                };
                static const SFunc c_funcs[] = {
                        {"UPPER",    Upper,    1, 1},
                        {"LOWER",    Lower,    1, 1},
                        {"LENGTH",   Length,   1, 1},
                        {"ABS",      Abs,      1, 1},
                        {"ROUND",    Round,    1, 2},
                        {"SUBSTR",   Substr,   2, 3},
                        {"SUBSTRING",Substr,   2, 3},
                        {"TRIM",     Trim,     1, 2},
                        {"LTRIM",    LTrim,    1, 2},
                        {"RTRIM",    RTrim,    1, 2},
                        {"COALESCE", Coalesce, 2, SIZE_MAX},
                        {"IFNULL",   IfNull,   2, 2},
                        {"NULLIF",   NullIf,   2, 2},
                        {"REPLACE",  Replace,  3, 3},
                        {"INSTR",    Instr,    2, 2},
                        {"MIN",      Min,      2, SIZE_MAX},
                        {"MAX",      Max,      2, SIZE_MAX},
                };

                std::string sUpper = sName;
                for(char& c : sUpper)
                    if(c >= 'a' && c <= 'z')
                        c = (char)(c - 'a' + 'A');

                for(const SFunc& f : c_funcs)
                {
                    if(sUpper != f.pszName)
                        continue;

                    func = f.func;
                    if(nArgs < f.nMin || nArgs > f.nMax)
                    {
                        sError = "wrong number of arguments to function " + sUpper + "()";
                        return true;
                    }
                    sError.clear();
                    return true;
                }
                return false;
            }

            CSQLValue CFunctionExpr::Evaluate(const IFieldValues& values) const
            {
                switch(m_func)
                {
                    case Coalesce:
                    case IfNull:
                        for(size_t i = 0; i < m_args.size(); ++i)
                        {
                            CSQLValue value = m_args[i]->Evaluate(values);
                            if(!value.IsNull())
                                return value;
                        }
                        return CSQLValue();
                    default:
                        break;
                }

                std::vector<CSQLValue> args;
                args.reserve(m_args.size());
                for(size_t i = 0; i < m_args.size(); ++i)
                    args.push_back(m_args[i]->Evaluate(values));

                if(m_func == NullIf)
                {
                    int n = 0;
                    if(CompareValues(args[0], args[1], n) && n == 0)
                        return CSQLValue();
                    return args[0];
                }

                // the rest: NULL in an argument gives NULL
                for(size_t i = 0; i < args.size(); ++i)
                    if(args[i].IsNull())
                        return CSQLValue();

                switch(m_func)
                {
                    case Upper:
                    case Lower:
                    {
                        std::string s = args[0].ToText();
                        for(char& c : s)
                        {
                            if(m_func == Upper && c >= 'a' && c <= 'z')
                                c = (char)(c - 'a' + 'A');
                            else if(m_func == Lower && c >= 'A' && c <= 'Z')
                                c = (char)(c - 'A' + 'a');
                        }
                        return CSQLValue::MakeText(s);
                    }
                    case Length:
                        return CSQLValue::MakeInteger((int64_t)Utf8Length(args[0].ToText()));
                    case Abs:
                    {
                        CSQLValue number = args[0].ToNumeric();
                        if(number.GetType() == CSQLValue::Integer)
                        {
                            if(number.GetInteger() == INT64_MIN)
                                return CSQLValue::MakeReal(c_dInt64Limit);
                            return CSQLValue::MakeInteger(number.GetInteger() < 0 ? -number.GetInteger() : number.GetInteger());
                        }
                        return CSQLValue::MakeReal(std::fabs(number.GetReal()));
                    }
                    case Round:
                    {
                        int64_t nDigits = args.size() > 1 ? args[1].ToInteger() : 0;
                        nDigits = (std::max)((int64_t)0, (std::min)((int64_t)15, nDigits));
                        double dScale = std::pow(10., (double)nDigits);
                        double d = args[0].ToReal();
                        return CSQLValue::MakeReal(std::round(d * dScale) / dScale);
                    }
                    case Substr:
                        return CSQLValue::MakeText(SubstrText(args[0].ToText(), args[1].ToInteger(),
                                                              args.size() > 2 ? args[2].ToInteger() : 0, args.size() > 2));
                    case Trim:
                    case LTrim:
                    case RTrim:
                    {
                        std::string sChars = args.size() > 1 ? args[1].ToText() : std::string(" ");
                        return CSQLValue::MakeText(TrimText(args[0].ToText(), sChars, m_func != RTrim, m_func != LTrim));
                    }
                    case Replace:
                    {
                        std::string s = args[0].ToText();
                        std::string sFrom = args[1].ToText();
                        std::string sTo = args[2].ToText();
                        if(sFrom.empty())
                            return CSQLValue::MakeText(s);

                        std::string sResult;
                        size_t nPos = 0;
                        for(size_t nFound; (nFound = s.find(sFrom, nPos)) != std::string::npos; nPos = nFound + sFrom.size())
                            sResult += s.substr(nPos, nFound - nPos) + sTo;
                        sResult += s.substr(nPos);
                        return CSQLValue::MakeText(sResult);
                    }
                    case Instr:
                    {
                        std::string s = args[0].ToText();
                        size_t nFound = s.find(args[1].ToText());
                        if(nFound == std::string::npos)
                            return CSQLValue::MakeInteger(0);
                        return CSQLValue::MakeInteger((int64_t)Utf8Length(s.substr(0, nFound)) + 1);
                    }
                    case Min:
                    case Max:
                    {
                        size_t nBest = 0;
                        for(size_t i = 1; i < args.size(); ++i)
                        {
                            int n = 0;
                            CompareValues(args[i], args[nBest], n);
                            if((m_func == Min && n < 0) || (m_func == Max && n > 0))
                                nBest = i;
                        }
                        return args[nBest];
                    }
                    default:
                        break;
                }
                return CSQLValue();
            }
        }
    }
}
