// AddShapeFileDlg.cpp : implementation of the CAddShapeFileDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "AddShapeFileDlg.h"
#include "DialogUtils.h"

using namespace DialogUtils;

CAddShapeFileDlg::CAddShapeFileDlg() : m_bLoaded(false)
{

}

LRESULT CAddShapeFileDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	m_editPath = GetDlgItem(IDC_SHAPE_PATH);
	m_pages.Create(m_hWnd, IDC_TABS);
	m_pages.SetSource(nullptr, TestMapDraw::SDataSource());
	return TRUE;
}

std::wstring CAddShapeFileDlg::GetPathText() const
{
	return Trim(GetText(m_editPath));
}

void CAddShapeFileDlg::LoadTable(const std::wstring& sPath)
{
	if(sPath == m_sLoadedPath)
		return;

	m_sLoadedPath = sPath;
	m_bLoaded = false;
	if(!sPath.empty() && ::GetFileAttributesW(sPath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		try
		{
			m_table = TestMapDraw::CMapProject::GetShapefileInfo(WideToFilePath(sPath));
			m_bLoaded = true;
		}
		catch (std::exception& exc)
		{
			std::wstring sMsg = L"Failed to read the shape file:\n" + ExceptionText(exc);
			MessageBox(sMsg.c_str(), L"Add Shape File", MB_OK | MB_ICONWARNING);
		}
	}

	TestMapDraw::SDataSource source;
	source.sPath = WideToFilePath(sPath);
	m_pages.SetSource(m_bLoaded ? &m_table : nullptr, source);
}

LRESULT CAddShapeFileDlg::OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = GetPathText();
	CFileDialog fileDlg(TRUE, _T("shp"), sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("ESRI shape files (*.shp)\0*.shp\0All Files (*.*)\0*.*\0"), m_hWnd);
	if(fileDlg.DoModal() != IDOK)
		return 0;

	m_editPath.SetWindowText(fileDlg.m_szFileName);
	LoadTable(fileDlg.m_szFileName);
	return 0;
}

LRESULT CAddShapeFileDlg::OnPathKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LoadTable(GetPathText()); // path typed / pasted by hand
	return 0;
}

LRESULT CAddShapeFileDlg::OnTabChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	m_pages.OnTabChanged();
	return 0;
}

LRESULT CAddShapeFileDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sPath = GetPathText();
	if(sPath.empty() || ::GetFileAttributesW(sPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBox(L"Select an existing shape file.", L"Add Shape File", MB_OK | MB_ICONWARNING);
		m_editPath.SetFocus();
		return 0;
	}

	LoadTable(sPath);
	if(!m_bLoaded)
		return 0;

	if(!m_pages.GetLayerParams(m_params))
		return 0;

	m_sPath = sPath;
	EndDialog(wID);
	return 0;
}

LRESULT CAddShapeFileDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
