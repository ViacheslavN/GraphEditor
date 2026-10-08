// MapView.h : interface of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MapProject.h"
#include "../../drawer/MapDrawer.h"

// posted from the draw thread when the map drawing is finished (wParam - 1 if canceled)
#define WM_MAP_DRAWING_FINISHED (WM_APP + 1)
// posted from the conversion thread: progress (wParam - copied features), finish
#define WM_CONVERT_PROGRESS     (WM_APP + 2)
#define WM_CONVERT_FINISHED     (WM_APP + 3)

class CMapView : public CWindowImpl<CMapView>
{
public:
	DECLARE_WND_CLASS_EX(NULL, CS_DBLCLKS, -1)

	CMapView();
	~CMapView();

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CMapView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
		MESSAGE_HANDLER(WM_MAP_DRAWING_FINISHED, OnMapDrawingFinished)
		MESSAGE_HANDLER(WM_CONVERT_PROGRESS, OnConvertProgress)
		MESSAGE_HANDLER(WM_CONVERT_FINISHED, OnConvertFinished)
	ALT_MSG_MAP( 1 )	//	Forwarded by frame
		COMMAND_ID_HANDLER(ID_REDRAW_MAP, OnRedrawMap)
		COMMAND_ID_HANDLER(ID_FULL_ZOOM, OnFullZoom)
		COMMAND_ID_HANDLER(ID_ZOOM_TO_LAYER, OnZoomToLayer)
		COMMAND_ID_HANDLER(ID_ZOOM_IN, OnZoomIn)
		COMMAND_ID_HANDLER(ID_ZOOM_OUT, OnZoomOut)
		COMMAND_ID_HANDLER(ID_ADD_SHAPE_FILE, OnAddShapeFile)
		COMMAND_ID_HANDLER(ID_ADD_SQLITE_DB, OnAddSQLiteDb)
		COMMAND_ID_HANDLER(ID_ADD_RASTER, OnAddRaster)
		COMMAND_ID_HANDLER(ID_CONVERT_SHAPE_TO_SQLITE, OnConvertShapeToSQLite)
		COMMAND_ID_HANDLER(ID_REMOVE_ALL_LAYERS, OnRemoveAllLayers)
		COMMAND_ID_HANDLER(ID_CLEAR_SELECTION, OnClearSelection)
		COMMAND_ID_HANDLER(ID_VIEW_3D, OnView3D)
		COMMAND_ID_HANDLER(ID_TILT_UP, OnTilt)
		COMMAND_ID_HANDLER(ID_TILT_DOWN, OnTilt)
		COMMAND_ID_HANDLER(ID_ROTATE_LEFT, OnRotate)
		COMMAND_ID_HANDLER(ID_ROTATE_RIGHT, OnRotate)
		COMMAND_ID_HANDLER(ID_RESET_ROTATION, OnRotate)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMapDrawingFinished(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnConvertProgress(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnConvertFinished(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnRedrawMap(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFullZoom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomToLayer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomIn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomOut(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAddShapeFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAddSQLiteDb(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAddRaster(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnConvertShapeToSQLite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRemoveAllLayers(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClearSelection(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnView3D(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTilt(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRotate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void NewProject();
	bool ZoomToLayer(int nLayerIndex);
	bool OpenProject(const wchar_t *pszFile);
	bool SaveProject(const wchar_t *pszFile);
	bool AddShapeFile(const wchar_t *pszFile, const TestMapDraw::SLayerParams& params = TestMapDraw::SLayerParams());
	bool AddSQLiteDatabase(const wchar_t *pszFile, const std::string& sTableName = std::string(), const TestMapDraw::SLayerParams& params = TestMapDraw::SLayerParams());
	double GetCurrentScale() const; // scale denominator of the map view, 0 - no map yet
	bool AddRaster(const wchar_t *pszFile);
	// copies the shape file into the SQLite database in a background thread
	bool StartConvertToSQLite(const wchar_t *pszShapeFile, const wchar_t *pszDatabase);
	bool IsConverting() const;
	void Redraw();

	// pseudo 3D (navigator) view
	bool Is3DMode() const;
	void Set3DMode(bool b3D);
	void ChangeTilt(double dDelta);
	void ChangeRotation(double dDelta);

	// status bar to show the cursor position and the scale
	void SetStatusBar(HWND hWndStatusBar);

private:
	// called by the drawer, possibly from its threads
	void OnInvalidate(const GraphEngine::Display::GPoint* pPoint, const GraphEngine::Display::GRect* pRect, bool bForce);
	void OnFinishMapDrawing(bool bCanceled);

	void ZoomAt(const GraphEngine::Display::GPoint& pt, double dMult);
	void SelectAt(const GraphEngine::Display::GPoint& pt, bool bAddToSelection);
	void UpdateStatus(const GraphEngine::Display::GPoint* pPoint);
	void ShowError(const std::exception& exc, const wchar_t* pszCaption);
	void SetMapToDrawer(bool bZoomToFull);
	void OnLayersAdded(bool bFirstLayers);
	void StopConversion();

private:
	TestMapDraw::CMapProject m_project;
	GraphEngine::Cartography::CMapDrawerPtr m_ptrDrawer;
	GraphEngine::Display::IGraphicsPtr m_ptrScreen; // window sized buffer, BitBlt to the window
	HWND m_hWndStatusBar;

	// shape file -> SQLite conversion
	std::thread m_convertThread;
	std::atomic<bool> m_bConverting;
	std::atomic<bool> m_bConvertCancel;
	std::wstring m_sConvertDatabase;
	std::wstring m_sConvertShapeFile;
	std::string m_sConvertTable;  // set by the thread, read after WM_CONVERT_FINISHED
	std::string m_sConvertError;
	int64_t m_nConverted;

	bool m_bLbDown;
	bool m_bPan;
	GraphEngine::Display::GPoint m_LbDownPt;
};
