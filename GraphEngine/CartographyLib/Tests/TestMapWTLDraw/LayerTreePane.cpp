// LayerTreePane.cpp : implementation of the CLayerTreePane class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MapView.h"
#include "LayerTreePane.h"
#include "DialogUtils.h"
#include "../../legend/Legend.h"
#include "../../../DisplayLib/Symbols/SymbolPreview.h"

#include <algorithm>

using namespace GraphEngine;
using namespace DialogUtils;

namespace
{
	const int BaseImageSize = 16;   // at 96 dpi

	// 32 bit top-down DIB, the pixels are 0xAARRGGBB
	HBITMAP CreateDib(int cx, int cy, DWORD** ppBits)
	{
		BITMAPINFO bmi = {0};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = cx;
		bmi.bmiHeader.biHeight = -cy;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		return ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void**)ppBits, NULL, 0);
	}

	// GDI doesn't set the alpha, the 32 bit image list takes it: the images are opaque
	void MakeOpaque(DWORD* pBits, int nCount)
	{
		for(int i = 0; i < nCount; ++i)
			pBits[i] |= 0xFF000000;
	}

	std::wstring LayerText(Cartography::ILayerPtr ptrLayer)
	{
		std::wstring sName = Utf8ToWide(ptrLayer->GetName());
		if(sName.empty())
			sName = L"<no name>";
		return sName;
	}
}

CLayerTreePane::CLayerTreePane() :
	m_pView(nullptr),
	m_nImageSize(BaseImageSize),
	m_dDpi(96.),
	m_bUpdating(false),
	m_nLayersCounter(0),
	m_nRevision(0),
	m_bDragging(false),
	m_hDragItem(NULL)
{

}

CLayerTreePane::~CLayerTreePane()
{
	if(m_images.m_hImageList)
		m_images.Destroy();
}

LRESULT CLayerTreePane::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDC hDC = GetDC();
	m_dDpi = (double)::GetDeviceCaps(hDC, LOGPIXELSX);
	ReleaseDC(hDC);
	m_nImageSize = ::MulDiv(BaseImageSize, (int)m_dDpi, 96);

	m_tree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
		TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_LAYER_TREE);
	// check boxes are set after the creation, before the items (see TVS_CHECKBOXES)
	m_tree.ModifyStyle(0, TVS_CHECKBOXES);
	CreateImages();
	return 0;
}

LRESULT CLayerTreePane::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_vecNodes.clear();
	m_ptrMap.reset();
	bHandled = FALSE;
	return 0;
}

LRESULT CLayerTreePane::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	RECT rc;
	GetClientRect(&rc);
	if(m_tree.IsWindow())
		m_tree.SetWindowPos(NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
	return 0;
}

LRESULT CLayerTreePane::OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if(m_tree.IsWindow())
		m_tree.SetFocus();
	return 0;
}

// ---------------- images

