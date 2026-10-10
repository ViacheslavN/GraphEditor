// MapPropertiesDlg.cpp : implementation of the CMapPropertiesDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MapView.h"
#include "MapPropertiesDlg.h"
#include "DialogUtils.h"

#include <cstdio>

using namespace GraphEngine;
using namespace TestMapDraw;
using namespace DialogUtils;

namespace
{
	const CommonLib::Units AllUnits[] =
	{
		CommonLib::UnitsUnknown, CommonLib::UnitsMeters, CommonLib::UnitsKilometers, CommonLib::UnitsDecimalDegrees,
		CommonLib::UnitsFeet, CommonLib::UnitsYards, CommonLib::UnitsMiles, CommonLib::UnitsNauticalMiles, CommonLib::UnitsInches,
		CommonLib::UnitsPoints, CommonLib::UnitsMillimeters, CommonLib::UnitsCentimeters, CommonLib::UnitsDecimeters
	};
	const int UnitsCount = sizeof(AllUnits) / sizeof(AllUnits[0]);
}

CMapPropertiesDlg::CMapPropertiesDlg(CMapView* pView) : m_pView(pView), m_bChanged(false), m_bUpdating(false)
{

}

LRESULT CMapPropertiesDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());
	m_comboPreset = GetDlgItem(IDC_MAP_CS_PRESET);
	m_comboUnits = GetDlgItem(IDC_MAP_UNITS);

	for(int i = 0; i < UnitsCount; ++i)
	{
		int nItem = m_comboUnits.AddString(Utf8ToWide(CMapProject::UnitsName(AllUnits[i])).c_str());
		m_comboUnits.SetItemData(nItem, (DWORD_PTR)AllUnits[i]);
	}

	FillControls(m_pView->GetProject().GetMapParams());
	::EnableWindow(GetDlgItem(IDC_MAP_REF_SCALE_CURRENT), m_pView->GetCurrentScale() > 0.);
	return TRUE;
}

void CMapPropertiesDlg::FillControls(const SMapParams& params)
{
	m_bUpdating = true;
	SetDlgItemText(IDC_MAP_NAME, Utf8ToWide(params.sName).c_str());

	// the presets: item 0 - the current system of the map, then WGS 84, Web Mercator, UTM, the systems of the layers, none
	m_sCurrentProj4 = params.sSpatialReference;
	m_vecPresets = m_pView->GetProject().GetCoordinateSystemPresets();
	m_vecPresets.push_back({"No coordinate system (no projection of the layers)", std::string()});
	m_comboPreset.ResetContent();
	m_comboPreset.AddString(L"<the current coordinate system of the map>");
	for(size_t i = 0; i < m_vecPresets.size(); ++i)
		m_comboPreset.AddString(Utf8ToWide(m_vecPresets[i].sName).c_str());
	m_comboPreset.SetCurSel(0);

	SetDlgItemText(IDC_MAP_PROJ4, Utf8ToWide(params.sSpatialReference).c_str());
	SetDlgItemText(IDC_MAP_EPSG, L"");
	SelectUnits(params.units);

	CheckDlgButton(IDC_MAP_REF_SCALE_ON, params.bReferenceScale ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_MAP_REF_SCALE, FormatScale(params.dReferenceScale).c_str());
	::EnableWindow(GetDlgItem(IDC_MAP_REF_SCALE), params.bReferenceScale);
	SetDlgItemText(IDC_MAP_BG_COLOR, Utf8ToWide(CSymbolFactory::ColorToText(params.background)).c_str());
	m_bUpdating = false;

	UpdateInfo(nullptr);
	FillExtent();
}

void CMapPropertiesDlg::FillExtent()
{
	std::wstring sExtent = L"<no layers with data>";
	try
	{
		if(m_pView->GetProject().HasDataLayers())
		{
			Cartography::IMapPtr ptrMap = m_pView->GetProject().GetMap();
			CommonLib::bbox bb = ptrMap->GetFullExtent(ptrMap->GetSpatialReference())->GetBoundingBox();
			wchar_t sz[256];
			swprintf(sz, 256, L"X %.10g .. %.10g\nY %.10g .. %.10g", bb.xMin, bb.xMax, bb.yMin, bb.yMax);
			sExtent = sz;
		}
	}
	catch (std::exception& exc)
	{
		sExtent = ExceptionText(exc);
	}
	SetDlgItemText(IDC_MAP_EXTENT, sExtent.c_str());
}

void CMapPropertiesDlg::SelectUnits(CommonLib::Units units)
{
	for(int i = 0; i < m_comboUnits.GetCount(); ++i)
	{
		if((CommonLib::Units)m_comboUnits.GetItemData(i) == units)
		{
			m_comboUnits.SetCurSel(i);
			return;
		}
	}
	m_comboUnits.SetCurSel(0);
}

bool CMapPropertiesDlg::UpdateInfo(CommonLib::Units* pUnits)
{
	std::string sProj4 = WideToUtf8(Trim(GetText(GetDlgItem(IDC_MAP_PROJ4)), L" \t\r\n"));
	try
	{
		std::string sInfo = CMapProject::DescribeCoordinateSystem(sProj4, pUnits);
		SetDlgItemText(IDC_MAP_CS_INFO, Utf8ToWide(sInfo).c_str());
		return true;
	}
	catch (std::exception&)
	{
		SetDlgItemText(IDC_MAP_CS_INFO, L"Wrong coordinate system: the projection library doesn't accept the proj4 text.");
		return false;
	}
}

