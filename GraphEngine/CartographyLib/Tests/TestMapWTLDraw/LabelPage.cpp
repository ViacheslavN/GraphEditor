// LabelPage.cpp : implementation of the CLabelPage class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "LabelPage.h"
#include "DialogUtils.h"

#include <algorithm>
#include <cwchar>

using namespace DialogUtils;
using namespace GraphEngine;

namespace
{
	// number from the edit: false - not a number; empty text gives dDefault
	bool ParseNumber(const std::wstring& sText, double dDefault, double& dValue)
	{
		std::wstring s = Trim(sText, L" \t");
		if(s.empty())
		{
			dValue = dDefault;
			return true;
		}
		std::replace(s.begin(), s.end(), L',', L'.');

		wchar_t* pszEnd = nullptr;
		dValue = wcstod(s.c_str(), &pszEnd);
		return pszEnd != s.c_str() && *pszEnd == 0;
	}

	// scale denominator, "1:50000" or "50000", 0 - empty (all scales), -1 - invalid
	double ParseScale(const std::wstring& sText)
	{
		std::wstring s = Trim(sText, L" \t");
		if(s.size() > 2 && s.compare(0, 2, L"1:") == 0)
			s = s.substr(2);
		s.erase(std::remove(s.begin(), s.end(), L' '), s.end());

		double dScale = 0.;
		if(!ParseNumber(s, 0., dScale) || dScale < 0.)
			return -1.;
		return dScale;
	}

	std::wstring FormatNumber(double dValue)
	{
		wchar_t sz[64];
		swprintf(sz, 64, L"%g", dValue);
		return sz;
	}

	// presets of the point label positions (priorities of Cartography::ePointLabelPosition:
	// left top, center top, right top, right center, right bottom, center bottom, left bottom, left center, center)
	struct SPointPreset
	{
		const wchar_t* pszName;
		int priorities[Cartography::PointLabelPositionCount];
	};

	const SPointPreset PointPresets[] =
	{
		{L"Around, right top first", {2, 3, 1, 2, 3, 3, 3, 3, 0}},
		{L"Right top only",          {0, 0, 1, 0, 0, 0, 0, 0, 0}},
		{L"Right / left",            {0, 0, 0, 1, 0, 0, 0, 2, 0}},
		{L"Top / bottom",            {0, 1, 0, 0, 0, 2, 0, 0, 0}},
		{L"On the point",            {0, 0, 0, 0, 0, 0, 0, 0, 1}},
	};
	const int PointPresetCount = sizeof(PointPresets) / sizeof(PointPresets[0]);
}

CLabelPage::CLabelPage() : m_bHasFields(false), m_dDefaultScale(0.)
{

}

