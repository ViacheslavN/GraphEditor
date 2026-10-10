// TestMapDraw.cpp : main source file for TestMapWTLDraw.exe
//

#include "stdafx.h"

#include "resource.h"

#include "MapView.h"
#include "aboutdlg.h"
#include "LayerTreePane.h"
#include "MainFrm.h"

CAppModule _Module;

int Run(LPTSTR lpstrCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	// TestMapWTLDraw.exe file.shp | file.sqlite - opens the data at start
	if(lpstrCmdLine && lpstrCmdLine[0] != 0)
	{
		std::wstring sFile = lpstrCmdLine;
		if(sFile.size() > 1 && sFile.front() == L'"' && sFile.back() == L'"')
			sFile = sFile.substr(1, sFile.size() - 2);

		std::wstring sExt;
		size_t nDot = sFile.find_last_of(L'.');
		if(nDot != std::wstring::npos)
			sExt = sFile.substr(nDot);

		if(_wcsicmp(sExt.c_str(), L".sqlite") == 0 || _wcsicmp(sExt.c_str(), L".db") == 0)
			wndMain.m_view.AddSQLiteDatabase(sFile.c_str());
		else
			wndMain.m_view.AddShapeFile(sFile.c_str());
	}

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