HBITMAP CLayerTreePane::CreateBaseImage(eBaseImage image) const
{
	const int n = m_nImageSize;
	DWORD* pBits = nullptr;
	HBITMAP hBitmap = CreateDib(n, n, &pBits);
	if(!hBitmap)
		return NULL;

	HDC hDC = ::CreateCompatibleDC(NULL);
	HGDIOBJ hOld = ::SelectObject(hDC, hBitmap);
	RECT rcAll = {0, 0, n, n};
	HBRUSH hBg = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
	::FillRect(hDC, &rcAll, hBg);
	::DeleteObject(hBg);

	auto box = [&](int l, int t, int r, int b, COLORREF fill, COLORREF line)
	{
		HBRUSH hBrush = ::CreateSolidBrush(fill);
		HPEN hPen = ::CreatePen(PS_SOLID, 1, line);
		HGDIOBJ hOldBrush = ::SelectObject(hDC, hBrush);
		HGDIOBJ hOldPen = ::SelectObject(hDC, hPen);
		::Rectangle(hDC, l * n / 16, t * n / 16, r * n / 16, b * n / 16);
		::SelectObject(hDC, hOldBrush);
		::SelectObject(hDC, hOldPen);
		::DeleteObject(hBrush);
		::DeleteObject(hPen);
	};

	switch(image)
	{
		case ImageMap:
		{
			// globe
			HBRUSH hBrush = ::CreateSolidBrush(RGB(120, 180, 230));
			HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(40, 90, 150));
			HGDIOBJ hOldBrush = ::SelectObject(hDC, hBrush);
			HGDIOBJ hOldPen = ::SelectObject(hDC, hPen);
			::Ellipse(hDC, n / 16, n / 16, n * 15 / 16, n * 15 / 16);
			::MoveToEx(hDC, n / 16, n / 2, NULL);
			::LineTo(hDC, n * 15 / 16, n / 2);
			::Arc(hDC, n * 5 / 16, n / 16, n * 11 / 16, n * 15 / 16, n / 2, n / 16, n / 2, n * 15 / 16);
			::SelectObject(hDC, hOldBrush);
			::SelectObject(hDC, hOldPen);
			::DeleteObject(hBrush);
			::DeleteObject(hPen);
			break;
		}
		case ImageGroup:
			// folder
			box(1, 3, 8, 6, RGB(230, 190, 80), RGB(160, 120, 30));
			box(1, 5, 15, 14, RGB(250, 210, 100), RGB(160, 120, 30));
			break;
		case ImageFeatureLayer:
			// three shapes
			box(1, 8, 9, 15, RGB(250, 200, 80), RGB(120, 90, 20));
			box(6, 2, 15, 10, RGB(130, 200, 160), RGB(40, 110, 70));
			break;
		case ImageRasterLayer:
			// cells
			box(1, 1, 9, 9, RGB(90, 150, 90), RGB(60, 60, 60));
			box(8, 1, 15, 9, RGB(200, 190, 120), RGB(60, 60, 60));
			box(1, 8, 9, 15, RGB(160, 200, 230), RGB(60, 60, 60));
			box(8, 8, 15, 15, RGB(120, 120, 120), RGB(60, 60, 60));
			break;
		default:
			break;   // blank
	}

	::GdiFlush();
	::SelectObject(hDC, hOld);
	::DeleteDC(hDC);
	MakeOpaque(pBits, n * n);
	return hBitmap;
}

void CLayerTreePane::CreateImages()
{
	// the tree doesn't own the image list: the old one is destroyed after the tree takes the new one
	CImageList oldImages;
	if(m_images.m_hImageList)
		oldImages.Attach(m_images.Detach());

	m_images.Create(m_nImageSize, m_nImageSize, ILC_COLOR32, BaseImageCount + 16, 16);
	for(int i = 0; i < BaseImageCount; ++i)
		AddImage(CreateBaseImage((eBaseImage)i));

	m_tree.SetImageList(m_images, TVSIL_NORMAL);
	if(oldImages.m_hImageList)
		oldImages.Destroy();
}

int CLayerTreePane::AddImage(HBITMAP hBitmap)
{
	if(!hBitmap)
		return ImageBlank;
	int nIndex = m_images.Add(hBitmap);
	::DeleteObject(hBitmap);
	return nIndex >= 0 ? nIndex : (int)ImageBlank;
}

