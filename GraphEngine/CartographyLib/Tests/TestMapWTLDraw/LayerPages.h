// LayerPages.h : tab control with the Symbology, Annotation and Labels pages, shared by the Add layer dialogs
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SymbologyPage.h"
#include "AnnotationPage.h"
#include "LabelPage.h"

class CLayerPages
{
public:
	enum { PageSymbology = 0, PageAnnotation = 1, PageLabels = 2, PageCount = 3 };

	CLayerPages();

	// creates the pages over the tab control of the dialog (call from WM_INITDIALOG)
	void Create(HWND hDialog, int nTabId);
	// call from the TCN_SELCHANGE handler of the tab control
	void OnTabChanged();
	void SelectPage(int nPage);

	// table of the new layer, nullptr - no table (the pages are disabled)
	void SetSource(const TestMapDraw::STableInfo* pTable, const TestMapDraw::SDataSource& source);
	void SetLayerIndex(int nIndex) { m_nLayerIndex = nIndex; }
	void SetDefaultScale(double dScale) { m_annotation.SetDefaultScale(dScale); m_labels.SetDefaultScale(dScale); }

	// false - a wrong value, the page with it is shown (and the message)
	bool GetLayerParams(TestMapDraw::SLayerParams& params);

private:
	CTabCtrl        m_tabs;
	CSymbologyPage  m_symbology;
	CAnnotationPage m_annotation;
	CLabelPage      m_labels;
	int             m_nLayerIndex;
};
