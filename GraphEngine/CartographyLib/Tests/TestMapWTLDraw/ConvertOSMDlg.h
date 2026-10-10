// ConvertOSMDlg.h : "Convert from OpenStreetMap" dialog - the OSM file, its layers / tables (read before the conversion),
// the output SQLite database and the conversion settings
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef HAVE_OSM_CONVERTOR

#include "../../../Convertors/OSM/OSMConvertorLib/OSMConvertorLib.h"

class CConvertOSMDlg : public CDialogImpl<CConvertOSMDlg>
{
public:
	enum { IDD = IDD_CONVERT_OSM };

	CConvertOSMDlg();

	// results (valid after IDOK): the map with the selection applied (Enabled flags), the output database (it doesn't exist)
	GraphEngine::Convertors::IOSMMapPtr GetOSMMap() const { return m_ptrOSMMap; }
	const std::wstring& GetOutputPath() const { return m_sOutputPath; }
	const GraphEngine::Convertors::SOSMConvertSettings& GetSettings() const { return m_settings; }
	bool GetAddToMap() const { return m_bAddToMap; }

	BEGIN_MSG_MAP(CConvertOSMDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnRun)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_OSM_BROWSE, OnBrowse)
		COMMAND_ID_HANDLER(IDC_OSM_READ, OnRead)
		COMMAND_ID_HANDLER(IDC_OSM_OUTPUT_BROWSE, OnOutputBrowse)
		COMMAND_ID_HANDLER(IDC_OSM_SELECT_ALL, OnSelectAll)
		COMMAND_ID_HANDLER(IDC_OSM_CLEAR_ALL, OnSelectAll)
		COMMAND_HANDLER(IDC_OSM_PATH, EN_CHANGE, OnPathChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRun(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRead(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOutputBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelectAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPathChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	// reads the metadata and the layers of the file (progress window, can be canceled), true - read
	bool ReadMap(const std::wstring& sPath);
	void FillDatasets();
	void UpdateInfo();
	GraphEngine::Convertors::IOSMDatasetPtr GetDataset(int nItem) const;

private:
	CEdit         m_editPath;
	CEdit         m_editOutput;
	CListViewCtrl m_listDatasets;

	GraphEngine::Convertors::IOSMMapPtr m_ptrOSMMap;
	std::wstring  m_sReadPath;      // file m_ptrOSMMap was read from
	std::wstring  m_sOutputPath;
	GraphEngine::Convertors::SOSMConvertSettings m_settings;
	bool          m_bAddToMap;
};

#endif
