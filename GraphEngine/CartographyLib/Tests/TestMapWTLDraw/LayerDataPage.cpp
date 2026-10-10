// LayerDataPage.cpp : implementation of the CLayerDataPage class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "LayerDataPage.h"
#include "DialogUtils.h"

#include <cstdio>

using namespace DialogUtils;

CLayerDataPage::CLayerDataPage()
{

}

void CLayerDataPage::FillCombo(CComboBox& combo, const std::string& sTableDefault, const std::vector<std::string>& vecNames,
                               const std::string& sCurrent, std::vector<std::string>& vecItems)
{
	combo.ResetContent();
	vecItems.clear();

	std::wstring sDefault = L"<table default: " + (sTableDefault.empty() ? std::wstring(L"none") : Utf8ToWide(sTableDefault)) + L">";
	combo.AddString(sDefault.c_str());
	vecItems.push_back(std::string());

	int nSelect = 0;
	for(size_t i = 0; i < vecNames.size(); ++i)
	{
		combo.AddString(Utf8ToWide(vecNames[i]).c_str());
		vecItems.push_back(vecNames[i]);
		if(!sCurrent.empty() && vecNames[i] == sCurrent)
			nSelect = (int)vecItems.size() - 1;
	}

	// a field of the layer which isn't a candidate any more (the table has changed): keep it
	if(!sCurrent.empty() && nSelect == 0)
	{
		std::wstring sMissing = Utf8ToWide(sCurrent) + L" (not in the table)";
		combo.AddString(sMissing.c_str());
		vecItems.push_back(sCurrent);
		nSelect = (int)vecItems.size() - 1;
	}
	combo.SetCurSel(nSelect);
}

std::string CLayerDataPage::Selected(const CComboBox& combo, const std::vector<std::string>& vecItems) const
{
	int nSel = combo.GetCurSel();
	return nSel >= 0 && nSel < (int)vecItems.size() ? vecItems[nSel] : std::string();
}

void CLayerDataPage::GetFields(std::string& sOIDField, std::string& sShapeField) const
{
	sOIDField = Selected(m_comboOID, m_vecOIDItems);
	sShapeField = Selected(m_comboShape, m_vecShapeItems);
}

LRESULT CLayerDataPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_comboOID = GetDlgItem(IDC_DATA_OID_FIELD);
	m_comboShape = GetDlgItem(IDC_DATA_SHAPE_FIELD);
	m_listFields = GetDlgItem(IDC_DATA_FIELDS);

	SetDlgItemText(IDC_DATA_TABLE, Utf8ToWide(m_info.sTable).c_str());
	SetDlgItemText(IDC_DATA_WORKSPACE, Utf8ToWide(m_info.sWorkspace).c_str());
	SetDlgItemText(IDC_DATA_GEOMETRY, Utf8ToWide(m_info.sGeometryType).c_str());
	SetDlgItemText(IDC_DATA_SPATREF, m_info.sSpatialReference.empty() ? L"<unknown>" : Utf8ToWide(m_info.sSpatialReference).c_str());

	if(m_info.extent.type & CommonLib::bbox_type_normal)
	{
		wchar_t szExtent[256];
		swprintf(szExtent, 256, L"X %.6g .. %.6g,  Y %.6g .. %.6g", m_info.extent.xMin, m_info.extent.xMax, m_info.extent.yMin, m_info.extent.yMax);
		SetDlgItemText(IDC_DATA_EXTENT, szExtent);
	}
	else
		SetDlgItemText(IDC_DATA_EXTENT, L"<empty>");

	FillCombo(m_comboOID, m_info.sTableOIDField, m_info.vecOIDFields, m_info.sOIDField, m_vecOIDItems);
	FillCombo(m_comboShape, m_info.sTableShapeField, m_info.vecShapeFields, m_info.sShapeField, m_vecShapeItems);

	m_listFields.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	RECT rc = {0};
	m_listFields.GetClientRect(&rc);
	int nWidth = rc.right - rc.left - ::GetSystemMetrics(SM_CXVSCROLL);
	m_listFields.InsertColumn(0, L"Name", LVCFMT_LEFT, nWidth * 45 / 100);
	m_listFields.InsertColumn(1, L"Type", LVCFMT_LEFT, nWidth * 25 / 100);
	m_listFields.InsertColumn(2, L"Used as", LVCFMT_LEFT, nWidth - nWidth * 45 / 100 - nWidth * 25 / 100);
	FillFieldList();
	return FALSE;   // child page: don't take the focus from the dialog
}

void CLayerDataPage::FillFieldList()
{
	std::string sOID, sShape;
	GetFields(sOID, sShape);
	if(sOID.empty())
		sOID = m_info.sTableOIDField;
	if(sShape.empty())
		sShape = m_info.sTableShapeField;

	m_listFields.DeleteAllItems();
	for(size_t i = 0; i < m_info.vecFields.size(); ++i)
	{
		const TestMapDraw::SLayerDataInfo::SField& field = m_info.vecFields[i];
		int nItem = m_listFields.InsertItem((int)i, Utf8ToWide(field.sName).c_str());
		m_listFields.SetItemText(nItem, 1, Utf8ToWide(field.sType).c_str());
		const wchar_t* pszUse = field.sName == sOID ? L"OID" : (field.sName == sShape ? L"Shape" : L"");
		m_listFields.SetItemText(nItem, 2, pszUse);
	}
}

LRESULT CLayerDataPage::OnFieldChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FillFieldList();
	return 0;
}
