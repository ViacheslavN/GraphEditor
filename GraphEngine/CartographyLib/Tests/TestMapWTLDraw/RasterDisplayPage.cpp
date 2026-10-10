// RasterDisplayPage.cpp : implementation of the CRasterDisplayPage class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "RasterDisplayPage.h"
#include "DialogUtils.h"
#include "../../renders/Raster/RasterRGBRenderer.h"
#include "../../renders/Raster/RasterStretchRenderer.h"

#include <cmath>

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace TestMapDraw;
using namespace DialogUtils;

namespace
{
	const wchar_t* Caption = L"Layer Properties";

	// the order of the stretch combo box
	const eRasterStretchType StretchTypes[] = {RasterStretchTypeNone, RasterStretchTypeMinMax, RasterStretchTypeStandardDeviation, RasterStretchTypePercentClip};

	std::wstring FormatNumber(double dValue)
	{
		wchar_t sz[64];
		swprintf(sz, 64, L"%g", dValue);
		return sz;
	}

	bool ParseNumber(const std::wstring& sText, double& dValue)
	{
		std::wstring s = Trim(sText, L" \t");
		for(wchar_t& c : s)
			if(c == L',')
				c = L'.';
		if(s.empty())
			return false;
		wchar_t* pszEnd = nullptr;
		dValue = wcstod(s.c_str(), &pszEnd);
		return pszEnd != s.c_str() && *pszEnd == 0 && std::isfinite(dValue);
	}
}

CRasterDisplayPage::CRasterDisplayPage() : m_nBandCount(0)
{

}

int CRasterDisplayPage::GammaToSlider(double dGamma)
{
	return (int)std::lround(std::log10((std::max)(dGamma, CRasterRendererBase::kMinGamma)) * 100.);
}

double CRasterDisplayPage::SliderToGamma(int nPos)
{
	// two decimals, as it is shown
	return std::round(std::pow(10., nPos / 100.) * 100.) / 100.;
}

bool CRasterDisplayPage::IsRGB() const
{
	return CComboBox(GetDlgItem(IDC_RASTER_RENDERER)).GetCurSel() == 0;
}

void CRasterDisplayPage::FillBands(int nId, bool bNone)
{
	CComboBox combo = GetDlgItem(nId);
	combo.ResetContent();
	if(bNone)
		combo.AddString(L"None");
	for(int i = 0; i < m_nBandCount; ++i)
		combo.AddString((L"Band " + std::to_wstring(i + 1)).c_str());
}

void CRasterDisplayPage::SelectBand(int nId, int nBand, bool bNone)
{
	int nIndex = bNone ? nBand + 1 : nBand;
	CComboBox(GetDlgItem(nId)).SetCurSel(nIndex >= 0 && nIndex < (bNone ? m_nBandCount + 1 : m_nBandCount) ? nIndex : (bNone ? 0 : -1));
}

int CRasterDisplayPage::SelectedBand(int nId, bool bNone) const
{
	int nSel = CComboBox(GetDlgItem(nId)).GetCurSel();
	if(nSel < 0)
		return bNone ? -1 : 0;
	return bNone ? nSel - 1 : nSel;
}

void CRasterDisplayPage::SetSlider(int nId, int nPos)
{
	CTrackBarCtrl(GetDlgItem(nId)).SetPos(nPos);
}

int CRasterDisplayPage::GetSlider(int nId) const
{
	return CTrackBarCtrl(GetDlgItem(nId)).GetPos();
}

