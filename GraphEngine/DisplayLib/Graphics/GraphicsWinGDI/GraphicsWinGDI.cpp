#include "GraphicsWinGDI.h"
#include "../../../CommonLib/str/StringEncoding.h"


namespace GraphEngine {
    namespace Display {

        CGraphicsWinGDI::CGraphicsWinGDI(HWND hWnd)
                : m_bIsDeleteDC(false),
                  m_pSelectedBrush(0),
                  m_pSelectedPen(0),
                  m_pSelectedFont(0),
                  m_dFontOrientation(0),
                  m_hfirstFont(0),
                  m_hFirstBrush(0),
                  m_hFirstPen(0)
        {
            m_hDC = ::GetDC(hWnd);
            m_bIsReleaseDC = true;
        }

        CGraphicsWinGDI::CGraphicsWinGDI(HDC hDC, bool dIsDeleteDC)
                : m_bIsDeleteDC(dIsDeleteDC),
                  m_bIsReleaseDC(false),
                  m_pSelectedBrush(0),
                  m_pSelectedPen(0),
                  m_pSelectedFont(0),
                  m_dFontOrientation(0),
                  m_hfirstFont(0),
                  m_hFirstBrush(0),
                  m_hFirstPen(0)
        {
            m_hDC = hDC;
        }

        CGraphicsWinGDI::~CGraphicsWinGDI()
        {
            if(m_hfirstFont)
            {
                HGDIOBJ gdiObj = ::SelectObject(m_hDC, m_hfirstFont);
                ::DeleteObject(gdiObj);
            }

            if(m_hFirstBrush)
            {
                HGDIOBJ gdiObj = ::SelectObject(m_hDC, m_hFirstBrush);
                ::DeleteObject(gdiObj);
            }

            if(m_hfirstFont)
            {
                HGDIOBJ gdiObj = ::SelectObject(m_hDC, m_hfirstFont);
                ::DeleteObject(gdiObj);
            }

            if(m_bIsDeleteDC)
                ::ReleaseDC(m_hWnd, m_hDC);

            if(m_bIsReleaseDC)
                ::DeleteDC(m_hDC);

        }

        eDeviceType  CGraphicsWinGDI::GetDeviceType() const
        {
            switch(::GetDeviceCaps(m_hDC, TECHNOLOGY))
            {
                case DT_RASCAMERA:
                case DT_RASDISPLAY:
                    return DeviceTypeDisplay;
                case DT_RASPRINTER:
                    return DeviceTypePrinter ;
                case DT_PLOTTER:
                    return DeviceTypePlotter;
                case DT_CHARSTREAM:
                case DT_METAFILE:
                case DT_DISPFILE:
                    return DeviceTypeMetafile;
            }
            return DeviceTypeDisplay;
        }

        void  CGraphicsWinGDI::StartDrawing(IDisplayTransformationPtr pDT)
        {
            m_pSelectedPen = nullptr;
            m_pSelectedBrush = nullptr;
            m_pSelectedFont = nullptr;
            m_dFontOrientation = 0;

            m_nSavedDC = ::SaveDC(m_hDC);
        }

        void  CGraphicsWinGDI:: EndDrawing()
        {
            if(m_hFirstPen)
            {
                HGDIOBJ gdiObj = ::SelectObject(m_hDC, m_hFirstPen);
                ::DeleteObject(gdiObj);
                m_hFirstPen = 0;
            }
            if(m_hFirstBrush)
            {
                HGDIOBJ gdiObj = ::SelectObject(m_hDC, m_hFirstBrush);
                ::DeleteObject(gdiObj);
                m_hFirstBrush = 0;
            }
            if(m_hfirstFont)
            {
                HGDIOBJ gdiObj = ::SelectObject(m_hDC, m_hfirstFont);
                ::DeleteObject(gdiObj);
                m_hfirstFont = 0;
            }
            ::RestoreDC(m_hDC, m_nSavedDC);
        }

        GRect   CGraphicsWinGDI::GetClipRect() const
        {
            GRect res;

            RECT rect;
            HRGN hrgn = ::CreateRectRgn(0, 0, 0, 0);
            if(ERROR != GetClipRgn(m_hDC, hrgn))
            {
                if(ERROR != GetRgnBox(hrgn, &rect))
                    res.Set(GUnits(rect.left), GUnits(rect.top), GUnits(rect.right), GUnits(rect.bottom));
            }
            else
            {
                res.Set(0, 0, GUnits(GetDeviceCaps(m_hDC, HORZRES)), GUnits(GetDeviceCaps(m_hDC, VERTRES)));
            }
            ::DeleteObject(hrgn);

            return res;
        }

