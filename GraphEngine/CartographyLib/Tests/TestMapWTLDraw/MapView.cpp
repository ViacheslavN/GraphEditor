// MapView.cpp : implementation of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MapView.h"
#include "AddShapeFileDlg.h"
#include "AddSQLiteDlg.h"
#include "ConvertOSMDlg.h"
#include "OSMProgressDlg.h"
#ifdef HAVE_OSM_CONVERTOR
#include "../../Map.h"
#include "../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../Convertors/OSM/OSMConvertorLib/OSMConvertor.h"
#endif
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

	// the drawer state is polled by a timer: a finished drawing is shown within DrawTimerPeriod,
	// while the map is drawn the window shows the progress every ProgressTicks timer ticks
	const UINT_PTR DrawTimerId = 1;
	const UINT DrawTimerPeriod = 100; // ms
	const int ProgressTicks = 5;

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
	m_nDrawTimer(0),
	m_nDrawCounter(0),
	m_nProgressTicks(0),
	m_bConverting(false),
	m_bConvertCancel(false),
	m_nConverted(0),
	m_bLbDown(false),
	m_bPan(false)
{
	m_LbDownPt.x = m_LbDownPt.y = 0;

	m_ptrDrawer = std::make_shared<Cartography::CMapDrawer>();
	m_ptrDrawer->SetMap(m_project.GetMap());
}

CMapView::~CMapView()
{
	StopConversion();
	m_ptrDrawer->StopDraw(true);
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

	m_nDrawCounter = m_ptrDrawer->GetDrawCounter();
	m_nDrawTimer = SetTimer(DrawTimerId, DrawTimerPeriod);
	return 0;
}

LRESULT CMapView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// no more repaints / messages after the window is gone
	if(m_nDrawTimer)
	{
		KillTimer(m_nDrawTimer);
		m_nDrawTimer = 0;
	}
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

LRESULT CMapView::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if(wParam != DrawTimerId)
	{
		bHandled = FALSE;
		return 0;
	}

	CheckDrawer();
	return 0;
}

void CMapView::CheckDrawer()
{
	uint64_t nCounter = m_ptrDrawer->GetDrawCounter();
	if(nCounter != m_nDrawCounter)
	{
		// one or more drawings are finished since the last check: show the result
		m_nDrawCounter = nCounter;
		m_nProgressTicks = 0;
		Invalidate(FALSE);
		if(!m_ptrDrawer->IsDrawing())
			OnMapDrawingFinished();
		return;
	}

	if(!m_ptrDrawer->IsDrawing())
	{
		m_nProgressTicks = 0;
		return;
	}

	// drawing in progress: show what is already drawn
	if(++m_nProgressTicks >= ProgressTicks)
	{
		m_nProgressTicks = 0;
		Invalidate(FALSE);
	}
}

void CMapView::OnMapDrawingFinished()
{
	std::string sError = m_ptrDrawer->GetLastError();
	if(!sError.empty() && m_hWndStatusBar)
	{
		std::wstring sText = L"Drawing failed: " + ToWide(sError);
		::SetWindowText(m_hWndStatusBar, sText.c_str());
		return;
	}

	UpdateStatus(nullptr);
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
		{
			m_ptrDrawer->MovePan(pt);
			Invalidate(FALSE); // the drawer only moves the picture, the window repaints itself
		}
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

LRESULT CMapView::OnZoomToLayer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Cartography::ILayersPtr ptrLayers = m_project.GetMap()->GetLayers();
	const int nLayerCount = ptrLayers->GetLayerCount();
	if(nLayerCount == 0)
		return 0;

	if(nLayerCount == 1)
	{
		ZoomToLayer(0);
		return 0;
	}

	// popup with the layers at the cursor, the last added (top) layer first
	const UINT nFirstCmd = 1;
	CMenu menu;
	menu.CreatePopupMenu();
	for(int i = nLayerCount - 1; i >= 0; --i)
	{
		std::wstring sName = ToWide(ptrLayers->GetLayer(i)->GetName());
		if(sName.empty())
			sName = L"<layer " + std::to_wstring(i + 1) + L">";
		menu.AppendMenu(MF_STRING, nFirstCmd + i, sName.c_str());
	}

	POINT pt;
	::GetCursorPos(&pt);
	RECT rc;
	GetWindowRect(&rc);
	if(!::PtInRect(&rc, pt)) // keyboard shortcut with the cursor outside - top-left corner of the view
	{
		pt.x = rc.left + 20;
		pt.y = rc.top + 20;
	}

	UINT nCmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, m_hWnd);
	if(nCmd >= nFirstCmd && nCmd < nFirstCmd + (UINT)nLayerCount)
		ZoomToLayer((int)(nCmd - nFirstCmd));
	return 0;
}

