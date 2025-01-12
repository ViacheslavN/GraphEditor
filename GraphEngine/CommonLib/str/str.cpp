#include "stdafx.h"
#include "str.h"

namespace CommonLib
{

template <class TChar>
long TStrtol(const TChar * str, TChar ** endptr, int base)
{

}

template<>
long TStrtol<wchar_t>(const wchar_t * str, wchar_t ** endptr, int base)
{
	return  wcstol(str, endptr, base);
}


template<>
long TStrtol<char>(const char * str, char ** endptr, int base)
{
	return  strtol(str, endptr, base);
}


template <class TChar, class TStr, class TContainer>
TStr StrFormatSafeT(const TStr& format, const TContainer& args, TChar startSymbol, TChar endSymbol)
{
	TStr result;
	for (size_t i = 0, sz = format.length(); i < sz;)
	{
		if (format[i] == startSymbol)
		{
			size_t pos = format.find(endSymbol, i);
			if (pos == TStr::npos)
			{
				result += format.c_str() + i;
				break;
			}

			size_t len = pos - i - 1;
			TStr subStr = format.substr(i + 1, len);

			TChar* pSubStrAfterNumber = 0;
			long  arg = TStrtol<TChar>(subStr.c_str(), &pSubStrAfterNumber, 10);
			if (subStr.c_str() == pSubStrAfterNumber)
			{
				arg = -1;
			}		 

			if (arg >= (long)args.size() || arg < 0)
			{
				result += startSymbol;
				result += subStr;
				result += endSymbol;
			}
			else
				result += args[arg];

			i = pos + 1;

		}
		else
		{
			result += format[i];
			i += 1;
		}

	}

	return result;
}

wstr str_format::StrFormatSafe(const wstr& str)
{
	return str;
}

wstr str_format::StrFormatSafe(const wstr& format, const wstrvec& args)
{
	return StrFormatSafeT(format, args, L'{', L'}');
}

wstr str_format::StrFormatSafe(const wstr& format, const wstr& arg1)
{
	wstrvec  args;
	args.push_back(arg1);

	return StrFormatSafe(format, args);
}

wstr str_format::StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2)
{
	wstrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);

	return StrFormatSafe(format, args);
}

wstr str_format::StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3)
{
	wstrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);

	return StrFormatSafe(format, args);
}

wstr str_format::StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3, const wstr& arg4)
{
	wstrvec args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);
	args.push_back(arg4);

	return StrFormatSafe(format, args);
}

wstr str_format::StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3, const wstr& arg4, const wstr& arg5)
{
	wstrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);
	args.push_back(arg4);
	args.push_back(arg5);

	return StrFormatSafe(format, args);
}

wstr str_format::StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3, const wstr& arg4, const wstr& arg5, const wstr& arg6)
{
	wstrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);
	args.push_back(arg4);
	args.push_back(arg5);
	args.push_back(arg6);

	return StrFormatSafe(format, args);
}
//////////////////////////////////////////////////////////////////////////////////////


astr str_format::StrFormatSafe(const astr& format, const astrvec& args)
{
	return StrFormatSafeT(format, args, '{', '}');
}

astr str_format::StrFormatSafe(const astr& str)
{
	return str;
}

astr str_format::StrFormatSafe(const astr& format, const astr& arg1)
{
	astrvec  args;
	args.push_back(arg1);

	return StrFormatSafe(format, args);
}
astr str_format::StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2)
{
	astrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);

	return StrFormatSafe(format, args);
}

astr str_format::StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3)
{
	astrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);

	return StrFormatSafe(format, args);
}

astr str_format::StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3, const astr& arg4)
{
	astrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);
	args.push_back(arg4);

	return StrFormatSafe(format, args);
}

astr str_format::StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3, const astr& arg4, const astr& arg5)
{
	astrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);
	args.push_back(arg4);
	args.push_back(arg5);

	return StrFormatSafe(format, args);
}


astr str_format::StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3, const astr& arg4, const astr& arg5, const astr& arg6)
{
	astrvec  args;
	args.push_back(arg1);
	args.push_back(arg2);
	args.push_back(arg3);
	args.push_back(arg4);
	args.push_back(arg5);
	args.push_back(arg6);

	return StrFormatSafe(format, args);
}


astr str_format::StrFormatAnySafe(const std::any& value)
{

    if (auto x = std::any_cast<bool>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<int8_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<uint8_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<int16_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<uint16_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<int32_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<uint32_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<int64_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<uint64_t>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<float>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<double>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<std::string>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    if (auto x = std::any_cast<const char*>(&value)) {
        return str_utils::AStrFrom(*x);
    }

    throw CExcBase("Converting any  to string failed, Unknown type any");
}


}