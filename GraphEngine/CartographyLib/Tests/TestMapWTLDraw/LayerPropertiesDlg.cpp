// LayerPropertiesDlg.cpp : implementation of the CLayerPropertiesDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MapView.h"
#include "LayerPropertiesDlg.h"
#include "DialogUtils.h"

using namespace GraphEngine;
using namespace TestMapDraw;
using namespace DialogUtils;

CLayerPropertiesDlg::CLayerPropertiesDlg(CMapView* pView, Cartography::ILayerPtr ptrLayer) :
	m_pView(pView), m_ptrLayer(ptrLayer), m_bSymbologyExact(true), m_nScaleDependent(-1), m_bChanged(false)
{
	m_ptrFeatureLayer = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(ptrLayer);
	if(m_ptrFeatureLayer.get() && !m_ptrFeatureLayer->GetLayerTable().get())
		m_ptrFeatureLayer.reset();   // no table - nothing to classify
}

std::wstring CLayerPropertiesDlg::DescribeSource() const
{
	std::wstring sText;
	if(m_ptrFeatureLayer.get())
	{
		GeoDatabase::ITablePtr ptrTable = m_ptrFeatureLayer->GetLayerTable();
		static const wchar_t* geometryNames[] = {L"points", L"lines", L"polygons"};
		sText = L"Feature layer, " + std::wstring(geometryNames[GeometryKindOf(ptrTable->GetGeometryType())]) +
		        L"\nTable: " + Utf8ToWide(ptrTable->GetDatasetName()) +
		        L"\nAttribute fields: " + std::to_wstring(m_table.vecFields.size());
		return sText;
	}

	if(Cartography::IRasterLayerPtr ptrRaster = std::dynamic_pointer_cast<Cartography::IRasterLayer>(m_ptrLayer))
	{
		GeoDatabase::IRasterDatasetPtr ptrDataset = ptrRaster->GetRasterDataset();
		sText = L"Raster layer";
		if(ptrDataset.get())
		{
			sText += L"\nDataset: " + Utf8ToWide(ptrDataset->GetDatasetViewName()) +
			         L"\nSize: " + std::to_wstring(ptrDataset->GetWidth()) + L" x " + std::to_wstring(ptrDataset->GetHeight()) +
			         L" pixels, bands: " + std::to_wstring(ptrDataset->GetBandCount());
		}
		return sText;
	}

	if(Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(m_ptrLayer))
		return L"Group layer\nLayers: " + std::to_wstring(ptrGroup->GetChildren()->GetLayerCount());

	return L"Layer";
}

void CLayerPropertiesDlg::AddPage(const wchar_t* pszName, HWND hPage)
{
	m_tabs.InsertItem((int)m_vecPages.size(), pszName);

	// the pages are children of the dialog over the display area of the tabs, after the tabs in the tab order
	RECT rc = {0};
	m_tabs.GetWindowRect(&rc);
	::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rc, 2);
	m_tabs.AdjustRect(FALSE, &rc);
	HWND hInsertAfter = m_vecPages.empty() ? (HWND)m_tabs : m_vecPages.back();
	::SetWindowPos(hPage, hInsertAfter, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE);
	m_vecPages.push_back(hPage);
}

void CLayerPropertiesDlg::SelectPage(int nPage)
{
	if(m_tabs.GetCurSel() != nPage)
		m_tabs.SetCurSel(nPage);
	for(int i = 0; i < (int)m_vecPages.size(); ++i)
		::ShowWindow(m_vecPages[i], i == nPage ? SW_SHOW : SW_HIDE);
}

void CLayerPropertiesDlg::ShowPageOf(HWND hControl)
{
	for(int i = 0; i < (int)m_vecPages.size(); ++i)
		if(m_vecPages[i] == hControl)
			SelectPage(i);
}

