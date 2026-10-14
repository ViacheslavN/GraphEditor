// ConvertDatabaseDlg.cpp : implementation of the CConvertDatabaseDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "ConvertDatabaseDlg.h"
#include "DialogUtils.h"

using namespace DialogUtils;

namespace
{
	const wchar_t* Caption = L"Convert to Database";
	const wchar_t* AllTablesText = L"<All spatial tables>";

	// the items of the combos
	enum { FormatSQLite = 0, FormatBPtreeDB = 1 };
	enum { IndexRTree = 0, IndexZOrder = 1 };

	bool FileExists(const std::wstring& sPath)
	{
		return !sPath.empty() && ::GetFileAttributesW(sPath.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool IsShapefile(const std::wstring& sPath)
	{
		return EndsWithNoCase(sPath, L".shp");
	}
}

int CConvertDatabaseDlg::s_nFormat = FormatBPtreeDB;
int CConvertDatabaseDlg::s_nSpatialIndex = IndexRTree;
bool CConvertDatabaseDlg::s_bCompress = true;

CConvertDatabaseDlg::CConvertDatabaseDlg()
{

}

bool CConvertDatabaseDlg::IsShapefileSource() const
{
	return IsShapefile(m_sSourcePath);
}

LRESULT CConvertDatabaseDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	m_editSource = GetDlgItem(IDC_CONV_SOURCE);
	m_editOutput = GetDlgItem(IDC_CONV_OUTPUT);
	m_comboTable = GetDlgItem(IDC_CONV_SOURCE_TABLE);
	m_comboFormat = GetDlgItem(IDC_CONV_FORMAT);
	m_comboSpatialIndex = GetDlgItem(IDC_CONV_SPATIAL_INDEX);

	// the order is FormatSQLite, FormatBPtreeDB / IndexRTree, IndexZOrder
	m_comboFormat.AddString(L"SQLite (.sqlite)");
	m_comboFormat.AddString(L"BPtreeDB (.btdb)");
	m_comboFormat.SetCurSel(s_nFormat);
	m_comboSpatialIndex.AddString(L"R-tree");
	m_comboSpatialIndex.AddString(L"Z-order");
	m_comboSpatialIndex.SetCurSel(s_nSpatialIndex);

	CheckDlgButton(IDC_CONV_COMPRESS, s_bCompress ? BST_CHECKED : BST_UNCHECKED);
	m_comboTable.EnableWindow(FALSE);
	UpdateControls();
	return TRUE;
}

bool CConvertDatabaseDlg::IsBPtreeDBFormat() const
{
	return m_comboFormat.GetCurSel() == FormatBPtreeDB;
}

void CConvertDatabaseDlg::SetSource(const std::wstring& sPath)
{
	if(sPath == m_sLoadedSource)
		return;

	m_sLoadedSource = sPath;
	m_vecTables.clear();
	m_comboTable.ResetContent();

	if(IsShapefile(sPath))
	{
		std::wstring sName = sPath.substr(sPath.find_last_of(L"\\/") + 1);
		m_comboTable.AddString(sName.substr(0, sName.size() - 4).c_str());
		m_comboTable.SetCurSel(0);
		m_comboTable.EnableWindow(FALSE);
	}
	else if(FileExists(sPath))
	{
		try
		{
			CWaitCursorGuard wait;
			std::vector<TestMapDraw::STableInfo> vecTables = TestMapDraw::CMapProject::GetDatabaseTables(WideToUtf8(sPath));
			for(const TestMapDraw::STableInfo& info : vecTables)
				m_vecTables.push_back(info.sName);
		}
		catch (std::exception& exc)
		{
			std::wstring sMsg = L"Failed to read tables of the database:\n" + ExceptionText(exc);
			MessageBox(sMsg.c_str(), Caption, MB_OK | MB_ICONWARNING);
		}

		// item 0 - all tables, item i + 1 - m_vecTables[i]
		if(!m_vecTables.empty())
		{
			m_comboTable.AddString(AllTablesText);
			for(const std::string& sName : m_vecTables)
				m_comboTable.AddString(Utf8ToWide(sName).c_str());
			m_comboTable.SetCurSel(m_vecTables.size() == 1 ? 1 : 0);
		}
		m_comboTable.EnableWindow(!m_vecTables.empty());
	}
	else
		m_comboTable.EnableWindow(FALSE);

	// the output next to the source: <source>.btdb / .sqlite (a database source of the same format - <source>_copy)
	if(Trim(GetText(m_editOutput)).empty() && !sPath.empty())
	{
		std::wstring sBase = sPath;
		size_t nDot = sBase.find_last_of(L'.');
		size_t nSlash = sBase.find_last_of(L"\\/");
		if(nDot != std::wstring::npos && (nSlash == std::wstring::npos || nDot > nSlash))
			sBase.resize(nDot);
		std::wstring sOutput = WithDatabaseExtension(sBase, IsBPtreeDBFormat());
		if(_wcsicmp(sOutput.c_str(), sPath.c_str()) == 0)
			sOutput = WithDatabaseExtension(sBase + L"_copy", IsBPtreeDBFormat());
		m_editOutput.SetWindowText(sOutput.c_str());
	}
	UpdateControls();
}

void CConvertDatabaseDlg::UpdateControls()
{
	// an existing database keeps its format: the tables are added to it
	std::wstring sOutput = Trim(GetText(m_editOutput));
	bool bExisting = FileExists(sOutput);
	if(bExisting)
		m_comboFormat.SetCurSel(IsBPtreeDBPath(sOutput) ? FormatBPtreeDB : FormatSQLite);
	m_comboFormat.EnableWindow(!bExisting);

	bool bBPtreeDB = IsBPtreeDBFormat();
	m_comboSpatialIndex.EnableWindow(bBPtreeDB);

	std::wstring sInfo;
	if(bExisting)
		sInfo = L"The output database exists: the tables are added to it (under new names if they are there already). ";
	if(bBPtreeDB)
	{
		sInfo += m_comboSpatialIndex.GetCurSel() == IndexZOrder ?
			L"Z-order: a B+ tree of the cells of a grid over the extent of the data, built after the rows are copied." :
			L"R-tree: the bounding boxes, packed after the rows are copied.";
	}
	else
		sInfo += L"SQLite: the R-tree module of SQLite.";
	SetDlgItemText(IDC_CONV_INFO, sInfo.c_str());
}

LRESULT CConvertDatabaseDlg::OnSourceBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = Trim(GetText(m_editSource));
	CFileDialog fileDlg(TRUE, NULL, sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("Shape files and databases (*.shp;*.sqlite;*.db;*.btdb)\0*.shp;*.sqlite;*.db;*.btdb\0ESRI shape files (*.shp)\0*.shp\0")
		_T("SQLite database (*.sqlite;*.db)\0*.sqlite;*.db\0BPtreeDB database (*.btdb)\0*.btdb\0All Files (*.*)\0*.*\0"), m_hWnd);
	fileDlg.m_ofn.lpstrTitle = _T("Shape file or database to convert");
	if(fileDlg.DoModal() != IDOK)
		return 0;