        void    CGraphicsWinGDI::SetClipRect(const GRect& rect)
        {
            HRGN hrgn = ::CreateRectRgn((int)(rect.xMin), (int)(rect.yMin), (int)(rect.xMax), (int)(rect.yMax));
            ::SelectClipRgn(m_hDC, hrgn);
            ::DeleteObject(hrgn);
        }

        void CGraphicsWinGDI::SetClipRgn(const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            HRGN hrgn = ::CreatePolyPolygonRgn(reinterpret_cast<const POINT*>(lpPoints), lpPolyCounts,  nCount, ALTERNATE);
            ::SelectClipRgn(m_hDC, hrgn);
            ::DeleteObject(hrgn);
        }

        void CGraphicsWinGDI::RemoveClip()
        {
            ::SelectClipRgn(m_hDC, 0);
        }

        void CGraphicsWinGDI::Erase(const Color& color, const GRect *pRect)
        {
            HBRUSH brush = ::CreateSolidBrush(color.GetRGB());
            RECT rc;
            if(pRect == 0)
                ::SetRect(&rc, 0, 0, ::GetDeviceCaps(m_hDC, HORZRES), ::GetDeviceCaps(m_hDC, VERTRES));
            else
                ::SetRect(&rc, (int)(pRect->xMin), (int)(pRect->yMin), (int)(pRect->xMax), (int)(pRect->yMax));
            ::FillRect(m_hDC, &rc, brush);
            ::DeleteObject(brush);
        }

        IGraphicsPtr  CGraphicsWinGDI::CreateCompatibleGraphics(GUnits width, GUnits height)
        {
            HDC hDC = ::CreateCompatibleDC(m_hDC);
            HBITMAP hBitmap = ::CreateCompatibleBitmap(m_hDC, (int)width, (int)height);
            DeleteObject(::SelectObject(hDC, hBitmap));
            return std::make_shared<CGraphicsWinGDI>(hDC, true);

        }
        void  CGraphicsWinGDI::Copy(IGraphicsPtr pSrc, const GPoint& srcPoint, const GRect& dstRect, bool bBlend)
        {
            if(CGraphicsWinGDI* pWinGDI = dynamic_cast<CGraphicsWinGDI*>(pSrc.get()))
                ::BitBlt(m_hDC, (int)dstRect.xMin, (int)dstRect.yMin, (int)dstRect.Width(), (int)dstRect.Height(), pWinGDI->m_hDC, (int)srcPoint.x, (int)srcPoint.y, SRCCOPY);
            else/* if(CGraphicsAgg* pAgg = dynamic_cast<CGraphicsAgg*>(pSrc))*/
            {
                BITMAPINFO bmi;
                ::memset(&bmi, 0, sizeof(bmi));
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                bmi.bmiHeader.biHeight = (int)pSrc->GetSurface()->Height();
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = pSrc->GetSurface()->Width();
                ::StretchDIBits(
                        m_hDC,
                        (int)dstRect.xMin, (int)dstRect.yMin, (int)dstRect.Width(), (int)dstRect.Height(),
                        (int)srcPoint.x, (int)(pSrc->GetSurface()->Height() - (srcPoint.y + dstRect.Height() - 1)), (int)dstRect.Width(), (int)dstRect.Height(),
                        pSrc->GetSurface()->Bits(),
                        &bmi,
                        DIB_RGB_COLORS, SRCCOPY );
            }
        }

        void CGraphicsWinGDI::DrawPoint(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint& Pt)
        {
            DrawPoint(pPen, pBbrush, Pt.x, Pt.y);
        }
        void CGraphicsWinGDI::DrawPoint(const PenPtr pPen, const BrushPtr pBbrush, GUnits dX, GUnits dY)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            const int DRAW_POINT_SIZE = 3;
            ::Ellipse(m_hDC, (int)(dX - DRAW_POINT_SIZE), (int)(dY - DRAW_POINT_SIZE), (int)(dX + DRAW_POINT_SIZE), (int)(dY + DRAW_POINT_SIZE));
        }
        void CGraphicsWinGDI::DrawPixel(GUnits dX, GUnits dY, const Color &color)
        {
            ::SetPixel(m_hDC, (int)dX, (int)dY, color.GetRGB());
        }

