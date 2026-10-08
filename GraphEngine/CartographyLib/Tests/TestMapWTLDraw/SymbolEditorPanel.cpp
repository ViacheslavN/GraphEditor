// SymbolEditorPanel.cpp : implementation of the CSymbolEditorPanel class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "SymbolEditorPanel.h"
#include "DialogUtils.h"

using namespace TestMapDraw;
using namespace DialogUtils;

CSymbolEditorPanel::CSymbolEditorPanel() : m_geometry(GeometryPolygon), m_bUpdating(false), m_bInMessage(false), m_bEnabled(true)
{
	m_params = CSymbolFactory::CreateDefault(m_geometry, GraphEngine::Display::Color(255, 204, 0));
}

LRESULT CSymbolEditorPanel::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_comboType = GetDlgItem(IDC_SYM_TYPE);
	for(int i = 0; i < RowCount; ++i)
	{
		m_labels[i] = GetDlgItem(IDC_PROP_LABEL0 + i);
		m_edits[i] = GetDlgItem(IDC_PROP_EDIT0 + i);
		m_combos[i] = GetDlgItem(IDC_PROP_COMBO0 + i);
		m_buttons[i] = GetDlgItem(IDC_PROP_BTN0 + i);
	}

	FillTypes();
	ShowProperties();
	return FALSE;   // child panel: don't take the focus from the dialog
}

void CSymbolEditorPanel::SetGeometry(eGeometryKind geometry)
{
	m_geometry = geometry;
	if(CSymbolFactory::GetGeometryKind(m_params.kind) != geometry)
		m_params = CSymbolFactory::CreateDefault(geometry, m_params.color);

	if(IsWindow())
	{
		FillTypes();
		ShowProperties();
	}
}

void CSymbolEditorPanel::SetParams(const SSymbolParams& params)
{
	m_params = params;
	if(IsWindow())
	{
		FillTypes();
		ShowProperties();
	}
}

void CSymbolEditorPanel::Enable(bool bEnable)
{
	m_bEnabled = bEnable;
	if(IsWindow())
		ShowProperties();
}

void CSymbolEditorPanel::FillTypes()
{
	m_bUpdating = true;
	m_comboType.ResetContent();
	std::vector<eSymbolKind> vecKinds = CSymbolFactory::GetSymbolKinds(m_geometry);
	for(size_t i = 0; i < vecKinds.size(); ++i)
	{
		int nItem = m_comboType.AddString(Utf8ToWide(CSymbolFactory::GetSymbolKindName(vecKinds[i])).c_str());
		m_comboType.SetItemData(nItem, (DWORD_PTR)vecKinds[i]);
		if(vecKinds[i] == m_params.kind)
			m_comboType.SetCurSel(nItem);
	}
	m_bUpdating = false;
}

void CSymbolEditorPanel::ShowProperties()
{
	m_bUpdating = true;
	m_vecProps = CSymbolFactory::GetProperties(m_params.kind);
	m_comboType.EnableWindow(m_bEnabled);

	for(int i = 0; i < RowCount; ++i)
	{
		bool bVisible = i < (int)m_vecProps.size();
		const SPropertyInfo* pProp = bVisible ? &m_vecProps[i] : nullptr;
		bool bChoice = bVisible && pProp->kind == PropertyChoice;
		bool bButton = bVisible && (pProp->kind == PropertyColor || pProp->kind == PropertyFile);

		m_labels[i].ShowWindow(bVisible ? SW_SHOW : SW_HIDE);
		m_edits[i].ShowWindow(bVisible && !bChoice ? SW_SHOW : SW_HIDE);
		m_combos[i].ShowWindow(bChoice ? SW_SHOW : SW_HIDE);
		m_buttons[i].ShowWindow(bButton ? SW_SHOW : SW_HIDE);
		m_edits[i].EnableWindow(m_bEnabled);
		m_combos[i].EnableWindow(m_bEnabled);
		m_buttons[i].EnableWindow(m_bEnabled);
		if(!bVisible)
			continue;

		m_labels[i].SetWindowText(Utf8ToWide(pProp->sLabel + ":").c_str());
		std::string sValue = CSymbolFactory::GetPropertyText(m_params, pProp->id);
		if(bChoice)
		{
			m_combos[i].ResetContent();
			for(size_t c = 0; c < pProp->vecChoices.size(); ++c)
				m_combos[i].AddString(Utf8ToWide(pProp->vecChoices[c]).c_str());
			int nSel = atoi(sValue.c_str());
			m_combos[i].SetCurSel(nSel >= 0 && nSel < (int)pProp->vecChoices.size() ? nSel : 0);
		}
		else
			m_edits[i].SetWindowText(Utf8ToWide(sValue).c_str());
	}
	m_bUpdating = false;
}

