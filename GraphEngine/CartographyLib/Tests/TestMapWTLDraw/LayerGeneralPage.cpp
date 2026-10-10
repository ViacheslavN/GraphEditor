// LayerGeneralPage.cpp : implementation of the CLayerGeneralPage class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "LayerGeneralPage.h"
#include "DialogUtils.h"

using namespace DialogUtils;

CLayerGeneralPage::CLayerGeneralPage() : m_bHasSelectable(false), m_dCurrentScale(0.)
{

}

void CLayerGeneralPage::SetValues(const SValues& values, bool bHasSelectable, const std::wstring& sSource, double dCurrentScale)
{
	m_values = values;
	m_bHasSelectable = bHasSelectable;
	m_sSource = sSource;
	m_dCurrentScale = dCurrentScale;
}

LRESULT CLayerGeneralPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_LAYER_NAME, Utf8ToWide(m_values.sName).c_str());

	// the multiline edit needs CR LF
	std::wstring sSource;
	for(size_t i = 0; i < m_sSource.size(); ++i)
	{
		if(m_sSource[i] == L'\n' && (i == 0 || m_sSource[i - 1] != L'\r'))
			sSource += L'\r';
		sSource += m_sSource[i];
	}
	SetDlgItemText(IDC_LAYER_SOURCE, sSource.c_str());

	CheckDlgButton(IDC_LAYER_VISIBLE, m_values.bVisible ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_LAYER_SELECTABLE, m_values.bSelectable ? BST_CHECKED : BST_UNCHECKED);
	::ShowWindow(GetDlgItem(IDC_LAYER_SELECTABLE), m_bHasSelectable ? SW_SHOW : SW_HIDE);

	SetDlgItemText(IDC_LAYER_MIN_SCALE, FormatScale(m_values.dMinimumScale).c_str());
	SetDlgItemText(IDC_LAYER_MAX_SCALE, FormatScale(m_values.dMaximumScale).c_str());

	bool bScale = m_dCurrentScale > 0.;
	::EnableWindow(GetDlgItem(IDC_MIN_SCALE_CURRENT), bScale);
	::EnableWindow(GetDlgItem(IDC_MAX_SCALE_CURRENT), bScale);
	std::wstring sCurrent = bScale ? L"Current map scale 1:" + FormatScale(m_dCurrentScale) : std::wstring(L"The map has no scale yet");
	SetDlgItemText(IDC_CURRENT_SCALE, sCurrent.c_str());

	bool bSymbols = m_values.nScaleDependent >= 0;
	::ShowWindow(GetDlgItem(IDC_LAYER_SCALE_SYMBOLS), bSymbols ? SW_SHOW : SW_HIDE);
	::ShowWindow(GetDlgItem(IDC_LAYER_REF_SCALE_NOTE), bSymbols ? SW_SHOW : SW_HIDE);
	if(bSymbols)
	{
		// mixed: some symbols are scaled - the third state keeps them as they are
		CheckDlgButton(IDC_LAYER_SCALE_SYMBOLS, m_values.nScaleDependent == TestMapDraw::CSymbolFactory::ScaleDependentYes ? BST_CHECKED :
		               (m_values.nScaleDependent == TestMapDraw::CSymbolFactory::ScaleDependentMixed ? BST_INDETERMINATE : BST_UNCHECKED));
		UpdateScaleNote();
	}
	return FALSE;   // child page: don't take the focus from the dialog
}

LRESULT CLayerGeneralPage::OnCurrentScale(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nEdit = wID == IDC_MIN_SCALE_CURRENT ? IDC_LAYER_MIN_SCALE : IDC_LAYER_MAX_SCALE;
	SetDlgItemText(nEdit, FormatScale(m_dCurrentScale).c_str());
	return 0;
}

LRESULT CLayerGeneralPage::OnScaleSymbols(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// the third state only when the layer has it (mixed), otherwise checked <-> unchecked
	if(m_values.nScaleDependent != TestMapDraw::CSymbolFactory::ScaleDependentMixed && IsDlgButtonChecked(IDC_LAYER_SCALE_SYMBOLS) == BST_INDETERMINATE)
		CheckDlgButton(IDC_LAYER_SCALE_SYMBOLS, BST_UNCHECKED);
	UpdateScaleNote();
	return 0;
}

void CLayerGeneralPage::UpdateScaleNote()
{
	std::wstring sNote;
	UINT nState = IsDlgButtonChecked(IDC_LAYER_SCALE_SYMBOLS);
	if(nState == BST_INDETERMINATE)
		sNote = L"Some symbols are scaled and some aren't, they are kept as they are. ";
	if(m_values.dMapReferenceScale > 0.)
		sNote += L"Scaled symbols have their size at the map reference scale 1:" + FormatScale(m_values.dMapReferenceScale) +
		         L" and grow when zoomed in.";
	else
		sNote += L"The map has no reference scale, the symbols keep their size on all the scales. "
		         L"Set it in the map properties.";
	SetDlgItemText(IDC_LAYER_REF_SCALE_NOTE, sNote.c_str());
}

bool CLayerGeneralPage::Error(const wchar_t* pszMessage, HWND hFocus)
{
	MessageBox(pszMessage, L"Layer Properties", MB_OK | MB_ICONWARNING);
	::SetFocus(hFocus);
	return false;
}

bool CLayerGeneralPage::GetValues(SValues& values)
{
	values = m_values;
	values.sName = WideToUtf8(Trim(GetText(GetDlgItem(IDC_LAYER_NAME)), L" \t"));
	values.bVisible = IsDlgButtonChecked(IDC_LAYER_VISIBLE) == BST_CHECKED;
	values.bSelectable = IsDlgButtonChecked(IDC_LAYER_SELECTABLE) == BST_CHECKED;

	double dMin = ParseScaleText(GetText(GetDlgItem(IDC_LAYER_MIN_SCALE)));
	if(dMin < 0.)
		return Error(L"Enter the scale as a number, e.g. 500000 (or leave it empty for no limit).", GetDlgItem(IDC_LAYER_MIN_SCALE));
	double dMax = ParseScaleText(GetText(GetDlgItem(IDC_LAYER_MAX_SCALE)));
	if(dMax < 0.)
		return Error(L"Enter the scale as a number, e.g. 1000 (or leave it empty for no limit).", GetDlgItem(IDC_LAYER_MAX_SCALE));
	if(dMin > 0. && dMax > 0. && dMax >= dMin)
		return Error(L"The \"zoomed in\" scale must be larger (a smaller number) than the \"zoomed out\" scale, "
		             L"e.g. hide when zoomed out beyond 1:500000 and when zoomed in beyond 1:1000.", GetDlgItem(IDC_LAYER_MAX_SCALE));

	values.dMinimumScale = dMin;
	values.dMaximumScale = dMax;

	if(m_values.nScaleDependent >= 0)
	{
		UINT nState = IsDlgButtonChecked(IDC_LAYER_SCALE_SYMBOLS);
		values.nScaleDependent = nState == BST_CHECKED ? TestMapDraw::CSymbolFactory::ScaleDependentYes :
		                         (nState == BST_INDETERMINATE ? TestMapDraw::CSymbolFactory::ScaleDependentMixed : TestMapDraw::CSymbolFactory::ScaleDependentNo);
	}
	return true;
}
