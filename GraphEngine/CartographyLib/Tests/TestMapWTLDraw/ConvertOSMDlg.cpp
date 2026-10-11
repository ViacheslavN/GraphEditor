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
#include "../../../Convertors/OSM/OSMConvertorLib/OSMNodeStore.h"
#include <filesystem>

using namespace DialogUtils;
using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
	const wchar_t* Caption = L"Convert from OpenStreetMap";
	const int TableItem = 0x10000;   // SItem::nDataset: layer index, TableItem + table index

	bool FileExists(const std::wstring& sPath)
	{
		return ::GetFileAttributesW(sPath.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool EndsWith(const std::wstring& sText, const wchar_t* pszSuffix)
	{
		size_t nLen = wcslen(pszSuffix);
		return sText.size() >= nLen && _wcsicmp(sText.c_str() + sText.size() - nLen, pszSuffix) == 0;
	}

	bool IsSplitData(const std::wstring& sPath)
	{
		return EndsWith(sPath, L"osm_split.xml");
	}

	// C:\data\czech-republic-latest.osm.pbf -> C:\data\czech-republic-latest.sqlite,
	// C:\data\czech_split\osm_split.xml -> C:\data\czech_split.sqlite
	std::wstring DefaultOutputPath(const std::wstring& sOSMPath)
	{
		if(IsSplitData(sOSMPath))
			return std::filesystem::path(sOSMPath).parent_path().wstring() + L".sqlite";
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
			case OSMTableRoutes:       return L"table (+ members)";
			case OSMTableRestrictions: return L"table";
			default:                   return L"table (key / value)";
		}
	}

	// 1234567 -> 1 234 567
	std::wstring CountText(uint64_t nCount)
	{
		std::wstring sDigits = std::to_wstring(nCount);
		std::wstring sText;
		for(size_t i = 0; i < sDigits.size(); ++i)
		{
			if(i > 0 && (sDigits.size() - i) % 3 == 0)
				sText += L' ';
			sText += sDigits[i];
		}
		return sText;
	}

	// Roads: lines, 12 345 features -> osm_roads
	std::wstring DatasetText(const IOSMDataset& dataset, const std::wstring& sKind, bool bScanned)
	{
		std::wstring sText = Utf8ToWide(dataset.GetDisplayName()) + L": " + sKind;
		if(bScanned)
			sText += L", " + CountText(dataset.GetFeatureCount()) + L" features";
		return sText + L" -> " + Utf8ToWide(dataset.GetTableName());
	}

	// addr_street (addr:street): text, 1 234 / osm_id: integer, always
	std::wstring ColumnText(const SOSMColumn& column, bool bScanned)
	{
		std::wstring sText = Utf8ToWide(column.sName);
		if(!column.sKey.empty() && column.sKey != column.sName)
			sText += L" (" + Utf8ToWide(column.sKey) + L")";
		sText += column.bInteger ? L": integer" : L": text";
		if(bScanned)
			sText += L", " + CountText(column.nCount);
		if(column.bRequired)
			sText += L", always";
		return sText;
	}
}

std::wstring CConvertOSMDlg::s_sTempDir;

CConvertOSMDlg::CConvertOSMDlg() : m_bAddToMap(true), m_bExportTags(false), m_bUpdating(false)
{

}

