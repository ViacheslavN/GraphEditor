// SymbologyPage.h : "Symbology" tab of the Add layer dialogs - symbol selector (simple / unique values / ranges)
// and the symbol of every value / range in the symbol editor
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include "MapProject.h"
#include "SymbolEditorPanel.h"

class CSymbologyPage : public CDialogImpl<CSymbologyPage>
{
public:
	enum { IDD = IDD_PAGE_SYMBOLOGY, MaxUniqueValues = 500, DefaultClasses = 5 };

	// queries of the field values of the layer table, may throw
	typedef std::function<std::vector<CommonLib::CVariant>(const std::string& sField, bool& bTruncated)> TUniqueQuery;
	typedef std::function<bool(const std::string& sField, double& dMin, double& dMax)> TRangeQuery;

	CSymbologyPage();

	// table of the layer, nullptr - no table yet (the page is disabled); baseColor - color of the default symbol
	void SetSource(const TestMapDraw::STableInfo* pTable, const GraphEngine::Display::Color& baseColor, TUniqueQuery uniqueQuery, TRangeQuery rangeQuery);
	// false - the symbology isn't complete or a symbol has a wrong value (the message is shown)
	bool GetSymbology(TestMapDraw::SSymbology& symbology);
	bool HasSource() const { return m_bHasSource; }

	BEGIN_MSG_MAP(CSymbologyPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_SELECTOR_TYPE, CBN_SELCHANGE, OnSelectorChanged)
		COMMAND_HANDLER(IDC_SYMB_FIELD, CBN_SELCHANGE, OnFieldChanged)
		COMMAND_ID_HANDLER(IDC_ADD_VALUES, OnAddValues)
		COMMAND_ID_HANDLER(IDC_CLASSIFY, OnClassify)
		COMMAND_ID_HANDLER(IDC_REMOVE_ITEM, OnRemoveItem)
		COMMAND_HANDLER(IDC_DRAW_OTHER, BN_CLICKED, OnDrawOther)
		NOTIFY_HANDLER(IDC_ITEMS, LVN_ITEMCHANGED, OnItemChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSelectorChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFieldChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAddValues(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClassify(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRemoveItem(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDrawOther(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

private:
	enum { OtherRow = 0 };   // list row 0 - other values, row i + 1 - value / range i

	void UpdateControls();
	void FillFields();
	void FillList(int nSelectRow);
	std::wstring ItemText(int nRow) const;
	TestMapDraw::SSymbolParams* RowSymbol(int nRow);   // nRow -1 - the simple symbol
	void EditRow(int nRow);
	void OnEditorChanged();
	int  ItemCount() const;
	std::string SelectedField() const;
	TestMapDraw::SSymbolParams BaseSymbol() const;   // symbol kind / properties for the new values / ranges

private:
	CComboBox     m_comboSelector;
	CComboBox     m_comboField;
	CListViewCtrl m_list;
	CButton       m_checkDrawOther;
	CEdit         m_editClasses;
	CSymbolEditorPanel m_editor;

	TestMapDraw::SSymbology   m_symbology;
	std::vector<TestMapDraw::SFieldInfo> m_vecFields;
	TestMapDraw::eGeometryKind m_geometry;
	GraphEngine::Display::Color m_baseColor;
	TUniqueQuery m_uniqueQuery;
	TRangeQuery  m_rangeQuery;
	bool m_bHasSource;
	bool m_bUpdating;
	int  m_nEditRow;   // row of the symbol in the editor, -1 - the simple symbol
};