LRESULT CLayerPropertiesDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());
	m_tabs = GetDlgItem(IDC_TABS);

	std::wstring sTitle = L"Layer Properties - " + Utf8ToWide(m_ptrLayer->GetName());
	SetWindowText(sTitle.c_str());

	double dScale = m_pView->GetCurrentScale();

	SLayerParams params;
	if(m_ptrFeatureLayer.get())
	{
		m_table = CMapProject::GetTableInfo(m_ptrFeatureLayer->GetLayerTable(), m_ptrFeatureLayer->GetName());
		CMapProject::GetLayerParams(m_ptrFeatureLayer, params, &m_bSymbologyExact);
	}

	CLayerGeneralPage::SValues values;
	values.sName = m_ptrLayer->GetName();
	values.bVisible = m_ptrLayer->GetVisible();
	values.bSelectable = m_ptrFeatureLayer.get() ? m_ptrFeatureLayer->GetSelectable() : false;
	values.dMinimumScale = m_ptrLayer->GetMinimumScale();
	values.dMaximumScale = m_ptrLayer->GetMaximumScale();
	if(m_ptrFeatureLayer.get())
	{
		m_nScaleDependent = CMapProject::GetLayerScaleDependent(m_ptrFeatureLayer);
		values.nScaleDependent = m_nScaleDependent;
		Cartography::IMapPtr ptrMap = m_pView->GetProject().GetMap();
		values.dMapReferenceScale = ptrMap->GetHasReferenceScale() ? ptrMap->GetReferenceScale() : 0.;
	}
	m_general.SetValues(values, m_ptrFeatureLayer.get() != nullptr, DescribeSource(), dScale);
	m_general.Create(m_hWnd);
	AddPage(L"General", m_general);

	if(m_ptrFeatureLayer.get())
	{
		// the values are read from the layer table: the drawing is stopped first (Apply redraws the map)
		CMapView* pView = m_pView;
		GeoDatabase::ITablePtr ptrTable = m_ptrFeatureLayer->GetLayerTable();
		CSymbologyPage::TUniqueQuery uniqueQuery = [pView, ptrTable](const std::string& sField, bool& bTruncated)
		{
			pView->StopDrawing();
			return CMapProject::GetUniqueValues(ptrTable, sField, CSymbologyPage::MaxUniqueValues, &bTruncated);
		};
		CSymbologyPage::TRangeQuery rangeQuery = [pView, ptrTable](const std::string& sField, double& dMin, double& dMax)
		{
			pView->StopDrawing();
			return CMapProject::GetValueRange(ptrTable, sField, dMin, dMax);
		};

		m_dataInfo = CMapProject::GetLayerDataInfo(m_ptrFeatureLayer);
		m_data.SetInfo(m_dataInfo);
		m_data.Create(m_hWnd);
		AddPage(L"Data", m_data);

		m_symbology.Create(m_hWnd);
		m_symbology.SetSource(&m_table, params.ptrSymbology.get() ? params.ptrSymbology->simpleSymbol.color : CMapProject::GetLayerColor(0),
		                      uniqueQuery, rangeQuery);
		if(params.ptrSymbology.get())
			m_symbology.SetSymbology(*params.ptrSymbology);
		AddPage(L"Symbology", m_symbology);

		m_annotation.SetDefaultScale(dScale);
		m_annotation.SetAnnotation(params.annotation);
		m_annotation.Create(m_hWnd);
		m_annotation.SetFields(&m_table.vecFields);
		AddPage(L"Annotation", m_annotation);

		m_labels.SetDefaultScale(dScale);
		m_labels.SetLabels(params.labels);
		m_labels.Create(m_hWnd);
		m_labels.SetFields(&m_table.vecFields);
		AddPage(L"Labels", m_labels);

		if(!m_bSymbologyExact)
			SetDlgItemText(IDC_SYMBOLOGY_NOTE, L"Some symbols of the layer can't be edited here, they are kept until the symbology is changed.");
	}

	SelectPage(0);
	::SetFocus(m_general.GetDlgItem(IDC_LAYER_NAME));
	return FALSE;   // the focus is set
}

bool CLayerPropertiesDlg::Apply()
{
	CLayerGeneralPage::SValues values;
	if(!m_general.GetValues(values))
	{
		ShowPageOf(m_general);
		return false;
	}

	SLayerParams params;
	if(m_ptrFeatureLayer.get())
	{
		// the symbology is set only when it was changed: the symbols the page can't show are kept
		if(m_symbology.IsModified())
		{
			std::shared_ptr<SSymbology> ptrSymbology = std::make_shared<SSymbology>();
			if(!m_symbology.GetSymbology(*ptrSymbology))
			{
				ShowPageOf(m_symbology);
				return false;
			}
			params.ptrSymbology = ptrSymbology;
		}

		if(!m_annotation.GetAnnotation(params.annotation))
		{
			ShowPageOf(m_annotation);
			return false;
		}
		if(!m_labels.GetLabels(params.labels))
		{
			ShowPageOf(m_labels);
			return false;
		}
	}

	m_pView->StopDrawing();
	try
	{
		CWaitCursorGuard waitCursor;
		if(m_ptrFeatureLayer.get())
		{
			CMapProject::ApplyLayerParams(m_ptrFeatureLayer, params);   // throws before changing the layer (image files)
			m_ptrFeatureLayer->SetSelectable(values.bSelectable);
			// set only when the user has changed the check box: the symbols of a new symbology keep their own flags
			if(values.nScaleDependent != m_nScaleDependent && values.nScaleDependent != CSymbolFactory::ScaleDependentMixed)
				CMapProject::SetLayerScaleDependent(m_ptrFeatureLayer, values.nScaleDependent == CSymbolFactory::ScaleDependentYes);
			m_nScaleDependent = CMapProject::GetLayerScaleDependent(m_ptrFeatureLayer);

			std::string sOIDField, sShapeField;
			m_data.GetFields(sOIDField, sShapeField);
			if(sOIDField != m_dataInfo.sOIDField || sShapeField != m_dataInfo.sShapeField)
			{
				CMapProject::SetLayerDataFields(m_ptrFeatureLayer, sOIDField, sShapeField);
				if(sOIDField != m_dataInfo.sOIDField)   // the selected ids are values of the old field
					m_pView->GetProject().GetMap()->GetSelection()->ClearForLayer(m_ptrFeatureLayer->GetLayerId());
				m_dataInfo.sOIDField = sOIDField;
				m_dataInfo.sShapeField = sShapeField;
			}
		}

		m_ptrLayer->SetName(values.sName);
		m_ptrLayer->SetVisible(values.bVisible);
		m_ptrLayer->SetMinimumScale(values.dMinimumScale);
		m_ptrLayer->SetMaximumScale(values.dMaximumScale);
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"Layer Properties", MB_OK | MB_ICONERROR);
		m_pView->Redraw();
		return false;
	}

	if(params.ptrSymbology.get())
	{
		m_symbology.ResetModified();
		m_bSymbologyExact = true;
		SetDlgItemText(IDC_SYMBOLOGY_NOTE, L"");
	}

	std::wstring sTitle = L"Layer Properties - " + Utf8ToWide(values.sName);
	SetWindowText(sTitle.c_str());

	m_bChanged = true;
	m_pView->OnLayersChanged(true);   // redraw, the layers panel shows the new name / legend
	return true;
}

LRESULT CLayerPropertiesDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(Apply())
		EndDialog(wID);
	return 0;
}

LRESULT CLayerPropertiesDlg::OnApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Apply();
	return 0;
}

LRESULT CLayerPropertiesDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT CLayerPropertiesDlg::OnTabChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	SelectPage(m_tabs.GetCurSel());
	return 0;
}
