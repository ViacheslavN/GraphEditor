// LayerPages.cpp : implementation of the CLayerPages class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "LayerPages.h"

using namespace TestMapDraw;

CLayerPages::CLayerPages() : m_nLayerIndex(0)
{

}

void CLayerPages::Create(HWND hDialog, int nTabId)
{
	m_tabs = ::GetDlgItem(hDialog, nTabId);
	m_tabs.InsertItem(PageSymbology, L"Symbology");
	m_tabs.InsertItem(PageAnnotation, L"Annotation");

	m_symbology.Create(hDialog);
	m_annotation.Create(hDialog);

	// the pages are children of the dialog (not of the tab control) placed over the display area of the tabs,
	// right after the tab control in the tab order
	RECT rc = {0};
	m_tabs.GetWindowRect(&rc);
	::MapWindowPoints(NULL, hDialog, (LPPOINT)&rc, 2);
	m_tabs.AdjustRect(FALSE, &rc);

	HWND hInsertAfter = m_tabs;
	HWND pages[PageCount] = {m_symbology, m_annotation};
	for(int i = 0; i < PageCount; ++i)
	{
		::SetWindowPos(pages[i], hInsertAfter, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE);
		hInsertAfter = pages[i];
	}

	SelectPage(PageSymbology);
}

void CLayerPages::SelectPage(int nPage)
{
	if(m_tabs.GetCurSel() != nPage)
		m_tabs.SetCurSel(nPage);
	m_symbology.ShowWindow(nPage == PageSymbology ? SW_SHOW : SW_HIDE);
	m_annotation.ShowWindow(nPage == PageAnnotation ? SW_SHOW : SW_HIDE);
}

void CLayerPages::OnTabChanged()
{
	SelectPage(m_tabs.GetCurSel());
}

void CLayerPages::SetSource(const STableInfo* pTable, const SDataSource& source)
{
	SDataSource sourceCopy = source;
	CSymbologyPage::TUniqueQuery uniqueQuery = [sourceCopy](const std::string& sField, bool& bTruncated)
	{
		return CMapProject::GetUniqueValues(sourceCopy, sField, CSymbologyPage::MaxUniqueValues, &bTruncated);
	};
	CSymbologyPage::TRangeQuery rangeQuery = [sourceCopy](const std::string& sField, double& dMin, double& dMax)
	{
		return CMapProject::GetValueRange(sourceCopy, sField, dMin, dMax);
	};

	m_symbology.SetSource(pTable, CMapProject::GetLayerColor(m_nLayerIndex), uniqueQuery, rangeQuery);
	m_annotation.SetFields(pTable ? &pTable->vecFields : nullptr);
}

bool CLayerPages::GetLayerParams(SLayerParams& params)
{
	params = SLayerParams();

	if(m_symbology.HasSource())
	{
		std::shared_ptr<SSymbology> ptrSymbology = std::make_shared<SSymbology>();
		if(!m_symbology.GetSymbology(*ptrSymbology))
		{
			SelectPage(PageSymbology);
			return false;
		}
		params.ptrSymbology = ptrSymbology;
	}

	if(!m_annotation.GetAnnotation(params.annotation))
	{
		SelectPage(PageAnnotation);
		return false;
	}
	return true;
}
