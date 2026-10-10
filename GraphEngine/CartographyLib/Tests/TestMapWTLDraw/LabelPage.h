// LabelPage.h : "Labels" tab of the Add layer dialogs - label field, text and placement settings
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"

class CLabelPage : public CDialogImpl<CLabelPage>
{
public:
	enum { IDD = IDD_PAGE_LABELS };

	CLabelPage();

	// fields of the layer table, nullptr - no table (the page is disabled)
	void SetFields(const std::vector<TestMapDraw::SFieldInfo>* pFields);
	// initial value of the scale field (current map scale), 0 - empty
	void SetDefaultScale(double dScale) { m_dDefaultScale = dScale; }
	// false - a wrong value (the message is shown); labels off - empty field
	bool GetLabels(TestMapDraw::SLabelParams& labels);

	BEGIN_MSG_MAP(CLabelPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_ENABLE_LABELS, BN_CLICKED, OnUpdateControls)
		COMMAND_HANDLER(IDC_LABEL_DUPLICATES, CBN_SELCHANGE, OnUpdateControls)
		COMMAND_HANDLER(IDC_LABEL_COLOR_BTN, BN_CLICKED, OnColor)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnUpdateControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void FillFields();
	void UpdateControls();
	bool Error(const wchar_t* pszMessage, HWND hFocus);

private:
	CButton   m_checkLabels;
	CComboBox m_comboField;
	CEdit     m_editScale;
	CEdit     m_editFontSize;
	CEdit     m_editColor;
	CButton   m_btnColor;
	CEdit     m_editHalo;
	CComboBox m_comboStrategy;
	CEdit     m_editPriority;
	CComboBox m_comboPointPosition;
	CEdit     m_editOffset;
	CComboBox m_comboLineOrientation;
	CComboBox m_comboLinePosition;
	CComboBox m_comboPolygonPlacement;
	CButton   m_checkPolygonOutside;
	CComboBox m_comboDuplicates;
	CEdit     m_editDuplicateDistance;

	std::vector<TestMapDraw::SFieldInfo> m_vecFields;
	bool   m_bHasFields;
	double m_dDefaultScale;
};