void CMapPropertiesDlg::SetProj4(const std::string& sProj4)
{
	m_bUpdating = true;
	SetDlgItemText(IDC_MAP_PROJ4, Utf8ToWide(sProj4).c_str());
	m_bUpdating = false;

	CommonLib::Units units = CommonLib::UnitsUnknown;
	if(UpdateInfo(&units))
		SelectUnits(units);
}

LRESULT CMapPropertiesDlg::OnPresetChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(m_bUpdating)
		return 0;

	int nSel = m_comboPreset.GetCurSel();
	if(nSel == 0)
		SetProj4(m_sCurrentProj4);
	else if(nSel > 0 && nSel - 1 < (int)m_vecPresets.size())
		SetProj4(m_vecPresets[nSel - 1].sProj4);
	return 0;
}

LRESULT CMapPropertiesDlg::OnEpsgSet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nCode = _wtoi(GetText(GetDlgItem(IDC_MAP_EPSG)).c_str());
	try
	{
		SetProj4(CMapProject::CoordinateSystemFromEpsg(nCode));
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"Map Properties", MB_OK | MB_ICONWARNING);
		::SetFocus(GetDlgItem(IDC_MAP_EPSG));
	}
	return 0;
}

LRESULT CMapPropertiesDlg::OnProj4Changed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_bUpdating)
		UpdateInfo(nullptr);   // typed by hand: the units are chosen by the user
	return 0;
}

LRESULT CMapPropertiesDlg::OnRefScaleOn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool bOn = IsDlgButtonChecked(IDC_MAP_REF_SCALE_ON) == BST_CHECKED;
	::EnableWindow(GetDlgItem(IDC_MAP_REF_SCALE), bOn);
	if(bOn && Trim(GetText(GetDlgItem(IDC_MAP_REF_SCALE))).empty())
		SetDlgItemText(IDC_MAP_REF_SCALE, FormatScale(m_pView->GetCurrentScale()).c_str());
	return 0;
}

LRESULT CMapPropertiesDlg::OnRefScaleCurrent(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CheckDlgButton(IDC_MAP_REF_SCALE_ON, BST_CHECKED);
	::EnableWindow(GetDlgItem(IDC_MAP_REF_SCALE), TRUE);
	SetDlgItemText(IDC_MAP_REF_SCALE, FormatScale(m_pView->GetCurrentScale()).c_str());
	return 0;
}

LRESULT CMapPropertiesDlg::OnBackgroundColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	COLORREF rgbInit = RGB(255, 255, 255);
	try
	{
		Display::Color color = CSymbolFactory::TextToColor(WideToUtf8(GetText(GetDlgItem(IDC_MAP_BG_COLOR))));
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
	SetDlgItemText(IDC_MAP_BG_COLOR, Utf8ToWide(CSymbolFactory::ColorToText(Display::Color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)))).c_str());
	return 0;
}

bool CMapPropertiesDlg::Apply()
{
	SMapParams params = m_pView->GetProject().GetMapParams();
	params.sName = WideToUtf8(Trim(GetText(GetDlgItem(IDC_MAP_NAME)), L" \t"));
	params.sSpatialReference = WideToUtf8(Trim(GetText(GetDlgItem(IDC_MAP_PROJ4)), L" \t\r\n"));
	if(!UpdateInfo(nullptr))
	{
		MessageBox(L"The coordinate system is wrong: choose one, enter an EPSG code or a proj4 text.", L"Map Properties", MB_OK | MB_ICONWARNING);
		::SetFocus(GetDlgItem(IDC_MAP_PROJ4));
		return false;
	}

	int nUnits = m_comboUnits.GetCurSel();
	params.units = nUnits >= 0 ? (CommonLib::Units)m_comboUnits.GetItemData(nUnits) : CommonLib::UnitsUnknown;

	params.bReferenceScale = IsDlgButtonChecked(IDC_MAP_REF_SCALE_ON) == BST_CHECKED;
	if(params.bReferenceScale)
	{
		params.dReferenceScale = ParseScaleText(GetText(GetDlgItem(IDC_MAP_REF_SCALE)));
		if(params.dReferenceScale <= 0.)
		{
			MessageBox(L"Enter the reference scale as a number, e.g. 25000.", L"Map Properties", MB_OK | MB_ICONWARNING);
			::SetFocus(GetDlgItem(IDC_MAP_REF_SCALE));
			return false;
		}
	}

	try
	{
		params.background = CSymbolFactory::TextToColor(WideToUtf8(GetText(GetDlgItem(IDC_MAP_BG_COLOR))));   // empty - none
	}
	catch (std::exception&)
	{
		MessageBox(L"Enter the background color as #RRGGBB or leave it empty.", L"Map Properties", MB_OK | MB_ICONWARNING);
		::SetFocus(GetDlgItem(IDC_MAP_BG_COLOR));
		return false;
	}

	if(!m_pView->ApplyMapParams(params))
		return false;

	m_bChanged = true;
	FillControls(m_pView->GetProject().GetMapParams());
	return true;
}

LRESULT CMapPropertiesDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(Apply())
		EndDialog(wID);
	return 0;
}

LRESULT CMapPropertiesDlg::OnApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Apply();
	return 0;
}

LRESULT CMapPropertiesDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
