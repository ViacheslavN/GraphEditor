// MapView.cpp : implementation of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MapView.h"
#include "../../../CommonLib/str/StringEncoding.h"

#include <cmath>
#include <cstdio>

using namespace GraphEngine;

namespace
{
	const int PanThreshold = 3;       // pixels before a click becomes a pan
	const int SelectTolerance = 3;    // pixels around the click point
	const double WheelZoomStep = 0.2; // 20% per wheel notch
	const double TiltStep = 5.;       // degrees per key press / wheel notch (3D view)
	const double RotationStep = 15.;  // degrees per key press
	const double WheelRotationStep = 10.;

	// shapelib and the CommonLib file API open files with the ANSI (*A) functions on Windows
	std::string ToFilePath(const wchar_t* psz)
	{
		return CommonLib::StringEncoding::str_w2a_safe(std::wstring(psz ? psz : L""));
	}

	// SQLite opens files with UTF-8 names
	std::string ToUtf8(const wchar_t* psz)
	{
		return CommonLib::StringEncoding::str_w2utf8_safe(std::wstring(psz ? psz : L""));
	}

	std::wstring ToWide(const std::string& str)
	{
		return CommonLib::StringEncoding::str_utf82w_safe(str);
	}

	Display::GPoint PointFromLParam(LPARAM lParam)
	{
		// GET_X_LPARAM: the coordinates are signed (mouse capture outside the window)
		return Display::GPoint((Display::GUnits)(short)LOWORD(lParam), (Display::GUnits)(short)HIWORD(lParam));
	}
}

CMapView::CMapView() :
	m_hWndStatusBar(NULL),
	m_bConverting(false),
	m_bConvertCancel(false),
	m_nConverted(0),
	m_bLbDown(false),
	m_bPan(false)
{
	m_LbDownPt.x = m_LbDownPt.y = 0;

	m_ptrDrawer = std::make_shared<Cartography::CMapDrawer>();
	m_ptrDrawer->SetOnInvalidate(CommonLib::Delegate(this, &CMapView::OnInvalidate), true);
	m_ptrDrawer->SetOnFinishMapDrawing(CommonLib::Delegate(this, &CMapView::OnFinishMapDrawing), true);
	m_ptrDrawer->SetMap(m_project.GetMap());
}

CMapView::~CMapView()
{
	StopConversion();
	m_ptrDrawer->StopDraw(true);
	m_ptrDrawer->SetOnInvalidate(CommonLib::Delegate(this, &CMapView::OnInvalidate), false);
	m_ptrDrawer->SetOnFinishMapDrawing(CommonLib::Delegate(this, &CMapView::OnFinishMapDrawing), false);
}

BOOL CMapView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

void CMapView::SetStatusBar(HWND hWndStatusBar)
{
	m_hWndStatusBar = hWndStatusBar;
}

LRESULT CMapView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDC hDC = GetDC();
	double dpi = (double)::GetDeviceCaps(hDC, LOGPIXELSX);
	ReleaseDC(hDC);
	m_ptrDrawer->SetResolution(dpi);
	return 0;
}

LRESULT CMapView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// no more repaints / messages after the window is gone
	StopConversion();
	m_ptrDrawer->StopDraw(true);
	bHandled = FALSE;
	return 0;
}

LRESULT CMapView::OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return 1; // the whole client area is painted in OnPaint
}

LRESULT CMapView::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CPaintDC dc(m_hWnd);

	RECT rcClient;
	GetClientRect(&rcClient);
	int nWidth = rcClient.right - rcClient.left;
	int nHeight = rcClient.bottom - rcClient.top;
	if(nWidth <= 0 || nHeight <= 0)
		return 0;

	if(!m_ptrScreen.get() || (int)m_ptrScreen->GetWidth() != nWidth || (int)m_ptrScreen->GetHeight() != nHeight)
		m_ptrScreen = Display::IGraphics::CreateCGraphicsAgg((Display::GUnits)nWidth, (Display::GUnits)nHeight, false);

	try
	{
		m_ptrDrawer->Update(m_ptrScreen, nullptr, nullptr);
	}
	catch (std::exception&)
	{
		m_ptrScreen->Erase(Display::Color(255, 255, 255, 255));
	}

	::BitBlt(dc.m_hDC, 0, 0, nWidth, nHeight, m_ptrScreen->GetDC(), 0, 0, SRCCOPY);
	return 0;
}

