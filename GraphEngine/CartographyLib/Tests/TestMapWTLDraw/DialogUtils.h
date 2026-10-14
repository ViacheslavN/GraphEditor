// DialogUtils.h : small helpers of the dialogs (text conversions, window text)
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <cwchar>
#include <cstdlib>
#include "../../../../CommonLib/str/StringEncoding.h"

namespace DialogUtils
{
	inline std::wstring Utf8ToWide(const std::string& str)
	{
		return CommonLib::StringEncoding::str_utf82w_safe(str);
	}

	inline std::string WideToUtf8(const std::wstring& str)
	{
		return CommonLib::StringEncoding::str_w2utf8_safe(str);
	}

	// shapelib and the CommonLib file API open files with the ANSI (*A) functions on Windows
	inline std::string WideToFilePath(const std::wstring& str)
	{
		return CommonLib::StringEncoding::str_w2a_safe(str);
	}

	inline std::wstring GetText(HWND hWnd)
	{
		int nLen = ::GetWindowTextLengthW(hWnd);
		std::wstring sText(nLen + 1, L'\0');
		::GetWindowTextW(hWnd, &sText[0], nLen + 1);
		sText.resize(nLen);
		return sText;
	}

	// text without spaces and quotes around (path pasted from Explorer "Copy as path")
	inline std::wstring Trim(const std::wstring& sText, const wchar_t* pszChars = L" \t\"")
	{
		size_t nStart = sText.find_first_not_of(pszChars);
		if(nStart == std::wstring::npos)
			return std::wstring();
		size_t nEnd = sText.find_last_not_of(pszChars);
		return sText.substr(nStart, nEnd - nStart + 1);
	}

	// wait cursor while the object lives
	class CWaitCursorGuard
	{
	public:
		CWaitCursorGuard() : m_hOld(::SetCursor(::LoadCursor(NULL, IDC_WAIT))) {}
		~CWaitCursorGuard() { ::SetCursor(m_hOld); }
	private:
		HCURSOR m_hOld;
	};

	inline std::wstring ExceptionText(const std::exception& exc)
	{
		return Utf8ToWide(exc.what());
	}

	// scale denominator from "1:50000" or "50000" (spaces allowed), 0 - empty (no limit), -1 - invalid
	inline double ParseScaleText(const std::wstring& sText)
	{
		std::wstring s = Trim(sText, L" \t");
		if(s.size() > 2 && s.compare(0, 2, L"1:") == 0)
			s = s.substr(2);
		std::wstring sDigits;
		for(size_t i = 0; i < s.size(); ++i)
			if(s[i] != L' ')
				sDigits += (s[i] == L',' ? L'.' : s[i]);
		if(sDigits.empty())
			return 0.;

		wchar_t* pszEnd = nullptr;
		double dScale = wcstod(sDigits.c_str(), &pszEnd);
		if(pszEnd == sDigits.c_str() || *pszEnd != 0 || dScale < 0.)
			return -1.;
		return dScale;
	}

	inline bool EndsWithNoCase(const std::wstring& sText, const wchar_t* pszSuffix)
	{
		size_t nLen = wcslen(pszSuffix);
		return sText.size() >= nLen && _wcsicmp(sText.c_str() + sText.size() - nLen, pszSuffix) == 0;
	}

	// a BPtreeDB database file (.btdb), else SQLite
	inline bool IsBPtreeDBPath(const std::wstring& sPath)
	{
		return EndsWithNoCase(sPath, L".btdb");
	}

	// the path with the extension of the database format: a database extension (.sqlite, .db, .btdb) is replaced,
	// another one is kept and the extension is added
	inline std::wstring WithDatabaseExtension(const std::wstring& sPath, bool bBPtreeDB)
	{
		if(sPath.empty())
			return sPath;

		std::wstring sResult = sPath;
		for(const wchar_t* pszExt : {L".sqlite", L".db", L".btdb"})
		{
			if(EndsWithNoCase(sResult, pszExt))
			{
				sResult.resize(sResult.size() - wcslen(pszExt));
				break;
			}
		}
		return sResult + (bBPtreeDB ? L".btdb" : L".sqlite");
	}

	// the filter of the file dialogs of a database format
	inline const wchar_t* DatabaseFileFilter(bool bBPtreeDB)
	{
		return bBPtreeDB ? L"BPtreeDB database (*.btdb)\0*.btdb\0All Files (*.*)\0*.*\0"
		                 : L"SQLite database (*.sqlite;*.db)\0*.sqlite;*.db\0All Files (*.*)\0*.*\0";
	}

	// "50000", empty for 0
	inline std::wstring FormatScale(double dScale)
	{
		if(dScale <= 0.)
			return std::wstring();
		wchar_t sz[64];
		swprintf(sz, 64, L"%.0f", dScale);
		return sz;
	}
}
