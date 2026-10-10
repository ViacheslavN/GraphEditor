// ConvertOSMDlg.cpp : implementation of the CConvertOSMDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#ifdef HAVE_OSM_CONVERTOR

#include "ConvertOSMDlg.h"
#include "OSMProgressDlg.h"
#include "DialogUtils.h"
#include "../../../Convertors/OSM/OSMConvertorLib/OSMConvertor.h"

using namespace DialogUtils;
using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
	const wchar_t* Caption = L"Convert from OpenStreetMap";
	const LPARAM TableItem = 0x10000;   // item data: layer index, TableItem + table index

	enum eColumns { ColName, ColKind, ColCount, ColTable };

	bool FileExists(const std::wstring& sPath)
	{
		return ::GetFileAttributesW(sPath.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool EndsWith(const std::wstring& sText, const wchar_t* pszSuffix)
	{
		size_t nLen = wcslen(pszSuffix);
		return sText.size() >= nLen && _wcsicmp(sText.c_str() + sText.size() - nLen, pszSuffix) == 0;
	}

	// C:\data\czech-republic-latest.osm.pbf -> C:\data\czech-republic-latest.sqlite
	std::wstring DefaultOutputPath(const std::wstring& sOSMPath)
	{
		std::wstring sPath = sOSMPath;
		for(const wchar_t* pszSuffix : {L".osm.pbf", L".pbf", L".osm"})
		{
			if(EndsWith(sPath, pszSuffix))
			{
				sPath.resize(sPath.size() - wcslen(pszSuffix));
				break;
			}
		}
		return sPath + L".sqlite";
	}

	std::wstring GeometryText(eOSMGeometryType type)
	{
		switch(type)
		{
			case OSMGeometryPoint: return L"Points";
			case OSMGeometryLine:  return L"Lines";
			default:               return L"Polygons";
		}
	}

	std::wstring TableText(eOSMTableType type)
	{
		switch(type)
		{
			case OSMTableRoutes:       return L"Table (+ members)";
			case OSMTableRestrictions: return L"Table";
			default:                   return L"Table (key / value)";
		}
	}
}

CConvertOSMDlg::CConvertOSMDlg() : m_bAddToMap(true), m_bExportTags(false), m_bUpdating(false)
{

}

LRESULT CConvertOSMDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	m_editPath = GetDlgItem(IDC_OSM_PATH);
	m_editOutput = GetDlgItem(IDC_OSM_OUTPUT);
	m_listDatasets = GetDlgItem(IDC_OSM_DATASETS);

	m_listDatasets.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listDatasets.InsertColumn(ColName, L"Layer / table", LVCFMT_LEFT, 190);
	m_listDatasets.InsertColumn(ColKind, L"Geometry", LVCFMT_LEFT, 110);
	m_listDatasets.InsertColumn(ColCount, L"Features", LVCFMT_RIGHT, 90);
	m_listDatasets.InsertColumn(ColTable, L"Table name", LVCFMT_LEFT, 150);

	CheckDlgButton(IDC_OSM_WEB_MERCATOR, m_settings.bWebMercator ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_OSM_ADD_TO_MAP, m_bAddToMap ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_OSM_EXPORT_TAGS, m_bExportTags ? BST_CHECKED : BST_UNCHECKED);
	const SOSMCompressSettings& compression = m_settings.compression;
	CheckDlgButton(IDC_OSM_COMPRESS, compression.bEnabled ? BST_CHECKED : BST_UNCHECKED);
	CheckRadioButton(IDC_OSM_SCALE_AUTO, IDC_OSM_SCALE_MANUAL, compression.scale == OSMCompressScaleManual ? IDC_OSM_SCALE_MANUAL :
	                 (compression.scale == OSMCompressScaleMaximum ? IDC_OSM_SCALE_MAX : IDC_OSM_SCALE_AUTO));
	SetDlgItemText(IDC_OSM_SCALE_VALUE, std::to_wstring(compression.nManualScaleExponent).c_str());
	UpdateCompressControls();
	SetDlgItemInt(IDC_OSM_NODE_CACHE, m_settings.nNodeCacheMB, FALSE);

	UpdateInfo();
	return TRUE;
}