int CLayerTreePane::AddSymbolImage(Display::ISymbolPtr ptrSymbol)
{
	if(!ptrSymbol.get())
		return ImageBlank;

	try
	{
		// the symbol is copied by the preview: the symbols of the map can be drawn by the draw thread now
		COLORREF bg = ::GetSysColor(COLOR_WINDOW);
		Display::IGraphicsPtr ptrGraphics = Display::CSymbolPreview::CreatePreview(ptrSymbol, m_nImageSize, m_nImageSize,
			Display::Color(GetRValue(bg), GetGValue(bg), GetBValue(bg), 255), m_dDpi);

		DWORD* pBits = nullptr;
		HBITMAP hBitmap = CreateDib(m_nImageSize, m_nImageSize, &pBits);
		if(!hBitmap)
			return ImageBlank;

		HDC hDC = ::CreateCompatibleDC(NULL);
		HGDIOBJ hOld = ::SelectObject(hDC, hBitmap);
		::BitBlt(hDC, 0, 0, m_nImageSize, m_nImageSize, ptrGraphics->GetDC(), 0, 0, SRCCOPY);
		::GdiFlush();
		::SelectObject(hDC, hOld);
		::DeleteDC(hDC);
		MakeOpaque(pBits, m_nImageSize * m_nImageSize);
		return AddImage(hBitmap);
	}
	catch (std::exception&)
	{
		return ImageBlank;   // a symbol which can't be copied / drawn
	}
}

// ---------------- the tree

const CLayerTreePane::SNode* CLayerTreePane::GetNode(HTREEITEM hItem) const
{
	if(!hItem)
		return nullptr;
	size_t nIndex = (size_t)m_tree.GetItemData(hItem);
	return nIndex < m_vecNodes.size() ? &m_vecNodes[nIndex] : nullptr;
}

bool CLayerTreePane::IsChecked(HTREEITEM hItem) const
{
	return ((m_tree.GetItemState(hItem, TVIS_STATEIMAGEMASK) & TVIS_STATEIMAGEMASK) >> 12) == 2;
}

HTREEITEM CLayerTreePane::InsertItem(HTREEITEM hParent, const std::wstring& sText, int nImage, eNodeKind kind,
                                     Cartography::ILayerPtr ptrLayer, int nCheck)
{
	SNode node;
	node.kind = kind;
	node.ptrLayer = ptrLayer;
	m_vecNodes.push_back(node);

	TVINSERTSTRUCT tvis = {0};
	tvis.hParent = hParent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE;
	tvis.item.pszText = const_cast<LPWSTR>(sText.c_str());
	tvis.item.iImage = nImage;
	tvis.item.iSelectedImage = nImage;
	tvis.item.lParam = (LPARAM)(m_vecNodes.size() - 1);
	tvis.item.stateMask = TVIS_STATEIMAGEMASK;
	tvis.item.state = INDEXTOSTATEIMAGEMASK(nCheck < 0 ? 0 : (nCheck ? 2 : 1));   // 0 - no check box
	return m_tree.InsertItem(&tvis);
}

void CLayerTreePane::CheckMap()
{
	if(!m_pView || !m_tree.IsWindow() || m_bDragging)
		return;

	Cartography::IMapPtr ptrMap = m_pView->GetProject().GetMap();
	uint64_t nCounter = ptrMap->GetLayers()->GetChangeCounter();
	uint64_t nRevision = m_pView->GetLayersRevision();
	if(ptrMap == m_ptrMap && nCounter == m_nLayersCounter && nRevision == m_nRevision)
		return;

	m_ptrMap = ptrMap;
	m_nLayersCounter = nCounter;
	m_nRevision = nRevision;
	Rebuild();
}

void CLayerTreePane::Rebuild()
{
	Cartography::ILayerPtr ptrSelect = m_ptrSelectAfterRebuild.get() ? m_ptrSelectAfterRebuild : GetSelectedLayer();
	m_ptrSelectAfterRebuild.reset();

	m_bUpdating = true;
	m_tree.SetRedraw(FALSE);
	m_tree.DeleteAllItems();
	m_vecNodes.clear();
	CreateImages();   // the legend images of the old tree are dropped

	std::wstring sMapName = Utf8ToWide(m_ptrMap->GetName());
	HTREEITEM hRoot = InsertItem(TVI_ROOT, sMapName.empty() ? std::wstring(L"Layers") : sMapName, ImageMap, NodeMap, Cartography::ILayerPtr(), -1);
	AddLayers(hRoot, m_ptrMap->GetLayers());
	m_tree.Expand(hRoot, TVE_EXPAND);

	HTREEITEM hSelect = ptrSelect.get() ? FindLayerItem(hRoot, ptrSelect) : NULL;
	m_tree.SelectItem(hSelect ? hSelect : hRoot);
	if(hSelect)
		m_tree.EnsureVisible(hSelect);

	m_tree.SetRedraw(TRUE);
	m_tree.Invalidate();
	m_bUpdating = false;
}

