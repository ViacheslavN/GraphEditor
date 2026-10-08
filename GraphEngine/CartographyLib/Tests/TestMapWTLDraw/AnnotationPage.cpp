// AnnotationPage.cpp : implementation of the CAnnotationPage class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "AnnotationPage.h"
#include "DialogUtils.h"

#include <algorithm>
#include <cwchar>

using namespace DialogUtils;

namespace
{
	// scale denominator from the edit, 0 - empty (all scales), -1 - invalid
	double ParseScale(const std::wstring& sText)
	{
		std::wstring s = Trim(sText, L" \t");
		if(s.size() > 2 && s.compare(0, 2, L"1:") == 0)
			s = s.substr(2);
		s.erase(std::remove(s.begin(), s.end(), L' '), s.end());
		if(s.empty())
			return 0.;

		wchar_t* pszEnd = nullptr;
		double dScale = wcstod(s.c_str(), &pszEnd);
		if(pszEnd == s.c_str() || *pszEnd != 0 || dScale < 0.)
			return -1.;
		return dScale;
	}
}

CAnnotationPage::CAnnotationPage() : m_bHasFields(false), m_dDefaultScale(0.)
{

}

LRESULT CAnnotationPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_checkAnno = GetDlgItem(IDC_ENABLE_ANNO);
	m_comboField = GetDlgItem(IDC_ANNO_FIELD);
	m_editScale = GetDlgItem(IDC_ANNO_SCALE);

	m_checkAnno.SetCheck(BST_UNCHECKED);
	if(m_dDefaultScale > 0.)
	{
		wchar_t szScale[64];
		swprintf(szScale, 64, L"%.0f", m_dDefaultScale);
		m_editScale.SetWindowText(szScale);
	}

	FillFields();
	UpdateControls();
	return FALSE;   // child page: don't take the focus from the dialog
}

void CAnnotationPage::SetFields(const std::vector<TestMapDraw::SFieldInfo>* pFields)
{
	m_bHasFields = pFields && !pFields->empty();
	m_vecFields = pFields ? *pFields : std::vector<TestMapDraw::SFieldInfo>();
	if(IsWindow())
	{
		FillFields();
		UpdateControls();
	}
}

void CAnnotationPage::FillFields()
{
	m_comboField.ResetContent();
	int nSelect = -1;
	for(size_t i = 0; i < m_vecFields.size(); ++i)
	{
		m_comboField.AddString(Utf8ToWide(m_vecFields[i].sName).c_str());
		if(nSelect < 0 && m_vecFields[i].bText)
			nSelect = (int)i;   // the first text field by default
	}

	if(nSelect < 0 && !m_vecFields.empty())
		nSelect = 0;
	m_comboField.SetCurSel(nSelect);
}

void CAnnotationPage::UpdateControls()
{
	bool bEnabled = m_bHasFields && m_checkAnno.GetCheck() == BST_CHECKED;
	m_checkAnno.EnableWindow(m_bHasFields);
	m_comboField.EnableWindow(bEnabled);
	m_editScale.EnableWindow(bEnabled);
}

LRESULT CAnnotationPage::OnEnableAnno(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

bool CAnnotationPage::GetAnnotation(TestMapDraw::SAnnotationParams& anno)
{
	anno = TestMapDraw::SAnnotationParams();
	if(!m_bHasFields || m_checkAnno.GetCheck() != BST_CHECKED)
		return true;

	int nSel = m_comboField.GetCurSel();
	if(nSel < 0 || nSel >= (int)m_vecFields.size())
	{
		MessageBox(L"Select the annotation field.", L"Annotation", MB_OK | MB_ICONWARNING);
		m_comboField.SetFocus();
		return false;
	}

	double dScale = ParseScale(GetText(m_editScale));
	if(dScale < 0.)
	{
		MessageBox(L"Enter the scale as a number, e.g. 50000 (or leave it empty for all scales).", L"Annotation", MB_OK | MB_ICONWARNING);
		m_editScale.SetFocus();
		return false;
	}

	anno.sField = m_vecFields[nSel].sName;
	anno.dMinimumScale = dScale;
	return true;
}
