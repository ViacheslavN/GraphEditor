#include "stdafx.h"
#include "filesystem.h"
#include "../exception/WinExc.h"
#include "../utils/ObjHolder.h"
#include "../str/StringEncoding.h"

namespace CommonLib
{


	class CFindFileHandle : public TObjHolder<HANDLE>
	{
		public:
			CFindFileHandle(HANDLE handle) :
				TObjHolder<HANDLE>(handle, [](HANDLE& h) {
				if (INVALID_HANDLE_VALUE != h)
				{
					FindClose(h);
					h = INVALID_HANDLE_VALUE;
				}
				})
			{

			}

	};


	bool CFileUtils::IsFileExist(const wstr& path)
	{
		DWORD dwAttr = ::GetFileAttributes(path.c_str());
		return ((dwAttr != INVALID_FILE_ATTRIBUTES) && !(dwAttr & FILE_ATTRIBUTE_DIRECTORY));
	}

	void CFileUtils::DeleteFile(const wstr& path)
	{
		if (::DeleteFile(path.c_str()) == FALSE)
			throw CWinExc("Failed to delete file {0}", StringEncoding::str_w2a_safe(path));

	}

	void CFileUtils::FileDelFolder(const wstr& path)
	{
		if (::RemoveDirectoryW(path.c_str()) == FALSE)
			throw CWinExc("Failed to delete folder {0}", StringEncoding::str_w2a_safe(path));
	}

	void CFileUtils::RenameFile(const wstr& oldFile, const wstr& newFile)
	{
		if(::MoveFileW(oldFile.c_str(), newFile.c_str()) == FALSE)
			throw CWinExc("Failed rename file {0}->{1}", StringEncoding::str_w2a_safe(oldFile), StringEncoding::str_w2a_safe(newFile));
	}



	void CFileUtils::FileSearch(const wstr& searchMask, const TCheckFileObj& addDir, const TCheckFileObj& addFile)
	{
		WIN32_FIND_DATAW  searchData = { 0 };
		HANDLE  hSearch = FindFirstFileW(searchMask.c_str(), &searchData);
		if (INVALID_HANDLE_VALUE == hSearch)
			return;

		CFindFileHandle  findHandle(hSearch);
		do
		{
			wstr nextChild = searchData.cFileName;
			if (_wcsicmp(nextChild.c_str(), L".") == 0)
				continue;

			if (_wcsicmp(nextChild.c_str(), L"..") == 0)
				continue;

			if ((searchData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				if (!addDir(nextChild))
					break;
			}
			else
			{
				if(!addFile(nextChild))
					break;
			}

		} while (TRUE == FindNextFile(findHandle.Get(), &searchData));

	}


	void CFileUtils::FileSearch(const wstr& searchMask, wstrvec& folders, wstrvec& files)
	{

		TCheckFileObj addDir = [&folders](const wstr& dir)-> bool
		{
			folders.push_back(dir);
			return true;
		};

		TCheckFileObj addFile = [&files](const wstr& file)-> bool
		{
			files.push_back(file);
			return true;
		};

		FileSearch(searchMask, addDir, addFile);
	}


	bool CFileUtils::IsFileExist(const astr& path)
	{
		return IsFileExist(StringEncoding::str_a2w_safe(path));
	}

	void CFileUtils::RenameFile(const astr& oldFile, const astr& newFile)
	{
		RenameFile(StringEncoding::str_a2w_safe(oldFile), StringEncoding::str_a2w_safe(newFile));
	}
}