void CRasterDisplayPage::UpdateSliderTexts()
{
	SetDlgItemText(IDC_RASTER_BRIGHTNESS_VAL, std::to_wstring(GetSlider(IDC_RASTER_BRIGHTNESS)).c_str());
	SetDlgItemText(IDC_RASTER_CONTRAST_VAL, std::to_wstring(GetSlider(IDC_RASTER_CONTRAST)).c_str());
	SetDlgItemText(IDC_RASTER_SATURATION_VAL, std::to_wstring(GetSlider(IDC_RASTER_SATURATION)).c_str());
	SetDlgItemText(IDC_RASTER_TRANSPARENCY_VAL, (std::to_wstring(GetSlider(IDC_RASTER_TRANSPARENCY)) + L" %").c_str());
	wchar_t sz[32];
	swprintf(sz, 32, L"%.2f", SliderToGamma(GetSlider(IDC_RASTER_GAMMA)));
	SetDlgItemText(IDC_RASTER_GAMMA_VAL, sz);
}

void CRasterDisplayPage::UpdateControls()
{
	bool bRGB = IsRGB();
	for(int nId : {IDC_RASTER_RED, IDC_RASTER_GREEN, IDC_RASTER_BLUE})
		::EnableWindow(GetDlgItem(nId), bRGB);
	for(int nId : {IDC_RASTER_BAND, IDC_RASTER_FROM_COLOR, IDC_RASTER_FROM_BTN, IDC_RASTER_TO_COLOR, IDC_RASTER_TO_BTN, IDC_RASTER_INVERT})
		::EnableWindow(GetDlgItem(nId), !bRGB);

	int nStretch = CComboBox(GetDlgItem(IDC_RASTER_STRETCH)).GetCurSel();
	bool bClip = nStretch >= 0 && StretchTypes[nStretch] == RasterStretchTypePercentClip;
	bool bStdDev = nStretch >= 0 && StretchTypes[nStretch] == RasterStretchTypeStandardDeviation;
	::EnableWindow(GetDlgItem(IDC_RASTER_CLIP_LOW), bClip);
	::EnableWindow(GetDlgItem(IDC_RASTER_CLIP_HIGH), bClip);
	::EnableWindow(GetDlgItem(IDC_RASTER_STDDEV), bStdDev);
	::EnableWindow(GetDlgItem(IDC_RASTER_NODATA), IsDlgButtonChecked(IDC_RASTER_NODATA_ON) == BST_CHECKED);
}

