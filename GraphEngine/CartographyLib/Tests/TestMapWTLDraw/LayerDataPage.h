// LayerDataPage.h : "Data" tab of the layer properties - the source table, its fields,
// the OID field and the shape field the layer uses
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"

class CLayerDataPage : public CDialogImpl<CLayerDataPage>
{
public:
	enum { IDD = IDD_PAGE_DATA };

	CLayerDataPage();

	// before Create
	void SetInfo(const TestMapDraw::SLayerDataInfo& info) { m_info = info; }
	// the chosen fields, empty - the table default
	void GetFields(std::string& sOIDField, std::string& sShapeField) const;

	BEGIN_MSG_MAP(CLayerDataPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_DATA_OID_FIELD, CBN_SELCHANGE, OnFieldChanged)
		COMMAND_HANDLER(IDC_DATA_SHAPE_FIELD, CBN_SELCHANGE, OnFieldChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFieldChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	// first item - the table default (empty name), then the candidates; selects sCurrent
	static void FillCombo(CComboBox& combo, const std::string& sTableDefault, const std::vector<std::string>& vecNames,
	                      const std::string& sCurrent, std::vector<std::string>& vecItems);
	std::string Selected(const CComboBox& combo, const std::vector<std::string>& vecItems) const;
	void FillFieldList();

private:
	TestMapDraw::SLayerDataInfo m_info;
	CComboBox     m_comboOID;
	CComboBox     m_comboShape;
	CListViewCtrl m_listFields;
	std::vector<std::string> m_vecOIDItems;     // field name of every combo item, empty - the table default
	std::vector<std::string> m_vecShapeItems;
};
