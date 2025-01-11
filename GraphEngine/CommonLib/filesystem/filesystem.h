#pragma once


#ifdef _WIN32
	#include "FileWin.h"
#elif  __linux__
	#include "FilePosix.h"
#endif


namespace CommonLib
{
	class CFileUtils
	{
	public:
		static bool IsFileExist(const wstr& path);
		static void DelFile(const wstr& path);
		static void FileDelFolder(const wstr& path);
		static void RenameFile(const wstr& oldFile, const wstr& newFile);

#ifdef _WIN32
		typedef std::function<bool(const wstr&)> TCheckFileObj;
		static void FileSearch(const wstr& searchMask,  const TCheckFileObj& addDir, const TCheckFileObj& addFile);
		static void FileSearch(const wstr& searchMask, wstrvec& folders, wstrvec& files);
#elif  __linux__
		typedef std::function<bool(const astr&)> TCheckFileObj;
		static void FileSearch(const astr& searchMask, const TCheckFileObj& addDir, const TCheckFileObj& addFile);
		static void FileSearch(const astr& searchMask, astrvec& folders, astrvec& files);
#endif

		static bool IsFileExist(const astr& path);
		static void DelFile(const astr& path);
		static void FileDelFolder(const astr& path);
		static void RenameFile(const astr& oldFile, const astr& newFile);
        static void CreateDirectory(const astr& path);
	};
}