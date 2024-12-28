#pragma once

#include "CommonLib.h"

namespace CommonLib
{
	class CPathUtils
	{
	public:
        static astr GetPathDelim(const astr& path);
        static wstr GetPathDelim(const wstr& path);
        static size_t FindLastPathDelim( const wstr& path);
        static size_t FindLastPathDelim(const astr& path);



		static astr FindFileName(const astr& path);
		static astr FindOnlyFileName(const astr& path);
		static astr FindFileExtension(const astr& path);
		static astr FindFilePath(const astr& fullPath);
        static astr FileMergePath(const astr& dir, const astr& path);

		static wstr FindFileName(const wstr& path);
		static wstr FindOnlyFileName(const wstr& path);
		static wstr FindFileExtension(const wstr& path);
		static wstr FindFilePath(const wstr& fullPath);
        static wstr FileMergePath(const wstr& dir, const wstr& path);

	};
}