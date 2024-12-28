#include "stdafx.h"
#include "PathUtils.h"
#include "../str/StrUtils.h"

namespace CommonLib
{

    template<typename TStr, typename  TChar>
    size_t FindLastPathDelimT(const TStr& path, TChar slash, TChar backSlash)
    {
        size_t slashPos = path.find_last_of(slash);
        size_t backSlashPos = path.find_last_of(backSlash);
        if ( TStr::npos == slashPos )
            return backSlashPos;

        if ( TStr::npos == backSlashPos )
            return slashPos;

        return  slashPos > backSlashPos ? slashPos : backSlashPos;
    }


    astr CPathUtils::GetPathDelim(const astr& path)
    {
#ifdef _WIN32
        if (astr::npos != path.find_last_of( '\\' ) || astr::npos == path.find_last_of('/'))
            return "\\";
#endif
        return "/";
    }

    wstr CPathUtils::GetPathDelim(const wstr& path)
    {
#ifdef _WIN32
        if (wstr::npos != path.find_last_of(L'\\') || wstr::npos == path.find_last_of(L'/'))
            return L"\\";
#endif
        return L"/";
    }

    size_t CPathUtils::FindLastPathDelim( const wstr& path )
    {
        return  FindLastPathDelimT<wstr, wchar_t>(path, L'/', L'\\');
    }

    size_t CPathUtils::FindLastPathDelim(const astr& path)
    {
        return  FindLastPathDelimT<astr, char>(path, '/', '\\');
    }


	astr CPathUtils::FindFileName(const astr& path)
	{
		if (!path.empty())
		{
			size_t nLen = path.length();
			for (size_t i = nLen - 1; i >= 0; --i)
			{
				if (path[i] == '\\' || path[i] == '/')
					return StringUtils::Right(path, i + 1);
			}
		}

		return path;
	}

	astr CPathUtils::FindOnlyFileName(const astr& path)
	{
		astr sFileName = FindFileName(path);
		if (!sFileName.empty())
		{
			const size_t nLen = sFileName.length();
			for (size_t i = nLen - 1; i >= 0; --i)
			{
				if (sFileName[i] == '.')
					return StringUtils::Left(sFileName, i); 
			}
		}
		return sFileName;
	}

	astr CPathUtils::FindFileExtension(const astr& path)
	{
		astr sFileName = FindFileName(path);
		if (sFileName.empty())
			return astr();

		const size_t nLen =  sFileName.length();
		for (size_t i = nLen - 1; i >= 0; --i)
		{
			if (sFileName[i] == '.')
				return StringUtils::Right(sFileName, i + 1);
		}

		return astr();
	}

	astr CPathUtils::FindFilePath(const astr& fullPath)
	{
		if (!fullPath.empty())
		{
			size_t nLen =  fullPath.length();
			for (size_t i = nLen - 1; i >= 0; --i)
			{
				if (fullPath[i] == '\\' || fullPath[i] == '/')
					return	StringUtils::Left(fullPath, i + 1); 
			}
		}

		return fullPath;
	}

	wstr CPathUtils::FindFileName(const wstr& path)
	{
		if (!path.empty())
		{
			size_t nLen =  path.length();
			for (size_t i = nLen - 1; i >= 0; --i)
			{
				if (path[i] == L'\\' || path[i] == L'/')
					return StringUtils::Right(path, i + 1);
			}
		}

		return path;
	}

	wstr CPathUtils::FindOnlyFileName(const wstr& path)
	{
		wstr sFileName = FindFileName(path);
		if (!sFileName.empty())
		{
			const size_t nLen = sFileName.length();
			for (size_t i = nLen - 1; i >= 0; --i)
			{
				if (sFileName[i] == L'.')
					return StringUtils::Left(sFileName, i);
			}
		}
		return sFileName;
	}

	wstr CPathUtils::FindFileExtension(const wstr& path)
	{
		wstr sFileName = FindFileName(path);
		if (sFileName.empty())
			return wstr();

		if (sFileName.empty())
			return sFileName;

		const size_t nLen =  sFileName.length();
		for (size_t i = nLen - 1; i >= 0; --i)
		{
			if (sFileName[i] == L'.')
				return StringUtils::Right(sFileName, i + 1);
		}

		return wstr();
	}

	wstr CPathUtils::FindFilePath(const wstr& fullPath)
	{
		if (!fullPath.empty())
		{
			const size_t nLen = fullPath.length();
			for (size_t i = nLen - 1; i >= 0; --i)
			{
				if (fullPath[i] == L'\\' || fullPath[i] == L'/')
					return	StringUtils::Left(fullPath, i + 1);
			}
		}

		return fullPath;
	}

    template<typename TStr>
    TStr FileMergePathT(const TStr& dir, const TStr& localPath)
    {

        if(dir.empty())
            return  localPath;

        size_t lastdelim_pos = CPathUtils::FindLastPathDelim(dir);
        if(TStr::npos != lastdelim_pos && lastdelim_pos + 1 == dir.length())
            return dir + localPath;

        return dir  + CPathUtils::GetPathDelim(dir) + localPath;
    }

    astr CPathUtils::FileMergePath(const astr& dir, const astr& path)
    {
        return FileMergePathT<astr>(dir, path);
    }

    wstr CPathUtils::FileMergePath(const wstr& dir, const wstr& path)
    {
        return FileMergePathT<wstr>(dir, path);
    }
}