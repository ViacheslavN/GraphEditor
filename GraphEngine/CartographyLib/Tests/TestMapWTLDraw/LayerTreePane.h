// LayerTreePane.h : left pane of the main window - the layers of the map as a tree:
// group layers with their children, a check box of the visibility, the legend of the feature layers
// (symbol images by Display::CSymbolPreview). Right click on a layer - menu: properties, zoom to layer, remove;
// drag & drop - the order of the layers and moving them into / out of the groups, Del - remove.
// No events from the map: the tree is rebuilt when the map, its layer lists or CMapView::GetLayersRevision change (CheckMap).
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <set>

class CMapView;

// posted to apply the check box of the item (hItem in wParam) after the tree has changed it
#define WM_LAYERTREE_SYNC_CHECK     (WM_APP + 20)

class CLayerTreePane : public CWindowImpl<CLayerTreePane>
{
public:
	DECLARE_WND_CLASS_EX(_T("TestMapDraw_LayerTree"), 0, COLOR_WINDOW)

	CLayerTreePane();
	~CLayerTreePane();

	void SetMapView(CMapView* pView) { m_pView = pView; }
	// rebuilds the tree when the map has changed (called on idle by the frame)
	void CheckMap();
	// the layer of the selected item (a legend row - its layer), null - none
	GraphEngine::Cartography::ILayerPtr GetSelectedLayer() const;

	BEGIN_MSG_MAP(CLayerTreePane)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
		MESSAGE_HANDLER(WM_LAYERTREE_SYNC_CHECK, OnSyncCheck)
		NOTIFY_HANDLER(IDC_LAYER_TREE, NM_CLICK, OnClick)
		NOTIFY_HANDLER(IDC_LAYER_TREE, NM_RCLICK, OnRClick)
		NOTIFY_HANDLER(IDC_LAYER_TREE, NM_DBLCLK, OnDblClick)
		NOTIFY_HANDLER(IDC_LAYER_TREE, TVN_KEYDOWN, OnKeyDown)
		NOTIFY_HANDLER(IDC_LAYER_TREE, TVN_ITEMCHANGED, OnItemChanged)
		NOTIFY_HANDLER(IDC_LAYER_TREE, TVN_ITEMEXPANDED, OnItemExpanded)
		NOTIFY_HANDLER(IDC_LAYER_TREE, TVN_BEGINDRAG, OnBeginDrag)
	ALT_MSG_MAP( 1 )	//	commands forwarded by the frame
		COMMAND_ID_HANDLER(ID_LAYER_PROPERTIES, OnLayerProperties)
		COMMAND_ID_HANDLER(ID_LAYER_ZOOM, OnLayerZoom)
		COMMAND_ID_HANDLER(ID_LAYER_REMOVE, OnLayerRemove)
		COMMAND_ID_HANDLER(ID_NEW_GROUP_LAYER, OnNewGroupLayer)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSyncCheck(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnDblClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled);
	LRESULT OnKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnItemExpanded(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnBeginDrag(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	LRESULT OnLayerProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLayerZoom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLayerRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewGroupLayer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	enum eNodeKind
	{
		NodeMap,            // the root: the layers of the map
		NodeLayer,
		NodeLegendHeading,  // field of a classification
		NodeLegendClass     // symbol and label
	};

	struct SNode
	{
		eNodeKind kind = NodeMap;
		GraphEngine::Cartography::ILayerPtr ptrLayer;   // the layer of the item (of the legend row)
	};

	// where a dragged layer goes: into the list above / below the neighbour (null - on the top of the list)
	struct SDropTarget
	{
		bool bValid = false;
		HTREEITEM hItem = NULL;
		GraphEngine::Cartography::ILayersPtr ptrList;
		GraphEngine::Cartography::ILayerPtr  ptrNeighbour;
		bool bAbove = true;
	};

	enum eBaseImage
	{
		ImageMap = 0,
		ImageGroup,
		ImageFeatureLayer,
		ImageRasterLayer,
		ImageBlank,
		BaseImageCount
	};

	void Rebuild();
	void AddLayers(HTREEITEM hParent, GraphEngine::Cartography::ILayersPtr ptrLayers);
	HTREEITEM AddLayer(HTREEITEM hParent, GraphEngine::Cartography::ILayerPtr ptrLayer);
	void AddLegend(HTREEITEM hLayerItem, GraphEngine::Cartography::ILayerPtr ptrLayer, int& nLayerImage);
	HTREEITEM InsertItem(HTREEITEM hParent, const std::wstring& sText, int nImage, eNodeKind kind,
	                     GraphEngine::Cartography::ILayerPtr ptrLayer, int nCheck);   // nCheck: -1 - no check box, 0 / 1
	const SNode* GetNode(HTREEITEM hItem) const;
	HTREEITEM FindLayerItem(HTREEITEM hParent, GraphEngine::Cartography::ILayerPtr ptrLayer) const;
	HTREEITEM ItemAtCursor() const;
	bool IsChecked(HTREEITEM hItem) const;

	void CreateImages();
	int  AddImage(HBITMAP hBitmap);
	int  AddSymbolImage(GraphEngine::Display::ISymbolPtr ptrSymbol);
	HBITMAP CreateBaseImage(eBaseImage image) const;

	SDropTarget GetDropTarget(POINT ptTree) const;
	void EndDrag(bool bDrop, POINT ptTree);

	void ShowProperties(GraphEngine::Cartography::ILayerPtr ptrLayer);
	void RemoveLayer(GraphEngine::Cartography::ILayerPtr ptrLayer);

private:
	CMapView*       m_pView;
	CTreeViewCtrl   m_tree;
	CImageList      m_images;
	int             m_nImageSize;
	double          m_dDpi;
	std::vector<SNode> m_vecNodes;   // item data - index in the vector
	bool            m_bUpdating;     // the tree is filled by the code: no visibility changes

	// the state the tree was built from
	GraphEngine::Cartography::IMapPtr m_ptrMap;
	uint64_t        m_nLayersCounter;
	uint64_t        m_nRevision;
	std::set<CommonLib::CGuid> m_collapsedLayers;   // feature layers with the legend collapsed
	GraphEngine::Cartography::ILayerPtr m_ptrSelectAfterRebuild;

	// drag & drop
	bool            m_bDragging;
	HTREEITEM       m_hDragItem;
	GraphEngine::Cartography::ILayerPtr m_ptrDragLayer;
};