LRESULT CRasterDisplayPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	GeoDatabase::IRasterDatasetPtr ptrDataset = m_ptrLayer.get() ? m_ptrLayer->GetRasterDataset() : GeoDatabase::IRasterDatasetPtr();
	m_nBandCount = ptrDataset.get() ? ptrDataset->GetBandCount() : 0;

	CComboBox comboRenderer = GetDlgItem(IDC_RASTER_RENDERER);
	comboRenderer.AddString(L"RGB composite (bands as red, green, blue)");
	comboRenderer.AddString(L"Single band stretch (color ramp)");

	CComboBox comboStretch = GetDlgItem(IDC_RASTER_STRETCH);
	comboStretch.AddString(L"None (8-bit values as they are)");
	comboStretch.AddString(L"Minimum - maximum");
	comboStretch.AddString(L"Standard deviations");
	comboStretch.AddString(L"Percent clip (cumulative count cut)");

	FillBands(IDC_RASTER_RED, true);
	FillBands(IDC_RASTER_GREEN, true);
	FillBands(IDC_RASTER_BLUE, true);
	FillBands(IDC_RASTER_BAND, false);

	for(int nId : {IDC_RASTER_BRIGHTNESS, IDC_RASTER_CONTRAST, IDC_RASTER_SATURATION, IDC_RASTER_GAMMA})
		CTrackBarCtrl(GetDlgItem(nId)).SetRange(-100, 100);
	CTrackBarCtrl(GetDlgItem(IDC_RASTER_TRANSPARENCY)).SetRange(0, 100);

	IRasterRendererPtr ptrRenderer = m_ptrLayer.get() ? m_ptrLayer->GetRenderer() : IRasterRendererPtr();
	CRasterRendererBase* pBase = dynamic_cast<CRasterRendererBase*>(ptrRenderer.get());
	CRasterRGBRenderer* pRGB = dynamic_cast<CRasterRGBRenderer*>(ptrRenderer.get());
	CRasterStretchRenderer* pStretch = dynamic_cast<CRasterStretchRenderer*>(ptrRenderer.get());

	comboRenderer.SetCurSel(pStretch ? 1 : 0);
	SelectBand(IDC_RASTER_RED, pRGB ? pRGB->GetRedBandIndex() : 0, true);
	SelectBand(IDC_RASTER_GREEN, pRGB ? pRGB->GetGreenBandIndex() : (std::min)(1, m_nBandCount - 1), true);
	SelectBand(IDC_RASTER_BLUE, pRGB ? pRGB->GetBlueBandIndex() : (std::min)(2, m_nBandCount - 1), true);
	SelectBand(IDC_RASTER_BAND, pStretch ? pStretch->GetBand() : 0, false);

	Display::Color from = pStretch ? pStretch->GetFromColor() : Display::Color(0, 0, 0);
	Display::Color to = pStretch ? pStretch->GetToColor() : Display::Color(255, 255, 255);
	SetDlgItemText(IDC_RASTER_FROM_COLOR, Utf8ToWide(CSymbolFactory::ColorToText(from)).c_str());
	SetDlgItemText(IDC_RASTER_TO_COLOR, Utf8ToWide(CSymbolFactory::ColorToText(to)).c_str());
	CheckDlgButton(IDC_RASTER_INVERT, pStretch && pStretch->GetInvert() ? BST_CHECKED : BST_UNCHECKED);

	eRasterStretchType stretch = pBase ? pBase->GetStretchType() : RasterStretchTypePercentClip;
	for(int i = 0; i < (int)(sizeof(StretchTypes) / sizeof(StretchTypes[0])); ++i)
		if(StretchTypes[i] == stretch)
			comboStretch.SetCurSel(i);
	SetDlgItemText(IDC_RASTER_CLIP_LOW, FormatNumber(pBase ? pBase->GetClipLowPercent() : 2.).c_str());
	SetDlgItemText(IDC_RASTER_CLIP_HIGH, FormatNumber(pBase ? pBase->GetClipHighPercent() : 98.).c_str());
	SetDlgItemText(IDC_RASTER_STDDEV, FormatNumber(pBase ? pBase->GetStdDevCount() : 2.).c_str());

	SetSlider(IDC_RASTER_BRIGHTNESS, pBase ? pBase->GetBrightness() : 0);
	SetSlider(IDC_RASTER_CONTRAST, pBase ? pBase->GetContrast() : 0);
	SetSlider(IDC_RASTER_SATURATION, pBase ? pBase->GetSaturation() : 0);
	SetSlider(IDC_RASTER_GAMMA, GammaToSlider(pBase ? pBase->GetGamma() : 1.));
	SetSlider(IDC_RASTER_TRANSPARENCY, pBase ? pBase->GetTransparency() : 0);

	// NoData: one value for all the bands (GDAL_NODATA of the file when the layer was added)
	bool bNoData = pBase && !pBase->GetDisplayBackground() && !pBase->GetBackgroundValues().empty();
	CheckDlgButton(IDC_RASTER_NODATA_ON, bNoData ? BST_CHECKED : BST_UNCHECKED);
	double dNoData = 0.;
	if(pBase && !pBase->GetBackgroundValues().empty())
		SetDlgItemText(IDC_RASTER_NODATA, FormatNumber(pBase->GetBackgroundValues()[0]).c_str());
	else if(ptrDataset.get() && ptrDataset->GetNoDataValue(dNoData))
		SetDlgItemText(IDC_RASTER_NODATA, FormatNumber(dNoData).c_str());

	std::wstring sInfo;
	if(ptrDataset.get())
	{
		static const wchar_t* typeNames[] = {L"unknown", L"1 bit", L"2 bits", L"4 bits", L"8 bits unsigned", L"8 bits signed",
		                                     L"16 bits unsigned", L"16 bits signed", L"32 bits unsigned", L"32 bits signed",
		                                     L"32 bits float", L"64 bits float", L"64 bits signed", L"64 bits unsigned"};
		int nType = (int)ptrDataset->GetSourcePixelType();
		sInfo = std::to_wstring(m_nBandCount) + L" band(s), " +
		        (nType >= 0 && nType < (int)(sizeof(typeNames) / sizeof(typeNames[0])) ? typeNames[nType] : L"");
	}
	SetDlgItemText(IDC_RASTER_INFO, sInfo.c_str());

	UpdateSliderTexts();
	UpdateControls();
	return TRUE;
}