LRESULT CConvertOSMDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	m_editPath = GetDlgItem(IDC_OSM_PATH);
	m_editOutput = GetDlgItem(IDC_OSM_OUTPUT);
	m_treeDatasets = GetDlgItem(IDC_OSM_DATASETS);
	// TVS_CHECKBOXES is set after the control is created (see the documentation of the style)
	m_treeDatasets.ModifyStyle(0, TVS_CHECKBOXES);

	CheckDlgButton(IDC_OSM_WEB_MERCATOR, m_settings.bWebMercator ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_OSM_ADD_TO_MAP, m_bAddToMap ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_OSM_KEEP_NODES, BST_CHECKED);
	CheckDlgButton(IDC_OSM_EXPORT_TAGS, m_bExportTags ? BST_CHECKED : BST_UNCHECKED);
	const SOSMCompressSettings& compression = m_settings.compression;
	CheckDlgButton(IDC_OSM_COMPRESS, compression.bEnabled ? BST_CHECKED : BST_UNCHECKED);
	CheckRadioButton(IDC_OSM_SCALE_AUTO, IDC_OSM_SCALE_MANUAL, compression.scale == OSMCompressScaleManual ? IDC_OSM_SCALE_MANUAL :
	                 (compression.scale == OSMCompressScaleMaximum ? IDC_OSM_SCALE_MAX : IDC_OSM_SCALE_AUTO));
	SetDlgItemText(IDC_OSM_SCALE_VALUE, std::to_wstring(compression.nManualScaleExponent).c_str());
	UpdateCompressControls();
	SetDlgItemInt(IDC_OSM_NODE_CACHE, m_settings.nNodeCacheMB, FALSE);
	SetDlgItemInt(IDC_OSM_WRITE_BUFFER, m_settings.nNodeWriteBufferMB, FALSE);

	// the order is eOSMNodeStore
	CComboBox comboStore = GetDlgItem(IDC_OSM_NODE_STORE);
	comboStore.AddString(L"Auto (by the file size)");
	comboStore.AddString(L"Sorted (extracts: countries)");
	comboStore.AddString(L"Flat file (continents, the planet)");
	comboStore.SetCurSel((int)m_settings.nodeStore);
	UpdateNodeStoreInfo();

	SetDlgItemText(IDC_OSM_TEMP_DIR, s_sTempDir.c_str());
	UpdateTempInfo();

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
		SetDlgItemText(IDC_OSM_INFO, L"Choose an .osm or .osm.pbf file and press Analyze: its layers / tables and their columns are shown here. "
		                         L"Convert all converts everything without the analysis (faster, no counts, no OSM key columns).");
		return;
	}

	const CommonLib::bbox& bounds = m_ptrOSMMap->GetBounds();
	wchar_t szText[512];
	swprintf(szText, 512, L"%sNodes: %llu, ways: %llu, relations: %llu, %s.\nLongitude %.4f .. %.4f, latitude %.4f .. %.4f",
		m_ptrOSMMap->GetFormat() == OSMFormatSplit ? L"Split data (exported without the OSM file). " : L"",
		(unsigned long long)m_ptrOSMMap->GetNodeCount(), (unsigned long long)m_ptrOSMMap->GetWayCount(),
		(unsigned long long)m_ptrOSMMap->GetRelationCount(), m_ptrOSMMap->IsSorted() ? L"sorted" : L"not sorted (slower conversion)",
		bounds.xMin, bounds.xMax, bounds.yMin, bounds.yMax);
	SetDlgItemText(IDC_OSM_INFO, szText);
}