LRESULT CMapView::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	int nWidth = LOWORD(lParam);
	int nHeight = HIWORD(lParam);
	if(nWidth == 0 || nHeight == 0)
		return 0;

	try
	{
		m_ptrDrawer->SetSize(nWidth, nHeight, true);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Resize");
	}
	return 0;
}

void CMapView::OnInvalidate(const Display::GPoint* /*pPoint*/, const Display::GRect* /*pRect*/, bool bForce)
{
	// may be called from the draw thread: InvalidateRect is thread safe, painting happens in the UI thread
	if(!IsWindow())
		return;

	::InvalidateRect(m_hWnd, NULL, FALSE);
	if(bForce)
		::UpdateWindow(m_hWnd);
}

void CMapView::OnFinishMapDrawing(bool bCanceled)
{
	// draw thread -> UI thread
	if(IsWindow())
		PostMessage(WM_MAP_DRAWING_FINISHED, bCanceled ? 1 : 0, 0);
}

LRESULT CMapView::OnMapDrawingFinished(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	std::string sError = m_ptrDrawer->GetLastError();
	if(!sError.empty() && m_hWndStatusBar)
	{
		std::wstring sText = L"Drawing failed: " + ToWide(sError);
		::SetWindowText(m_hWndStatusBar, sText.c_str());
		return 0;
	}

	UpdateStatus(nullptr);
	return 0;
}

void CMapView::UpdateStatus(const Display::GPoint* pPoint)
{
	if(!m_hWndStatusBar)
		return;

	Display::IDisplayTransformationPtr ptrTrans = m_ptrDrawer->GetCalcTransformation();
	if(!ptrTrans.get())
	{
		::SetWindowText(m_hWndStatusBar, L"Add a shape file (Layers / Add Shape File)");
		return;
	}

	// view mode: rotation, 3D tilt
	wchar_t szView[64] = L"";
	double dRotation = m_ptrDrawer->GetRotation();
	if(m_ptrDrawer->Is3DMode())
		swprintf(szView, 64, L"   3D tilt %.0f\x00B0  rotation %.0f\x00B0", m_ptrDrawer->GetTilt(), dRotation);
	else if(dRotation != 0.)
		swprintf(szView, 64, L"   rotation %.0f\x00B0", dRotation);

	wchar_t szText[256];
	if(pPoint)
	{
		CommonLib::GisXYPoint mapPt;
		ptrTrans->DeviceToMap(pPoint, &mapPt, 1);
		swprintf(szText, 256, L"X: %.6f  Y: %.6f   Scale 1:%.0f%s   Layers: %d%s", mapPt.x, mapPt.y, ptrTrans->GetScale(), szView,
			m_project.GetMap()->GetLayers()->GetLayerCount(), m_ptrDrawer->IsDrawing() ? L"   drawing..." : L"");
	}
	else
	{
		swprintf(szText, 256, L"Scale 1:%.0f%s   Layers: %d%s", ptrTrans->GetScale(), szView,
			m_project.GetMap()->GetLayers()->GetLayerCount(), m_ptrDrawer->IsDrawing() ? L"   drawing..." : L"");
	}
	::SetWindowText(m_hWndStatusBar, szText);
}

LRESULT CMapView::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	m_LbDownPt = PointFromLParam(lParam);
	m_bLbDown = true;
	m_bPan = false;
	SetCapture();
	SetFocus();
	return 0;
}