LRESULT CLabelPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_checkLabels = GetDlgItem(IDC_ENABLE_LABELS);
	m_comboField = GetDlgItem(IDC_LABEL_FIELD);
	m_editScale = GetDlgItem(IDC_LABEL_SCALE);
	m_editFontSize = GetDlgItem(IDC_LABEL_FONT_SIZE);
	m_editColor = GetDlgItem(IDC_LABEL_COLOR);
	m_btnColor = GetDlgItem(IDC_LABEL_COLOR_BTN);
	m_editHalo = GetDlgItem(IDC_LABEL_HALO);
	m_comboStrategy = GetDlgItem(IDC_LABEL_STRATEGY);
	m_editPriority = GetDlgItem(IDC_LABEL_PRIORITY);
	m_comboPointPosition = GetDlgItem(IDC_LABEL_POINT_POS);
	m_editOffset = GetDlgItem(IDC_LABEL_OFFSET);
	m_comboLineOrientation = GetDlgItem(IDC_LABEL_LINE_ORIENT);
	m_comboLinePosition = GetDlgItem(IDC_LABEL_LINE_POS);
	m_comboPolygonPlacement = GetDlgItem(IDC_LABEL_POLY_PLACEMENT);
	m_checkPolygonOutside = GetDlgItem(IDC_LABEL_POLY_OUTSIDE);
	m_comboDuplicates = GetDlgItem(IDC_LABEL_DUPLICATES);
	m_editDuplicateDistance = GetDlgItem(IDC_LABEL_DUP_DISTANCE);

	// the defaults or the labels of the layer (SetLabels)
	const TestMapDraw::SLabelParams& defaults = m_labels;
	const Cartography::SLabelingOptions& options = defaults.options;

	m_checkLabels.SetCheck(defaults.sField.empty() ? BST_UNCHECKED : BST_CHECKED);
	double dScale = defaults.sField.empty() ? m_dDefaultScale : defaults.dMinimumScale;
	if(dScale > 0.)
	{
		wchar_t szScale[64];
		swprintf(szScale, 64, L"%.0f", dScale);
		m_editScale.SetWindowText(szScale);
	}

	m_editFontSize.SetWindowText(FormatNumber(defaults.dFontSize).c_str());
	m_editColor.SetWindowText(Utf8ToWide(TestMapDraw::CSymbolFactory::ColorToText(defaults.color)).c_str());
	m_editHalo.SetWindowText(FormatNumber(defaults.dHaloSize).c_str());

	// the combo items are in the order of the Cartography enums
	m_comboStrategy.AddString(L"Draw always (no check)");
	m_comboStrategy.AddString(L"First position only");
	m_comboStrategy.AddString(L"Search a free position");
	m_comboStrategy.SetCurSel(options.m_strategy);

	m_editPriority.SetWindowText(FormatNumber(options.m_nPriority).c_str());

	// the preset with the priorities of the layer, other priorities - one more item which keeps them
	int nPreset = -1;
	for(int i = 0; i < PointPresetCount; ++i)
	{
		m_comboPointPosition.AddString(PointPresets[i].pszName);
		if(nPreset < 0 && std::equal(options.m_pointPriorities, options.m_pointPriorities + Cartography::PointLabelPositionCount, PointPresets[i].priorities))
			nPreset = i;
	}
	if(nPreset < 0)
	{
		m_comboPointPosition.AddString(L"Current positions");
		nPreset = PointPresetCount;
	}
	m_comboPointPosition.SetCurSel(nPreset);
	m_editOffset.SetWindowText(FormatNumber(options.m_dOffset).c_str());

	m_comboLineOrientation.AddString(L"Horizontal");
	m_comboLineOrientation.AddString(L"Parallel");
	m_comboLineOrientation.AddString(L"Curved");
	m_comboLineOrientation.AddString(L"Perpendicular");
	m_comboLineOrientation.SetCurSel(options.m_lineOrientation);

	m_comboLinePosition.AddString(L"On the line");
	m_comboLinePosition.AddString(L"Above");
	m_comboLinePosition.AddString(L"Below");
	m_comboLinePosition.AddString(L"Above or below");
	m_comboLinePosition.SetCurSel(options.m_linePosition);

	m_comboPolygonPlacement.AddString(L"Horizontal");
	m_comboPolygonPlacement.AddString(L"Straight (main axis)");
	m_comboPolygonPlacement.AddString(L"Straight, then horizontal");
	m_comboPolygonPlacement.SetCurSel(options.m_polygonPlacement);
	m_checkPolygonOutside.SetCheck(options.m_bPolygonAllowOutside ? BST_CHECKED : BST_UNCHECKED);

	m_comboDuplicates.AddString(L"Allow");
	m_comboDuplicates.AddString(L"Remove (one per text)");
	m_comboDuplicates.AddString(L"Not closer than");
	m_comboDuplicates.SetCurSel(options.m_duplicateStrategy);
	m_editDuplicateDistance.SetWindowText(FormatNumber(options.m_dDuplicateDistance).c_str());

	FillFields();
	UpdateControls();
	return FALSE;   // child page: don't take the focus from the dialog
}

void CLabelPage::SetFields(const std::vector<TestMapDraw::SFieldInfo>* pFields)
{
	m_bHasFields = pFields && !pFields->empty();
	m_vecFields = pFields ? *pFields : std::vector<TestMapDraw::SFieldInfo>();
	if(IsWindow())
	{
		FillFields();
		UpdateControls();
	}
}

void CLabelPage::FillFields()
{
	m_comboField.ResetContent();
	int nSelect = -1;
	for(size_t i = 0; i < m_vecFields.size(); ++i)
	{
		m_comboField.AddString(Utf8ToWide(m_vecFields[i].sName).c_str());
		if(nSelect < 0 && m_vecFields[i].bText)
			nSelect = (int)i;   // the first text field by default
	}
	for(size_t i = 0; i < m_vecFields.size(); ++i)
	{
		if(!m_labels.sField.empty() && m_vecFields[i].sName == m_labels.sField)
			nSelect = (int)i;   // the field of the layer
	}

	if(nSelect < 0 && !m_vecFields.empty())
		nSelect = 0;
	m_comboField.SetCurSel(nSelect);
}

void CLabelPage::UpdateControls()
{
	bool bEnabled = m_bHasFields && m_checkLabels.GetCheck() == BST_CHECKED;
	m_checkLabels.EnableWindow(m_bHasFields);

	HWND controls[] = {m_comboField, m_editScale, m_editFontSize, m_editColor, m_btnColor, m_editHalo, m_comboStrategy, m_editPriority,
	                   m_comboPointPosition, m_editOffset, m_comboLineOrientation, m_comboLinePosition, m_comboPolygonPlacement,
	                   m_checkPolygonOutside, m_comboDuplicates};
	for(size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
		::EnableWindow(controls[i], bEnabled);

	m_editDuplicateDistance.EnableWindow(bEnabled && m_comboDuplicates.GetCurSel() == Cartography::DuplicateStrategyDistance);
}

LRESULT CLabelPage::OnUpdateControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

LRESULT CLabelPage::OnColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	COLORREF rgbInit = RGB(0, 0, 0);
	try
	{
		Display::Color color = TestMapDraw::CSymbolFactory::TextToColor(WideToUtf8(GetText(m_editColor)));
		if(color.GetA() != Display::Color::Transparent)
			rgbInit = RGB(color.GetR(), color.GetG(), color.GetB());
	}
	catch (std::exception&)
	{
	}

	CColorDialog dlg(rgbInit, CC_FULLOPEN, m_hWnd);
	if(dlg.DoModal() != IDOK)
		return 0;

	COLORREF rgb = dlg.GetColor();
	Display::Color color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
	m_editColor.SetWindowText(Utf8ToWide(TestMapDraw::CSymbolFactory::ColorToText(color)).c_str());
	return 0;
}