bool CConvertOSMDlg::AnalyzeMap(const std::wstring& sPath)
{
	if(sPath.empty() || !FileExists(sPath))
	{
		MessageBox(L"The OSM file doesn't exist.", Caption, MB_OK | MB_ICONWARNING);
		return false;
	}

	// the OSM reader takes UTF-8 names
	std::string sFilePath = WideToUtf8(sPath);
	IOSMMapPtr ptrMap;
	COSMProgressDlg progressDlg(L"Analyzing OpenStreetMap", [&sFilePath, &ptrMap](IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
	{
		COSMConvertor convertor;
		ptrMap = convertor.Analyze(sFilePath, ptrProgress, ptrCancel);
	});

	INT_PTR nResult = progressDlg.DoModal(m_hWnd);
	if(nResult == IDABORT)
	{
		std::wstring sMsg = L"Failed to analyze the OSM file:\n" + progressDlg.GetError();
		MessageBox(sMsg.c_str(), Caption, MB_OK | MB_ICONERROR);
	}
	if(nResult != IDOK || !ptrMap.get())
		return false;

	SetMap(ptrMap, sPath);
	return true;
}

void CConvertOSMDlg::SetMap(IOSMMapPtr ptrMap, const std::wstring& sPath)
{
	m_ptrOSMMap = ptrMap;
	m_sReadPath = sPath;    // before the path changes: the map is kept (OnPathChanged)
	if(Trim(GetText(m_editPath)) != sPath)
		m_editPath.SetWindowText(sPath.c_str());
	FillDatasets();
	UpdateInfo();
	UpdateCompressControls();   // the maximum precision is by the bounds of the map

	std::wstring sOutput = Trim(GetText(m_editOutput));
	if(sOutput.empty() || IsSplitData(sPath))
		m_editOutput.SetWindowText(DefaultOutputPath(sPath).c_str());
}

LRESULT CConvertOSMDlg::OnSplit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sPath = Trim(GetText(m_editPath));
	if(sPath.empty() || !FileExists(sPath) || IsSplitData(sPath))
	{
		MessageBox(L"Choose an .osm or .osm.pbf file to split.", Caption, MB_OK | MB_ICONWARNING);
		GotoDlgCtrl(m_editPath);
		return 0;
	}

	// the folder <file name>_split is created in the chosen folder
	std::filesystem::path osmPath(sPath);
	std::wstring sName = osmPath.filename().wstring();
	for(const wchar_t* pszSuffix : {L".osm.pbf", L".pbf", L".osm"})
	{
		if(EndsWith(sName, pszSuffix))
		{
			sName.resize(sName.size() - wcslen(pszSuffix));
			break;
		}
	}
	sName += L"_split";
	std::wstring sTitle = L"Folder for the split data (the folder " + sName + L" is created in it: nodes, ways, relations, metadata)";
	CFolderDialog dlg(m_hWnd, sTitle.c_str(), BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE);
	dlg.SetInitialFolder(osmPath.parent_path().wstring().c_str());
	if(dlg.DoModal() != IDOK)
		return 0;
	std::wstring sFolder = (std::filesystem::path(dlg.GetFolderPath()) / sName).wstring();

	if(FileExists((std::filesystem::path(sFolder) / L"osm_split.xml").wstring()))
	{
		std::wstring sMsg = L"The split data already exist:\n" + sFolder + L"\n\nReplace them?";
		if(MessageBox(sMsg.c_str(), Caption, MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
	}

	bool bKeepNodes = IsDlgButtonChecked(IDC_OSM_KEEP_NODES) == BST_CHECKED;
	std::string sFilePath = WideToUtf8(sPath);
	std::string sFolderUtf8 = WideToUtf8(sFolder);
	// the node store as for the conversion (Auto: flat for the planet / continents, sorted for extracts), its memory,
	// the temporary folder (empty - the split folder)
	SOSMConvertSettings settings = m_settings;
	int nStore = CComboBox(GetDlgItem(IDC_OSM_NODE_STORE)).GetCurSel();
	settings.nodeStore = nStore == OSMNodeStoreSorted ? OSMNodeStoreSorted : (nStore == OSMNodeStoreFlat ? OSMNodeStoreFlat : OSMNodeStoreAuto);
	BOOL bCacheOk = FALSE;
	UINT nCacheMB = GetDlgItemInt(IDC_OSM_NODE_CACHE, &bCacheOk, FALSE);
	settings.nNodeCacheMB = bCacheOk && nCacheMB >= 16 ? nCacheMB : 16;
	UINT nBufferMB = GetDlgItemInt(IDC_OSM_WRITE_BUFFER, &bCacheOk, FALSE);
	settings.nNodeWriteBufferMB = bCacheOk && nBufferMB >= 16 ? nBufferMB : 16;
	settings.sTempDir = WideToUtf8(Trim(GetText(GetDlgItem(IDC_OSM_TEMP_DIR))));
	IOSMMapPtr ptrMap;
	COSMProgressDlg progressDlg(L"Splitting OpenStreetMap", [&](IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
	{
		COSMConvertor convertor(settings);
		ptrMap = convertor.Split(sFilePath, sFolderUtf8, bKeepNodes, ptrProgress, ptrCancel);
	});

	INT_PTR nResult = progressDlg.DoModal(m_hWnd);

	// the report of the stages next to the data, also for a canceled / failed splitting
	std::wstring sLogPath = (std::filesystem::path(sFolder) / L"split.log").wstring();
	{
		std::string sResult = nResult == IDOK ? "done" : (nResult == IDCANCEL ? "canceled" : "failed: " + WideToUtf8(progressDlg.GetError()));
		std::string sReport = COSMConvertor::FormatReport(progressDlg.GetLastProgress(), sResult);
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(sFolder), ec);
		if(FILE* pFile = _wfopen(sLogPath.c_str(), L"wb"))
		{
			fwrite(sReport.data(), 1, sReport.size(), pFile);
			fclose(pFile);
		}
	}
	if(nResult == IDABORT)
	{
		std::wstring sMsg = L"Failed to split the OSM file:\n" + progressDlg.GetError() + L"\n\nThe details of the stages: " + sLogPath;
		MessageBox(sMsg.c_str(), Caption, MB_OK | MB_ICONERROR);
	}
	else if(nResult == IDCANCEL)
	{
		std::wstring sMsg = L"The splitting is canceled.\n\nThe details of the stages: " + sLogPath;
		MessageBox(sMsg.c_str(), Caption, MB_OK | MB_ICONINFORMATION);
	}
	if(nResult != IDOK || !ptrMap.get())
		return 0;

	SetMap(ptrMap, (std::filesystem::path(sFolder) / L"osm_split.xml").wstring());
	int nSeconds = (int)progressDlg.GetElapsedSeconds();
	wchar_t szText[1024];
	swprintf(szText, 1024, L"The OSM file is split into\n%s\nin %d min %d s (the details: split.log there).\n\n"
		L"Run exports the checked layers from the split data; Browse opens them later (osm_split.xml).",
		sFolder.c_str(), nSeconds / 60, nSeconds % 60);
	MessageBox(szText, Caption, MB_OK | MB_ICONINFORMATION);
	return 0;
}

void CConvertOSMDlg::FillDatasets()
{
	m_bUpdating = true;
	m_treeDatasets.DeleteAllItems();
	m_vecItems.clear();
	if(!m_ptrOSMMap.get())
	{
		m_bUpdating = false;
		return;
	}

	// everything is checked: the datasets without features are skipped anyway, the tags table by the check box;
	// the required columns have no check box
	const bool bScanned = m_ptrOSMMap->IsScanned();
	auto addItem = [this](HTREEITEM hParent, const std::wstring& sText, int nDataset, int nColumn) -> HTREEITEM
	{
		SItem item;
		item.nDataset = nDataset;
		item.nColumn = nColumn;
		m_vecItems.push_back(item);
		HTREEITEM hItem = m_treeDatasets.InsertItem(TVIF_TEXT | TVIF_PARAM, sText.c_str(), 0, 0, 0, 0, (LPARAM)m_vecItems.size(), hParent, TVI_LAST);
		return hItem;
	};
	auto addDataset = [&](IOSMDatasetPtr ptrDataset, const std::wstring& sKind, int nDataset)
	{
		HTREEITEM hDataset = addItem(TVI_ROOT, DatasetText(*ptrDataset, sKind, bScanned), nDataset, -1);
		bool bCheck = !bScanned || ptrDataset->GetFeatureCount() > 0;
		IOSMTablePtr ptrTable = std::dynamic_pointer_cast<IOSMTable>(ptrDataset);
		if(ptrTable.get() && ptrTable->GetTableType() == OSMTableTags)
			bCheck = bCheck && m_bExportTags;
		m_treeDatasets.SetCheckState(hDataset, bCheck);

		for(int c = 0; c < ptrDataset->GetColumnCount(); ++c)
		{
			const SOSMColumn& column = ptrDataset->GetColumn(c);
			HTREEITEM hColumn = addItem(hDataset, ColumnText(column, bScanned), nDataset, c);
			if(column.bRequired)
				m_treeDatasets.SetItemState(hColumn, INDEXTOSTATEIMAGEMASK(0), TVIS_STATEIMAGEMASK);
			else
				m_treeDatasets.SetCheckState(hColumn, TRUE);
		}
	};

	for(int i = 0; i < m_ptrOSMMap->GetLayerCount(); ++i)
		addDataset(m_ptrOSMMap->GetLayer(i), GeometryText(m_ptrOSMMap->GetLayer(i)->GetGeometryType()), i);
	for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
		addDataset(m_ptrOSMMap->GetTable(i), TableText(m_ptrOSMMap->GetTable(i)->GetTableType()), TableItem + i);
	m_bUpdating = false;
}

const CConvertOSMDlg::SItem* CConvertOSMDlg::GetItem(HTREEITEM hItem) const
{
	if(!hItem)
		return nullptr;
	LPARAM nData = m_treeDatasets.GetItemData(hItem);
	if(nData <= 0 || nData > (LPARAM)m_vecItems.size())
		return nullptr;
	return &m_vecItems[nData - 1];
}

IOSMDatasetPtr CConvertOSMDlg::GetDataset(int nDataset) const
{
	if(nDataset >= TableItem)
		return m_ptrOSMMap->GetTable(nDataset - TableItem);
	return m_ptrOSMMap->GetLayer(nDataset);
}

HTREEITEM CConvertOSMDlg::FindTagsItem() const
{
	if(!m_ptrOSMMap.get())
		return NULL;
	for(HTREEITEM hItem = m_treeDatasets.GetRootItem(); hItem; hItem = m_treeDatasets.GetNextSiblingItem(hItem))
	{
		const SItem* pItem = GetItem(hItem);
		if(!pItem)
			continue;
		IOSMTablePtr ptrTable = std::dynamic_pointer_cast<IOSMTable>(GetDataset(pItem->nDataset));
		if(ptrTable.get() && ptrTable->GetTableType() == OSMTableTags)
			return hItem;
	}
	return NULL;
}

void CConvertOSMDlg::SetExportTags(bool bExport)
{
	m_bExportTags = bExport;
	m_bUpdating = true;
	CheckDlgButton(IDC_OSM_EXPORT_TAGS, bExport ? BST_CHECKED : BST_UNCHECKED);
	HTREEITEM hItem = FindTagsItem();
	if(hItem && (m_treeDatasets.GetCheckState(hItem) != FALSE) != bExport)
		m_treeDatasets.SetCheckState(hItem, bExport);
	m_bUpdating = false;
}

CLayerColumnExcludeList CConvertOSMDlg::BuildExcludeList() const
{
	CLayerColumnExcludeList excludeList;
	for(HTREEITEM hDataset = m_treeDatasets.GetRootItem(); hDataset; hDataset = m_treeDatasets.GetNextSiblingItem(hDataset))
	{
		const SItem* pItem = GetItem(hDataset);
		if(!pItem)
			continue;
		IOSMDatasetPtr ptrDataset = GetDataset(pItem->nDataset);
		if(!m_treeDatasets.GetCheckState(hDataset))
			excludeList.ExcludeLayer(ptrDataset->GetName());

		// the columns are kept also for an excluded dataset: it can be included again by the list
		for(HTREEITEM hColumn = m_treeDatasets.GetChildItem(hDataset); hColumn; hColumn = m_treeDatasets.GetNextSiblingItem(hColumn))
		{
			const SItem* pColumn = GetItem(hColumn);
			if(!pColumn || pColumn->nColumn < 0)
				continue;
			const SOSMColumn& column = ptrDataset->GetColumn(pColumn->nColumn);
			if(!column.bRequired && !m_treeDatasets.GetCheckState(hColumn))
				excludeList.ExcludeColumn(ptrDataset->GetName(), column.sName);
		}
	}
	return excludeList;
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
	// the check box of the tags item is clicked in the tree
	NMTVITEMCHANGE* pChange = (NMTVITEMCHANGE*)pnmh;
	if(m_bUpdating || !(pChange->uChanged & TVIF_STATE) || ((pChange->uStateNew ^ pChange->uStateOld) & TVIS_STATEIMAGEMASK) == 0)
		return 0;
	if(pChange->hItem && pChange->hItem == FindTagsItem())
		SetExportTags(m_treeDatasets.GetCheckState(pChange->hItem) != FALSE);
	return 0;
}

LRESULT CConvertOSMDlg::OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = Trim(GetText(m_editPath));
	CFileDialog fileDlg(TRUE, _T("pbf"), sCurrent.empty() ? NULL : sCurrent.c_str(), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("OpenStreetMap (*.osm;*.pbf;osm_split.xml)\0*.osm;*.pbf;osm_split.xml\0All Files (*.*)\0*.*\0"), m_hWnd);
	if(fileDlg.DoModal() != IDOK)
		return 0;

	// only the path: the file is analyzed by Analyze (a big file takes minutes) or converted at once by Convert all
	m_editPath.SetWindowText(fileDlg.m_szFileName);
	m_editOutput.SetWindowText(DefaultOutputPath(fileDlg.m_szFileName).c_str());
	UpdateInfo();
	GotoDlgCtrl(GetDlgItem(IDC_OSM_ANALYZE));
	return 0;
}

LRESULT CConvertOSMDlg::OnAnalyze(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AnalyzeMap(Trim(GetText(m_editPath)));
	return 0;
}

LRESULT CConvertOSMDlg::OnNodeStoreChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateNodeStoreInfo();
	return 0;
}