LRESULT CMapView::OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	Display::GPoint pt = PointFromLParam(lParam);

	if (m_bLbDown)
	{
		if(!m_bPan && (std::abs((int)(pt.x - m_LbDownPt.x)) > PanThreshold || std::abs((int)(pt.y - m_LbDownPt.y)) > PanThreshold))
		{
			m_bPan = true;
			m_ptrDrawer->StartPan(m_LbDownPt);
		}

		if(m_bPan)
			m_ptrDrawer->MovePan(pt);
	}

	UpdateStatus(&pt);
	return 0;
}

LRESULT CMapView::OnLButtonUp(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	Display::GPoint pt = PointFromLParam(lParam);
	bool bLbDown = m_bLbDown;
	bool bPan = m_bPan;
	m_bLbDown = false;
	m_bPan = false;
	if(GetCapture() == m_hWnd)
		ReleaseCapture();

	if(!bLbDown)
		return 0;

	try
	{
		if (bPan)
			m_ptrDrawer->StopPan(pt);
		else
			SelectAt(pt, (wParam & MK_CONTROL) != 0);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Map");
	}
	return 0;
}

LRESULT CMapView::OnCaptureChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// capture lost (Alt+Tab, ...) in the middle of a pan: finish it where it is
	if(m_bLbDown && m_bPan)
	{
		m_bLbDown = false;
		m_bPan = false;
		POINT pt;
		::GetCursorPos(&pt);
		ScreenToClient(&pt);
		m_ptrDrawer->StopPan(Display::GPoint((Display::GUnits)pt.x, (Display::GUnits)pt.y));
	}
	m_bLbDown = false;
	return 0;
}

void CMapView::SelectAt(const Display::GPoint& pt, bool bAddToSelection)
{
	Display::IDisplayTransformationPtr ptrTrans = m_ptrDrawer->GetTransformation();
	if(!ptrTrans.get())
		return;

	Display::GRect rect(pt.x - SelectTolerance, pt.y - SelectTolerance, pt.x + SelectTolerance, pt.y + SelectTolerance);
	CommonLib::bbox bbox;
	ptrTrans->DeviceToMap(rect, bbox);
	bbox.type = CommonLib::bbox_type_normal;

	m_ptrDrawer->StopDraw(true);
	m_project.GetMap()->SelectFeatures(bbox, !bAddToSelection);
	m_ptrDrawer->Redraw();
}

void CMapView::ZoomAt(const Display::GPoint& pt, double dMult)
{
	Display::IDisplayTransformationPtr ptrTrans = m_ptrDrawer->GetCalcTransformation();
	if(!ptrTrans.get())
		return;

	Display::GRect devRect = ptrTrans->GetDeviceRect();
	if(!devRect.PointInRect(pt))
		return;

	// keep the map point under the cursor in place
	CommonLib::GisXYPoint location;
	ptrTrans->DeviceToMap(&pt, &location, 1);

	double scale = ptrTrans->GetScale();
	double newScale = scale * dMult;

	// limit zoom out to a few full extents
	Geometry::IEnvelopePtr ptrFull = m_project.GetMap()->GetFullExtent(m_project.GetMap()->GetSpatialReference());
	const CommonLib::bbox& curBox = ptrTrans->GetFittedBounds();
	if(ptrFull.get() && curBox.xMax > curBox.xMin && curBox.yMax > curBox.yMin)
	{
		const CommonLib::bbox& fullBox = ptrFull->GetBoundingBox();
		double fullScaleX = scale / (curBox.xMax - curBox.xMin) * (fullBox.xMax - fullBox.xMin);
		double fullScaleY = scale / (curBox.yMax - curBox.yMin) * (fullBox.yMax - fullBox.yMin);
		double fullScale = fullScaleX > fullScaleY ? fullScaleX : fullScaleY;
		if(newScale > 7 * fullScale)
			newScale = 7 * fullScale;
	}

	if(ptrTrans->GetUnits() != CommonLib::UnitsUnknown && newScale < 100.)
		newScale = 100.;

	if(newScale <= 0. || newScale == scale)
		return;

	double mult = newScale / scale;
	CommonLib::GisXYPoint mapPos = ptrTrans->GetMapPos();
	CommonLib::GisXYPoint newPos;
	newPos.x = location.x + (mapPos.x - location.x) * mult;
	newPos.y = location.y + (mapPos.y - location.y) * mult;

	m_ptrDrawer->StopDraw(true);
	ptrTrans->SetMapPos(newPos, newScale);
	m_ptrDrawer->Redraw();
	UpdateStatus(&pt);
}

