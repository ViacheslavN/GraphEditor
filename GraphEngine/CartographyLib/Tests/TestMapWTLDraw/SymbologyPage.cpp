// SymbologyPage.cpp : implementation of the CSymbologyPage class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "SymbologyPage.h"
#include "DialogUtils.h"

#include <algorithm>

using namespace TestMapDraw;
using namespace DialogUtils;
using GraphEngine::Display::Color;

namespace
{
	const wchar_t* SelectorNames[] = {L"Simple (one symbol)", L"Unique values", L"Ranges"};
}

CSymbologyPage::CSymbologyPage() : m_geometry(GeometryPolygon), m_baseColor(255, 204, 0), m_bHasSource(false), m_bUpdating(false), m_nEditRow(-1)
{
	m_symbology.simpleSymbol = CSymbolFactory::CreateDefault(m_geometry, m_baseColor);
	m_symbology.otherSymbol = m_symbology.simpleSymbol;
}

LRESULT CSymbologyPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_comboSelector = GetDlgItem(IDC_SELECTOR_TYPE);
	m_comboField = GetDlgItem(IDC_SYMB_FIELD);
	m_list = GetDlgItem(IDC_ITEMS);
	m_checkDrawOther = GetDlgItem(IDC_DRAW_OTHER);
	m_editClasses = GetDlgItem(IDC_CLASSES);

	for(int i = 0; i < 3; ++i)
		m_comboSelector.AddString(SelectorNames[i]);
	m_comboSelector.SetCurSel(0);
	m_editClasses.SetWindowText(std::to_wstring((int)DefaultClasses).c_str());
	m_checkDrawOther.SetCheck(BST_CHECKED);

	m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
	RECT rcList = {0};
	m_list.GetClientRect(&rcList);
	int nListWidth = rcList.right - rcList.left;
	int nValueWidth = nListWidth * 11 / 20;
	m_list.InsertColumn(0, L"Value", LVCFMT_LEFT, nValueWidth);
	m_list.InsertColumn(1, L"Symbol", LVCFMT_LEFT, nListWidth - nValueWidth - ::GetSystemMetrics(SM_CXVSCROLL));

	// the editor is put into the "Symbol" group box
	m_editor.Create(m_hWnd);
	RECT rcGroup = {0};
	::GetWindowRect(GetDlgItem(IDC_SYM_GROUP), &rcGroup);
	ScreenToClient(&rcGroup);
	RECT rcMargin = {6, 12, 0, 0};   // dialog units
	MapDialogRect(&rcMargin);
	m_editor.SetWindowPos(NULL, rcGroup.left + rcMargin.left, rcGroup.top + rcMargin.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	m_editor.SetOnChanged([this]() { OnEditorChanged(); });
	m_editor.ShowWindow(SW_SHOW);

	UpdateControls();
	return FALSE;   // child page: don't take the focus from the dialog
}

void CSymbologyPage::SetSource(const STableInfo* pTable, const Color& baseColor, TUniqueQuery uniqueQuery, TRangeQuery rangeQuery)
{
	m_bHasSource = pTable != nullptr;
	m_uniqueQuery = uniqueQuery;
	m_rangeQuery = rangeQuery;
	m_baseColor = baseColor;

	eGeometryKind geometry = pTable ? GeometryKindOf(pTable->shapeType) : GeometryPolygon;
	m_vecFields = pTable ? pTable->vecFields : std::vector<SFieldInfo>();

	// a new table: the symbols are made again for its geometry, the values belong to the old table
	m_geometry = geometry;
	m_symbology.simpleSymbol = CSymbolFactory::CreateDefault(m_geometry, m_baseColor);
	m_symbology.otherSymbol = CSymbolFactory::CreateDefault(m_geometry, Color(190, 190, 190));
	m_symbology.vecValues.clear();
	m_symbology.vecRanges.clear();
	m_symbology.sField.clear();

	if(!IsWindow())
		return;

	m_editor.SetGeometry(m_geometry);
	FillFields();
	FillList(OtherRow);
	UpdateControls();
}

int CSymbologyPage::ItemCount() const
{
	if(m_symbology.selector == SelectorUniqueValues)
		return (int)m_symbology.vecValues.size();
	if(m_symbology.selector == SelectorRanges)
		return (int)m_symbology.vecRanges.size();
	return 0;
}

std::string CSymbologyPage::SelectedField() const
{
	int nSel = m_comboField.GetCurSel();
	if(nSel < 0)
		return std::string();
	size_t nField = (size_t)m_comboField.GetItemData(nSel);
	return nField < m_vecFields.size() ? m_vecFields[nField].sName : std::string();
}