void CLayerTreePane::AddLayers(HTREEITEM hParent, Cartography::ILayersPtr ptrLayers)
{
	// the top layer (drawn last, the end of the list) is the first in the tree
	for(int i = ptrLayers->GetLayerCount() - 1; i >= 0; --i)
		AddLayer(hParent, ptrLayers->GetLayer(i));
}

HTREEITEM CLayerTreePane::AddLayer(HTREEITEM hParent, Cartography::ILayerPtr ptrLayer)
{
	int nImage = ImageBlank;
	Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayer);
	if(ptrGroup.get())
		nImage = ImageGroup;
	else if(std::dynamic_pointer_cast<Cartography::IRasterLayer>(ptrLayer).get())
		nImage = ImageRasterLayer;
	else if(std::dynamic_pointer_cast<Cartography::IFeatureLayer>(ptrLayer).get())
		nImage = ImageFeatureLayer;

	HTREEITEM hItem = InsertItem(hParent, LayerText(ptrLayer), nImage, NodeLayer, ptrLayer, ptrLayer->GetVisible() ? 1 : 0);
	if(ptrGroup.get())
	{
		AddLayers(hItem, ptrGroup->GetChildren());
		if(ptrGroup->GetExpanded())
			m_tree.Expand(hItem, TVE_EXPAND);
		return hItem;
	}

	int nLayerImage = nImage;
	AddLegend(hItem, ptrLayer, nLayerImage);
	if(nLayerImage != nImage)
		m_tree.SetItemImage(hItem, nLayerImage, nLayerImage);
	if(!m_collapsedLayers.count(ptrLayer->GetLayerId()))
		m_tree.Expand(hItem, TVE_EXPAND);
	return hItem;
}

void CLayerTreePane::AddLegend(HTREEITEM hLayerItem, Cartography::ILayerPtr ptrLayer, int& nLayerImage)
{
	std::vector<Cartography::ILegendGroupPtr> vecGroups;
	try
	{
		vecGroups = Cartography::CLegendUtils::GetLayerLegend(ptrLayer);
	}
	catch (std::exception&)
	{
		return;   // no legend
	}

	// one symbol without a label: it is the image of the layer, no legend rows
	if(vecGroups.size() == 1 && vecGroups[0]->GetClassCount() == 1 && vecGroups[0]->GetHeading().empty() &&
	   vecGroups[0]->GetClass(0)->GetLabel().empty())
	{
		nLayerImage = AddSymbolImage(vecGroups[0]->GetClass(0)->GetSymbol());
		return;
	}

	for(size_t g = 0; g < vecGroups.size(); ++g)
	{
		Cartography::ILegendGroupPtr ptrGroup = vecGroups[g];
		if(!ptrGroup->GetVisible())
			continue;
		if(!ptrGroup->GetHeading().empty())
			InsertItem(hLayerItem, Utf8ToWide(ptrGroup->GetHeading()), ImageBlank, NodeLegendHeading, ptrLayer, -1);

		for(int c = 0; c < ptrGroup->GetClassCount(); ++c)
		{
			Cartography::ILegendClassPtr ptrClass = ptrGroup->GetClass(c);
			InsertItem(hLayerItem, Utf8ToWide(ptrClass->GetLabel()), AddSymbolImage(ptrClass->GetSymbol()), NodeLegendClass, ptrLayer, -1);
		}
	}
}

