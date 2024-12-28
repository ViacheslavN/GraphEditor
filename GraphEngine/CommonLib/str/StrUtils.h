#pragma once

#include <vector>

namespace CommonLib
{
	class StringUtils
	{
	public:
		static std::string Left(const std::string& str, size_t pos);
		static std::string Right(const std::string& str, size_t pos);

		static std::wstring Left(const std::wstring& str, size_t pos);
		static std::wstring Right(const std::wstring& str, size_t pos);

        static bool ValidateAndFixUtf8String(char* data, size_t size);

        static std::string ToBase64(const std::string& str);
        static void FromBase64(const std::string& strInBase64, std::string & buffer);

        static std::vector<std::string> ArgsStringToArgsVector(const std::string& args);
	};
}