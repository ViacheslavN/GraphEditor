#pragma once
#include "convertToString.h"

namespace CommonLib
{

class str_format
{
public:

	static wstr StrFormatSafe(const wstr& format, const wstrvec& args);
		
	static wstr StrFormatSafe(const wstr& str);
	static wstr StrFormatSafe(const wstr& format, const wstr& arg1);
	static wstr StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2);
	static wstr StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3);
	static wstr StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3, const wstr& arg4);
	static wstr StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3, const wstr& arg4, const wstr& arg5);
	static wstr StrFormatSafe(const wstr& format, const wstr& arg1, const wstr& arg2, const wstr& arg3, const wstr& arg4, const wstr& arg5, const wstr& arg6);

	template<class TArg>
	static wstr WStrFormatSafeT(const wstr& format, const TArg& arg1)
	{
		return StrFormatSafe(format, str_utils::WStrFrom(arg1));
	}

	template<class TArg1, class TArg2>
	static wstr WStrFormatSafeT(const wstr& format, const TArg1& arg1, const TArg2& arg2)
	{
		return StrFormatSafe(format, str_utils::WStrFrom(arg1), str_utils::WStrFrom(arg2));
	}

	template<class TArg1, class TArg2, class TArg3>
	static wstr WStrFormatSafeT(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3)
	{
		return StrFormatSafe(format, str_utils::WStrFrom(arg1), str_utils::WStrFrom(arg2), str_utils::WStrFrom(arg3));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4>
	static wstr WStrFormatSafeT(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4)
	{
		return StrFormatSafe(format, str_utils::WStrFrom(arg1), str_utils::WStrFrom(arg2), str_utils::WStrFrom(arg3), str_utils::WStrFrom(arg4));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4, class TArg5>
	static wstr WStrFormatSafeT(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4, const TArg5& arg5)
	{
		return StrFormatSafe(format, str_utils::WStrFrom(arg1), str_utils::WStrFrom(arg2), str_utils::WStrFrom(arg3), str_utils::WStrFrom(arg4), str_utils::WStrFrom(arg5));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4, class TArg5, class TArg6>
	static wstr WStrFormatSafeT(const wstr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4, const TArg5& arg5, const TArg6& arg6)
	{
		return StrFormatSafe(format, str_utils::WStrFrom(arg1), str_utils::WStrFrom(arg2), str_utils::WStrFrom(arg3), str_utils::WStrFrom(arg4), str_utils::WStrFrom(arg5), str_utils::WStrFrom(arg6));
	}

	static astr StrFormatSafe(const astr& format, const astrvec& args);

	static astr StrFormatSafe(const astr& str);
	static astr StrFormatSafe(const astr& format, const astr& arg1);
	static astr StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2);
	static astr StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3);
	static astr StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3, const astr& arg4);
	static astr StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3, const astr& arg4, const astr& arg5);
	static astr StrFormatSafe(const astr& format, const astr& arg1, const astr& arg2, const astr& arg3, const astr& arg4, const astr& arg5, const astr& arg6);


	template<class TArg>
	static astr AStrFormatSafeT(const astr& format, const TArg& arg1)
	{
		return StrFormatSafe(format, str_utils::AStrFrom(arg1));
	}

	template<class TArg1, class TArg2>
	static astr AStrFormatSafeT(const astr& format, const TArg1& arg1, const TArg2& arg2)
	{
		return StrFormatSafe(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2));
	}

	template<class TArg1, class TArg2, class TArg3>
	static astr AStrFormatSafeT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3)
	{
		return StrFormatSafe(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4>
	static astr AStrFormatSafeT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4)
	{
		return StrFormatSafe(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), str_utils::AStrFrom(arg4));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4, class TArg5>
	static astr AStrFormatSafeT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4, const TArg5& arg5)
	{
		return StrFormatSafe(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), str_utils::AStrFrom(arg4), str_utils::AStrFrom(arg5));
	}

	template<class TArg1, class TArg2, class TArg3, class TArg4, class TArg5, class TArg6>
	static astr AStrFormatSafeT(const astr& format, const TArg1& arg1, const TArg2& arg2, const TArg3& arg3, const TArg4& arg4, const TArg5& arg5, const TArg6& arg6)
	{
		return StrFormatSafe(format, str_utils::AStrFrom(arg1), str_utils::AStrFrom(arg2), str_utils::AStrFrom(arg3), str_utils::AStrFrom(arg4), str_utils::AStrFrom(arg5), str_utils::AStrFrom(arg6));
	}

};
	
}