LRESULT CMapView::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	double zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	if(zDelta == 0)
		return 0;

	WORD nKeys = GET_KEYSTATE_WPARAM(wParam);
	try
	{
		// Ctrl+Wheel - tilt of the 3D view, Shift+Wheel - rotation
		if((nKeys & MK_CONTROL) && m_ptrDrawer->Is3DMode())
		{
			ChangeTilt(zDelta > 0 ? TiltStep : -TiltStep);
			return 0;
		}
		if(nKeys & MK_SHIFT)
		{
			ChangeRotation(zDelta > 0 ? -WheelRotationStep : WheelRotationStep);
			return 0;
		}
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"View");
		return 0;
	}

	// wheel coordinates are screen coordinates
	POINT pt;
	::GetCursorPos(&pt);
	ScreenToClient(&pt);

	double dMult = zDelta > 0 ? (1. - WheelZoomStep) : (1. + WheelZoomStep);
	try
	{
		ZoomAt(Display::GPoint((Display::GUnits)pt.x, (Display::GUnits)pt.y), dMult);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Zoom");
	}
	return 0;
}

void CMapView::Redraw()
{
	try
	{
		m_ptrDrawer->Redraw();
		UpdateStatus(nullptr);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Redraw");
	}
}

LRESULT CMapView::OnRedrawMap(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Redraw();
	return 0;
}

LRESULT CMapView::OnFullZoom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	try
	{
		m_ptrDrawer->ZoomToFullExtent();
		UpdateStatus(nullptr);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Full extent");
	}
	return 0;
}

LRESULT CMapView::OnZoomIn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	RECT rc;
	GetClientRect(&rc);
	ZoomAt(Display::GPoint((Display::GUnits)(rc.right - rc.left) / 2, (Display::GUnits)(rc.bottom - rc.top) / 2), 0.5);
	return 0;
}

LRESULT CMapView::OnZoomOut(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	RECT rc;
	GetClientRect(&rc);
	ZoomAt(Display::GPoint((Display::GUnits)(rc.right - rc.left) / 2, (Display::GUnits)(rc.bottom - rc.top) / 2), 2.);
	return 0;
}

LRESULT CMapView::OnClearSelection(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(m_bConverting)
	{
		m_bConvertCancel = true; // Esc cancels the running conversion
		return 0;
	}

	if(m_project.GetMap()->GetSelection()->IsEmpty())
		return 0;

	m_ptrDrawer->StopDraw(true);
	m_project.GetMap()->GetSelection()->Clear();
	Redraw();
	return 0;
}

bool CMapView::Is3DMode() const
{
	return m_ptrDrawer->Is3DMode();
}

void CMapView::Set3DMode(bool b3D)
{
	try
	{
		m_ptrDrawer->Set3DMode(b3D); // the same position, scale and rotation, redraws the map
		UpdateStatus(nullptr);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"3D view");
	}
}

void CMapView::ChangeTilt(double dDelta)
{
	if(!m_ptrDrawer->Is3DMode())
		return;

	m_ptrDrawer->SetTilt(m_ptrDrawer->GetTilt() + dDelta);
	UpdateStatus(nullptr);
}

void CMapView::ChangeRotation(double dDelta)
{
	// positive angle turns the map clockwise on the screen
	m_ptrDrawer->SetRotation(m_ptrDrawer->GetRotation() + dDelta);
	UpdateStatus(nullptr);
}

LRESULT CMapView::OnView3D(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Set3DMode(!Is3DMode());
	return 0;
}

