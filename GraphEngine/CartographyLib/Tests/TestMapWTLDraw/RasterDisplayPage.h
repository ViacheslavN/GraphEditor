// RasterDisplayPage.h : "Symbology" tab of the layer properties for a raster layer - the renderer (RGB composite /
// single band stretch), the bands, the stretch, brightness, contrast, saturation, gamma, transparency, NoData
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"
#include "../../layers/RasterLayer.h"

class CRasterDisplayPage : public CDialogImpl<CRasterDisplayPage>
{
public:
	enum { IDD = IDD_PAGE_RASTER };

	CRasterDisplayPage();

	// before Create
	void SetLayer(GraphEngine::Cartography::IRasterLayerPtr ptrLayer) { m_ptrLayer = ptrLayer; }
	// sets the values to the layer (a new renderer when its kind is changed), false - a wrong value (the message is shown);
	// the drawing must be stopped
	bool Apply();

	BEGIN_MSG_MAP(CRasterDisplayPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_HSCROLL, OnSlider)
		COMMAND_HANDLER(IDC_RASTER_RENDERER, CBN_SELCHANGE, OnRendererChanged)
		COMMAND_HANDLER(IDC_RASTER_STRETCH, CBN_SELCHANGE, OnStretchChanged)
		COMMAND_HANDLER(IDC_RASTER_NODATA_ON, BN_CLICKED, OnNoDataChanged)
		COMMAND_ID_HANDLER(IDC_RASTER_FROM_BTN, OnColor)
		COMMAND_ID_HANDLER(IDC_RASTER_TO_BTN, OnColor)
		COMMAND_ID_HANDLER(IDC_RASTER_RESET, OnReset)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSlider(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
	LRESULT OnRendererChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStretchChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNoDataChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnColor(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void FillBands(int nId, bool bNone);
	void SelectBand(int nId, int nBand, bool bNone);
	int  SelectedBand(int nId, bool bNone) const;
	void SetSlider(int nId, int nPos);
	int  GetSlider(int nId) const;
	void UpdateSliderTexts();
	void UpdateControls();
	bool IsRGB() const;
	bool Error(const wchar_t* pszMessage, int nFocusId);

	// gamma <-> slider -100 .. 100 (logarithmic, 0 - 1.0)
	static int    GammaToSlider(double dGamma);
	static double SliderToGamma(int nPos);

private:
	GraphEngine::Cartography::IRasterLayerPtr m_ptrLayer;
	int m_nBandCount;
};