void CConvertOSMDlg::UpdateInfo()
{
	bool bRead = m_ptrOSMMap.get() != nullptr;
	GetDlgItem(IDOK).EnableWindow(bRead);
	for(int nId : {IDC_OSM_SELECT_ALL, IDC_OSM_CLEAR_ALL})
		GetDlgItem(nId).EnableWindow(bRead);

	if(!bRead)
	{
		SetDlgItemText(IDC_OSM_INFO, L"Choose an .osm or .osm.pbf file and press Read: its layers and tables are shown here. "
		                         L"Convert all converts everything without reading the file first (faster, no counts).");
		return;
	}

	const CommonLib::bbox& bounds = m_ptrOSMMap->GetBounds();
	wchar_t szText[512];
	swprintf(szText, 512, L"Nodes: %llu, ways: %llu, relations: %llu, %s.\nLongitude %.4f .. %.4f, latitude %.4f .. %.4f",
		(unsigned long long)m_ptrOSMMap->GetNodeCount(), (unsigned long long)m_ptrOSMMap->GetWayCount(),
		(unsigned long long)m_ptrOSMMap->GetRelationCount(), m_ptrOSMMap->IsSorted() ? L"sorted" : L"not sorted (slower conversion)",
		bounds.xMin, bounds.xMax, bounds.yMin, bounds.yMax);
	SetDlgItemText(IDC_OSM_INFO, szText);
}