HTREEITEM CLayerTreePane::FindLayerItem(HTREEITEM hParent, Cartography::ILayerPtr ptrLayer) const
{
	for(HTREEITEM hItem = m_tree.GetChildItem(hParent); hItem; hItem = m_tree.GetNextSiblingItem(hItem))
	{
		const SNode* pNode = GetNode(hItem);
		if(!pNode || pNode->kind != NodeLayer)
			continue;
		if(pNode->ptrLayer == ptrLayer)
			return hItem;

		HTREEITEM hFound = FindLayerItem(hItem, ptrLayer);
		if(hFound)
			return hFound;
	}
	return NULL;
}

Cartography::ILayerPtr CLayerTreePane::GetSelectedLayer() const
{
	if(!m_tree.IsWindow())
		return Cartography::ILayerPtr();
	const SNode* pNode = GetNode(m_tree.GetSelectedItem());
	return pNode ? pNode->ptrLayer : Cartography::ILayerPtr();
}

HTREEITEM CLayerTreePane::ItemAtCursor() const
{
	POINT pt;
	::GetCursorPos(&pt);
	m_tree.ScreenToClient(&pt);
	UINT nFlags = 0;
	return m_tree.HitTest(pt, &nFlags);
}

// ---------------- visibility, expand state

LRESULT CLayerTreePane::OnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	POINT pt;
	::GetCursorPos(&pt);
	m_tree.ScreenToClient(&pt);
	UINT nFlags = 0;
	HTREEITEM hItem = m_tree.HitTest(pt, &nFlags);
	if(hItem && (nFlags & TVHT_ONITEMSTATEICON))
		PostMessage(WM_LAYERTREE_SYNC_CHECK, (WPARAM)hItem);   // the tree toggles the check box after NM_CLICK
	return 0;
}

LRESULT CLayerTreePane::OnItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTVITEMCHANGE* pChange = (NMTVITEMCHANGE*)pnmh;
	if(!m_bUpdating && (pChange->uChanged & TVIF_STATE) && ((pChange->uStateNew ^ pChange->uStateOld) & TVIS_STATEIMAGEMASK))
		PostMessage(WM_LAYERTREE_SYNC_CHECK, (WPARAM)pChange->hItem);
	return 0;
}

LRESULT CLayerTreePane::OnSyncCheck(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HTREEITEM hItem = (HTREEITEM)wParam;
	const SNode* pNode = m_bUpdating ? nullptr : GetNode(hItem);
	if(!pNode || pNode->kind != NodeLayer || !pNode->ptrLayer.get())
		return 0;

	bool bVisible = IsChecked(hItem);
	if(bVisible == pNode->ptrLayer->GetVisible())
		return 0;   // already applied (click and the state change both post it)

	m_pView->StopDrawing();
	pNode->ptrLayer->SetVisible(bVisible);
	m_pView->Redraw();
	return 0;
}

LRESULT CLayerTreePane::OnItemExpanded(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if(m_bUpdating)
		return 0;

	LPNMTREEVIEW pTreeView = (LPNMTREEVIEW)pnmh;
	const SNode* pNode = GetNode(pTreeView->itemNew.hItem);
	if(!pNode || pNode->kind != NodeLayer || !pNode->ptrLayer.get())
		return 0;

	bool bExpanded = pTreeView->action == TVE_EXPAND;
	if(Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(pNode->ptrLayer))
		ptrGroup->SetExpanded(bExpanded);   // saved with the project
	else if(bExpanded)
		m_collapsedLayers.erase(pNode->ptrLayer->GetLayerId());
	else
		m_collapsedLayers.insert(pNode->ptrLayer->GetLayerId());
	return 0;
}