bool CSymbolEditorPanel::CommitRow(int nRow, bool bShowError)
{
	if(nRow < 0 || nRow >= (int)m_vecProps.size())
		return true;

	const SPropertyInfo& prop = m_vecProps[nRow];
	std::string sOld = CSymbolFactory::GetPropertyText(m_params, prop.id);
	std::string sNew;
	if(prop.kind == PropertyChoice)
		sNew = std::to_string(m_combos[nRow].GetCurSel() < 0 ? 0 : m_combos[nRow].GetCurSel());
	else
		sNew = WideToUtf8(prop.kind == PropertyFile || prop.kind == PropertyText ? Trim(GetText(m_edits[nRow])) : Trim(GetText(m_edits[nRow]), L" \t"));

	if(sNew == sOld)
		return true;

	try
	{
		CSymbolFactory::SetPropertyText(m_params, prop.id, sNew);
	}
	catch (std::exception& exc)
	{
		if(bShowError && !m_bInMessage)
		{
			m_bInMessage = true;
			std::wstring sMsg = Utf8ToWide(prop.sLabel) + L": " + ExceptionText(exc);
			MessageBox(sMsg.c_str(), L"Symbol", MB_OK | MB_ICONWARNING);
			m_bInMessage = false;
		}
		m_bUpdating = true;
		m_edits[nRow].SetWindowText(Utf8ToWide(sOld).c_str());
		m_bUpdating = false;
		return false;
	}

	NotifyChanged();
	return true;
}

bool CSymbolEditorPanel::Commit()
{
	if(!IsWindow())
		return true;

	for(int i = 0; i < (int)m_vecProps.size(); ++i)
	{
		if(!CommitRow(i, true))
		{
			if(m_vecProps[i].kind == PropertyChoice)
				m_combos[i].SetFocus();
			else
				m_edits[i].SetFocus();
			return false;
		}
	}
	return true;
}

void CSymbolEditorPanel::NotifyChanged()
{
	if(m_onChanged)
		m_onChanged();
}

LRESULT CSymbolEditorPanel::OnTypeChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(m_bUpdating)
		return 0;

	int nSel = m_comboType.GetCurSel();
	if(nSel < 0)
		return 0;

	eSymbolKind kind = (eSymbolKind)m_comboType.GetItemData(nSel);
	m_params = CSymbolFactory::ChangeKind(m_params, kind);
	ShowProperties();
	NotifyChanged();
	return 0;
}

LRESULT CSymbolEditorPanel::OnEditKillFocus(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_bUpdating && !m_bInMessage)
		CommitRow(wID - IDC_PROP_EDIT0, true);
	return 0;
}

LRESULT CSymbolEditorPanel::OnComboChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_bUpdating)
		CommitRow(wID - IDC_PROP_COMBO0, true);
	return 0;
}

LRESULT CSymbolEditorPanel::OnButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nRow = wID - IDC_PROP_BTN0;
	if(nRow < 0 || nRow >= (int)m_vecProps.size())
		return 0;

	const SPropertyInfo& prop = m_vecProps[nRow];
	if(prop.kind == PropertyColor)
	{
		COLORREF rgbInit = RGB(0, 0, 0);
		try
		{
			GraphEngine::Display::Color color = CSymbolFactory::TextToColor(WideToUtf8(GetText(m_edits[nRow])));
			if(color.GetA() != GraphEngine::Display::Color::Transparent)
				rgbInit = RGB(color.GetR(), color.GetG(), color.GetB());
		}
		catch (std::exception&)
		{
		}

		CColorDialog dlg(rgbInit, CC_FULLOPEN, m_hWnd);
		if(dlg.DoModal() != IDOK)
			return 0;

		COLORREF rgb = dlg.GetColor();
		GraphEngine::Display::Color color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
		m_edits[nRow].SetWindowText(Utf8ToWide(CSymbolFactory::ColorToText(color)).c_str());
		CommitRow(nRow, true);
	}
	else if(prop.kind == PropertyFile)
	{
		std::wstring sCurrent = Trim(GetText(m_edits[nRow]));
		CFileDialog dlg(TRUE, NULL, sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
			_T("Images (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0"), m_hWnd);
		if(dlg.DoModal() != IDOK)
			return 0;

		m_edits[nRow].SetWindowText(dlg.m_szFileName);
		CommitRow(nRow, true);
	}
	return 0;
}
