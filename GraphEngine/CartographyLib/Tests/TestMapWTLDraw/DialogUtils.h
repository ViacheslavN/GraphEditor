// DialogUtils.h : small helpers of the dialogs (text conversions, window text)
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include "../../../CommonLib/str/StringEncoding.h"

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
}