void CSymbologyPage::FillFields()
{
	m_bUpdating = true;
	m_comboField.ResetContent();
	for(size_t i = 0; i < m_vecFields.size(); ++i)
	{
		// ranges need numbers
		if(m_symbology.selector == SelectorRanges && !m_vecFields[i].bNumeric)
			continue;

		int nItem = m_comboField.AddString(Utf8ToWide(m_vecFields[i].sName).c_str());
		m_comboField.SetItemData(nItem, (DWORD_PTR)i);
		if(m_vecFields[i].sName == m_symbology.sField)
			m_comboField.SetCurSel(nItem);
	}

	if(m_comboField.GetCurSel() < 0)
	{
		m_symbology.sField.clear();
		m_symbology.vecValues.clear();
		m_symbology.vecRanges.clear();
	}
	m_bUpdating = false;
}

std::wstring CSymbologyPage::ItemText(int nRow) const
{
	if(nRow == OtherRow)
		return L"<other values>";

	int nItem = nRow - 1;
	if(m_symbology.selector == SelectorUniqueValues)
		return Utf8ToWide(m_symbology.vecValues[nItem].sLabel);
	return Utf8ToWide(m_symbology.vecRanges[nItem].sLabel);
}

SSymbolParams* CSymbologyPage::RowSymbol(int nRow)
{
	if(nRow < 0 || m_symbology.selector == SelectorSimple)
		return &m_symbology.simpleSymbol;
	if(nRow == OtherRow)
		return &m_symbology.otherSymbol;

	int nItem = nRow - 1;
	if(m_symbology.selector == SelectorUniqueValues && nItem < (int)m_symbology.vecValues.size())
		return &m_symbology.vecValues[nItem].symbol;
	if(m_symbology.selector == SelectorRanges && nItem < (int)m_symbology.vecRanges.size())
		return &m_symbology.vecRanges[nItem].symbol;
	return &m_symbology.otherSymbol;
}

void CSymbologyPage::FillList(int nSelectRow)
{
	m_bUpdating = true;
	m_list.DeleteAllItems();
	if(m_symbology.selector != SelectorSimple)
	{
		int nRows = ItemCount() + 1;
		for(int nRow = 0; nRow < nRows; ++nRow)
		{
			m_list.InsertItem(nRow, ItemText(nRow).c_str());
			m_list.SetItemText(nRow, 1, Utf8ToWide(CSymbolFactory::Describe(*RowSymbol(nRow))).c_str());
		}

		if(nSelectRow < 0 || nSelectRow >= nRows)
			nSelectRow = OtherRow;
		m_list.SelectItem(nSelectRow);
	}
	m_bUpdating = false;

	EditRow(m_symbology.selector == SelectorSimple ? -1 : nSelectRow);
}

void CSymbologyPage::EditRow(int nRow)
{
	m_nEditRow = nRow;
	m_editor.SetParams(*RowSymbol(nRow));
}

void CSymbologyPage::OnEditorChanged()
{
	if(m_bUpdating)
		return;

	*RowSymbol(m_nEditRow) = m_editor.GetParams();
	if(m_nEditRow >= 0 && m_nEditRow < m_list.GetItemCount())
		m_list.SetItemText(m_nEditRow, 1, Utf8ToWide(CSymbolFactory::Describe(m_editor.GetParams())).c_str());
}

void CSymbologyPage::UpdateControls()
{
	bool bSimple = m_symbology.selector == SelectorSimple;
	bool bUnique = m_symbology.selector == SelectorUniqueValues;
	bool bRanges = m_symbology.selector == SelectorRanges;
	bool bField = m_bHasSource && !SelectedField().empty();

	m_comboSelector.EnableWindow(m_bHasSource);
	::ShowWindow(GetDlgItem(IDC_SYMB_FIELD_LABEL), bSimple ? SW_HIDE : SW_SHOW);
	m_comboField.ShowWindow(bSimple ? SW_HIDE : SW_SHOW);
	m_comboField.EnableWindow(m_bHasSource);

	::ShowWindow(GetDlgItem(IDC_ADD_VALUES), bUnique ? SW_SHOW : SW_HIDE);
	::EnableWindow(GetDlgItem(IDC_ADD_VALUES), bField);
	::ShowWindow(GetDlgItem(IDC_CLASSES_LABEL), bRanges ? SW_SHOW : SW_HIDE);
	m_editClasses.ShowWindow(bRanges ? SW_SHOW : SW_HIDE);
	::ShowWindow(GetDlgItem(IDC_CLASSIFY), bRanges ? SW_SHOW : SW_HIDE);
	::EnableWindow(GetDlgItem(IDC_CLASSIFY), bField);
	::ShowWindow(GetDlgItem(IDC_REMOVE_ITEM), bSimple ? SW_HIDE : SW_SHOW);
	::EnableWindow(GetDlgItem(IDC_REMOVE_ITEM), m_bHasSource && ItemCount() > 0);
	m_checkDrawOther.ShowWindow(bSimple ? SW_HIDE : SW_SHOW);
	m_checkDrawOther.EnableWindow(m_bHasSource);
	m_list.ShowWindow(bSimple ? SW_HIDE : SW_SHOW);

	m_editor.Enable(m_bHasSource);
}

TestMapDraw::SSymbolParams CSymbologyPage::BaseSymbol() const
{
	// the symbol in the editor: choose its type and properties first, then make the values / ranges
	return m_editor.GetParams();
}

