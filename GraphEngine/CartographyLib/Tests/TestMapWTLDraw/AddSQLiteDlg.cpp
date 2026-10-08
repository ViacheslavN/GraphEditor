// AddSQLiteDlg.cpp : implementation of the CAddSQLiteDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "AddSQLiteDlg.h"
#include "DialogUtils.h"

using namespace DialogUtils;

namespace
{
	const wchar_t* AllTablesText = L"<All spatial tables>";
}

CAddSQLiteDlg::CAddSQLiteDlg()
{

}

LRESULT CAddSQLiteDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	m_editPath = GetDlgItem(IDC_DB_PATH);
	m_comboTable = GetDlgItem(IDC_TABLE);
	m_comboTable.EnableWindow(FALSE);
	m_pages.Create(m_hWnd, IDC_TABS);
	UpdatePages();
	return TRUE;
}

std::wstring CAddSQLiteDlg::GetPathText() const
{
	return Trim(GetText(m_editPath));
}

void CAddSQLiteDlg::LoadTables(const std::wstring& sPath)
{
	if(sPath == m_sTablesPath)
		return;

	m_sTablesPath = sPath;
	m_vecTables.clear();
	m_comboTable.ResetContent();

	if(!sPath.empty() && ::GetFileAttributesW(sPath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		try
		{
			m_vecTables = TestMapDraw::CMapProject::GetSQLiteTables(WideToUtf8(sPath));
		}
		catch (std::exception& exc)
		{
			std::wstring sMsg = L"Failed to read tables of the database:\n" + ExceptionText(exc);
			MessageBox(sMsg.c_str(), L"Add SQLite Database", MB_OK | MB_ICONWARNING);
		}
	}

	if(!m_vecTables.empty())
	{
		// item 0 - all tables, item i + 1 - m_vecTables[i]
		m_comboTable.AddString(AllTablesText);
		for(size_t i = 0; i < m_vecTables.size(); ++i)
			m_comboTable.AddString(Utf8ToWide(m_vecTables[i].sName).c_str());

		// one table - select it, so the symbology / annotation can be set at once
		m_comboTable.SetCurSel(m_vecTables.size() == 1 ? 1 : 0);
	}
	m_comboTable.EnableWindow(!m_vecTables.empty());

	UpdatePages();
}

int CAddSQLiteDlg::GetSelectedTable() const
{
	int nSel = m_comboTable.GetCurSel();
	if(nSel <= 0 || nSel > (int)m_vecTables.size())
		return -1;
	return nSel - 1;
}

void CAddSQLiteDlg::UpdatePages()
{
	int nTable = GetSelectedTable();
	TestMapDraw::SDataSource source;
	source.bSQLite = true;
	source.sPath = WideToUtf8(m_sTablesPath);   // SQLite opens files with UTF-8 names
	source.sTable = nTable >= 0 ? m_vecTables[nTable].sName : std::string();
	m_pages.SetSource(nTable >= 0 ? &m_vecTables[nTable] : nullptr, source);
}

LRESULT CAddSQLiteDlg::OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = GetPathText();
	CFileDialog fileDlg(TRUE, _T("sqlite"), sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("SQLite database (*.sqlite;*.db)\0*.sqlite;*.db\0All Files (*.*)\0*.*\0"), m_hWnd);
	if(fileDlg.DoModal() != IDOK)
		return 0;

	m_editPath.SetWindowText(fileDlg.m_szFileName);
	LoadTables(fileDlg.m_szFileName);
	return 0;
}

LRESULT CAddSQLiteDlg::OnPathKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LoadTables(GetPathText()); // path typed / pasted by hand
	return 0;
}

LRESULT CAddSQLiteDlg::OnTableChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdatePages();
	return 0;
}

LRESULT CAddSQLiteDlg::OnTabChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	m_pages.OnTabChanged();
	return 0;
}

LRESULT CAddSQLiteDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sPath = GetPathText();
	if(sPath.empty() || ::GetFileAttributesW(sPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBox(L"Select an existing SQLite database.", L"Add SQLite Database", MB_OK | MB_ICONWARNING);
		m_editPath.SetFocus();
		return 0;
	}

	LoadTables(sPath);
	if(m_vecTables.empty())
	{
		MessageBox(L"The database has no spatial tables.", L"Add SQLite Database", MB_OK | MB_ICONWARNING);
		m_editPath.SetFocus();
		return 0;
	}

	// symbology / annotation only for one table (the pages are empty for all tables)
	if(!m_pages.GetLayerParams(m_params))
		return 0;

	int nTable = GetSelectedTable();
	m_sPath = sPath;
	m_sTableName = nTable >= 0 ? m_vecTables[nTable].sName : std::string();
	EndDialog(wID);
	return 0;
}

LRESULT CAddSQLiteDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
