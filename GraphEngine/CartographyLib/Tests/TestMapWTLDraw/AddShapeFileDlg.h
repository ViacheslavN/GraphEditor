// AddShapeFileDlg.h : "Add Shape File" dialog - shape file path, symbology and annotation tabs
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "LayerPages.h"

class CAddShapeFileDlg : public CDialogImpl<CAddShapeFileDlg>
{
public:
	enum { IDD = IDD_ADD_SHAPEFILE };

	CAddShapeFileDlg();

	// before DoModal: index of the new layer (color of the default symbol), current map scale (annotation scale)
	void SetLayerIndex(int nIndex) { m_pages.SetLayerIndex(nIndex); }
	void SetDefaultScale(double dScale) { m_pages.SetDefaultScale(dScale); }

	// results (valid after IDOK)
	const std::wstring& GetPath() const { return m_sPath; }
	const TestMapDraw::SLayerParams& GetLayerParams() const { return m_params; }

	BEGIN_MSG_MAP(CAddShapeFileDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse)
		COMMAND_HANDLER(IDC_SHAPE_PATH, EN_KILLFOCUS, OnPathKillFocus)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, OnTabChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPathKillFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTabChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

private:
	std::wstring GetPathText() const;
	void LoadTable(const std::wstring& sPath);   // reads the fields of the shape file into the pages

private:
	CEdit m_editPath;
	CLayerPages m_pages;

	std::wstring m_sPath;
	std::wstring m_sLoadedPath;   // shape file the pages were filled from
	bool m_bLoaded;
	TestMapDraw::STableInfo m_table;
	TestMapDraw::SLayerParams m_params;
};