LRESULT CSymbologyPage::OnSelectorChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(m_bUpdating)
		return 0;

	m_editor.Commit();
	int nSel = m_comboSelector.GetCurSel();
	eSelectorKind selector = (eSelectorKind)(nSel < 0 ? 0 : nSel);
	if(selector == m_symbology.selector)
		return 0;

	m_symbology.selector = selector;
	FillFields();   // ranges show the numeric fields only
	FillList(OtherRow);
	UpdateControls();
	return 0;
}

LRESULT CSymbologyPage::OnFieldChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(m_bUpdating)
		return 0;

	std::string sField = SelectedField();
	if(sField == m_symbology.sField)
		return 0;

	// the values of the old field don't fit
	m_symbology.sField = sField;
	m_symbology.vecValues.clear();
	m_symbology.vecRanges.clear();
	FillList(OtherRow);
	UpdateControls();
	return 0;
}

LRESULT CSymbologyPage::OnAddValues(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::string sField = SelectedField();
	if(sField.empty() || !m_uniqueQuery || !m_editor.Commit())
		return 0;

	try
	{
		CWaitCursorGuard waitCursor;
		bool bTruncated = false;
		std::vector<CommonLib::CVariant> vecValues = m_uniqueQuery(sField, bTruncated);
		m_symbology.vecValues = CSymbologyBuilder::MakeUniqueItems(vecValues, BaseSymbol());
		FillList(m_symbology.vecValues.empty() ? OtherRow : 1);
		UpdateControls();

		if(bTruncated)
		{
			std::wstring sMsg = L"The field has more than " + std::to_wstring((int)MaxUniqueValues) + L" values, only the first of them are added.";
			MessageBox(sMsg.c_str(), L"Unique values", MB_OK | MB_ICONINFORMATION);
		}
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"Unique values", MB_OK | MB_ICONWARNING);
	}
	return 0;
}

LRESULT CSymbologyPage::OnClassify(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::string sField = SelectedField();
	if(sField.empty() || !m_rangeQuery || !m_editor.Commit())
		return 0;

	int nClasses = _wtoi(GetText(m_editClasses).c_str());
	if(nClasses < 1 || nClasses > 100)
	{
		MessageBox(L"Enter the number of classes from 1 to 100.", L"Ranges", MB_OK | MB_ICONWARNING);
		m_editClasses.SetFocus();
		return 0;
	}

	try
	{
		CWaitCursorGuard waitCursor;
		double dMin = 0., dMax = 0.;
		if(!m_rangeQuery(sField, dMin, dMax))
		{
			MessageBox(L"The field has no numeric values.", L"Ranges", MB_OK | MB_ICONINFORMATION);
			return 0;
		}

		m_symbology.vecRanges = CSymbologyBuilder::MakeRanges(dMin, dMax, nClasses, BaseSymbol());
		FillList(1);
		UpdateControls();
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"Ranges", MB_OK | MB_ICONWARNING);
	}
	return 0;
}

LRESULT CSymbologyPage::OnRemoveItem(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nRow = m_list.GetSelectedIndex();
	if(nRow <= OtherRow)
		return 0;   // the other values row stays

	int nItem = nRow - 1;
	if(m_symbology.selector == SelectorUniqueValues && nItem < (int)m_symbology.vecValues.size())
		m_symbology.vecValues.erase(m_symbology.vecValues.begin() + nItem);
	else if(m_symbology.selector == SelectorRanges && nItem < (int)m_symbology.vecRanges.size())
		m_symbology.vecRanges.erase(m_symbology.vecRanges.begin() + nItem);

	FillList((std::min)(nRow, ItemCount()));
	UpdateControls();
	return 0;
}

LRESULT CSymbologyPage::OnDrawOther(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_symbology.bDrawOther = m_checkDrawOther.GetCheck() == BST_CHECKED;
	return 0;
}

LRESULT CSymbologyPage::OnItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if(m_bUpdating)
		return 0;

	LPNMLISTVIEW pListView = (LPNMLISTVIEW)pnmh;
	if(!(pListView->uChanged & LVIF_STATE) || !(pListView->uNewState & LVIS_SELECTED) || (pListView->uOldState & LVIS_SELECTED))
		return 0;

	// the editor keeps the edits of the previous row (EN_KILLFOCUS may come after the selection)
	m_editor.Commit();
	EditRow(pListView->iItem);
	return 0;
}

bool CSymbologyPage::GetSymbology(SSymbology& symbology)
{
	if(!m_bHasSource)
		return false;

	if(!m_editor.Commit())
		return false;

	if(m_symbology.selector != SelectorSimple && m_symbology.sField.empty())
	{
		MessageBox(L"Select the field of the unique values / ranges.", L"Symbology", MB_OK | MB_ICONWARNING);
		m_comboField.SetFocus();
		return false;
	}

	m_symbology.bDrawOther = m_checkDrawOther.GetCheck() == BST_CHECKED;
	symbology = m_symbology;
	return true;
}
