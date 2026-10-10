// LayerPropertiesDlg.h : "Layer Properties" dialog (right click on a layer in the layers panel):
// General (name, visibility, scale range) for every layer; Data (table, OID / shape field), Symbology, Annotation,
// Labels for feature layers
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "LayerGeneralPage.h"
#include "LayerDataPage.h"
#include "SymbologyPage.h"
#include "AnnotationPage.h"
#include "LabelPage.h"

class CMapView;

class CLayerPropertiesDlg : public CDialogImpl<CLayerPropertiesDlg>
{
public:
	enum { IDD = IDD_LAYER_PROPERTIES };

	CLayerPropertiesDlg(CMapView* pView, GraphEngine::Cartography::ILayerPtr ptrLayer);

	// the layer was changed (OK or Apply)
	bool IsChanged() const { return m_bChanged; }

	BEGIN_MSG_MAP(CLayerPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_APPLY, OnApply)
		NOTIFY_HANDLER(IDC_TABS, TCN_SELCHANGE, OnTabChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTabChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

private:
	void AddPage(const wchar_t* pszName, HWND hPage);
	void SelectPage(int nPage);
	void ShowPageOf(HWND hControl);
	std::wstring DescribeSource() const;
	// takes the values of the pages and sets them to the layer (the drawing is stopped, the map is redrawn),
	// false - a wrong value or the layer can't be changed
	bool Apply();

private:
	CMapView* m_pView;
	GraphEngine::Cartography::ILayerPtr        m_ptrLayer;
	GraphEngine::Cartography::IFeatureLayerPtr m_ptrFeatureLayer;   // null - not a feature layer (only General)
	TestMapDraw::STableInfo m_table;
	TestMapDraw::SLayerDataInfo m_dataInfo;   // the fields of the layer when the page was filled / applied
	bool m_bSymbologyExact;
	int  m_nScaleDependent;   // of the symbols of the layer when the dialog was opened / applied, -1 - no symbols
	bool m_bChanged;

	CTabCtrl           m_tabs;
	std::vector<HWND>  m_vecPages;
	CLayerGeneralPage  m_general;
	CLayerDataPage     m_data;
	CSymbologyPage     m_symbology;
	CAnnotationPage    m_annotation;
	CLabelPage         m_labels;
};
