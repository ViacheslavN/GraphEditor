// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "MapView.h"
#include "LayerTreePane.h"
#include "MainFrm.h"

namespace
{
	const TCHAR ProjectFilter[] = _T("Map project (*.xml)\0*.xml\0All Files (*.*)\0*.*\0");
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
	// no events from the map: the layers panel compares the map state on idle
	m_layerTree.CheckMap();
	UISetCheck(ID_VIEW_LAYERS, m_splitter.GetSinglePaneMode() == SPLIT_PANE_NONE ? 1 : 0);

	bool b3D = m_view.Is3DMode();
	UISetCheck(ID_VIEW_3D, b3D ? 1 : 0);
	UIEnable(ID_TILT_UP, b3D);
	UIEnable(ID_TILT_DOWN, b3D);
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// create command bar window
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// attach menu
	m_CmdBar.AttachMenu(GetMenu());
	// load command bar images
	m_CmdBar.LoadImages(IDR_MAINFRAME);
	// remove old menu
	SetMenu(NULL);

	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

	CreateSimpleStatusBar();

	m_hWndClient = m_splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_splitter.SetSplitterExtendedStyle(0);   // the layers panel keeps its width when the window is resized

	m_layerTree.SetMapView(&m_view);
	m_layerTree.Create(m_splitter, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_view.Create(m_splitter, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
	m_view.SetStatusBar(m_hWndStatusBar);
	m_splitter.SetSplitterPanes(m_layerTree, m_view);

	UpdateLayout();
	HDC hDC = GetDC();
	int nDpi = ::GetDeviceCaps(hDC, LOGPIXELSX);
	ReleaseDC(hDC);
	m_splitter.SetSplitterPos(::MulDiv(240, nDpi, 96));
	m_layerTree.CheckMap();

	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UpdateTitle();
	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

void CMainFrame::UpdateTitle()
{
	std::wstring sTitle = L"TestMapDraw";
	if(!m_sProjectFile.empty())
		sTitle += L" - " + m_sProjectFile;
	SetWindowText(sTitle.c_str());
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_view.NewProject();
	m_sProjectFile.clear();
	UpdateTitle();
	return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog fileDlg(TRUE, _T("xml"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, ProjectFilter, m_hWnd);
	if ( fileDlg.DoModal() != IDOK )
		return 0;

	if(m_view.OpenProject(fileDlg.m_szFileName))
		m_sProjectFile = fileDlg.m_szFileName;
	else
		m_sProjectFile.clear();

	UpdateTitle();
	return 0;
}

LRESULT CMainFrame::OnFileSave(WORD wNotifyCode, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled)
{
	if(m_sProjectFile.empty())
		return OnFileSaveAs(wNotifyCode, ID_FILE_SAVE_AS, hWndCtl, bHandled);

	m_view.SaveProject(m_sProjectFile.c_str());
	return 0;
}

LRESULT CMainFrame::OnFileSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog fileDlg(FALSE, _T("xml"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ProjectFilter, m_hWnd);
	if ( fileDlg.DoModal() != IDOK )
		return 0;

	if(m_view.SaveProject(fileDlg.m_szFileName))
	{
		m_sProjectFile = fileDlg.m_szFileName;
		UpdateTitle();
	}
	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;	// initially visible
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewLayers(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool bShow = m_splitter.GetSinglePaneMode() != SPLIT_PANE_NONE;
	m_splitter.SetSinglePaneMode(bShow ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
	UISetCheck(ID_VIEW_LAYERS, bShow ? 1 : 0);
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}
