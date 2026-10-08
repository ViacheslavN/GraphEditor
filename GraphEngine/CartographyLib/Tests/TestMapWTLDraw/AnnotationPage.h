// AnnotationPage.h : "Annotation" tab of the Add layer dialogs - annotation field and the visibility scale
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"

class CAnnotationPage : public CDialogImpl<CAnnotationPage>
{
public:
	enum { IDD = IDD_PAGE_ANNOTATION };

	CAnnotationPage();

	// fields of the layer table, nullptr - no table (the page is disabled)
	void SetFields(const std::vector<TestMapDraw::SFieldInfo>* pFields);
	// initial value of the scale field (current map scale), 0 - empty
	void SetDefaultScale(double dScale) { m_dDefaultScale = dScale; }
	// false - a wrong value (the message is shown); annotation off - empty field
	bool GetAnnotation(TestMapDraw::SAnnotationParams& anno);

	BEGIN_MSG_MAP(CAnnotationPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_ENABLE_ANNO, BN_CLICKED, OnEnableAnno)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEnableAnno(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void FillFields();
	void UpdateControls();

private:
	CButton   m_checkAnno;
	CComboBox m_comboField;
	CEdit     m_editScale;

	std::vector<TestMapDraw::SFieldInfo> m_vecFields;
	bool   m_bHasFields;
	double m_dDefaultScale;
};
