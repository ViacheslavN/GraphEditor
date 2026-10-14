// ConvertDatabaseDlg.h : "Convert to Database" dialog - a shape file or the spatial tables of a database (SQLite / BPtreeDB)
// into a new or existing database: SQLite or BPtreeDB with the R-tree / Z-order spatial index
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"

class CConvertDatabaseDlg : public CDialogImpl<CConvertDatabaseDlg>
{
public:
	enum { IDD = IDD_CONVERT_DB };

	CConvertDatabaseDlg();

	// results (valid after IDOK)
	const std::wstring& GetSourcePath() const { return m_sSourcePath; }
	bool IsShapefileSource() const;
	const std::string& GetSourceTable() const { return m_sSourceTable; }     // empty - all spatial tables (a database)
	const std::wstring& GetOutputPath() const { return m_sOutputPath; }
	const TestMapDraw::SDatabaseOptions& GetOptions() const { return m_options; }

	BEGIN_MSG_MAP(CConvertDatabaseDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_CONV_SOURCE_BROWSE, OnSourceBrowse)
		COMMAND_ID_HANDLER(IDC_CONV_OUTPUT_BROWSE, OnOutputBrowse)
		COMMAND_HANDLER(IDC_CONV_SOURCE, EN_KILLFOCUS, OnSourceKillFocus)
		COMMAND_HANDLER(IDC_CONV_FORMAT, CBN_SELCHANGE, OnFormatChanged)
		COMMAND_HANDLER(IDC_CONV_SPATIAL_INDEX, CBN_SELCHANGE, OnSpatialIndexChanged)
		COMMAND_HANDLER(IDC_CONV_OUTPUT, EN_KILLFOCUS, OnOutputKillFocus)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSourceBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOutputBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSourceKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFormatChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSpatialIndexChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOutputKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	// the source: a shape file (one table) or a database (its spatial tables in the combo)
	void SetSource(const std::wstring& sPath);
	bool IsBPtreeDBFormat() const;
	// an existing output database tells the format; the spatial index only for BPtreeDB; the hint
	void UpdateControls();

private:
	CEdit     m_editSource;
	CEdit     m_editOutput;
	CComboBox m_comboTable;
	CComboBox m_comboFormat;
	CComboBox m_comboSpatialIndex;

	std::wstring m_sLoadedSource;   // the source the table combo was filled for
	std::vector<std::string> m_vecTables;

	std::wstring m_sSourcePath;
	std::string  m_sSourceTable;
	std::wstring m_sOutputPath;
	TestMapDraw::SDatabaseOptions m_options;

	// kept while the application runs
	static int s_nFormat;
	static int s_nSpatialIndex;
	static bool s_bCompress;
};
