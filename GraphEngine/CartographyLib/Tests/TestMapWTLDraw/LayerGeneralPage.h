// LayerGeneralPage.h : "General" tab of the layer properties - name, source, visibility, scale range, scale dependent symbols
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"

class CLayerGeneralPage : public CDialogImpl<CLayerGeneralPage>
{
public:
	enum { IDD = IDD_PAGE_GENERAL };

	struct SValues
	{
		std::string sName;
		bool        bVisible = true;
		bool        bSelectable = true;
		double      dMinimumScale = 0.;   // hidden when the scale denominator is bigger (zoomed out), 0 - no limit
		double      dMaximumScale = 0.;   // hidden when the scale denominator is smaller (zoomed in), 0 - no limit
		// symbols scaled with the map reference scale: CSymbolFactory::ScaleDependentNo / Yes / Mixed, -1 - no symbols (hidden)
		int         nScaleDependent = -1;
		double      dMapReferenceScale = 0.;   // shown in the note, 0 - the map has no reference scale
	};

	CLayerGeneralPage();

	// before Create: the current values, the source description, the current map scale (0 - no map)
	void SetValues(const SValues& values, bool bHasSelectable, const std::wstring& sSource, double dCurrentScale);
	// false - a wrong value (the message is shown)
	bool GetValues(SValues& values);

	BEGIN_MSG_MAP(CLayerGeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_MIN_SCALE_CURRENT, OnCurrentScale)
		COMMAND_ID_HANDLER(IDC_MAX_SCALE_CURRENT, OnCurrentScale)
		COMMAND_HANDLER(IDC_LAYER_SCALE_SYMBOLS, BN_CLICKED, OnScaleSymbols)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCurrentScale(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnScaleSymbols(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	bool Error(const wchar_t* pszMessage, HWND hFocus);
	void UpdateScaleNote();

private:
	SValues      m_values;
	bool         m_bHasSelectable;
	std::wstring m_sSource;
	double       m_dCurrentScale;
};
