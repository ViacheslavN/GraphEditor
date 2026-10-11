// ConvertOSMDlg.h : "Convert from OpenStreetMap" dialog - the OSM file, its layers / tables and their columns
// (Analyze before the conversion, the unchecked ones go to the exclude list), the output SQLite database and the conversion settings
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

	// results (valid after IDOK): the analyzed map, what isn't converted (unchecked layers / tables / columns),
	// the output database (it doesn't exist);
	// "Convert all": the map isn't analyzed (IsScanned false, no counts, the columns of the schema), nothing is excluded
	// (but the tags table by its check box)
	GraphEngine::Convertors::IOSMMapPtr GetOSMMap() const { return m_ptrOSMMap; }
	const GraphEngine::Convertors::CLayerColumnExcludeList& GetExcludeList() const { return m_excludeList; }
	const std::wstring& GetOutputPath() const { return m_sOutputPath; }
	const GraphEngine::Convertors::SOSMConvertSettings& GetSettings() const { return m_settings; }
	bool GetAddToMap() const { return m_bAddToMap; }

	BEGIN_MSG_MAP(CConvertOSMDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnRun)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_OSM_BROWSE, OnBrowse)
		COMMAND_ID_HANDLER(IDC_OSM_ANALYZE, OnAnalyze)
		COMMAND_ID_HANDLER(IDC_OSM_CONVERT_ALL, OnConvertAll)
		COMMAND_ID_HANDLER(IDC_OSM_SPLIT, OnSplit)
		COMMAND_HANDLER(IDC_OSM_EXPORT_TAGS, BN_CLICKED, OnExportTags)
		COMMAND_HANDLER(IDC_OSM_COMPRESS, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_AUTO, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_MAX, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_MANUAL, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_WEB_MERCATOR, BN_CLICKED, OnCompressChanged)
		COMMAND_HANDLER(IDC_OSM_SCALE_VALUE, EN_CHANGE, OnCompressChanged)
		NOTIFY_HANDLER(IDC_OSM_DATASETS, TVN_ITEMCHANGED, OnDatasetChanged)
		COMMAND_ID_HANDLER(IDC_OSM_OUTPUT_BROWSE, OnOutputBrowse)
		COMMAND_ID_HANDLER(IDC_OSM_SELECT_ALL, OnSelectAll)
		COMMAND_ID_HANDLER(IDC_OSM_CLEAR_ALL, OnSelectAll)
		COMMAND_HANDLER(IDC_OSM_PATH, EN_CHANGE, OnPathChanged)
		COMMAND_HANDLER(IDC_OSM_NODE_STORE, CBN_SELCHANGE, OnNodeStoreChanged)
		COMMAND_ID_HANDLER(IDC_OSM_TEMP_BROWSE, OnTempBrowse)
		COMMAND_HANDLER(IDC_OSM_TEMP_DIR, EN_CHANGE, OnTempChanged)
		COMMAND_HANDLER(IDC_OSM_OUTPUT, EN_CHANGE, OnTempChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRun(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAnalyze(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// all the layers and tables without analyzing the file first (no counts, no OSM key columns, one pass less)
	LRESULT OnConvertAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	// the OSM file into the split data (a folder: nodes, ways, relations, metadata), they are opened then
	LRESULT OnSplit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOutputBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCompressChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnExportTags(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDatasetChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnSelectAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPathChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNodeStoreChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTempBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTempChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	// analyzes the file: metadata, layers / tables, their columns (progress window, can be canceled), true - done;
	// the split data (osm_split.xml) are opened at once
	bool AnalyzeMap(const std::wstring& sPath);
	// the analyzed map is shown, the output database is suggested
	void SetMap(GraphEngine::Convertors::IOSMMapPtr ptrMap, const std::wstring& sPath);
	// the output database (asks to replace it) and the settings, false - stay in the dialog
	bool PrepareRun();
	// the compression settings from the controls, false - a wrong manual scale (sError)
	bool GetCompressSettings(GraphEngine::Convertors::SOSMCompressSettings& compression, std::wstring& sError) const;
	// enables the scale controls, shows the resulting precision
	void UpdateCompressControls();
	// the node store chosen for the file (Auto - by its size) and its disk space
	void UpdateNodeStoreInfo();
	// the folder of the temporary files: the field, empty - the folder of the output database
	std::wstring TempFolder() const;
	// the folder and its free disk space
	void UpdateTempInfo();
	// the tree: layers / tables with their columns, all checked (the datasets without features and the tags table - by m_bExportTags - not)
	void FillDatasets();
	void UpdateInfo();
	// the unchecked datasets / columns of the tree
	GraphEngine::Convertors::CLayerColumnExcludeList BuildExcludeList() const;
	// the item of the tree
	struct SItem
	{
		int nDataset = -1;   // layer index or TableItem + table index
		int nColumn = -1;    // -1 - the dataset itself
	};
	const SItem* GetItem(HTREEITEM hItem) const;
	GraphEngine::Convertors::IOSMDatasetPtr GetDataset(int nDataset) const;
	HTREEITEM FindTagsItem() const;   // the tags table in the tree, NULL - not analyzed / no such table
	// the check box and the tags item of the tree show the same choice
	void SetExportTags(bool bExport);

private:
	CEdit         m_editPath;
	CEdit         m_editOutput;
	CTreeViewCtrl m_treeDatasets;
	std::vector<SItem> m_vecItems;  // item data of the tree: index + 1

	GraphEngine::Convertors::IOSMMapPtr m_ptrOSMMap;
	GraphEngine::Convertors::CLayerColumnExcludeList m_excludeList;
	std::wstring  m_sReadPath;      // file m_ptrOSMMap was analyzed from
	std::wstring  m_sOutputPath;
	GraphEngine::Convertors::SOSMConvertSettings m_settings;
	bool          m_bAddToMap;
	bool          m_bExportTags;    // the tags table is converted (off by default: it is the biggest table)
	bool          m_bUpdating;      // the tree / check box are changed by the code
	static std::wstring s_sTempDir; // the folder of the temporary files, kept while the application runs
};

#endif