LRESULT CLayerTreePane::OnKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMTVKEYDOWN pKey = (LPNMTVKEYDOWN)pnmh;
	HTREEITEM hSelected = m_tree.GetSelectedItem();
	if(pKey->wVKey == VK_SPACE && hSelected)
		PostMessage(WM_LAYERTREE_SYNC_CHECK, (WPARAM)hSelected);
	else if(pKey->wVKey == VK_DELETE)
		::PostMessage(GetTopLevelParent(), WM_COMMAND, ID_LAYER_REMOVE, 0);   // the frame forwards it here, as the menu
	else if(pKey->wVKey == VK_RETURN)
		::PostMessage(GetTopLevelParent(), WM_COMMAND, ID_LAYER_PROPERTIES, 0);
	return 0;
}

// ---------------- properties, commands

LRESULT CLayerTreePane::OnRClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	HTREEITEM hItem = ItemAtCursor();
	const SNode* pNode = GetNode(hItem);
	if(!pNode)
		return 1;

	m_tree.SelectItem(hItem);
	if(pNode->kind == NodeMap)
	{
		// the map: its properties, a new group
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, ID_MAP_PROPERTIES, L"Map &properties...");
		menu.AppendMenu(MF_STRING, ID_FULL_ZOOM, L"&Full extent");
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_NEW_GROUP_LAYER, L"New &group layer");
		menu.SetMenuDefaultItem(ID_MAP_PROPERTIES);
		POINT pt;
		::GetCursorPos(&pt);
		UINT nCmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		if(nCmd == ID_MAP_PROPERTIES)
		{
			if(m_pView->ShowMapProperties())
				CheckMap();
		}
		else if(nCmd == ID_FULL_ZOOM || nCmd == ID_NEW_GROUP_LAYER)
			::PostMessage(GetTopLevelParent(), WM_COMMAND, nCmd, 0);   // the frame forwards them to the view / here
		return 1;
	}

	// a layer (or a legend row of it): the layer menu
	Cartography::ILayerPtr ptrLayer = pNode->ptrLayer;
	if(!ptrLayer.get())
		return 1;

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, ID_LAYER_PROPERTIES, L"&Properties...");
	menu.AppendMenu(MF_STRING, ID_LAYER_ZOOM, L"&Zoom to layer");
	menu.AppendMenu(MF_SEPARATOR);
	if(std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayer).get())
	{
		menu.AppendMenu(MF_STRING, ID_NEW_GROUP_LAYER, L"New &group layer inside");
		menu.AppendMenu(MF_SEPARATOR);
	}
	menu.AppendMenu(MF_STRING, ID_LAYER_REMOVE, L"&Remove layer");
	menu.SetMenuDefaultItem(ID_LAYER_PROPERTIES);

	POINT pt;
	::GetCursorPos(&pt);
	UINT nCmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	switch(nCmd)
	{
		case ID_LAYER_PROPERTIES:
			ShowProperties(ptrLayer);
			break;
		case ID_LAYER_ZOOM:
			m_pView->ZoomToLayer(ptrLayer);
			break;
		case ID_LAYER_REMOVE:
			RemoveLayer(ptrLayer);
			break;
		case ID_NEW_GROUP_LAYER:
			::PostMessage(GetTopLevelParent(), WM_COMMAND, ID_NEW_GROUP_LAYER, 0);   // into the selected group
			break;
	}
	return 1;   // no WM_CONTEXTMENU
}

LRESULT CLayerTreePane::OnDblClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled)
{
	// double click on a legend row - the properties of its layer (a layer row expands / collapses as usual)
	const SNode* pNode = GetNode(ItemAtCursor());
	if(pNode && (pNode->kind == NodeLegendClass || pNode->kind == NodeLegendHeading))
	{
		ShowProperties(pNode->ptrLayer);
		return 1;
	}
	bHandled = FALSE;
	return 0;
}

void CLayerTreePane::ShowProperties(Cartography::ILayerPtr ptrLayer)
{
	if(!ptrLayer.get() || !m_pView)
		return;

	m_ptrSelectAfterRebuild = ptrLayer;
	m_pView->ShowLayerProperties(ptrLayer);
	CheckMap();
	m_ptrSelectAfterRebuild.reset();   // not changed - no rebuild
}

