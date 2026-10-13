#include "SQLValue.h"
#include "../Values.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>
#include <functional>

namespace bptreedb
{
	namespace sql
	{
		namespace
		{
			const double TwoPow63 = 9223372036854775808.0;

			// integer and double: exact order (NaN is the smallest number)
			int CompareIntDouble(int64_t n, double d)
			{
				if (std::isnan(d))
					return 1;
				if (d >= TwoPow63)
					return -1;
				if (d < -TwoPow63)
					return 1;
				int64_t nTrunc = (int64_t)d;
				if (n < nTrunc)
					return -1;
				if (n > nTrunc)
					return 1;
				double dFrac = d - (double)nTrunc;
				if (dFrac > 0)
					return -1;
				if (dFrac < 0)
					return 1;
				return 0;
			}

			int CompareDoubles(double a, double b)
			{
				bool bNanA = std::isnan(a), bNanB = std::isnan(b);
				if (bNanA || bNanB)
					return bNanA == bNanB ? 0 : (bNanA ? -1 : 1);
				return a < b ? -1 : (a > b ? 1 : 0);
			}

			int CompareBytes(const byte_t* pA, size_t nA, const byte_t* pB, size_t nB)
			{
				int nRes = std::memcmp(pA, pB, (std::min)(nA, nB));
				if (nRes != 0)
					return nRes < 0 ? -1 : 1;
				return nA < nB ? -1 : (nA > nB ? 1 : 0);
			}

			int CompareNumbers(const TValue& left, const TValue& right)
			{
				const int64_t* pL = left.GetPtr<int64_t>();
				const int64_t* pR = right.GetPtr<int64_t>();
				if (pL && pR)
					return *pL < *pR ? -1 : (*pL > *pR ? 1 : 0);
				if (pL)
					return CompareIntDouble(*pL, right.Get<double>());
				if (pR)
					return -CompareIntDouble(*pR, left.Get<double>());
				return CompareDoubles(left.Get<double>(), right.Get<double>());
			}

			const char* KindName(eValueKind kind)
			{
				switch (kind)
				{
				case eValueKind::Null: return "NULL";
				case eValueKind::Integer: return "integer";
				case eValueKind::Double: return "double";
				case eValueKind::String: return "string";
				case eValueKind::Blob: return "blob";
				case eValueKind::Geometry: return "geometry";
				default: return "value";
				}
			}

			bool AddOverflow(int64_t a, int64_t b, int64_t& r)
			{
				if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
					return true;
				r = a + b;
				return false;
			}

			bool SubOverflow(int64_t a, int64_t b, int64_t& r)
			{
				if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b))
					return true;
				r = a - b;
				return false;
			}

			bool MulOverflow(int64_t a, int64_t b, int64_t& r)
			{
				if (a == 0 || b == 0)
				{
					r = 0;
					return false;
				}
				if ((a == -1 && b == INT64_MIN) || (b == -1 && a == INT64_MIN))
					return true;
				int64_t p = (int64_t)((uint64_t)a * (uint64_t)b);
				if (p / b != a)
					return true;
				r = p;
				return false;
			}

			// a string operand of arithmetic as a number
			TValue ArithOperand(const TValue& value)
			{
				eValueKind kind = ValueKind(value);
				if (IsNumberKind(kind))
					return value;
				if (kind == eValueKind::String)
				{
					TValue number;
					if (ParseNumber(value.Get<CommonLib::astr_t>(), number))
						return number;
					throw CommonLib::CExcBase("'{0}' isn't a number", value.Get<CommonLib::astr_t>());
				}
				throw CommonLib::CExcBase("a {0} in an arithmetic expression", KindName(kind));
			}

			// ASCII case folding of LIKE
			inline unsigned char Fold(unsigned char c)
			{
				return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
			}

			// the length of the UTF-8 character at p
			inline size_t CharLen(const std::string& s, size_t p)
			{
				size_t n = 1;
				while (p + n < s.size() && ((unsigned char)s[p + n] & 0xC0) == 0x80)
					++n;
				return n;
			}