void CConvertOSMDlg::UpdateNodeStoreInfo()
{
	int nSel = CComboBox(GetDlgItem(IDC_OSM_NODE_STORE)).GetCurSel();
	eOSMNodeStore type = nSel == OSMNodeStoreSorted ? OSMNodeStoreSorted : (nSel == OSMNodeStoreFlat ? OSMNodeStoreFlat : OSMNodeStoreAuto);

	std::wstring sPath = Trim(GetText(m_editPath));
	std::error_code ec;
	uint64_t nFileSize = sPath.empty() ? 0 : std::filesystem::file_size(std::filesystem::path(sPath), ec);
	if(ec)
		nFileSize = 0;
	eOSMNodeStore resolved = IOSMNodeStore::ResolveType(type, WideToUtf8(sPath), nFileSize);

	// the disk space in the temporary folder: 16 bytes per node / 8 bytes per id up to the biggest one (~14 billion now, sparse)
	std::wstring sInfo;
	if(type == OSMNodeStoreAuto)
		sInfo = resolved == OSMNodeStoreFlat ? L"Flat for this file. " : L"Sorted for this file. ";
	sInfo += resolved == OSMNodeStoreFlat ? L"Temporary file up to ~115 GB (sparse), 64-bit only" :
	                                        L"Temporary file of 16 bytes per node, the cache in memory";
	SetDlgItemText(IDC_OSM_NODE_STORE_INFO, sInfo.c_str());
}