LRESULT CLayerTreePane::OnLayerProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Cartography::ILayerPtr ptrLayer = GetSelectedLayer();
	if(!ptrLayer.get())
	{
		::MessageBeep(MB_ICONASTERISK);
		return 0;
	}
	ShowProperties(ptrLayer);
	return 0;
}

LRESULT CLayerTreePane::OnLayerZoom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Cartography::ILayerPtr ptrLayer = GetSelectedLayer();
	if(!ptrLayer.get())
	{
		::MessageBeep(MB_ICONASTERISK);
		return 0;
	}
	m_pView->ZoomToLayer(ptrLayer);
	return 0;
}

void CLayerTreePane::RemoveLayer(Cartography::ILayerPtr ptrLayer)
{
	std::wstring sMsg = L"Remove the layer '" + LayerText(ptrLayer) + L"'";
	if(Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayer))
		sMsg += L" with its layers";
	sMsg += L" from the map?";
	if(MessageBox(sMsg.c_str(), L"Remove layer", MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	try
	{
		m_pView->StopDrawing();
		m_pView->GetProject().RemoveLayer(ptrLayer);
		m_pView->OnLayersChanged(true);
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"Remove layer", MB_OK | MB_ICONERROR);
		m_pView->Redraw();
	}
}

LRESULT CLayerTreePane::OnLayerRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Cartography::ILayerPtr ptrLayer = GetSelectedLayer();
	if(!ptrLayer.get())
	{
		::MessageBeep(MB_ICONASTERISK);
		return 0;
	}
	RemoveLayer(ptrLayer);
	return 0;
}

LRESULT CLayerTreePane::OnNewGroupLayer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_pView)
		return 0;

	// into the selected group, otherwise on the top of the map
	Cartography::IGroupLayerPtr ptrParent = std::dynamic_pointer_cast<Cartography::IGroupLayer>(GetSelectedLayer());
	try
	{
		m_pView->StopDrawing();
		m_ptrSelectAfterRebuild = m_pView->GetProject().AddGroupLayer("New group", ptrParent);
		m_pView->OnLayersChanged(false);   // an empty group draws nothing
		CheckMap();
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"New group layer", MB_OK | MB_ICONERROR);
	}
	return 0;
}

// ---------------- drag & drop

LRESULT CLayerTreePane::OnBeginDrag(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMTREEVIEW pTreeView = (LPNMTREEVIEW)pnmh;
	const SNode* pNode = GetNode(pTreeView->itemNew.hItem);
	if(!pNode || pNode->kind != NodeLayer || !pNode->ptrLayer.get())
		return 0;

	m_hDragItem = pTreeView->itemNew.hItem;
	m_ptrDragLayer = pNode->ptrLayer;
	m_bDragging = true;
	m_tree.SelectItem(m_hDragItem);
	SetCapture();
	return 0;
}