LRESULT CMapView::OnTilt(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	try
	{
		ChangeTilt(wID == ID_TILT_UP ? TiltStep : -TiltStep);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Tilt");
	}
	return 0;
}

LRESULT CMapView::OnRotate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	try
	{
		if(wID == ID_RESET_ROTATION)
		{
			m_ptrDrawer->SetRotation(0.);
			UpdateStatus(nullptr);
		}
		else
			ChangeRotation(wID == ID_ROTATE_RIGHT ? RotationStep : -RotationStep);
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Rotate");
	}
	return 0;
}

LRESULT CMapView::OnRemoveAllLayers(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	NewProject();
	return 0;
}

LRESULT CMapView::OnAddShapeFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog fileDlg(TRUE, _T("shp"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, _T("ESRI shape files (*.shp)\0*.shp\0All Files (*.*)\0*.*\0"), m_hWnd);
	if ( fileDlg.DoModal() != IDOK )
		return 0;

	AddShapeFile(fileDlg.m_szFileName);
	return 0;
}

void CMapView::SetMapToDrawer(bool bZoomToFull)
{
	m_ptrDrawer->SetMap(m_project.GetMap()); // new map: transformation is created from the full extent
	m_ptrDrawer->Redraw();
	UpdateStatus(nullptr);
}

void CMapView::OnLayersAdded(bool bFirstLayers)
{
	if(bFirstLayers)
		SetMapToDrawer(true); // the first layer sets the coordinate system of the map
	else
		m_ptrDrawer->ZoomToFullExtent();

	UpdateStatus(nullptr);
}

bool CMapView::AddShapeFile(const wchar_t *pszFile)
{
	try
	{
		m_ptrDrawer->StopDraw(true);
		bool bFirstLayer = m_project.GetMap()->GetLayers()->GetLayerCount() == 0;
		m_project.AddShapefile(ToFilePath(pszFile));
		OnLayersAdded(bFirstLayer);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Add shape file");
		return false;
	}
}

bool CMapView::AddSQLiteDatabase(const wchar_t *pszFile, const std::string& sTableName)
{
	try
	{
		m_ptrDrawer->StopDraw(true);
		bool bFirstLayer = m_project.GetMap()->GetLayers()->GetLayerCount() == 0;
		m_project.AddSQLiteDatabase(ToUtf8(pszFile), sTableName);
		OnLayersAdded(bFirstLayer);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Add SQLite database");
		return false;
	}
}