LRESULT CRasterDisplayPage::OnSlider(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if(!lParam)
	{
		bHandled = FALSE;
		return 0;
	}
	UpdateSliderTexts();
	return 0;
}

LRESULT CRasterDisplayPage::OnRendererChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

LRESULT CRasterDisplayPage::OnStretchChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

LRESULT CRasterDisplayPage::OnNoDataChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateControls();
	return 0;
}

LRESULT CRasterDisplayPage::OnColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nEditId = wID == IDC_RASTER_FROM_BTN ? IDC_RASTER_FROM_COLOR : IDC_RASTER_TO_COLOR;
	COLORREF rgbInit = wID == IDC_RASTER_FROM_BTN ? RGB(0, 0, 0) : RGB(255, 255, 255);
	try
	{
		Display::Color color = CSymbolFactory::TextToColor(WideToUtf8(GetText(GetDlgItem(nEditId))));
		rgbInit = RGB(color.GetR(), color.GetG(), color.GetB());
	}
	catch (std::exception&)
	{
	}

	CColorDialog dlg(rgbInit, CC_FULLOPEN, m_hWnd);
	if(dlg.DoModal() != IDOK)
		return 0;
	COLORREF rgb = dlg.GetColor();
	SetDlgItemText(nEditId, Utf8ToWide(CSymbolFactory::ColorToText(Display::Color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)))).c_str());
	return 0;
}

LRESULT CRasterDisplayPage::OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetSlider(IDC_RASTER_BRIGHTNESS, 0);
	SetSlider(IDC_RASTER_CONTRAST, 0);
	SetSlider(IDC_RASTER_SATURATION, 0);
	SetSlider(IDC_RASTER_GAMMA, 0);
	SetSlider(IDC_RASTER_TRANSPARENCY, 0);
	UpdateSliderTexts();
	return 0;
}

bool CRasterDisplayPage::Error(const wchar_t* pszMessage, int nFocusId)
{
	MessageBox(pszMessage, Caption, MB_OK | MB_ICONWARNING);
	::SetFocus(GetDlgItem(nFocusId));
	return false;
}

