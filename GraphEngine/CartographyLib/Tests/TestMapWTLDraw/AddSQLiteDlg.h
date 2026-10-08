// AddSQLiteDlg.h : "Add SQLite Database" dialog - database path, table, symbology and annotation tabs
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "LayerPages.h"

class CAddSQLiteDlg : public CDialogImpl<CAddSQLiteDlg>
{
public:
	enum { IDD = IDD_ADD_SQLITE };

	CAddSQLiteDlg();

	// before DoModal: index of the new layer (color of the default symbol), current map scale (annotation scale)
	void SetLayerIndex(int nIndex) { m_pages.SetLayerIndex(nIndex); }
	void SetDefaultScale(double dScale) { m_pages.SetDefaultScale(dScale); }

	// results (valid after IDOK)
	const std::wstring& GetPath() const { return m_sPath; }
	const std::string& GetTableName() const { return m_sTableName; }   // empty - all spatial tables
	const TestMapDraw::SLayerParams& GetLayerParams() const { return m_params; }

	BEGIN_MSG_MAP(CAddSQLiteDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse)
		COMMAND_HANDLER(IDC_DB_PATH, EN_KILLFOCUS, OnPathKillFocus)
		COMMAND_HANDLER(IDC_TABLE, CBN_SELCHANGE, OnTableChanged)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, OnTabChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPathKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTableChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTabChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

private:
	std::wstring GetPathText() const;
	void LoadTables(const std::wstring& sPath);   // fills the table combo from the database spatial tables
	int  GetSelectedTable() const;                // index in m_vecTables, -1 - all tables
	void UpdatePages();                           // the pages work with one table only

private:
	CEdit     m_editPath;
	CComboBox m_comboTable;
	CLayerPages m_pages;

	std::wstring m_sPath;
	std::wstring m_sTablesPath;  // database the table combo was filled from
	std::vector<TestMapDraw::STableInfo> m_vecTables;
	std::string m_sTableName;
	TestMapDraw::SLayerParams m_params;
};
