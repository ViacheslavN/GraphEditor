// SymbolEditorPanel.h : child panel editing one symbol: its type and the properties of the type
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include "SymbologyModel.h"

class CSymbolEditorPanel : public CDialogImpl<CSymbolEditorPanel>
{
public:
	enum { IDD = IDD_SYMBOL_EDITOR, RowCount = 8 };

	typedef std::function<void()> TOnChanged;

	CSymbolEditorPanel();

	void SetOnChanged(TOnChanged onChanged) { m_onChanged = onChanged; }
	// symbol types of the geometry in the type combo
	void SetGeometry(TestMapDraw::eGeometryKind geometry);
	void SetParams(const TestMapDraw::SSymbolParams& params);
	const TestMapDraw::SSymbolParams& GetParams() const { return m_params; }
	// reads all the edits, false - a wrong value (the message is shown)
	bool Commit();
	void Enable(bool bEnable);

	BEGIN_MSG_MAP(CSymbolEditorPanel)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_SYM_TYPE, CBN_SELCHANGE, OnTypeChanged)
		COMMAND_RANGE_CODE_HANDLER(IDC_PROP_EDIT0, IDC_PROP_EDIT0 + RowCount - 1, EN_KILLFOCUS, OnEditKillFocus)
		COMMAND_RANGE_CODE_HANDLER(IDC_PROP_COMBO0, IDC_PROP_COMBO0 + RowCount - 1, CBN_SELCHANGE, OnComboChanged)
		COMMAND_RANGE_CODE_HANDLER(IDC_PROP_BTN0, IDC_PROP_BTN0 + RowCount - 1, BN_CLICKED, OnButton)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTypeChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnComboChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void FillTypes();
	void ShowProperties();
	// row value -> m_params; false - wrong value (the old value is shown back, a message when bShowError)
	bool CommitRow(int nRow, bool bShowError);
	void NotifyChanged();

private:
	CComboBox m_comboType;
	CStatic   m_labels[RowCount];
	CEdit     m_edits[RowCount];
	CComboBox m_combos[RowCount];
	CButton   m_buttons[RowCount];

	TestMapDraw::eGeometryKind              m_geometry;
	TestMapDraw::SSymbolParams              m_params;
	std::vector<TestMapDraw::SPropertyInfo> m_vecProps;
	TOnChanged m_onChanged;
	bool       m_bUpdating;    // controls are filled by the code
	bool       m_bInMessage;   // a message box is shown (kill focus comes again)
	bool       m_bEnabled;
};