LRESULT CMapView::OnAddSQLiteDb(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog fileDlg(TRUE, _T("sqlite"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, _T("SQLite database (*.sqlite;*.db)\0*.sqlite;*.db\0All Files (*.*)\0*.*\0"), m_hWnd);
	if ( fileDlg.DoModal() != IDOK )
		return 0;

	AddSQLiteDatabase(fileDlg.m_szFileName);
	return 0;
}

bool CMapView::IsConverting() const
{
	return m_bConverting;
}

void CMapView::StopConversion()
{
	m_bConvertCancel = true;
	if(m_convertThread.joinable())
		m_convertThread.join();
	m_bConverting = false;
}

LRESULT CMapView::OnConvertShapeToSQLite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(m_bConverting)
	{
		::MessageBox(m_hWnd, L"A conversion is already running (Esc - cancel).", L"Convert to SQLite", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	CFileDialog shapeDlg(TRUE, _T("shp"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, _T("ESRI shape files (*.shp)\0*.shp\0All Files (*.*)\0*.*\0"), m_hWnd);
	shapeDlg.m_ofn.lpstrTitle = _T("Shape file to convert");
	if ( shapeDlg.DoModal() != IDOK )
		return 0;

	// a new database is created, an existing one gets one more table
	CFileDialog dbDlg(FALSE, _T("sqlite"), NULL, OFN_HIDEREADONLY, _T("SQLite database (*.sqlite;*.db)\0*.sqlite;*.db\0All Files (*.*)\0*.*\0"), m_hWnd);
	dbDlg.m_ofn.lpstrTitle = _T("SQLite database (new or existing)");
	if ( dbDlg.DoModal() != IDOK )
		return 0;

	StartConvertToSQLite(shapeDlg.m_szFileName, dbDlg.m_szFileName);
	return 0;
}

bool CMapView::StartConvertToSQLite(const wchar_t *pszShapeFile, const wchar_t *pszDatabase)
{
	if(m_bConverting)
		return false;

	if(m_convertThread.joinable())
		m_convertThread.join();

	m_sConvertShapeFile = pszShapeFile;
	m_sConvertDatabase = pszDatabase;
	m_sConvertTable.clear();
	m_sConvertError.clear();
	m_nConverted = 0;
	m_bConvertCancel = false;
	m_bConverting = true;

	std::string sShapeFile = ToFilePath(pszShapeFile);
	std::string sDatabase = ToUtf8(pszDatabase);
	HWND hWnd = m_hWnd;

	if(m_hWndStatusBar)
		::SetWindowText(m_hWndStatusBar, L"Converting to SQLite...");

	m_convertThread = std::thread([this, hWnd, sShapeFile, sDatabase]()
	{
		try
		{
			int64_t nCopied = 0;
			m_sConvertTable = TestMapDraw::CMapProject::ConvertShapefileToSQLite(sShapeFile, sDatabase,
				[this, hWnd](int64_t nRows)
				{
					::PostMessage(hWnd, WM_CONVERT_PROGRESS, (WPARAM)nRows, 0);
					return !m_bConvertCancel;
				}, &nCopied);
			m_nConverted = nCopied;
		}
		catch (std::exception& exc)
		{
			m_sConvertError = exc.what();
		}

		::PostMessage(hWnd, WM_CONVERT_FINISHED, 0, 0);
	});

	return true;
}

LRESULT CMapView::OnConvertProgress(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if(m_hWndStatusBar && m_bConverting)
	{
		wchar_t szText[128];
		swprintf(szText, 128, L"Converting to SQLite: %llu features (Esc - cancel)", (unsigned long long)wParam);
		::SetWindowText(m_hWndStatusBar, szText);
	}
	return 0;
}

LRESULT CMapView::OnConvertFinished(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if(m_convertThread.joinable())
		m_convertThread.join();
	m_bConverting = false;
	UpdateStatus(nullptr);

	if(!m_sConvertError.empty())
	{
		std::wstring sMsg = ToWide(m_sConvertError);
		::MessageBox(m_hWnd, sMsg.c_str(), L"Convert to SQLite", MB_OK | MB_ICONERROR);
		return 0;
	}

	wchar_t szText[1024];
	swprintf(szText, 1024, L"%lld features are copied into table '%s' of\n%s\n\nAdd the table to the map?",
		(long long)m_nConverted, ToWide(m_sConvertTable).c_str(), m_sConvertDatabase.c_str());
	if(::MessageBox(m_hWnd, szText, L"Convert to SQLite", MB_YESNO | MB_ICONQUESTION) == IDYES)
		AddSQLiteDatabase(m_sConvertDatabase.c_str(), m_sConvertTable);

	return 0;
}

void CMapView::NewProject()
{
	m_ptrDrawer->StopDraw(true);
	m_project.New();
	SetMapToDrawer(true);
}

bool CMapView::OpenProject(const wchar_t *pszFile)
{
	try
	{
		m_ptrDrawer->StopDraw(true);
		m_project.Load(ToFilePath(pszFile));
		SetMapToDrawer(true);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Open project");
		m_project.New();
		SetMapToDrawer(true);
		return false;
	}
}

bool CMapView::SaveProject(const wchar_t *pszFile)
{
	try
	{
		m_project.Save(ToFilePath(pszFile));
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Save project");
		return false;
	}
}

void CMapView::ShowError(const std::exception& exc, const wchar_t* pszCaption)
{
	std::wstring sMsg = ToWide(exc.what());
	::MessageBox(m_hWnd, sMsg.c_str(), pszCaption, MB_OK | MB_ICONERROR);
}