bool CRasterDisplayPage::Apply()
{
	if(!m_ptrLayer.get() || !m_hWnd)
		return true;

	// the values first: nothing is changed on an error
	const bool bRGB = IsRGB();
	int nStretch = CComboBox(GetDlgItem(IDC_RASTER_STRETCH)).GetCurSel();
	eRasterStretchType stretch = nStretch >= 0 ? StretchTypes[nStretch] : RasterStretchTypePercentClip;

	double dClipLow = 2., dClipHigh = 98., dStdDev = 2.;
	if(stretch == RasterStretchTypePercentClip)
	{
		if(!ParseNumber(GetText(GetDlgItem(IDC_RASTER_CLIP_LOW)), dClipLow) || !ParseNumber(GetText(GetDlgItem(IDC_RASTER_CLIP_HIGH)), dClipHigh) ||
		   dClipLow < 0. || dClipHigh > 100. || dClipLow >= dClipHigh)
			return Error(L"Enter the clip percents: 0 <= low < high <= 100, e.g. 2 and 98.", IDC_RASTER_CLIP_LOW);
	}
	if(stretch == RasterStretchTypeStandardDeviation)
	{
		if(!ParseNumber(GetText(GetDlgItem(IDC_RASTER_STDDEV)), dStdDev) || dStdDev <= 0.)
			return Error(L"Enter the number of standard deviations, e.g. 2.", IDC_RASTER_STDDEV);
	}

	Display::Color from, to;
	if(!bRGB)
	{
		try
		{
			from = CSymbolFactory::TextToColor(WideToUtf8(GetText(GetDlgItem(IDC_RASTER_FROM_COLOR))));
			to = CSymbolFactory::TextToColor(WideToUtf8(GetText(GetDlgItem(IDC_RASTER_TO_COLOR))));
		}
		catch (std::exception&)
		{
			return Error(L"Enter the ramp colors as #RRGGBB.", IDC_RASTER_FROM_COLOR);
		}
	}

	bool bNoData = IsDlgButtonChecked(IDC_RASTER_NODATA_ON) == BST_CHECKED;
	double dNoData = 0.;
	if(bNoData && !ParseNumber(GetText(GetDlgItem(IDC_RASTER_NODATA)), dNoData))
		return Error(L"Enter the NoData value as a number, e.g. -32767.", IDC_RASTER_NODATA);

	// the renderer: the same object, or a new one of the other kind with the common settings
	IRasterRendererPtr ptrRenderer = m_ptrLayer->GetRenderer();
	eRasterRendererID id = bRGB ? RasterRGBRendererID : RasterStretchRendererID;
	if(!ptrRenderer.get() || ptrRenderer->GetRasterRendererID() != (uint32_t)id)
		ptrRenderer = CRasterLayer::CreateRenderer(id, m_ptrLayer->GetRasterDataset(), ptrRenderer);

	CRasterRendererBase* pBase = dynamic_cast<CRasterRendererBase*>(ptrRenderer.get());
	if(!pBase)
		return true;

	if(CRasterRGBRenderer* pRGB = dynamic_cast<CRasterRGBRenderer*>(pBase))
	{
		int red = SelectedBand(IDC_RASTER_RED, true), green = SelectedBand(IDC_RASTER_GREEN, true), blue = SelectedBand(IDC_RASTER_BLUE, true);
		if(red < 0 && green < 0 && blue < 0)
			return Error(L"Choose a band for at least one of the channels.", IDC_RASTER_RED);
		pRGB->SetBandIndices(red, green, blue, pRGB->GetAlphaBandIndex());
	}
	if(CRasterStretchRenderer* pStretch = dynamic_cast<CRasterStretchRenderer*>(pBase))
	{
		pStretch->SetBand(SelectedBand(IDC_RASTER_BAND, false));
		pStretch->SetColorRamp(from, to);
		pStretch->SetInvert(IsDlgButtonChecked(IDC_RASTER_INVERT) == BST_CHECKED);
	}

	pBase->SetStretchType(stretch);
	if(stretch == RasterStretchTypePercentClip)
		pBase->SetClipPercents(dClipLow, dClipHigh);
	if(stretch == RasterStretchTypeStandardDeviation)
		pBase->SetStdDevCount(dStdDev);

	pBase->SetBrightness(GetSlider(IDC_RASTER_BRIGHTNESS));
	pBase->SetContrast(GetSlider(IDC_RASTER_CONTRAST));
	pBase->SetSaturation(GetSlider(IDC_RASTER_SATURATION));
	pBase->SetGamma(SliderToGamma(GetSlider(IDC_RASTER_GAMMA)));
	pBase->SetTransparency(GetSlider(IDC_RASTER_TRANSPARENCY));

	// the statistics are calculated again only when NoData is changed
	std::vector<double> vecNoData;
	if(bNoData)
		vecNoData.assign((std::max)(1, m_nBandCount), dNoData);
	if(vecNoData != pBase->GetBackgroundValues())
		pBase->SetBackgroundValues(vecNoData);
	pBase->SetDisplayBackground(!bNoData);

	m_ptrLayer->SetRenderer(ptrRenderer);
	return true;
}