        void CGraphicsWinGDI::DrawLineSeg(const PenPtr pPen, const GPoint& P1, const GPoint& P2)
        {
            DrawLineSeg(pPen, P1.x, P1.y, P2.x, P2.y);
        }
        void CGraphicsWinGDI::DrawLineSeg(const PenPtr pPen, GUnits dX1, GUnits dY1, GUnits dX2, GUnits dY2)
        {
            SelectPen(pPen);
            ::MoveToEx(m_hDC, (int)dX1, (int)dY1, 0);
            ::LineTo(m_hDC, (int)dX2, (int)dY2);
        }

        void CGraphicsWinGDI::DrawLine(const PenPtr pPen, const GPoint* pPoints, int nNumPoints)
        {
            SelectPen(pPen);
            ::Polyline(m_hDC, reinterpret_cast<const POINT*>(pPoints), nNumPoints);
        }

        void CGraphicsWinGDI::DrawRoundRect(const PenPtr pPen, const BrushPtr  pBbrush, const GRect& Rect, GUnits radius)
        {
            DrawRect(pPen, pBbrush, Rect);
        }

        void CGraphicsWinGDI::DrawRect(const PenPtr pPen, const BrushPtr  pBbrush, const GRect& Rect)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            ::Rectangle(m_hDC, (int)Rect.xMin, (int)Rect.yMin, (int)Rect.xMax, (int)Rect.yMax);
        }

        void CGraphicsWinGDI::DrawRect(PenPtr pPen, BrushPtr  pBbrush, const GPoint& LTPoint, const GPoint& RBPoint)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            ::Rectangle(m_hDC, (int)LTPoint.x, (int)LTPoint.y, (int)RBPoint.x, (int)RBPoint.y);
        }

        void CGraphicsWinGDI::DrawRect(PenPtr pPen, BrushPtr pBbrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            ::Rectangle(m_hDC, (int)dLTX, (int)dLTY, (int)dRBX, (int)dRBY);
        }

        void CGraphicsWinGDI::DrawRectEx(const PenPtr pPen, const BrushPtr pBbrush, const GRect& Rect, const GPoint& originMin, const GPoint& originMax){}

        void CGraphicsWinGDI::DrawEllipse(const PenPtr pPen, const BrushPtr pBbrush, const GRect& Rect)
        {
            DrawEllipse(pPen, pBbrush, Rect.xMin, Rect.yMin, Rect.xMax, Rect.yMax);
        }

        void CGraphicsWinGDI::DrawEllipse(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint& LTPoint, const GPoint& RBPoint)
        {
            DrawEllipse(pPen, pBbrush, LTPoint.x, LTPoint.y, RBPoint.x, RBPoint.y);
        }

        void CGraphicsWinGDI::DrawEllipse(const PenPtr pPen, const BrushPtr pBbrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            ::Ellipse(m_hDC, (int)dLTX, (int)dLTY, (int)dRBX, (int)dRBY);
        }

        void CGraphicsWinGDI::DrawPolygon(const PenPtr pPen, const BrushPtr  pBbrush, const GPoint* pPoints, int nNumPoints)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            ::Polygon(m_hDC, reinterpret_cast<const POINT*>(pPoints), nNumPoints);
        }

        void CGraphicsWinGDI::DrawPolyPolygon(const PenPtr pPen, const BrushPtr pBbrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            SelectPen(pPen);
            SelectBrush(pBbrush);
            ::PolyPolygon(m_hDC, reinterpret_cast<const POINT*>(lpPoints), lpPolyCounts, nCount);
        }

        void CGraphicsWinGDI::DrawPolygonEx(const PenPtr pPen, const BrushPtr pBbrush, const GPoint* pPoints, int nNumPoints, const GPoint& originMin, const GPoint& originMax){}
        void CGraphicsWinGDI::DrawPolyPolygonEx(const PenPtr pPen, const BrushPtr pBbrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount, const GPoint& originMin, const GPoint& originMax){}

        void CGraphicsWinGDI::QueryTextMetrics(const FontPtr pFont, GUnits* height, GUnits* baseLine, GUnits* lineSpacing)
        {
            CreateFont(pFont);

            TEXTMETRIC metric;
            ::GetTextMetrics(m_hDC, &metric);

            if (height)
                *height = (GUnits)metric.tmHeight;

            if (baseLine)
                *baseLine = (GUnits)metric.tmAscent;
        }
        void CGraphicsWinGDI::QueryTextMetrics(const FontPtr pFont, const wchar_t* text, int len, GUnits* width, GUnits* height, GUnits* baseLine)
        {
            CreateFont(pFont);

            RECT rc;
            ::SetRect(&rc, 0, 0, 1, 1);
            ::DrawTextW(m_hDC, text, len, &rc, DT_CALCRECT);

            if (width)
                *width = (GUnits)(abs(rc.right - rc.left));

            if (height)
                *height = (GUnits)(abs(rc.bottom - rc.top));

            if (baseLine)
            {
                TEXTMETRIC metric;
                ::GetTextMetrics(m_hDC, &metric);
                *baseLine = (GUnits)(metric.tmAscent);
            }
        }
        void CGraphicsWinGDI::DrawText(const FontPtr pFont, const wchar_t* text, int len, const GPoint& point, int drawFlags)
        {
            if(len == 0)
                return;

            CreateFont(pFont);

            int nMode = SetBkMode(m_hDC, pFont->GetTransparent() ? TRANSPARENT : OPAQUE);
            COLORREF clrBg = SetBkColor(m_hDC, pFont->GetBgColor().GetRGB());
            COLORREF clrTx = SetTextColor(m_hDC, pFont->GetColor().GetRGB());
            UINT aligment = 0;
            GUnits height, baseline;
            //QueryTextMetrics(font, &text[0], 1, &height, 0, &baseline);

            TEXTMETRIC metrics;
            ::GetTextMetrics(m_hDC, &metrics);
            height = (GUnits)(metrics.tmHeight);
            baseline = (GUnits)(metrics.tmAscent);

            int stringCount = 1;
            for(const wchar_t* ptr = text; *ptr != L'\0' && (ptr - text) < len; ptr++)
                if(*ptr == L'\n')
                    stringCount++;

            double x_height = 0;
            double y_height = height;
            double x_baseline = 0;
            double y_baseline = baseline;

            if(pFont->GetOrientation() != 0.0)
            {
                double a = DEG2RAD(pFont->GetOrientation());
                double x_koef = sin(a);
                double y_koef = cos(a);

                x_height = height * x_koef;
                y_height = height * y_koef;

                x_baseline = baseline * x_koef;
                y_baseline = baseline * y_koef;
            }

            switch(pFont->GetTextHAlignment())
            {
                case TextHAlignmentLeft:
                    aligment |= TA_LEFT;
                    break;
                case TextHAlignmentCenter:
                    aligment |= TA_CENTER;
                    break;
                case TextHAlignmentRight:
                    aligment |= TA_RIGHT;
                    break;
            }

            GUnits dx = point.x;
            GUnits dy = point.y;

            switch(pFont->GetTextVAlignment())
            {
                case TextVAlignmentTop:
                    aligment |= TA_TOP;
                    break;
                case TextVAlignmentCenter:
                    aligment |= TA_TOP;
#ifdef _FLOAT_GUNITS_
                    dx -= GUnits((x_height * stringCount) / 2);
                    dy -= GUnits((y_height * stringCount) / 2);
#else
                    dx -= GUnits((x_height * stringCount) / 2 + 0.5);
				    dy -= GUnits((y_height * stringCount) / 2 + 0.5);
#endif
                    break;
                case TextVAlignmentBaseline:
                    aligment |= TA_TOP;
#ifdef _FLOAT_GUNITS_
                    dx -= GUnits((x_height * (stringCount - 1)) + x_baseline);
                    dy -= GUnits((y_height * (stringCount - 1)) + y_baseline);
#else
                    dx -= GUnits((x_height * (stringCount - 1)) + x_baseline + 0.5);
				    dy -= GUnits((y_height * (stringCount - 1)) + y_baseline + 0.5);
#endif
                    break;
                case TextVAlignmentBottom:
                    aligment |= TA_TOP;
#ifdef _FLOAT_GUNITS_
                    dy -= GUnits((x_height * stringCount));
                    dy -= GUnits((y_height * stringCount));
#else
                    dy -= GUnits((x_height * stringCount) + 0.5);
				    dy -= GUnits((y_height * stringCount) + 0.5);
#endif
                    break;
            }

            UINT oldAlignment =  SetTextAlign(m_hDC, aligment);
            const wchar_t* text_ = text;
            bool printed = false;
            for(const wchar_t* ptr = text_; ; ptr++)
            {
                if(*ptr == L'\n' || *ptr == L'\r')
                {
                    if(printed)
                    {
                        text_ = ptr + 1;
                        continue;
                    }
                }
                if(printed && *ptr == L'\0')
                    break;

                printed = false;

                if(*ptr == L'\0' || *ptr == L'\n' || *ptr == L'\r' || ptr - text >= len)
                {

                    if (pFont->GetHaloSize() != 0 && (drawFlags & TextDrawHaloOnly) && ptr - text_ > 0)
                    {
                        // Draw halo
                        ::BeginPath(m_hDC);
                        ::ExtTextOut(m_hDC, (int)dx, (int)dy, 0/*clipFlag*/, 0/*pClipRect*/, text_, UINT(ptr - text_), 0);
                        ::EndPath(m_hDC);
                        HPEN penHalo = ::CreatePen(PS_SOLID, (int)(pFont->GetHaloSize() * 2 + 1), pFont->GetBgColor().GetRGB());
                        HGDIOBJ penOld = ::SelectObject(m_hDC, penHalo);
                        ::StrokePath(m_hDC);
                        ::SelectObject(m_hDC, penOld);
                        ::DeleteObject(penHalo);
                    }


                    if(drawFlags & TextDrawTextOnly && ptr - text_ > 0)
                        ExtTextOut(m_hDC, (int)dx, (int)dy, 0, 0, text_, UINT(ptr - text_), 0);

                    dy += GUnits(y_height + 0.5);
                    dx += GUnits(x_height + 0.5);
                    if(*ptr == L'\0' || ptr - text >= len)
                        break;
                    printed = true;
                    ptr--;
                }
            }

            SetTextAlign(m_hDC, oldAlignment);
            SetBkColor(m_hDC, clrBg);
            SetTextColor(m_hDC, clrTx);
            SetBkMode(m_hDC, nMode);
        }

        void CGraphicsWinGDI::DrawTextByLine(const FontPtr pFont, const wchar_t* text, int len, const GPoint* pPoints, int nNumPoints){}

        void CGraphicsWinGDI::DrawBitmap(const BitmapPtr bitmap, const GRect& dstRect, bool flip_y, unsigned char alpha ){}

        void CGraphicsWinGDI::DrawRotatedBitmap(const BitmapPtr bitmap, const GPoint& center, double angle, bool flip_y, unsigned char alpha, double scale_x, double scale_y, bool clip, bool onGrid ){}

        Color CGraphicsWinGDI::GetPixel(GUnits x, GUnits y)
        {
            COLORREF ref = ::GetPixel(m_hDC, (int)x, (int)y);
            return Color(ref);
        }

        GPoint CGraphicsWinGDI::GetViewportOrg() const
        {
            POINT point;
            ::SetViewportOrgEx(m_hDC, 0, 0, &point);
            ::SetViewportOrgEx(m_hDC, point.x, point.y, 0);

            return GPoint((GUnits)point.x, (GUnits)point.y);
        }
        void   CGraphicsWinGDI::SetViewportOrg(const GPoint& org)
        {
            ::SetViewportOrgEx(m_hDC, (int)org.x, (int)org.y, 0);
        }

        GPoint CGraphicsWinGDI::GetBrushOrg() const
        {
            POINT point;

            ::SetBrushOrgEx(m_hDC, 0, 0, &point);
            ::SetBrushOrgEx(m_hDC, point.x, point.y, 0);

            return GPoint((GUnits)point.x, (GUnits)point.y);
        }
        void   CGraphicsWinGDI::SetBrushOrg(const GPoint& org)
        {
            ::SetBrushOrgEx(m_hDC, (int)org.x, (int)org.y, 0);
        }

        const BitmapPtr CGraphicsWinGDI::GetSurface() const
        {
            return m_ptrSurface;
        }

        BitmapPtr CGraphicsWinGDI::GetSurface()
        {
            return m_ptrSurface;
        }

        size_t CGraphicsWinGDI::GetWidth() const
        {
            return (size_t)m_nWidth;
        }

        size_t CGraphicsWinGDI::GetHeight() const
        {
            return (size_t)m_nHeight;
        }

        void CGraphicsWinGDI::Lock(){}
        void CGraphicsWinGDI::UnLock(){}

        void   CGraphicsWinGDI::SelectPen(const PenPtr pPen)
        {
            if(pPen != m_pSelectedPen && pPen)
            {
                HPEN hpen = CreatePen(pPen);
                m_pSelectedPen = pPen;
                if(m_hFirstPen != 0)
                    ::DeleteObject(::SelectObject(m_hDC, hpen));
                else
                    m_hFirstPen = (HPEN)::SelectObject(m_hDC, hpen);
            }
        }
        void   CGraphicsWinGDI::SelectBrush(const BrushPtr pBrush)
        {
            if(pBrush != m_pSelectedBrush)
            {
                HBRUSH hbrush = CreateBrush(pBrush);
                m_pSelectedBrush = pBrush;

                if(m_hFirstBrush != 0)
                    ::DeleteObject(::SelectObject(m_hDC, hbrush));
                else
                    m_hFirstBrush = (HBRUSH)::SelectObject(m_hDC, hbrush);
            }
        }
        HPEN   CGraphicsWinGDI::CreatePen(const PenPtr pPen)
        {
            LOGPEN logpen;
            logpen.lopnColor = pPen->GetColor().GetRGB();
            logpen.lopnStyle = pPen->GetPenType();
            logpen.lopnWidth.x = (LONG)pPen->GetWidth();
            return ::CreatePenIndirect(&logpen);
        }
        HBRUSH CGraphicsWinGDI::CreateBrush(const BrushPtr pBrush)
        {
            LOGBRUSH logbrush;
            logbrush.lbColor = pBrush->GetColor().GetRGB();
            logbrush.lbStyle = pBrush->GetType();
            //switch(brush->htype_)
            //{
            //case BrushHTypeBdiagonal:
            //  logbrush.lbHatch = HS_BDIAGONAL;
            //  break;
            //case BrushHTypeCross:
            //  logbrush.lbHatch = HS_CROSS;
            //  break;
            //case BrushHTypeDiagcross:
            //  logbrush.lbHatch = HS_DIAGCROSS;
            //  break;
            //case BrushHTypeFdiagonal:
            //  logbrush.lbHatch = HS_FDIAGONAL;
            //  break;
            //case BrushHTypeHorizontal:
            //  logbrush.lbHatch = HS_HORIZONTAL;
            //  break;
            //case BrushHTypeVertical:
            //  logbrush.lbHatch = HS_VERTICAL;
            //  break;
            //}
            return ::CreateBrushIndirect(&logbrush);
        }
        void   CGraphicsWinGDI::CreateFont(const FontPtr pFont)
        {
            if(m_pSelectedFont != pFont || m_dFontOrientation != pFont->GetOrientation())
            {
                LOGFONT logFont;
                ::memset(&logFont, 0, sizeof(LOGFONT));
                logFont.lfCharSet = (BYTE)pFont->GetCharSet();
                wcscpy(logFont.lfFaceName, CommonLib::StringEncoding::str_utf82w_safe(pFont->GetFace()).c_str());
                logFont.lfHeight = LONG(-pFont->GetSize());
                logFont.lfItalic = (pFont->GetStyle() & FontStyleItalic) ? TRUE : FALSE;
                logFont.lfOrientation = LONG(pFont->GetOrientation() * 10.0);
                logFont.lfEscapement = LONG(pFont->GetOrientation() * 10.0);
                logFont.lfUnderline = (pFont->GetStyle()& FontStyleUnderline) ? TRUE : FALSE;
                logFont.lfWeight = (pFont->GetStyle() & FontStyleBold) ? FW_BOLD : FW_NORMAL;
                m_pSelectedFont = pFont;
                m_dFontOrientation = pFont->GetOrientation();
                HFONT font = ::CreateFontIndirect(&logFont);
                if(m_hfirstFont != 0)
                    ::DeleteObject(::SelectObject(m_hDC, font));
                else
                    m_hfirstFont = (HFONT)::SelectObject(m_hDC, font);
            }
        }


    }
    }