bool CConvertOSMDlg::ReadMap(const std::wstring& sPath)
{
	if(sPath.empty() || !FileExists(sPath))
	{
		MessageBox(L"The OSM file doesn't exist.", Caption, MB_OK | MB_ICONWARNING);
		return false;
	}

	// readosm opens the file with fopen (ANSI path on Windows)
	std::string sFilePath = WideToFilePath(sPath);
	IOSMMapPtr ptrMap;
	COSMProgressDlg progressDlg(L"Reading OpenStreetMap", [&sFilePath, &ptrMap](IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
	{
		COSMConvertor convertor;
		ptrMap = convertor.ReadMap(sFilePath, ptrProgress, ptrCancel);
	});

	INT_PTR nResult = progressDlg.DoModal(m_hWnd);
	if(nResult == IDABORT)
	{
		std::wstring sMsg = L"Failed to read the OSM file:\n" + progressDlg.GetError();
		MessageBox(sMsg.c_str(), Caption, MB_OK | MB_ICONERROR);
	}
	if(nResult != IDOK || !ptrMap.get())
		return false;

	m_ptrOSMMap = ptrMap;
	m_sReadPath = sPath;
	FillDatasets();
	UpdateInfo();
	UpdateCompressControls();   // the maximum precision is by the bounds of the map

	std::wstring sOutput = Trim(GetText(m_editOutput));
	if(sOutput.empty())
		m_editOutput.SetWindowText(DefaultOutputPath(sPath).c_str());
	return true;
}

void CConvertOSMDlg::FillDatasets()
{
	m_listDatasets.DeleteAllItems();
	if(!m_ptrOSMMap.get())
		return;

	// the datasets without features are unchecked (they are skipped anyway)
	m_bUpdating = true;
	auto addItem = [this](IOSMDatasetPtr ptrDataset, const std::wstring& sKind, LPARAM nData)
	{
		int nItem = m_listDatasets.GetItemCount();
		nItem = m_listDatasets.InsertItem(nItem, Utf8ToWide(ptrDataset->GetDisplayName()).c_str());
		m_listDatasets.SetItemText(nItem, ColKind, sKind.c_str());
		m_listDatasets.SetItemText(nItem, ColCount, std::to_wstring(ptrDataset->GetFeatureCount()).c_str());
		m_listDatasets.SetItemText(nItem, ColTable, Utf8ToWide(ptrDataset->GetTableName()).c_str());
		m_listDatasets.SetItemData(nItem, nData);
		bool bCheck = ptrDataset->GetEnabled() && ptrDataset->GetFeatureCount() > 0;
		IOSMTablePtr ptrTable = std::dynamic_pointer_cast<IOSMTable>(ptrDataset);
		if(ptrTable.get() && ptrTable->GetTableType() == OSMTableTags)
			bCheck = bCheck && m_bExportTags;
		m_listDatasets.SetCheckState(nItem, bCheck);
	};

	for(int i = 0; i < m_ptrOSMMap->GetLayerCount(); ++i)
		addItem(m_ptrOSMMap->GetLayer(i), GeometryText(m_ptrOSMMap->GetLayer(i)->GetGeometryType()), i);
	for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
		addItem(m_ptrOSMMap->GetTable(i), TableText(m_ptrOSMMap->GetTable(i)->GetTableType()), TableItem + i);
	m_bUpdating = false;
}

int CConvertOSMDlg::FindTagsItem() const
{
	if(!m_ptrOSMMap.get())
		return -1;
	for(int i = 0; i < m_listDatasets.GetItemCount(); ++i)
	{
		IOSMTablePtr ptrTable = std::dynamic_pointer_cast<IOSMTable>(GetDataset(i));
		if(ptrTable.get() && ptrTable->GetTableType() == OSMTableTags)
			return i;
	}
	return -1;
}

void CConvertOSMDlg::SetExportTags(bool bExport)
{
	m_bExportTags = bExport;
	m_bUpdating = true;
	CheckDlgButton(IDC_OSM_EXPORT_TAGS, bExport ? BST_CHECKED : BST_UNCHECKED);
	int nItem = FindTagsItem();
	if(nItem >= 0 && (m_listDatasets.GetCheckState(nItem) != FALSE) != bExport)
		m_listDatasets.SetCheckState(nItem, bExport);
	m_bUpdating = false;
}

bool CConvertOSMDlg::GetCompressSettings(SOSMCompressSettings& compression, std::wstring& sError) const
{
	compression.bEnabled = IsDlgButtonChecked(IDC_OSM_COMPRESS) == BST_CHECKED;
	compression.scale = IsDlgButtonChecked(IDC_OSM_SCALE_MANUAL) == BST_CHECKED ? OSMCompressScaleManual :
	                    (IsDlgButtonChecked(IDC_OSM_SCALE_MAX) == BST_CHECKED ? OSMCompressScaleMaximum : OSMCompressScaleAuto);
	if(compression.scale != OSMCompressScaleManual)
		return true;

	std::wstring sValue = Trim(GetText(GetDlgItem(IDC_OSM_SCALE_VALUE)), L" \t");
	wchar_t* pEnd = nullptr;
	long nValue = wcstol(sValue.c_str(), &pEnd, 10);
	if(sValue.empty() || !pEnd || *pEnd != 0 || nValue < -22 || nValue > 22)
	{
		sError = L"Enter the decimal places of the coordinates as a number from -22 to 22 "
		         L"(2 - centimeters of Web Mercator, 7 - 1e-7 degree, -1 - tens of meters).";
		return false;
	}
	compression.nManualScaleExponent = (int)nValue;
	return true;
}

void CConvertOSMDlg::UpdateCompressControls()
{
	SOSMConvertSettings settings = m_settings;
	settings.bWebMercator = IsDlgButtonChecked(IDC_OSM_WEB_MERCATOR) == BST_CHECKED;
	std::wstring sError;
	bool bValid = GetCompressSettings(settings.compression, sError);
	bool bEnabled = settings.compression.bEnabled;

	for(int nId : {IDC_OSM_SCALE_AUTO, IDC_OSM_SCALE_MAX, IDC_OSM_SCALE_MANUAL})
		GetDlgItem(nId).EnableWindow(bEnabled);
	GetDlgItem(IDC_OSM_SCALE_VALUE).EnableWindow(bEnabled && settings.compression.scale == OSMCompressScaleManual);
	GetDlgItem(IDC_OSM_SCALE_INFO).EnableWindow(bEnabled);

	std::wstring sInfo;
	if(!bEnabled)
		sInfo = L"The coordinates are stored as doubles.";
	else if(!bValid)
		sInfo = L"Decimal places: a number from -22 to 22.";
	else
	{
		try
		{
			// the precision the conversion will use: CEnvelope::GetCompressParams (auto), the extent (maximum), manual
			int k = COSMConvertor::CompressParams(settings, m_ptrOSMMap).nScaleExponent;
			int nMax = GeometryCompression::MaxCompressParamsForExtent(COSMConvertor::OutputExtent(settings, m_ptrOSMMap)).nScaleExponent;
			wchar_t szText[256];
			swprintf(szText, 256, L"Step %g %s (%d decimal places).", GeometryCompression::Pow10(-k), settings.bWebMercator ? L"m" : L"degree", k);
			sInfo = szText;
			if(settings.compression.scale == OSMCompressScaleMaximum)
				sInfo += m_ptrOSMMap.get() && m_ptrOSMMap->IsScanned() ? L" By the bounds of the file." : L" For the whole world (Read gives the bounds of the file).";
			else if(k > nMax)
				sInfo += L" More than the maximum " + std::to_wstring(nMax) + L": big coordinates lose digits.";
		}
		catch (std::exception& exc)
		{
			sInfo = ExceptionText(exc);
		}
	}
	SetDlgItemText(IDC_OSM_SCALE_INFO, sInfo.c_str());
}

LRESULT CConvertOSMDlg::OnCompressChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateCompressControls();
	return 0;
}