CLayerTreePane::SDropTarget CLayerTreePane::GetDropTarget(POINT ptTree) const
{
	SDropTarget target;
	if(!m_ptrDragLayer.get() || !m_pView)
		return target;

	TestMapDraw::CMapProject& project = m_pView->GetProject();
	UINT nFlags = 0;
	HTREEITEM hItem = m_tree.HitTest(ptTree, &nFlags);
	const SNode* pNode = GetNode(hItem);
	if(!pNode)
		return target;

	target.hItem = hItem;
	switch(pNode->kind)
	{
		case NodeMap:
			target.ptrList = m_ptrMap->GetLayers();   // on the top of the map
			break;

		case NodeLegendHeading:
		case NodeLegendClass:
			// a legend row is below its layer: under the layer
			target.ptrList = project.FindParentList(pNode->ptrLayer);
			target.ptrNeighbour = pNode->ptrLayer;
			target.bAbove = false;
			target.hItem = m_tree.GetParentItem(hItem);
			break;

		case NodeLayer:
		{
			RECT rc = {0};
			m_tree.GetItemRect(hItem, &rc, FALSE);
			int nHeight = (std::max)(1, (int)(rc.bottom - rc.top));
			double dPos = (double)(ptTree.y - rc.top) / nHeight;   // 0 - top of the row, 1 - bottom

			Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(pNode->ptrLayer);
			if(ptrGroup.get() && dPos > 0.25 && dPos < 0.75)
			{
				target.ptrList = ptrGroup->GetChildren();   // into the group, on its top
				break;
			}

			target.ptrList = project.FindParentList(pNode->ptrLayer);
			target.ptrNeighbour = pNode->ptrLayer;
			target.bAbove = dPos < 0.5;   // above in the tree - drawn later
			break;
		}
	}

	// not onto itself, not into the dragged group
	Cartography::ILayerPtr ptrTargetLayer = pNode->ptrLayer;
	target.bValid = target.ptrList.get() && target.ptrNeighbour != m_ptrDragLayer &&
	                !(ptrTargetLayer.get() && TestMapDraw::CMapProject::IsInGroup(ptrTargetLayer, m_ptrDragLayer));
	return target;
}

LRESULT CLayerTreePane::OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if(!m_bDragging)
	{
		bHandled = FALSE;
		return 0;
	}

	POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
	MapWindowPoints(m_tree, &pt, 1);

	// scroll when the cursor is at the top / bottom edge
	RECT rcTree;
	m_tree.GetClientRect(&rcTree);
	if(pt.y < m_nImageSize / 2)
		m_tree.SendMessage(WM_VSCROLL, SB_LINEUP, 0);
	else if(pt.y > rcTree.bottom - m_nImageSize / 2)
		m_tree.SendMessage(WM_VSCROLL, SB_LINEDOWN, 0);

	SDropTarget target = GetDropTarget(pt);
	m_tree.SelectDropTarget(target.bValid ? target.hItem : NULL);
	::SetCursor(::LoadCursor(NULL, target.bValid ? IDC_ARROW : IDC_NO));
	return 0;
}

LRESULT CLayerTreePane::OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if(!m_bDragging)
	{
		bHandled = FALSE;
		return 0;
	}

	POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
	MapWindowPoints(m_tree, &pt, 1);
	EndDrag(true, pt);
	return 0;
}

LRESULT CLayerTreePane::OnCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if(m_bDragging)
	{
		POINT pt = {0, 0};
		EndDrag(false, pt);   // Esc, another window took the mouse
	}
	bHandled = FALSE;
	return 0;
}

void CLayerTreePane::EndDrag(bool bDrop, POINT ptTree)
{
	if(!m_bDragging)
		return;

	SDropTarget target;
	if(bDrop)
		target = GetDropTarget(ptTree);

	m_bDragging = false;
	Cartography::ILayerPtr ptrLayer = m_ptrDragLayer;
	m_ptrDragLayer.reset();
	m_hDragItem = NULL;
	m_tree.SelectDropTarget(NULL);
	if(::GetCapture() == m_hWnd)
		ReleaseCapture();   // WM_CAPTURECHANGED: m_bDragging is already false

	if(!bDrop || !target.bValid)
		return;

	try
	{
		m_pView->StopDrawing();
		if(m_pView->GetProject().MoveLayer(ptrLayer, target.ptrList, target.ptrNeighbour, target.bAbove))
		{
			m_ptrSelectAfterRebuild = ptrLayer;
			m_pView->OnLayersChanged(true);
		}
		else
			m_pView->Redraw();
	}
	catch (std::exception& exc)
	{
		MessageBox(ExceptionText(exc).c_str(), L"Move layer", MB_OK | MB_ICONERROR);
		m_pView->Redraw();
	}
	CheckMap();
}
