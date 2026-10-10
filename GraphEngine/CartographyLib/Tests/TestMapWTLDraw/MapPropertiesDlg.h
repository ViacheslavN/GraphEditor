// MapPropertiesDlg.h : "Map Properties" dialog - name, coordinate system (presets, EPSG code, proj4),
// map units, reference scale, background
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"

class CMapView;

class CMapPropertiesDlg : public CDialogImpl<CMapPropertiesDlg>
{
public:
	enum { IDD = IDD_MAP_PROPERTIES };

	explicit CMapPropertiesDlg(CMapView* pView);

	bool IsChanged() const { return m_bChanged; }

	BEGIN_MSG_MAP(CMapPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_APPLY, OnApply)
		COMMAND_HANDLER(IDC_MAP_CS_PRESET, CBN_SELCHANGE, OnPresetChanged)
		COMMAND_ID_HANDLER(IDC_MAP_EPSG_SET, OnEpsgSet)
		COMMAND_HANDLER(IDC_MAP_PROJ4, EN_CHANGE, OnProj4Changed)
		COMMAND_HANDLER(IDC_MAP_REF_SCALE_ON, BN_CLICKED, OnRefScaleOn)
		COMMAND_ID_HANDLER(IDC_MAP_REF_SCALE_CURRENT, OnRefScaleCurrent)
		COMMAND_ID_HANDLER(IDC_MAP_BG_BTN, OnBackgroundColor)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPresetChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEpsgSet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProj4Changed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefScaleOn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefScaleCurrent(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBackgroundColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void FillControls(const TestMapDraw::SMapParams& params);
	void FillExtent();
	// sets the proj4 text (preset, EPSG): the units follow the coordinate system
	void SetProj4(const std::string& sProj4);
	// the description of the proj4 text, false - wrong text
	bool UpdateInfo(CommonLib::Units* pUnits);
	void SelectUnits(CommonLib::Units units);
	bool Apply();

private:
	CMapView* m_pView;
	bool      m_bChanged;
	bool      m_bUpdating;
	std::vector<TestMapDraw::SCoordinateSystemPreset> m_vecPresets;   // combo item i + 1 (item 0 - the current)
	std::string m_sCurrentProj4;

	CComboBox m_comboPreset;
	CComboBox m_comboUnits;
};