LRESULT CConvertOSMDlg::OnExportTags(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_bUpdating)
		SetExportTags(IsDlgButtonChecked(IDC_OSM_EXPORT_TAGS) == BST_CHECKED);
	return 0;
}

LRESULT CConvertOSMDlg::OnDatasetChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	// the check box of the tags item is clicked in the list
	NMLISTVIEW* pItem = (NMLISTVIEW*)pnmh;
	if(m_bUpdating || !(pItem->uChanged & LVIF_STATE) || ((pItem->uNewState ^ pItem->uOldState) & LVIS_STATEIMAGEMASK) == 0)
		return 0;
	if(pItem->iItem >= 0 && pItem->iItem == FindTagsItem())
		SetExportTags(m_listDatasets.GetCheckState(pItem->iItem) != FALSE);
	return 0;
}

IOSMDatasetPtr CConvertOSMDlg::GetDataset(int nItem) const
{
	LPARAM nData = m_listDatasets.GetItemData(nItem);
	if(nData >= TableItem)
		return m_ptrOSMMap->GetTable((int)(nData - TableItem));
	return m_ptrOSMMap->GetLayer((int)nData);
}

LRESULT CConvertOSMDlg::OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = Trim(GetText(m_editPath));
	CFileDialog fileDlg(TRUE, _T("pbf"), sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("OpenStreetMap (*.osm;*.pbf)\0*.osm;*.pbf\0All Files (*.*)\0*.*\0"), m_hWnd);
	if(fileDlg.DoModal() != IDOK)
		return 0;

	m_editPath.SetWindowText(fileDlg.m_szFileName);
	m_editOutput.SetWindowText(DefaultOutputPath(fileDlg.m_szFileName).c_str());
	ReadMap(fileDlg.m_szFileName);
	return 0;
}

LRESULT CConvertOSMDlg::OnRead(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ReadMap(Trim(GetText(m_editPath)));
	return 0;
}

LRESULT CConvertOSMDlg::OnPathChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// another file: the layers must be read again
	if(m_ptrOSMMap.get() && Trim(GetText(m_editPath)) != m_sReadPath)
	{
		m_ptrOSMMap.reset();
		m_sReadPath.clear();
		m_listDatasets.DeleteAllItems();
		UpdateInfo();
	}
	return 0;
}

LRESULT CConvertOSMDlg::OnOutputBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = Trim(GetText(m_editOutput));
	CFileDialog fileDlg(FALSE, _T("sqlite"), sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY,
		_T("SQLite database (*.sqlite;*.db)\0*.sqlite;*.db\0All Files (*.*)\0*.*\0"), m_hWnd);
	fileDlg.m_ofn.lpstrTitle = _T("Output SQLite database");
	if(fileDlg.DoModal() != IDOK)
		return 0;

	m_editOutput.SetWindowText(fileDlg.m_szFileName);
	return 0;
}

LRESULT CConvertOSMDlg::OnSelectAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_listDatasets.GetItemCount(); ++i)
		m_listDatasets.SetCheckState(i, wID == IDC_OSM_SELECT_ALL);
	return 0;
}

