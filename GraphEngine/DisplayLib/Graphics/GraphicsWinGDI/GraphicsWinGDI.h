#pragma once
#include "../../DisplayLib.h"

namespace GraphEngine {
    namespace Display {
        class CGraphicsWinGDI : public IGraphics
        {
        public:
            CGraphicsWinGDI(HWND hWnd);
            CGraphicsWinGDI(HDC hDC, bool dIsdeleteDC = false);
            ~CGraphicsWinGDI();



            virtual eDeviceType  GetDeviceType() const;
            virtual void        StartDrawing(IDisplayTransformationPtr pDT = nullptr);
            virtual void        EndDrawing();
            virtual GRect       GetClipRect() const ;
            virtual void        SetClipRect(const GRect& rect);
            virtual void        SetClipRgn(const GPoint* lpPoints, const int *lpPolyCounts, int nCount);
            virtual void        RemoveClip() ;
            virtual void        Erase(const Color& color, const GRect *rect = 0) ;
            virtual IGraphicsPtr   CreateCompatibleGraphics(GUnits width, GUnits height);
            virtual void        Copy(IGraphicsPtr src, const GPoint& srcPoint, const GRect& dstRect, bool bBlend = true);

            virtual void DrawPoint(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint& Pt);
            virtual void DrawPoint(const PenPtr pPen, const BrushPtr  pBbrush, GUnits dX, GUnits dY);
            virtual void DrawPixel(GUnits dX, GUnits dY, const Color &color);

            virtual void DrawLineSeg(const PenPtr pPen, const GPoint& P1, const GPoint& P2);
            virtual void DrawLineSeg(const PenPtr pPen, GUnits dX1, GUnits dY1, GUnits dX2, GUnits dY2);

            virtual void DrawLine(const PenPtr pPen, const GPoint* pPoints, int nNumPoints) ;

            virtual void DrawRoundRect(const PenPtr pPen, const BrushPtr  pBbrush, const GRect& Rect, GUnits radius);
            virtual void DrawRect(const PenPtr pPen, const BrushPtr  pBbrush, const GRect& Rect);
            virtual void DrawRect(PenPtr pPen, BrushPtr  pBbrush, const GPoint& LTPoint, const GPoint& RBPoint);
            virtual void DrawRect(PenPtr pPen, BrushPtr  pBbrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY) ;

            virtual void DrawRectEx(const PenPtr pPen, const BrushPtr  pBbrush, const GRect& Rect, const GPoint& originMin, const GPoint& originMax);
            virtual void DrawEllipse(const PenPtr pPen, const BrushPtr  pBbrush, const GRect& Rect);
            virtual void DrawEllipse(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint& LTPoint, const GPoint& RBPoint);
            virtual void DrawEllipse(const PenPtr pPen, const BrushPtr  pBbrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY);

            virtual void DrawPolygon(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint* pPoints, int nNumPoints);
            virtual void DrawPolyPolygon(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount);

            virtual void DrawPolygonEx(const PenPtr pPen, const BrushPtr pBbrush, const GPoint* pPoints, int nNumPoints, const GPoint& originMin, const GPoint& originMax);
            virtual void DrawPolyPolygonEx(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount, const GPoint& originMin, const GPoint& originMax);

            virtual void QueryTextMetrics(const FontPtr pFont, GUnits* height, GUnits* baseLine, GUnits* lineSpacing) ;
            virtual void QueryTextMetrics(const FontPtr pFont, const wchar_t* text, int len, GUnits* width, GUnits* height, GUnits* baseLine);
            virtual void DrawText(const FontPtr pFont, const wchar_t* text, int len, const GPoint& point, int drawFlags = TextDrawAll);
            virtual void DrawTextByLine(const FontPtr pFont, const wchar_t* text, int len, const GPoint* pPoints, int nNumPoints);

            virtual void DrawBitmap(const BitmapPtr bitmap, const GRect& dstRect, bool flip_y, unsigned char alpha = 255);
            virtual void DrawRotatedBitmap(const BitmapPtr bitmap, const GPoint& center, double angle, bool flip_y = false, unsigned char alpha = 255, double scale_x = 1.0, double scale_y = 1.0, bool clip = true, bool onGrid = false);

            virtual Color GetPixel(GUnits x, GUnits y);

            virtual GPoint GetViewportOrg() const;
            virtual void   SetViewportOrg(const GPoint& org);

            virtual GPoint GetBrushOrg() const ;
            virtual void   SetBrushOrg(const GPoint& org);

            virtual const BitmapPtr GetSurface() const;
            virtual BitmapPtr GetSurface();

            virtual size_t GetWidth() const;
            virtual size_t GetHeight() const;

            virtual void Lock();
            virtual void UnLock();
#ifdef _WIN32
            virtual HDC GetDC() const {			return m_hDC;	}
#endif
        private:
            void   SelectPen(const PenPtr pPen);
            void   SelectBrush(const BrushPtr pBrush);
            HPEN   CreatePen(const PenPtr pPen);
            HBRUSH CreateBrush(const BrushPtr pBrush);
            void   CreateFont(const FontPtr pFont);
        private:
            PenPtr    m_pSelectedPen;
            BrushPtr  m_pSelectedBrush;
            FontPtr   m_pSelectedFont;

            bool    m_bIsDeleteDC;
            bool    m_bIsReleaseDC;
            HWND    m_hWnd;
            int     m_nSavedDC;
            BitmapPtr  m_ptrSurface;
            HFONT m_hfirstFont;
            HPEN  m_hFirstPen;
            HBRUSH  m_hFirstBrush;
            HDC m_hDC;
            GUnits m_nWidth;
            GUnits m_nHeight;
            double  m_dFontOrientation;
        };



    }
    }