std::wstring CConvertOSMDlg::TempFolder() const
{
	std::wstring sFolder = Trim(GetText(GetDlgItem(IDC_OSM_TEMP_DIR)));
	if(!sFolder.empty())
		return sFolder;
	// the drive of the result is chosen for the big data: the temporary files go next to it
	std::wstring sOutput = Trim(GetText(GetDlgItem(IDC_OSM_OUTPUT)));
	if(sOutput.empty())
		return std::wstring();
	return std::filesystem::path(sOutput).parent_path().wstring();
}

void CConvertOSMDlg::UpdateTempInfo()
{
	std::wstring sFolder = TempFolder();
	if(sFolder.empty())
	{
		SetDlgItemText(IDC_OSM_TEMP_INFO, L"Empty - the folder of the output database (the planet needs about 150 GB there)");
		return;
	}

	// the free space of the nearest existing folder (the folder is created by Run)
	std::filesystem::path path(sFolder);
	std::error_code ec;
	while(!path.empty() && !std::filesystem::exists(path, ec) && path.has_parent_path() && path.parent_path() != path)
		path = path.parent_path();
	std::wstring sInfo = Trim(GetText(GetDlgItem(IDC_OSM_TEMP_DIR))).empty() ? L"The output folder: " : L"";
	std::filesystem::space_info space = std::filesystem::space(path, ec);
	if(!ec)
	{
		wchar_t szFree[64];
		swprintf(szFree, 64, L"%.1f GB free", space.available / (1024. * 1024 * 1024));
		sInfo += std::wstring(szFree) + L" (the node coordinates of the planet need ~115 GB, of a country 16 bytes per node)";
	}
	else
		sInfo += L"the folder isn't available";
	SetDlgItemText(IDC_OSM_TEMP_INFO, sInfo.c_str());
}