LRESULT CConvertOSMDlg::OnRun(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_ptrOSMMap.get())
		return 0;

	// the selection
	int nSelected = 0;
	for(int i = 0; i < m_listDatasets.GetItemCount(); ++i)
	{
		IOSMDatasetPtr ptrDataset = GetDataset(i);
		bool bEnabled = m_listDatasets.GetCheckState(i) != FALSE;
		ptrDataset->SetEnabled(bEnabled);
		if(bEnabled && ptrDataset->GetFeatureCount() > 0)
			++nSelected;
	}
	if(nSelected == 0)
	{
		MessageBox(L"Check at least one layer or table with features.", Caption, MB_OK | MB_ICONWARNING);
		return 0;
	}

	if(!PrepareRun())
		return 0;
	EndDialog(IDOK);
	return 0;
}

LRESULT CConvertOSMDlg::OnConvertAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sPath = Trim(GetText(m_editPath));
	if(sPath.empty() || !FileExists(sPath))
	{
		MessageBox(L"The OSM file doesn't exist.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editPath);
		return 0;
	}
	if(Trim(GetText(m_editOutput)).empty())
		m_editOutput.SetWindowText(DefaultOutputPath(sPath).c_str());

	IOSMMapPtr ptrMap;
	try
	{
		// readosm opens the file with fopen (ANSI path on Windows)
		COSMConvertor convertor;
		ptrMap = convertor.CreateMap(WideToFilePath(sPath));
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), Caption, MB_OK | MB_ICONERROR);
		return 0;
	}

	if(!PrepareRun())
		return 0;

	// all the datasets are enabled, the tags table by the check box
	for(int i = 0; i < ptrMap->GetTableCount(); ++i)
	{
		if(ptrMap->GetTable(i)->GetTableType() == OSMTableTags)
			ptrMap->GetTable(i)->SetEnabled(IsDlgButtonChecked(IDC_OSM_EXPORT_TAGS) == BST_CHECKED);
	}
	m_ptrOSMMap = ptrMap;
	m_sReadPath = sPath;
	EndDialog(IDOK);
	return 0;
}

bool CConvertOSMDlg::PrepareRun()
{
	// the output database is created by the conversion
	std::wstring sOutput = Trim(GetText(m_editOutput));
	if(sOutput.empty())
	{
		MessageBox(L"Set the output SQLite database.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editOutput);
		return false;
	}
	if(FileExists(sOutput))
	{
		std::wstring sMsg = L"The database already exists:\n" + sOutput + L"\n\nReplace it?";
		if(MessageBox(sMsg.c_str(), Caption, MB_YESNO | MB_ICONQUESTION) != IDYES)
			return false;

		for(const wchar_t* pszSuffix : {L"", L"-wal", L"-shm", L"-journal"})
		{
			std::wstring sFile = sOutput + pszSuffix;
			if(FileExists(sFile) && !::DeleteFileW(sFile.c_str()))
			{
				std::wstring sError = L"Failed to delete " + sFile + L"\n(is it opened in the map?)";
				MessageBox(sError.c_str(), Caption, MB_OK | MB_ICONERROR);
				return false;
			}
		}
	}

	// settings
	BOOL bOk = FALSE;
	UINT nCacheMB = GetDlgItemInt(IDC_OSM_NODE_CACHE, &bOk, FALSE);
	m_settings.nNodeCacheMB = bOk && nCacheMB >= 16 ? nCacheMB : 16;
	m_settings.bWebMercator = IsDlgButtonChecked(IDC_OSM_WEB_MERCATOR) == BST_CHECKED;
	std::wstring sCompressError;
	if(!GetCompressSettings(m_settings.compression, sCompressError))
	{
		MessageBox(sCompressError.c_str(), Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(GetDlgItem(IDC_OSM_SCALE_VALUE));
		return false;
	}
	wchar_t szLanguage[32] = {0};
	GetDlgItemText(IDC_OSM_LANGUAGE, szLanguage, 32);
	m_settings.sNameLanguage = WideToUtf8(Trim(szLanguage));
	m_bAddToMap = IsDlgButtonChecked(IDC_OSM_ADD_TO_MAP) == BST_CHECKED;
	m_sOutputPath = sOutput;
	return true;
}

LRESULT CConvertOSMDlg::OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(IDCANCEL);
	return 0;
}

#endif
