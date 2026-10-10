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

	// results (valid after IDOK): the map with the selection applied (Enabled flags), the output database (it doesn't exist);
	// "Convert all": the map isn't read (IsScanned false, no counts), all the datasets are enabled
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
		COMMAND_ID_HANDLER(IDC_OSM_CONVERT_ALL, OnConvertAll)
		COMMAND_HANDLER(IDC_OSM_EXPORT_TAGS, BN_CLICKED, OnExportTags)
		COMMAND_HANDLER(IDC_OSM_COMPRESS, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_AUTO, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_MAX, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_MANUAL, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_WEB_MERCATOR, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_VALUE, EN_CHANGE, OnCompressChanged)
		NOTIFY_HANDLER(IDC_OSM_DATASETS, LVN_ITEMCHANGED, OnDatasetChanged)
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
	// all the layers and tables without reading the file first (no counts, one pass less)
	LRESULT OnConvertAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOutputBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCompressChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExportTags(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDatasetChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnSelectAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPathChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	// reads the metadata and the layers of the file (progress window, can be canceled), true - read
	bool ReadMap(const std::wstring& sPath);
	// the output database (asks to replace it) and the settings, false - stay in the dialog
	bool PrepareRun();
	// the compression settings from the controls, false - a wrong manual scale (sError)
	bool GetCompressSettings(GraphEngine::Convertors::SOSMCompressSettings& compression, std::wstring& sError) const;
	// enables the scale controls, shows the resulting precision
	void UpdateCompressControls();
	void FillDatasets();
	void UpdateInfo();
	GraphEngine::Convertors::IOSMDatasetPtr GetDataset(int nItem) const;
	int  FindTagsItem() const;   // the tags table in the list, -1 - not read / no such table
	// the check box and the tags item of the list show the same choice
	void SetExportTags(bool bExport);

private:
	CEdit         m_editPath;
	CEdit         m_editOutput;
	CListViewCtrl m_listDatasets;

	GraphEngine::Convertors::IOSMMapPtr m_ptrOSMMap;
	std::wstring  m_sReadPath;      // file m_ptrOSMMap was read from
	std::wstring  m_sOutputPath;
	GraphEngine::Convertors::SOSMConvertSettings m_settings;
	bool          m_bAddToMap;
	bool          m_bExportTags;    // the tags table is converted (off by default: it is the biggest table)
	bool          m_bUpdating;      // the list / check box are changed by the code
};

#endif