bool CMapView::ZoomToLayer(int nLayerIndex)
{
	try
	{
		CommonLib::bbox bb;
		if(!m_project.GetLayerExtent(nLayerIndex, bb))
		{
			::MessageBox(m_hWnd, L"The layer has no extent in the map coordinate system.", L"Zoom to layer", MB_OK | MB_ICONINFORMATION);
			return false;
		}

		m_ptrDrawer->ZoomIn(bb);
		UpdateStatus(nullptr);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Zoom to layer");
		return false;
	}
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
	CAddShapeFileDlg dlg;
	dlg.SetDefaultScale(GetCurrentScale());
	dlg.SetLayerIndex(m_project.GetMap()->GetLayers()->GetLayerCount());
	if ( dlg.DoModal(m_hWnd) != IDOK )
		return 0;

	AddShapeFile(dlg.GetPath().c_str(), dlg.GetLayerParams());
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

double CMapView::GetCurrentScale() const
{
	if(m_project.GetMap()->GetLayers()->GetLayerCount() == 0)
		return 0.;

	Display::IDisplayTransformationPtr ptrTrans = m_ptrDrawer->GetCalcTransformation();
	return ptrTrans.get() ? ptrTrans->GetScale() : 0.;
}

bool CMapView::AddShapeFile(const wchar_t *pszFile, const TestMapDraw::SLayerParams& params)
{
	try
	{
		m_ptrDrawer->StopDraw(true);
		bool bFirstLayer = m_project.GetMap()->GetLayers()->GetLayerCount() == 0;
		m_project.AddShapefile(ToFilePath(pszFile), params);
		OnLayersAdded(bFirstLayer);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Add shape file");
		return false;
	}
}

bool CMapView::AddSQLiteDatabase(const wchar_t *pszFile, const std::string& sTableName, const TestMapDraw::SLayerParams& params)
{
	try
	{
		m_ptrDrawer->StopDraw(true);
		bool bFirstLayer = m_project.GetMap()->GetLayers()->GetLayerCount() == 0;
		m_project.AddSQLiteDatabase(ToUtf8(pszFile), sTableName, params);
		OnLayersAdded(bFirstLayer);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Add SQLite database");
		return false;
	}
}

LRESULT CMapView::OnAddRaster(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog fileDlg(TRUE, _T("tif"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, _T("TIFF / GeoTIFF (*.tif;*.tiff)\0*.tif;*.tiff\0All Files (*.*)\0*.*\0"), m_hWnd);
	if ( fileDlg.DoModal() != IDOK )
		return 0;

	AddRaster(fileDlg.m_szFileName);
	return 0;
}

bool CMapView::AddRaster(const wchar_t *pszFile)
{
	try
	{
		m_ptrDrawer->StopDraw(true);
		bool bFirstLayer = m_project.GetMap()->GetLayers()->GetLayerCount() == 0;
		m_project.AddRaster(ToUtf8(pszFile)); // the TIFF reader opens UTF-8 paths (TIFFOpenW)
		OnLayersAdded(bFirstLayer);
		return true;
	}
	catch (std::exception& exc)
	{
		ShowError(exc, L"Add raster");
		return false;
	}
}

LRESULT CMapView::OnAddSQLiteDb(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAddSQLiteDlg dlg;
	dlg.SetDefaultScale(GetCurrentScale());
	dlg.SetLayerIndex(m_project.GetMap()->GetLayers()->GetLayerCount());
	if ( dlg.DoModal(m_hWnd) != IDOK )
		return 0;

	AddSQLiteDatabase(dlg.GetPath().c_str(), dlg.GetTableName(), dlg.GetLayerParams());
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

LRESULT CMapView::OnConvertFromOSM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const wchar_t* pszCaption = L"Convert from OpenStreetMap";
#ifndef HAVE_OSM_CONVERTOR
	::MessageBox(m_hWnd, L"The OSM converter isn't built (expat was not found by CMake).", pszCaption, MB_OK | MB_ICONINFORMATION);
	return 0;
#else
	if(m_bConverting)
	{
		::MessageBox(m_hWnd, L"A conversion is already running.", pszCaption, MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	// the file, its layers / tables, the output
	CConvertOSMDlg dlg;
	if(dlg.DoModal(m_hWnd) != IDOK)
		return 0;

	Convertors::IOSMMapPtr ptrOSMMap = dlg.GetOSMMap();
	Convertors::SOSMConvertSettings settings = dlg.GetSettings();
	std::wstring sOutput = dlg.GetOutputPath();
	std::string sOutputUtf8 = ToUtf8(sOutput.c_str());   // SQLite opens files with UTF-8 names

	std::wstring sName = sOutput.substr(sOutput.find_last_of(L"\\/") + 1);
	size_t nDot = sName.find_last_of(L'.');
	if(nDot != std::wstring::npos && nDot > 0)
		sName.resize(nDot);
	std::string sNameUtf8 = ToUtf8(sName.c_str());

	// the converter adds the layers to a separate map (it works in another thread than the drawer),
	// they are moved into the project map when it is done
	GeoDatabase::IDatabaseWorkspacePtr ptrDb;
	Cartography::IMapPtr ptrNewMap = dlg.GetAddToMap() ? std::make_shared<Cartography::CMap>() : Cartography::IMapPtr();

	COSMProgressDlg progressDlg(L"Converting OpenStreetMap",
		[&](Convertors::IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel)
		{
			ptrDb = GeoDatabase::CSQLiteWorkspace::Create(sNameUtf8.c_str(), sOutputUtf8.c_str(), CommonLib::CGuid::CreateNew());
			Convertors::COSMConvertor convertor(settings);
			convertor.Convert(ptrOSMMap, ptrNewMap, ptrDb, ptrProgress, ptrCancel);
		},
		ptrOSMMap->GetNodeCount(), ptrOSMMap->GetWayCount(), ptrOSMMap->GetRelationCount());

	m_bConverting = true;
	INT_PTR nResult = progressDlg.DoModal(m_hWnd);
	m_bConverting = false;

	if(nResult != IDOK)
	{
		// the conversion is rolled back, the new database is not needed
		ptrNewMap.reset();
		ptrDb.reset();
		for(const wchar_t* pszSuffix : {L"", L"-wal", L"-shm", L"-journal"})
			::DeleteFileW((sOutput + pszSuffix).c_str());

		if(nResult == IDCANCEL)
			::MessageBox(m_hWnd, L"The conversion is canceled.", pszCaption, MB_OK | MB_ICONINFORMATION);
		else
		{
			std::wstring sMsg = L"The conversion failed:\n" + progressDlg.GetError();
			::MessageBox(m_hWnd, sMsg.c_str(), pszCaption, MB_OK | MB_ICONERROR);
		}
		return 0;
	}

	int nLayers = 0;
	try
	{
		if(ptrNewMap.get())
		{
			m_ptrDrawer->StopDraw(true);
			bool bFirstLayers = m_project.GetMap()->GetLayers()->GetLayerCount() == 0;
			nLayers = m_project.AddConvertedLayers(ptrDb, ptrNewMap);
			if(nLayers > 0)
				OnLayersAdded(bFirstLayers);
		}
	}
	catch (std::exception& exc)
	{
		ShowError(exc, pszCaption);
	}

	int nSeconds = (int)progressDlg.GetElapsedSeconds();
	wchar_t szText[1024];
	swprintf(szText, 1024, L"The OSM data are converted into\n%s\nin %d min %d s, %d layers are added to the map.",
		sOutput.c_str(), nSeconds / 60, nSeconds % 60, nLayers);
	::MessageBox(m_hWnd, szText, pszCaption, MB_OK | MB_ICONINFORMATION);
	return 0;
#endif
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
