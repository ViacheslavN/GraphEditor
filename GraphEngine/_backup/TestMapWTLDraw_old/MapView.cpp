// TestGraphicsView.cpp : implementation of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MapView.h"

/*class CTrackCancel : public GisEngine::GisCommon::ITrackCancel
{
public:
	virtual void Cancel(){}
	virtual bool Continue(){return true;}
};*/

BOOL CMapView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

LRESULT CMapView::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CPaintDC dc(m_hWnd);


    GraphEngine::Display::GRect OutRect;
    GraphEngine::Display::GPoint OutPoint;
	OutRect.Set(0, 0, m_ptrGraphics->GetWidth(), m_ptrGraphics->GetHeight());


		OutPoint.x = 0;
		OutPoint.y = 0;

   // m_ptrGraphics->Erase(GraphEngine::Display::Color::Red);
    m_ptrGraphics->Erase(GraphEngine::Display::Color(0, 255, 0, 255));




    ::BitBlt(dc.m_hDC, 0, 0, m_ptrGraphics->GetWidth(), m_ptrGraphics->GetHeight(), m_ptrGraphics->GetDC(), 0, 0, SRCCOPY);
	//GisEngine::Display::CGraphicsWinGDI graphics(dc);
	//graphics.Copy(m_pGraphicsOpenGLWin.get(), OutPoint, OutRect);*/

	return 0;

}
LRESULT  CMapView::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	DWORD fwSizeType = wParam;     
	int nWidth = LOWORD(lParam);   
	int nHeight = HIWORD(lParam);  
	if(nWidth == 0 || nHeight == 0)
		return 0;

    m_ptrGraphics = GraphEngine::Display::IGraphics::CreateCGraphicsAgg(nWidth, nHeight, true);


	return 0;
}

LRESULT  CMapView::OnWireRender(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{


/*	if(!m_pGraphicsAgg.get())
		return 0;

	{

		GisEngine::Display::CBrush brush;
		GisEngine::Display::CPen Pen;
		brush.setColor(GisEngine::Display::Color(255, 0, 0, 255));
		brush.setBgColor(GisEngine::Display::Color(0, 255, 0, 255));

		Pen.setColor(GisEngine::Display::Color(0, 255, 0, 255));

		std::vector<GisEngine::Display::GPoint> vecPoint;

		std::vector<int> vecCounts;
 

 

		vecPoint.push_back(GisEngine::Display::GPoint(100, 100));
		vecPoint.push_back(GisEngine::Display::GPoint(200, 300));
		vecPoint.push_back(GisEngine::Display::GPoint(400, 250));
		vecPoint.push_back(GisEngine::Display::GPoint(500, 600));
		vecPoint.push_back(GisEngine::Display::GPoint(550, 20));
		vecPoint.push_back(GisEngine::Display::GPoint(100, 100));
		vecCounts.push_back(vecPoint.size());

		m_pGraphicsAgg->DrawPolyPolygon(nullptr, &brush, &vecPoint[0], &vecCounts[0], vecCounts.size());



	}
	Invalidate(FALSE);
	return 0;*/

	return 0;
}

LRESULT CMapView::OnTriangleRender(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{


	return 0;
}