LRESULT CConvertOSMDlg::OnTempChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateTempInfo();
	return 0;
}

LRESULT CConvertOSMDlg::OnTempBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	std::wstring sCurrent = TempFolder();
	CFolderDialog dlg(m_hWnd, L"Folder for the temporary files of the conversion (node coordinates, multipolygon ways)",
	                  BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE);
	if(!sCurrent.empty())
		dlg.SetInitialFolder(sCurrent.c_str());
	if(dlg.DoModal() != IDOK)
		return 0;
	SetDlgItemText(IDC_OSM_TEMP_DIR, dlg.GetFolderPath());
	return 0;
}

LRESULT CConvertOSMDlg::OnPathChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UpdateNodeStoreInfo();
	// another file: it must be analyzed again
	if(m_ptrOSMMap.get() && Trim(GetText(m_editPath)) != m_sReadPath)
	{
		m_ptrOSMMap.reset();
		m_sReadPath.clear();
		FillDatasets();
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
	// the layers / tables and their columns (the required columns have no check box)
	BOOL bCheck = wID == IDC_OSM_SELECT_ALL;
	for(HTREEITEM hDataset = m_treeDatasets.GetRootItem(); hDataset; hDataset = m_treeDatasets.GetNextSiblingItem(hDataset))
	{
		m_treeDatasets.SetCheckState(hDataset, bCheck);
		for(HTREEITEM hColumn = m_treeDatasets.GetChildItem(hDataset); hColumn; hColumn = m_treeDatasets.GetNextSiblingItem(hColumn))
		{
			const SItem* pItem = GetItem(hColumn);
			if(pItem && pItem->nColumn >= 0 && !GetDataset(pItem->nDataset)->GetColumn(pItem->nColumn).bRequired)
				m_treeDatasets.SetCheckState(hColumn, bCheck);
		}
	}
	return 0;
}