	m_editSource.SetWindowText(fileDlg.m_szFileName);
	SetSource(fileDlg.m_szFileName);
	return 0;
}

LRESULT CConvertDatabaseDlg::OnOutputBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool bBPtreeDB = IsBPtreeDBFormat();
	std::wstring sCurrent = Trim(GetText(m_editOutput));
	CFileDialog fileDlg(FALSE, bBPtreeDB ? _T("btdb") : _T("sqlite"), sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY,
		DatabaseFileFilter(bBPtreeDB), m_hWnd);
	fileDlg.m_ofn.lpstrTitle = _T("Output database (new or existing)");
	if(fileDlg.DoModal() != IDOK)
		return 0;

	m_editOutput.SetWindowText(fileDlg.m_szFileName);
	UpdateControls();
	return 0;
}

LRESULT CConvertDatabaseDlg::OnSourceKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetSource(Trim(GetText(m_editSource)));   // a path typed / pasted by hand
	return 0;
}

LRESULT CConvertDatabaseDlg::OnOutputKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

LRESULT CConvertDatabaseDlg::OnFormatChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// the output gets the extension of the format
	std::wstring sOutput = Trim(GetText(m_editOutput));
	if(!sOutput.empty())
		m_editOutput.SetWindowText(WithDatabaseExtension(sOutput, IsBPtreeDBFormat()).c_str());
	UpdateControls();
	return 0;
}

LRESULT CConvertDatabaseDlg::OnSpatialIndexChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

LRESULT CConvertDatabaseDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sSource = Trim(GetText(m_editSource));
	if(!FileExists(sSource))
	{
		MessageBox(L"Select an existing shape file or database.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editSource);
		return 0;
	}
	SetSource(sSource);
	if(!IsShapefile(sSource) && m_vecTables.empty())
	{
		MessageBox(L"The source database has no spatial tables.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editSource);
		return 0;
	}

	std::wstring sOutput = Trim(GetText(m_editOutput));
	if(sOutput.empty())
	{
		MessageBox(L"Set the output database.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editOutput);
		return 0;
	}
	// a new database: the extension of the format; an existing one: as it is (its format)
	if(!FileExists(sOutput))
		sOutput = WithDatabaseExtension(sOutput, IsBPtreeDBFormat());
	if(TestMapDraw::CMapProject::IsSameFile(WideToUtf8(sOutput), WideToUtf8(sSource)))
	{
		MessageBox(L"The source and the output are the same file.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editOutput);
		return 0;
	}

	int nTable = m_comboTable.GetCurSel();
	m_sSourceTable = (!IsShapefile(sSource) && nTable > 0 && nTable <= (int)m_vecTables.size()) ? m_vecTables[nTable - 1] : std::string();
	m_sSourcePath = sSource;
	m_sOutputPath = sOutput;

	s_nFormat = m_comboFormat.GetCurSel();
	s_nSpatialIndex = m_comboSpatialIndex.GetCurSel();
	m_options.format = IsBPtreeDBPath(sOutput) ? TestMapDraw::DatabaseBPtreeDB : TestMapDraw::DatabaseSQLite;
	m_options.spatialIndex.type = s_nSpatialIndex == IndexZOrder ? GraphEngine::GeoDatabase::bsitZOrder : GraphEngine::GeoDatabase::bsitRTree;
	s_bCompress = IsDlgButtonChecked(IDC_CONV_COMPRESS) == BST_CHECKED;
	m_options.bCompressGeometry = s_bCompress;
	EndDialog(wID);
	return 0;
}

LRESULT CConvertDatabaseDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