			bool LikeFrom(const std::string& sText, size_t t, const std::string& sPattern, size_t p, int nEscape)
			{
				while (p < sPattern.size())
				{
					unsigned char c = (unsigned char)sPattern[p];
					if (nEscape >= 0 && c == (unsigned char)nEscape)
					{
						if (p + 1 >= sPattern.size())
							throw CommonLib::CExcBase("LIKE: the pattern ends with the escape character");
						size_t nLen = CharLen(sPattern, p + 1);
						if (t + nLen > sText.size() || sText.compare(t, nLen, sPattern, p + 1, nLen) != 0)
						{
							if (!(nLen == 1 && t < sText.size() && Fold((unsigned char)sText[t]) == Fold((unsigned char)sPattern[p + 1])))
								return false;
						}
						t += nLen;
						p += 1 + nLen;
						continue;
					}
					if (c == '%')
					{
						while (p < sPattern.size() && sPattern[p] == '%')
							++p;
						if (p == sPattern.size())
							return true;
						for (size_t i = t; i <= sText.size(); i += (i < sText.size() ? CharLen(sText, i) : 1))
						{
							if (LikeFrom(sText, i, sPattern, p, nEscape))
								return true;
							if (i == sText.size())
								break;
						}
						return false;
					}
					if (t >= sText.size())
						return false;
					if (c == '_')
					{
						t += CharLen(sText, t);
						++p;
						continue;
					}
					if (Fold((unsigned char)sText[t]) != Fold(c))
						return false;
					++t;
					++p;
				}
				return t == sText.size();
			}
		}

		eValueKind ValueKind(const TValue& value)
		{
			if (value.IsNull())
				return eValueKind::Null;
			if (value.IsType<int64_t>())
				return eValueKind::Integer;
			if (value.IsType<double>())
				return eValueKind::Double;
			if (value.IsType<CommonLib::astr_t>())
				return eValueKind::String;
			if (value.IsType<CommonLib::CBlobPtr_t>())
				return eValueKind::Blob;
			if (value.IsType<CommonLib::IGeoShapePtr>())
				return eValueKind::Geometry;
			return eValueKind::Other;
		}

		TValue NormalizeValue(const TValue& value)
		{
			TValue result(value);
			NormalizeInPlace(result);
			return result;
		}

		void NormalizeInPlace(TValue& value)
		{
			if (value.IsNull() || value.IsType<int64_t>() || value.IsType<double>() || value.IsType<CommonLib::astr_t>())
				return;
			if (value.IsType<int32_t>()) value = (int64_t)value.Get<int32_t>();
			else if (value.IsType<int16_t>()) value = (int64_t)value.Get<int16_t>();
			else if (value.IsType<int8_t>()) value = (int64_t)value.Get<int8_t>();
			else if (value.IsType<uint32_t>()) value = (int64_t)value.Get<uint32_t>();
			else if (value.IsType<uint16_t>()) value = (int64_t)value.Get<uint16_t>();
			else if (value.IsType<uint8_t>()) value = (int64_t)value.Get<uint8_t>();
			else if (value.IsType<uint64_t>())
			{
				uint64_t n = value.Get<uint64_t>();
				if (n <= (uint64_t)INT64_MAX)
					value = (int64_t)n;
				else
					value = (double)n;
			}
			else if (value.IsType<float>()) value = (double)value.Get<float>();
			else if (value.IsType<bool>()) value = (int64_t)(value.Get<bool>() ? 1 : 0);
			else if (value.IsType<CommonLib::wstr_t>())
				value = CommonLib::StringEncoding::str_w2utf8_safe(value.Get<CommonLib::wstr_t>());
		}

		eDataTypes ValueType(const TValue& value)
		{
			switch (ValueKind(value))
			{
			case eValueKind::Integer: return dtInteger64;
			case eValueKind::Double: return dtDouble;
			case eValueKind::String: return dtString;
			case eValueKind::Blob: return dtBlob;
			case eValueKind::Geometry: return dtGeometry;
			default: return dtUnknown;
			}
		}

		bool IsIntegerType(eDataTypes type)
		{
			switch (type)
			{
			case dtInteger8: case dtInteger16: case dtInteger32: case dtInteger64:
			case dtUInteger8: case dtUInteger16: case dtUInteger32: case dtUInteger64: case dtOid:
				return true;
			default:
				return false;
			}
		}

		bool ParseNumber(const std::string& s, TValue& value)
		{
			size_t nBegin = 0, nEnd = s.size();
			while (nBegin < nEnd && std::isspace((unsigned char)s[nBegin]))
				++nBegin;
			while (nEnd > nBegin && std::isspace((unsigned char)s[nEnd - 1]))
				--nEnd;
			if (nBegin == nEnd)
				return false;
			std::string sNum = s.substr(nBegin, nEnd - nBegin);

			bool bInteger = sNum.find_first_of(".eE") == std::string::npos;
			if (bInteger)
			{
				errno = 0;
				char* pEnd = nullptr;
				long long n = std::strtoll(sNum.c_str(), &pEnd, 10);
				if (*pEnd == 0 && errno != ERANGE && pEnd != sNum.c_str())
				{
					value = (int64_t)n;
					return true;
				}
			}
			char* pEnd = nullptr;
			double d = std::strtod(sNum.c_str(), &pEnd);
			if (pEnd == sNum.c_str() || *pEnd != 0)
				return false;
			// inf / nan as words aren't numbers of SQL
			if (std::isalpha((unsigned char)sNum[0]) || ((sNum[0] == '-' || sNum[0] == '+') && sNum.size() > 1 && std::isalpha((unsigned char)sNum[1])))
				return false;
			value = d;
			return true;
		}

		int64_t ToInt64(const TValue& value)
		{
			TValue number = ArithOperand(value);
			if (number.IsType<int64_t>())
				return number.Get<int64_t>();
			double d = number.Get<double>();
			if (std::isnan(d) || d >= TwoPow63 || d < -TwoPow63)
				throw CommonLib::CExcBase("{0} is out of the integer range", d);
			return (int64_t)d;
		}

		double ToDouble(const TValue& value)
		{
			TValue number = ArithOperand(value);
			if (number.IsType<int64_t>())
				return (double)number.Get<int64_t>();
			return number.Get<double>();
		}

		std::string ToText(const TValue& value)
		{
			switch (ValueKind(value))
			{
			case eValueKind::String:
				return value.Get<CommonLib::astr_t>();
			case eValueKind::Integer:
				return std::to_string(value.Get<int64_t>());
			case eValueKind::Double:
				{
					double d = value.Get<double>();
					char buf[64];
					std::snprintf(buf, sizeof(buf), "%.15g", d);
					if (std::strtod(buf, nullptr) != d)
						std::snprintf(buf, sizeof(buf), "%.17g", d);
					return buf;
				}
			case eValueKind::Null:
				throw CommonLib::CExcBase("NULL as text");
			default:
				throw CommonLib::CExcBase("a {0} can't be text", KindName(ValueKind(value)));
			}
		}

		int CompareValues(const TValue& left, const TValue& right)
		{
			eValueKind kindL = ValueKind(left), kindR = ValueKind(right);
			if (IsNumberKind(kindL) && IsNumberKind(kindR))
				return CompareNumbers(left, right);
			if (kindL == eValueKind::String && kindR == eValueKind::String)
			{
				const std::string& a = left.Get<CommonLib::astr_t>();
				const std::string& b = right.Get<CommonLib::astr_t>();
				int nRes = a.compare(b);
				return nRes < 0 ? -1 : (nRes > 0 ? 1 : 0);
			}
			if (IsNumberKind(kindL) && kindR == eValueKind::String)
			{
				TValue number;
				if (ParseNumber(right.Get<CommonLib::astr_t>(), number))
					return CompareNumbers(left, number);
				return -1;
			}
			if (kindL == eValueKind::String && IsNumberKind(kindR))
				return -CompareValues(right, left);
			if (kindL == eValueKind::Blob && kindR == eValueKind::Blob)
			{
				const CommonLib::CBlobPtr_t& a = left.Get<CommonLib::CBlobPtr_t>();
				const CommonLib::CBlobPtr_t& b = right.Get<CommonLib::CBlobPtr_t>();
				return CompareBytes(a->Buffer(), a->Size(), b->Buffer(), b->Size());
			}
			throw CommonLib::CExcBase("a {0} and a {1} can't be compared", KindName(kindL), KindName(kindR));
		}

		bool GroupEqual(const TValue& left, const TValue& right)
		{
			eValueKind kindL = ValueKind(left), kindR = ValueKind(right);
			if (kindL == eValueKind::Null || kindR == eValueKind::Null)
				return kindL == kindR;
			if (IsNumberKind(kindL) && IsNumberKind(kindR))
				return CompareNumbers(left, right) == 0;
			if (kindL != kindR)
				return false;
			switch (kindL)
			{
			case eValueKind::String:
				return left.Get<CommonLib::astr_t>() == right.Get<CommonLib::astr_t>();
			case eValueKind::Blob:
				return CompareValues(left, right) == 0;
			case eValueKind::Geometry:
				{
					const CommonLib::IGeoShapePtr& a = left.Get<CommonLib::IGeoShapePtr>();
					const CommonLib::IGeoShapePtr& b = right.Get<CommonLib::IGeoShapePtr>();
					if (a.get() == b.get())
						return true;
					if (a.get() == nullptr || b.get() == nullptr)
						return false;
					return CompareBytes(a->Data(), a->Size(), b->Data(), b->Size()) == 0;
				}
			default:
				return false;
			}
		}

		size_t HashValue(const TValue& value)
		{
			switch (ValueKind(value))
			{
			case eValueKind::Null:
				return 0x9e3779b9;
			case eValueKind::Integer:
				return std::hash<int64_t>()(value.Get<int64_t>());
			case eValueKind::Double:
				{
					// an integer double hashes as the integer: 2 and 2.0 are one group
					double d = value.Get<double>();
					if (std::isnan(d))
						return 0x7ff8;
					if (d >= -TwoPow63 && d < TwoPow63 && d == std::floor(d))
						return std::hash<int64_t>()((int64_t)d);
					return std::hash<double>()(d);
				}
			case eValueKind::String:
				return std::hash<std::string>()(value.Get<CommonLib::astr_t>());
			case eValueKind::Blob:
				{
					const CommonLib::CBlobPtr_t& ptrBlob = value.Get<CommonLib::CBlobPtr_t>();
					return std::hash<std::string>()(std::string((const char*)ptrBlob->Buffer(), ptrBlob->Size()));
				}
			case eValueKind::Geometry:
				{
					const CommonLib::IGeoShapePtr& ptrShape = value.Get<CommonLib::IGeoShapePtr>();
					if (ptrShape.get() == nullptr)
						return 1;
					return std::hash<std::string>()(std::string((const char*)ptrShape->Data(), ptrShape->Size()));
				}
			default:
				return 2;
			}
		}

		size_t SRowHash::operator()(const TRow& row) const
		{
			size_t nHash = row.size();
			for (const TValue& value : row)
				nHash ^= HashValue(value) + 0x9e3779b97f4a7c15ULL + (nHash << 6) + (nHash >> 2);
			return nHash;
		}

		bool SRowEqual::operator()(const TRow& left, const TRow& right) const
		{
			if (left.size() != right.size())
				return false;
			for (size_t i = 0; i < left.size(); ++i)
				if (!GroupEqual(left[i], right[i]))
					return false;
			return true;
		}

		int Truth(const TValue& value)
		{
			switch (ValueKind(value))
			{
			case eValueKind::Null:
				return -1;
			case eValueKind::Integer:
				return value.Get<int64_t>() != 0 ? 1 : 0;
			case eValueKind::Double:
				return value.Get<double>() != 0 ? 1 : 0;
			case eValueKind::String:
				{
					TValue number;
					if (ParseNumber(value.Get<CommonLib::astr_t>(), number))
						return Truth(number);
					return 0;
				}
			default:
				throw CommonLib::CExcBase("a {0} isn't a condition", KindName(ValueKind(value)));
			}
		}

		TValue MakeTruth(int nTruth)
		{
			if (nTruth < 0)
				return TValue();
			return TValue((int64_t)nTruth);
		}

		TValue Arithmetic(eArithOp op, const TValue& left, const TValue& right)
		{
			if (left.IsNull() || right.IsNull())
				return TValue();
			TValue a = ArithOperand(left), b = ArithOperand(right);
			if (a.IsType<int64_t>() && b.IsType<int64_t>())
			{
				int64_t x = a.Get<int64_t>(), y = b.Get<int64_t>(), r = 0;
				switch (op)
				{
				case eArithOp::Add:
					if (!AddOverflow(x, y, r))
						return TValue(r);
					return TValue((double)x + (double)y);
				case eArithOp::Subtract:
					if (!SubOverflow(x, y, r))
						return TValue(r);
					return TValue((double)x - (double)y);
				case eArithOp::Multiply:
					if (!MulOverflow(x, y, r))
						return TValue(r);
					return TValue((double)x * (double)y);
				case eArithOp::Divide:
					if (y == 0)
						return TValue();
					if (x == INT64_MIN && y == -1)
						return TValue(-(double)x);
					return TValue(x / y);
				case eArithOp::Modulo:
					if (y == 0)
						return TValue();
					if (y == -1)
						return TValue((int64_t)0);
					return TValue(x % y);
				}
			}

			double x = ToDouble(a), y = ToDouble(b);
			switch (op)
			{
			case eArithOp::Add: return TValue(x + y);
			case eArithOp::Subtract: return TValue(x - y);
			case eArithOp::Multiply: return TValue(x * y);
			case eArithOp::Divide:
				if (y == 0)
					return TValue();
				return TValue(x / y);
			case eArithOp::Modulo:
				if (y == 0)
					return TValue();
				return TValue(std::fmod(x, y));
			}
			return TValue();
		}

		TValue Negate(const TValue& value)
		{
			if (value.IsNull())
				return TValue();
			TValue a = ArithOperand(value);
			if (a.IsType<int64_t>())
			{
				int64_t n = a.Get<int64_t>();
				if (n == INT64_MIN)
					return TValue(-(double)n);
				return TValue(-n);
			}
			return TValue(-a.Get<double>());
		}

		bool LikeMatch(const std::string& sText, const std::string& sPattern, int nEscape)
		{
			return LikeFrom(sText, 0, sPattern, 0, nEscape);
		}

		std::string ValueToSQL(const TValue& value)
		{
			switch (ValueKind(value))
			{
			case eValueKind::Null: return "NULL";
			case eValueKind::String: return "'" + value.Get<CommonLib::astr_t>() + "'";
			case eValueKind::Integer:
			case eValueKind::Double: return ToText(value);
			default: return std::string("<") + KindName(ValueKind(value)) + ">";
			}
		}
	}
}