LRESULT CConvertOSMDlg::OnRun(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_ptrOSMMap.get())
		return 0;

	// the selection: the unchecked layers / tables / columns are excluded
	CLayerColumnExcludeList excludeList = BuildExcludeList();
	int nSelected = 0;
	for(int i = 0; i < m_ptrOSMMap->GetLayerCount(); ++i)
	{
		IOSMLayerPtr ptrLayer = m_ptrOSMMap->GetLayer(i);
		if(!excludeList.IsLayerExcluded(ptrLayer->GetName()) && ptrLayer->GetFeatureCount() > 0)
			++nSelected;
	}
	for(int i = 0; i < m_ptrOSMMap->GetTableCount(); ++i)
	{
		IOSMTablePtr ptrTable = m_ptrOSMMap->GetTable(i);
		if(!excludeList.IsLayerExcluded(ptrTable->GetName()) && ptrTable->GetFeatureCount() > 0)
			++nSelected;
	}
	if(nSelected == 0)
	{
		MessageBox(L"Check at least one layer or table with features.", Caption, MB_OK | MB_ICONWARNING);
		return 0;
	}

	if(!PrepareRun())
		return 0;
	m_excludeList = excludeList;
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
		COSMConvertor convertor;
		ptrMap = convertor.CreateMap(WideToUtf8(sPath));   // UTF-8 names
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), Caption, MB_OK | MB_ICONERROR);
		return 0;
	}

	if(!PrepareRun())
		return 0;

	// everything is converted, the tags table by the check box
	m_excludeList.Clear();
	if(IsDlgButtonChecked(IDC_OSM_EXPORT_TAGS) != BST_CHECKED)
	{
		for(int i = 0; i < ptrMap->GetTableCount(); ++i)
		{
			if(ptrMap->GetTable(i)->GetTableType() == OSMTableTags)
				m_excludeList.ExcludeLayer(ptrMap->GetTable(i)->GetName());
		}
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
	UINT nBufferMB = GetDlgItemInt(IDC_OSM_WRITE_BUFFER, &bOk, FALSE);
	m_settings.nNodeWriteBufferMB = bOk && nBufferMB >= 16 ? nBufferMB : 16;

	// the temporary files: the folder is created when it doesn't exist
	s_sTempDir = Trim(GetText(GetDlgItem(IDC_OSM_TEMP_DIR)));
	std::wstring sTempDir = TempFolder();
	if(!sTempDir.empty())
	{
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(sTempDir), ec);
		if(!std::filesystem::is_directory(std::filesystem::path(sTempDir), ec))
		{
			std::wstring sMsg = L"The folder for the temporary files can't be created:\n" + sTempDir;
			MessageBox(sMsg.c_str(), Caption, MB_OK | MB_ICONWARNING);
			GotoDlgCtrl(GetDlgItem(IDC_OSM_TEMP_DIR));
			return false;
		}
	}
	m_settings.sTempDir = WideToUtf8(sTempDir);   // empty - the system temporary folder
	int nStore = CComboBox(GetDlgItem(IDC_OSM_NODE_STORE)).GetCurSel();
	m_settings.nodeStore = nStore == OSMNodeStoreSorted ? OSMNodeStoreSorted : (nStore == OSMNodeStoreFlat ? OSMNodeStoreFlat : OSMNodeStoreAuto);
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