bool CLabelPage::Error(const wchar_t* pszMessage, HWND hFocus)
{
	MessageBox(pszMessage, L"Labels", MB_OK | MB_ICONWARNING);
	::SetFocus(hFocus);
	return false;
}

bool CLabelPage::GetLabels(TestMapDraw::SLabelParams& labels)
{
	labels = TestMapDraw::SLabelParams();
	if(!m_bHasFields || m_checkLabels.GetCheck() != BST_CHECKED)
		return true;

	int nSel = m_comboField.GetCurSel();
	if(nSel < 0 || nSel >= (int)m_vecFields.size())
		return Error(L"Select the label field.", m_comboField);

	double dScale = ParseScale(GetText(m_editScale));
	if(dScale < 0.)
		return Error(L"Enter the scale as a number, e.g. 50000 (or leave it empty for all scales).", m_editScale);

	double dFontSize = 0.;
	if(!ParseNumber(GetText(m_editFontSize), labels.dFontSize, dFontSize) || dFontSize <= 0.)
		return Error(L"Enter the text size in mm, e.g. 3.", m_editFontSize);

	Display::Color color;
	try
	{
		color = TestMapDraw::CSymbolFactory::TextToColor(WideToUtf8(GetText(m_editColor)));
	}
	catch (std::exception&)
	{
		return Error(L"Enter the text color as #RRGGBB.", m_editColor);
	}
	if(color.GetA() == Display::Color::Transparent)
		return Error(L"Enter the text color as #RRGGBB.", m_editColor);

	double dHalo = 0.;
	if(!ParseNumber(GetText(m_editHalo), 0., dHalo) || dHalo < 0.)
		return Error(L"Enter the halo size in mm (0 - no halo).", m_editHalo);

	double dPriority = 0.;
	if(!ParseNumber(GetText(m_editPriority), 0., dPriority) || dPriority < 0. || dPriority != (int)dPriority)
		return Error(L"Enter the priority as a whole number, 0 - the labels are placed first.", m_editPriority);

	double dOffset = 0.;
	if(!ParseNumber(GetText(m_editOffset), 0., dOffset) || dOffset < 0.)
		return Error(L"Enter the offset from the feature in mm.", m_editOffset);

	Cartography::SLabelingOptions& options = labels.options;
	options.m_duplicateStrategy = (Cartography::eDuplicateStrategy)(std::max)(0, m_comboDuplicates.GetCurSel());
	double dDistance = options.m_dDuplicateDistance;
	if(options.m_duplicateStrategy == Cartography::DuplicateStrategyDistance &&
	   (!ParseNumber(GetText(m_editDuplicateDistance), dDistance, dDistance) || dDistance <= 0.))
		return Error(L"Enter the min distance between the labels with the same text in mm.", m_editDuplicateDistance);

	labels.sField = m_vecFields[nSel].sName;
	labels.dMinimumScale = dScale;
	labels.dFontSize = dFontSize;
	labels.color = color;
	labels.dHaloSize = dHalo;

	options.m_strategy = (Cartography::eLabelStrategy)(std::max)(0, m_comboStrategy.GetCurSel());
	options.m_nPriority = (int)dPriority;
	options.m_dOffset = dOffset;
	options.m_lineOrientation = (Cartography::eLineLabelOrientation)(std::max)(0, m_comboLineOrientation.GetCurSel());
	options.m_linePosition = (Cartography::eLineLabelPosition)(std::max)(0, m_comboLinePosition.GetCurSel());
	options.m_polygonPlacement = (Cartography::ePolygonLabelPlacement)(std::max)(0, m_comboPolygonPlacement.GetCurSel());
	options.m_bPolygonAllowOutside = m_checkPolygonOutside.GetCheck() == BST_CHECKED;
	options.m_dDuplicateDistance = dDistance;

	int nPreset = m_comboPointPosition.GetCurSel();
	for(int i = 0; i < Cartography::PointLabelPositionCount; ++i)
	{
		// "Current positions" (not a preset) - the priorities of the layer
		options.m_pointPriorities[i] = (nPreset >= 0 && nPreset < PointPresetCount) ? PointPresets[nPreset].priorities[i]
		                                                                           : m_labels.options.m_pointPriorities[i];
	}
	options.m_dMaxCurvedCharAngle = m_labels.options.m_dMaxCurvedCharAngle;   // not on the page
	return true;
}
