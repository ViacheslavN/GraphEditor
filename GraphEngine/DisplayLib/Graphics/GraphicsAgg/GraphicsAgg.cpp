#include "GraphicsAgg.h"
#include "../../Agg/sin_cos_table.h"

namespace GraphEngine {
    namespace Display {

        CGraphicsAgg::CGraphicsAgg(unsigned char* buf, GUnits  width, GUnits height, bool bRelease, bool flipY) :
                m_nWidth(width),
                m_nHeight(height),
                m_offset_stroke(m_vertex_src),
                m_conv_stroke(m_offset_stroke),
                m_dash_stroke(m_vertex_src),
                m_dash_offset_stroke(m_dash_stroke),
                m_dash_conv_stroke(m_dash_offset_stroke),
                m_font_manager(m_font_engine),
                m_curves(m_font_manager.path_adaptor()),
                m_contour(m_curves),
                m_bspline(this->m_vertex_src),
                m_fsegm(m_curves),
                m_ftrans(m_fsegm, m_tcurve),
                m_fcontour(m_ftrans),
                m_rendering_buffer(m_rbuf),
#ifdef _WIN32
                m_dc(0),
#endif
                m_bflipY(flipY)


        {

            size_t line_size =  4 * (((size_t)width * 32 + 31) / 32);
            m_ptrSurface = std::make_shared<CBitmap>();

            m_ptrSurface->Attach(buf, size_t(width), size_t(height), BitmapFormatType32bppARGB, 0, bRelease);
            m_rbuf.attach(m_ptrSurface->Bits(), (uint32_t)width, (uint32_t)height, flipY ? (int)line_size : -1 *(int)line_size);
            m_renderer.attach(m_renderer_base);
            m_renderer_base.attach(this->m_rendering_buffer);


        }

        CGraphicsAgg::CGraphicsAgg( GUnits  width, GUnits height, bool flipY):
                m_nWidth(width),
                m_nHeight(height),
                m_offset_stroke(m_vertex_src),
                m_conv_stroke(m_offset_stroke),
                m_dash_stroke(m_vertex_src),
                m_dash_offset_stroke(m_dash_stroke),
                m_dash_conv_stroke(m_dash_offset_stroke),
                m_font_manager(m_font_engine),
                m_curves(m_font_manager.path_adaptor()),
                m_contour(m_curves),
                m_bspline(this->m_vertex_src),
                m_fsegm(m_curves),
                m_ftrans(m_fsegm, m_tcurve),
                m_fcontour(m_ftrans),
                m_rendering_buffer(m_rbuf),
#ifdef _WIN32
                m_dc(0),
#endif
                m_bflipY(flipY)
        {
            size_t line_size =  4 * (((size_t)width * 32 + 31) / 32);
#ifdef _WIN32
            m_dc = ::CreateCompatibleDC(0);


            size_t img_size = line_size* (int)height;
            size_t rgb_size = 0;
            size_t full_size = sizeof(BITMAPINFOHEADER);// + rgb_size + img_size;

            BITMAPINFO bitmapInfo;
            bitmapInfo.bmiHeader.biSize   = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth  = (LONG)width;
            bitmapInfo.bmiHeader.biHeight = (LONG)height;
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;
            bitmapInfo.bmiHeader.biSizeImage = (DWORD)img_size;
            bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
            bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
            bitmapInfo.bmiHeader.biClrUsed = 0;
            bitmapInfo.bmiHeader.biClrImportant = 0;


            unsigned char* buf;
            HBITMAP bitmap = ::CreateDIBSection(m_dc, &bitmapInfo, DIB_RGB_COLORS, (void**)&buf, 0, 0);
            m_oldBitmap = (HBITMAP)::SelectObject(m_dc, bitmap);
            m_ptrSurface = std::make_shared<CBitmap>();
            m_ptrSurface->Attach(buf, size_t(width), size_t(height), BitmapFormatType32bppARGB);
            m_rbuf.attach(m_ptrSurface->Bits(), (int)width, (int)height, flipY ? (int)line_size : -1 *(int)line_size);
#else

            int nBufSize = (int)line_size* height;
			unsigned char* pBuf = new unsigned char[nBufSize];
			m_ptrSurface->Attach(pBuf, size_t(width), size_t(height),BitmapFormatType32bppARGB, 0, true);
			m_rbuf.attach(m_ptrSurface->Bits(), (int)width, (int)height, (int)(flipY ? line_size : -line_size));
#endif

            m_renderer.attach(m_renderer_base);
            m_renderer_base.attach(this->m_rendering_buffer);
        }

        CGraphicsAgg::~CGraphicsAgg()
        {

#ifdef _WIN32
            ::DeleteObject(::SelectObject(m_dc, m_oldBitmap));
            ::DeleteDC(m_dc);
#endif
        }

        eDeviceType  CGraphicsAgg::GetDeviceType() const
        {
            return DeviceTypeDisplay;
        }

        void   CGraphicsAgg::StartDrawing(IDisplayTransformationPtr pDT)
        {
            RemoveClip();
        }
        void CGraphicsAgg::EndDrawing()
        {
            RemoveClip();
        }
        GRect CGraphicsAgg::GetClipRect() const
        {
            return m_ClipRect;
        }
        void CGraphicsAgg::SetClipRect(const GRect& rect)
        {
            GRect ClipRect = rect;
            ClipRect.xMin += m_org.x;
            ClipRect.xMax += m_org.x;
            ClipRect.yMin += m_org.y;
            ClipRect.yMax += m_org.y;
            this->m_rasterizer.clip_box(ClipRect.xMin, ClipRect.yMin, ClipRect.xMax, ClipRect.yMax);
            this->m_renderer_base.clip_box((int)ClipRect.xMin, (int)ClipRect.yMin, (int)(ClipRect.xMax - 1), (int)(ClipRect.yMax - 1));
            m_ClipRect = ClipRect;
        }

        void CGraphicsAgg::SetClipRgn(const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            int totalCount = 0;
            for(int ind = 0; ind < nCount; ++ind)
                totalCount += lpPolyCounts[ind];
            std::vector<agg::point_d> aggPts;
            aggPts.reserve(totalCount);
            for(int ind = 0; ind < totalCount; ++ind)
                aggPts.push_back(agg::point_d(lpPoints[ind].x, lpPoints[ind].y));
            this->m_renderer_base.set_clip_rgn(&aggPts[0], lpPolyCounts, nCount);
        }

        void CGraphicsAgg::RemoveClip()
        {
            m_renderer_base.reset_clipping(true);
            m_rasterizer.clip_box(0, 0, (double )GetSurface()->Width(), (double )GetSurface()->Height());
            m_ClipRect.Set(0, 0, (GUnits)GetSurface()->Width(), (GUnits)GetSurface()->Height());
            m_renderer_base.reset_polygon_clipping();
        }

        void CGraphicsAgg::Erase(const Color& color, const GRect *rect)
        {
            if(rect == 0)
            {
                if(color.GetRGBA() == 0)
                    ::memset(GetSurface()->Bits(), 0, 4 * this->m_renderer_base.width() * this->m_renderer_base.height());
                else
                    this->m_renderer_base.clear(agg::rgba8(color.GetR(), color.GetG(), color.GetB(), color.GetA()));
            }
            else
                this->m_renderer_base.copy_bar((int)rect->xMin, (int)rect->yMin, (int)(rect->xMax - 1), (int)(rect->yMax - 1), agg::rgba8(color.GetR(), color.GetG(), color.GetB(), color.GetA()));
        }

        IGraphicsPtr   CGraphicsAgg::CreateCompatibleGraphics(GUnits width, GUnits height)
        {
            return std::make_shared<CGraphicsAgg>(width, height, m_bflipY);
        }

        void CGraphicsAgg::InCopy(const GPoint& srcPoint, const GRect& dstRect)
        {
            GPoint srcPoint2 = srcPoint;
            GRect dstRect2 = dstRect;
            if(dstRect2.xMax > (GUnits)this->m_renderer_base.width())
                dstRect2.xMax = (GUnits)this->m_renderer_base.width();
            if(dstRect2.yMax > (GUnits)this->m_renderer_base.height())
                dstRect2.yMax = (GUnits)this->m_renderer_base.height();
            if(dstRect2.xMin < 0)
            {
                srcPoint2.x -= dstRect2.xMin;
                dstRect2.xMin = 0;
            }
            if(dstRect2.yMin < 0)
            {
                srcPoint2.y -= dstRect2.yMin;
                dstRect2.yMin = 0;
            }

            if(dstRect2.IsEmpty())
                return;

            bool forwardY = (srcPoint2.y > dstRect2.yMin);
            bool forwardX = (srcPoint2.y != dstRect2.yMin) || ((srcPoint2.y == dstRect2.yMin) && (srcPoint2.x > dstRect2.xMin));

            int ybegin = (int)((forwardY) ? srcPoint2.y : (srcPoint2.y + dstRect2.Height()));
            int yend = (int)((forwardY) ? (srcPoint2.y + dstRect2.Height()) : srcPoint2.y - 1);

            int yoffset = (int)(dstRect2.yMin - srcPoint2.y);
            int inc = (forwardY) ? 1 : -1;

            for(int y = ybegin; y != yend; y += inc)
            {
                if(y >= (int)this->m_renderer_base.height() || y < 0)
                    continue;

                if(y + yoffset >= (int)this->m_renderer_base.height() || y + yoffset < 0)
                    continue;

                agg::int8u* row = this->m_renderer_base.ren().row_ptr(y);
                agg::int8u* dstRow = this->m_renderer_base.ren().row_ptr(y + yoffset);

#ifdef _FLOAT_GUNITS_
                if(forwardX)
                    ::memcpy(dstRow + (int)(dstRect2.xMin * 4), row + (int)(srcPoint2.x * 4), (int)(dstRect2.Width() * 4));
                else
                    ::memmove(dstRow + (int)(dstRect2.xMin * 4), row + (int)(srcPoint2.x * 4), (int)(dstRect2.Width() * 4));
#else
                if(forwardX)
					 ::memcpy(dstRow + (dstRect2.xMin << 2), row + (srcPoint2.x << 2), dstRect2.Width() << 2);
				 else
					 ::memmove(dstRow + (dstRect2.xMin << 2), row + (srcPoint2.x << 2), dstRect2.Width() << 2);
#endif

            }
        }
        void CGraphicsAgg::Copy(IGraphicsPtr pSrc, const GPoint& srcPt, const GRect& dstRC, bool bBlend)
        {
            GPoint srcPoint = srcPt;
            srcPoint.x += m_org.x;
            srcPoint.y += m_org.y;
            GRect dstRect = dstRC;
            dstRect.xMin += m_org.x;
            dstRect.xMax += m_org.x;
            dstRect.yMin += m_org.y;
            dstRect.yMax += m_org.y;
            if(pSrc.get() == this)
                InCopy(srcPoint, dstRect);
            else if (CGraphicsAgg * pGraphicsAgg = dynamic_cast<CGraphicsAgg*>(pSrc.get()))
            {
                agg::rect_i rect((int)srcPoint.x, (int)srcPoint.y, (int)(srcPoint.x + dstRect.Width() - 1), (int)(srcPoint.y + dstRect.Height() - 1));
                m_renderer_base.blend_from(pGraphicsAgg->m_rendering_buffer, &rect, (int)(dstRect.xMin - srcPoint.x), (int)(dstRect.yMin - srcPoint.y));
            }
            copy(pSrc, srcPoint, dstRect);
        }

        void CGraphicsAgg::copy(IGraphicsPtr pSrc, const GPoint& srcPoint, const GRect& dstRect, bool bBlend)
        {
            agg::rect_i rect((int)srcPoint.x, (int)srcPoint.y, (int)(srcPoint.x + dstRect.Width() - 1), (int)(srcPoint.y + dstRect.Height() - 1));
            for(int y = (int)srcPoint.y, dsty = (int)dstRect.yMin; y < (int)(srcPoint.y + dstRect.Height()); y++, dsty++)
            {
                for(int x = (int)srcPoint.x, dstx = (int)dstRect.xMin; x < (int)(srcPoint.x + dstRect.Width()); x++, dstx++)
                {
                    if(dstx >= 0 && dstx < (int)this->m_rendering_buffer.width() && dsty >= 0 && dsty < (int)m_rendering_buffer.height())
                    {
                        Color pixel = pSrc->GetPixel((GUnits)x, (GUnits)y);
                        this->m_rendering_buffer.blend_pixel(dstx, dsty, agg::rgba8(pixel.GetR(), pixel.GetG(), pixel.GetB(), pixel.GetA()), pixel.GetA());
                    }
                }
            }
        }

        void CGraphicsAgg::DrawPoint(const PenPtr pPen, const BrushPtr  pBrush, const GPoint& Pt)
        {
            DrawPoint(pPen, pBrush, Pt.x, Pt.y);
        }

        void CGraphicsAgg::DrawPoint(const PenPtr pPen, const BrushPtr pBrush, GUnits dX, GUnits dY)
        {


        }
        void CGraphicsAgg::DrawPixel(GUnits X, GUnits Y, const Color &color)
        {
            // int nIndex = dY * rbuf_.width() * 4 + dX * 4;

            // *(Color::ColorType*)&m_surface.bits()[nIndex] = color.GetRGBA();
            this->m_rendering_buffer.blend_pixel((int)X, (int)Y, agg::rgba8(color.GetR(), color.GetG(), color.GetB(),  (uint32_t)color.GetA()), 255);
        }

        void CGraphicsAgg::DrawLineSeg(const PenPtr pPen, const GPoint& P1, const GPoint& P2)
        {
            DrawLineSeg(pPen, P1.x, P1.y, P2.x, P2.y);
        }
        void CGraphicsAgg::DrawLineSeg(const PenPtr pPen, GUnits dX1, GUnits dY1, GUnits dX2, GUnits dY2)
        {
            GPoint points[2];
            points[0].x = dX1;
            points[0].y = dY1;
            points[1].x = dX2;
            points[1].y = dY2;
            DrawLine(pPen, points, 2);
        }

        void CGraphicsAgg::DrawLine(const PenPtr pPen, const GPoint* pPoints, int nNumPoints)
        {
            if(pPen == 0 || pPen->GetPenType() == PenTypeNull)
                return;

            if(pPen->GetOffset() != 0 && nNumPoints == 2 && pPoints[0] == pPoints[1])
                return;

            if(pPen->GetOffset() != 0)
            {
                this->m_offset_stroke.SetOffset(pPen->GetOffset());
                this->m_dash_offset_stroke.SetOffset(pPen->GetOffset());
            }

            if(pPoints == 0)
                return;

            if(pPen->GetWidth() == 0)
                return;

            this->m_vertex_src.Attach(pPoints, &nNumPoints, 1);
            draw_line(pPen);

            if(pPen->GetOffset() != 0)
            {
                this->m_offset_stroke.SetOffset(0);
                this->m_dash_offset_stroke.SetOffset(0);
            }
        }
        template <typename Stroke>
        void AppendPen(const PenPtr pPen, Stroke& stroke)
        {
            switch(pPen->GetCapType())
            {
                case CapTypeButt:
                    stroke.line_cap(agg::butt_cap);
                    break;
                case CapTypeRound:
                    stroke.line_cap(agg::round_cap);
                    break;
                case CapTypeSquare:
                    stroke.line_cap(agg::square_cap);
                    break;
            }
            switch(pPen->GetJoinType())
            {
                case JoinTypeMiter:
                    stroke.line_join(agg::miter_join);
                    break;
                case JoinTypeRound:
                    stroke.line_join(agg::round_join);
                    break;
                case JoinTypeBevel:
                    stroke.line_join(agg::bevel_join);
                    break;
            }
        }

        void CGraphicsAgg::draw_line(const PenPtr pPen)
        {
            if(pPen->GetPenType() != PenTypeSolid || pPen->GetTemplates().size() > 0)
            {
                this->m_dash_stroke.remove_all_dashes();
                if(pPen->GetTemplates().size() > 0)
                {
                    for(size_t i = 0, sz = pPen->GetTemplates().size(); i < sz; i++)
                        this->m_dash_stroke.add_dash(pPen->GetTemplates()[i].first, pPen->GetTemplates()[i].second);
                }
                else
                {
                    switch(pPen->GetPenType())
                    {
                        case PenTypeDash:
                            this->m_dash_stroke.add_dash(3 * pPen->GetWidth(), 2 * pPen->GetWidth());
                            break;
                        case PenTypeDot:
                            this->m_dash_stroke.add_dash(pPen->GetWidth(), 2 * pPen->GetWidth());
                            break;
                        case PenTypeDashDot:
                            this->m_dash_stroke.add_dash(3 * pPen->GetWidth(), 2 * pPen->GetWidth());
                            this->m_dash_stroke.add_dash(pPen->GetWidth(), 2 * pPen->GetWidth());
                            break;
                        case PenTypeDashDotDot:
                            this->m_dash_stroke.add_dash(3 * pPen->GetWidth(), 2 * pPen->GetWidth());
                            this->m_dash_stroke.add_dash(pPen->GetWidth(), 2 * pPen->GetWidth());
                            this->m_dash_stroke.add_dash(pPen->GetWidth(), 2 * pPen->GetWidth());
                            break;
                    }
                }
            }

            this->m_renderer.color(agg::rgba8(pPen->GetColor().GetR(),
                                              pPen->GetColor().GetG(),
                                              pPen->GetColor().GetB(),
                                              pPen->GetColor().GetA()));

            if(pPen->GetPenType() != PenTypeSolid || pPen->GetTemplates().size() > 0)
            {
                AppendPen(pPen, this->m_dash_conv_stroke);
                this->m_dash_conv_stroke.width(pPen->GetWidth());
                this->m_rasterizer.add_path(this->m_dash_conv_stroke);
            }
            else
            {
                AppendPen(pPen, this->m_conv_stroke);
                this->m_conv_stroke.width(pPen->GetWidth());
                this->m_rasterizer.add_path(this->m_conv_stroke);
            }
            if(pPen->GetTexture() == nullptr)
            {
                agg::render_scanlines(this->m_rasterizer, this->m_scanline,
                                      this->m_renderer);
            }
            else
            {
                if(pPen->GetTexture()->Bpp() != 32)
                    return;

                BitmapPtr pTexture = pPen->GetTexture();
                int width = (int)pTexture->Width();
                int height = (int)pTexture->Height();
                m_pattern_rbuf.attach(pTexture->Bits(), width, height, -width*4);
                m_pattern_rgba32.attach(this->m_pattern_rbuf);

                pattern_accessor_t pattern_accessor(this->m_pattern_rgba32);
                span_pattern_generator_t span_pattern_generator(pattern_accessor, (int)m_brushOrg.x, (int)m_brushOrg.y);
                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline,
                                         this->m_renderer_base, this->m_span_allocator,
                                         span_pattern_generator);
            }
        }

        void CGraphicsAgg::draw_poly_polygon(const BrushPtr pBrush, const GPoint& originMin , const GPoint& originMax)
        {
            BitmapPtr pTexture = pBrush->GetTexture();
            if(pTexture)
                if(pTexture->Bpp() != 32)
                    return;

            const Color& color = pBrush->GetColor();
            const Color& bgColor = pBrush->GetBgColor();

            m_renderer.color(agg::rgba8(color.GetR(),color.GetG(), color.GetB(), color.GetA()));
            m_rasterizer.add_path(this->m_vertex_src);

            if(pTexture && pBrush->GetType() == BrushTypeTextured)
            {


                int width = (int)pTexture->Width();
                int height = (int)pTexture->Height();
                this->m_pattern_rbuf.attach(pTexture->Bits(), width, height, -width*4);
                this->m_pattern_rgba32.attach(this->m_pattern_rbuf);

                pattern_accessor_t pattern_accessor(this->m_pattern_rgba32);
                span_pattern_generator_t span_pattern_generator(pattern_accessor, (int)m_brushOrg.x, (int)m_brushOrg.y);
                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline,
                                         this->m_renderer_base, this->m_span_allocator,
                                         span_pattern_generator);
            }
            else if(pBrush->GetType() == BrushTypeGradientHorizontal)
            {
                typedef agg::span_interpolator_linear<> interpolator_t;
                typedef agg::gradient_linear_color<agg::rgba8> color_func_t;
                typedef agg::span_gradient<agg::rgba8, interpolator_t, agg::gradient_x, color_func_t> span_gradient_t;

                color_func_t color_func(agg::rgba8(color.GetR(), color.GetG(), color.GetB(), color.GetA()),
                                        agg::rgba8(bgColor.GetR(), bgColor.GetG(), bgColor.GetB(), bgColor.GetA()));
                agg::gradient_x gradient_func;
                agg::trans_affine tr;
                interpolator_t interpolator(tr);

                double d1 = 1000000000, d2 = 0;
                this->m_vertex_src.rewind(0);

                if(originMin == originMax)
                {
#ifdef INTEGER_VERTEX
                    int x, y;
					 while(this->m_vertex_src.vertex(&x, &y) != agg::path_cmd_stop)
					 {
						 if(d1 > x * agg::poly_subpixel_scale) d1 = x * agg::poly_subpixel_scale;
						 if(d2 < x * agg::poly_subpixel_scale) d2 = x * agg::poly_subpixel_scale;
					 }
#else
                    double x, y;
                    while(this->m_vertex_src.vertex(&x, &y) != agg::path_cmd_stop)
                    {
                        if(d1 > x) d1 = x;
                        if(d2 < x) d2 = x;
                    }
#endif
                }
                else
                {
                    d1 = originMin.x;
                    d2 = originMax.x;
                }
                span_gradient_t gradient(interpolator, gradient_func, color_func, d1, d2);
                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline,
                                         this->m_renderer_base, this->m_span_allocator, gradient);
            }
            else if(pBrush->GetType() == BrushTypeGradientVertical)
            {
                typedef agg::span_interpolator_linear<> interpolator_t;
                typedef agg::gradient_linear_color<agg::rgba8> color_func_t;
                typedef agg::span_gradient<agg::rgba8, interpolator_t, agg::gradient_y, color_func_t> span_gradient_t;

                color_func_t color_func(agg::rgba8(color.GetR(), color.GetG(), color.GetB(), color.GetA()),
                                        agg::rgba8(bgColor.GetR(), bgColor.GetG(), bgColor.GetB(), bgColor.GetA()));
                agg::gradient_y gradient_func;
                agg::trans_affine tr;
                interpolator_t interpolator(tr);

                double d1 = 1000000000, d2 = 0;
                this->m_vertex_src.rewind(0);

                if(originMin == originMax)
                {
#ifdef INTEGER_VERTEX
                    int x, y;
					 while(this->m_vertex_src.vertex(&x, &y) != agg::path_cmd_stop)
					 {
						 if(d1 > y * agg::poly_subpixel_scale) d1 = y * agg::poly_subpixel_scale;
						 if(d2 < y * agg::poly_subpixel_scale) d2 = y * agg::poly_subpixel_scale;
					 }
#else
                    double x, y;
                    while(this->m_vertex_src.vertex(&x, &y) != agg::path_cmd_stop)
                    {
                        if(d1 > y) d1 = y;
                        if(d2 < y) d2 = y;
                    }
#endif
                }
                else
                {
                    d1 = originMin.y;
                    d2 = originMax.y;
                }
                span_gradient_t gradient(interpolator, gradient_func, color_func, d1, d2);
                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline,
                                         this->m_renderer_base, this->m_span_allocator, gradient);
            }
            else
                agg::render_scanlines(this->m_rasterizer, this->m_scanline,
                                      this->m_renderer);
        }
        void CGraphicsAgg::DrawRoundRect(const PenPtr pPen, const BrushPtr  pBrush, const GRect& Rect, GUnits radius)
        {
            this->m_rounded_rect.init(Rect.xMin, Rect.yMin, Rect.xMax, Rect.yMax, radius);
            this->m_rounded_rect.normalize_radius();
            this->m_vertex_src.Attach(this->m_rounded_rect);

            draw_poly_polygon(pBrush);
            if(pPen && pPen->GetPenType() != PenTypeNull)
                draw_line(pPen);
        }

        void CGraphicsAgg::DrawRect(const PenPtr pPen, const BrushPtr  pBrush, const GRect& Rect)
        {
            GPoint points[5];
            points[0].x = Rect.xMin;
            points[0].y = Rect.yMin;

            points[1].x = Rect.xMin;
            points[1].y = Rect.yMax - 1;

            points[2].x = Rect.xMax - 1;
            points[2].y = Rect.yMax - 1;

            points[3].x = Rect.xMax - 1;
            points[3].y = Rect.yMin;

            points[4].x = Rect.xMin;
            points[4].y = Rect.yMin;

            DrawPolygon(pPen, pBrush, points, 5);
        }

        void CGraphicsAgg::DrawRect(PenPtr pPen, BrushPtr pBrush, const GPoint& LTPoint, const GPoint& RBPoint)
        {
            GPoint points[5];
            points[0].x = LTPoint.x;
            points[0].y = LTPoint.y;

            points[1].x = LTPoint.x;
            points[1].y = RBPoint.y - 1;

            points[2].x = RBPoint.x - 1;
            points[2].y = RBPoint.y - 1;

            points[3].x = RBPoint.x - 1;
            points[3].y = LTPoint.y;

            points[4].x = LTPoint.x;
            points[4].y = LTPoint.y;

            DrawPolygon(pPen, pBrush, points, 5);
        }

        void CGraphicsAgg::DrawRect(PenPtr pPen, BrushPtr  pBrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY)
        {
            GPoint points[5];
            points[0].x = dLTX;
            points[0].y = dLTY;

            points[1].x = dLTY;
            points[1].y = dRBY - 1;

            points[2].x = dRBX - 1;
            points[2].y = dRBY - 1;

            points[3].x = dRBX- 1;
            points[3].y = dLTY;

            points[4].x = dLTX;
            points[4].y = dLTX;

            DrawPolygon(pPen, pBrush, points, 5);
        }

        void CGraphicsAgg::DrawRectEx(const PenPtr pPen, const BrushPtr  pBrush, const GRect& Rect, const GPoint& originMin, const GPoint& originMax)
        {
            GPoint points[5];
            points[0].x = Rect.xMin;
            points[0].y = Rect.yMin;

            points[1].x = Rect.xMin;
            points[1].y = Rect.yMax - 1;

            points[2].x = Rect.xMax - 1;
            points[2].y = Rect.yMax - 1;

            points[3].x = Rect.xMax - 1;
            points[3].y = Rect.yMin;

            points[4].x = Rect.xMin;
            points[4].y = Rect.yMin;
            DrawPolygonEx(pPen, pBrush, points, 5, originMin, originMax);
        }

        void CGraphicsAgg::DrawEllipse(const PenPtr pPen, const BrushPtr  pBrush, const GRect& Rect)
        {
            DrawEllipse(pPen, pBrush, Rect.xMin, Rect.yMin, Rect.xMax, Rect.yMax);
        }

        void CGraphicsAgg::DrawEllipse(const PenPtr pPen, const BrushPtr  pBrush, const GPoint& LTPoint, const GPoint& RBPoint)
        {
            DrawEllipse(pPen, pBrush, LTPoint.x, LTPoint.y, RBPoint.x, RBPoint.y);
        }

        void CGraphicsAgg::DrawEllipse(const PenPtr pPen, const BrushPtr  pBrush, GUnits dLTX, GUnits dLTY, GUnits dRBX, GUnits dRBY)
        {
            GUnits width = dRBX - dLTX;
            GUnits height = dRBY - dLTY;
            this->m_ellipse.init(dLTX + width / 2, dLTY + height / 2, width / 2, height / 2);
            this->m_vertex_src.Attach(this->m_ellipse);

            draw_poly_polygon(pBrush);
            if(pPen && pPen->GetPenType() != PenTypeNull)
                draw_line(pPen);
        }

        void CGraphicsAgg::DrawPolygon(const PenPtr pPen, const BrushPtr  pBrush, const GPoint* pPoints, int nNumPoints)
        {
            DrawPolyPolygon(pPen, pBrush, pPoints, &nNumPoints, 1);
        }
        void CGraphicsAgg::DrawPolyPolygon(const PenPtr pPen, const BrushPtr  pBrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
        {
            if(nCount == 0)
                return;

            if(pBrush && pBrush->GetType() != BrushTypeNull)
            {
                this->m_vertex_src.Attach(lpPoints, lpPolyCounts, nCount);
                draw_poly_polygon(pBrush);
            }

            if(pPen && pPen->GetPenType() != PenTypeNull)
                for(int i = 0, offset = 0; i < nCount; offset += lpPolyCounts[i], i++)
                    DrawLine(pPen, lpPoints + offset, lpPolyCounts[i]);

        }

        void CGraphicsAgg::DrawPolygonEx(const PenPtr pPen, const BrushPtr  pBrush, const GPoint* pPoints, int nNumPoints, const GPoint& originMin, const GPoint& originMax)
        {
            DrawPolyPolygonEx(pPen, pBrush, pPoints, &nNumPoints, 1, originMin, originMax);
        }
        void CGraphicsAgg::DrawPolyPolygonEx(const PenPtr pPen, const BrushPtr  pBrush, const GPoint* lpPoints, const int *lpPolyCounts, int nCount, const GPoint& originMin, const GPoint& originMax)
        {
            if(nCount == 0)
                return;

            if(pBrush && pBrush->GetType() != BrushTypeNull)
            {
                if(pBrush->GetType() == BrushTypeGradientHorizontal || pBrush->GetType() == BrushTypeGradientVertical)
                {
                    this->m_vertex_src.Attach(lpPoints, lpPolyCounts, nCount);
                    draw_poly_polygon(pBrush, originMin, originMax);
                }
                else
                    DrawPolyPolygon(0, pBrush, lpPoints, lpPolyCounts, nCount);
            }

            if(pPen && pPen->GetPenType() != PenTypeNull)
                for(int i = 0, offset = 0; i < nCount; offset += lpPolyCounts[i], i++)
                    DrawLine(pPen, lpPoints + offset, lpPolyCounts[i]);
        }


        std::string CGraphicsAgg::get_font_path(const FontPtr pFont, bool& customDecoration)
        {
            std::string fullFontName;
            customDecoration = true;
#ifdef WIN32
            char spzBuf[MAX_PATH];
            ::GetWindowsDirectoryA(spzBuf, MAX_PATH);
            fullFontName = spzBuf;
            fullFontName += "\\Fonts\\";

            HKEY key;
            if(::RegOpenKeyExA(HKEY_LOCAL_MACHINE,  "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) == ERROR_SUCCESS)
            {
                char str[256];
                ::memset(str, 0, sizeof(str));
                DWORD len = 255 * sizeof(TCHAR);
                std::string  sFace = pFont->GetFace();

                customDecoration = false;

                if(pFont->GetStyle() & FontStyleBold)
                    sFace += " Bold";

                if(pFont->GetStyle() & FontStyleItalic)
                    sFace += " Italic";


                LONG ret = ::RegQueryValueExA(key,  std::string ((sFace + " (TrueType)")).c_str(), 0, 0, (LPBYTE)str, &len);
                if(ret == ERROR_SUCCESS)
                {
                    ::RegCloseKey(key);
                    fullFontName += str;
                    return fullFontName;
                }
                ret = ::RegQueryValueExA(key, (pFont->GetFace() + " (TrueType)").c_str(), 0, 0, (LPBYTE)str, &len);
                customDecoration = true;
                if(ret == ERROR_SUCCESS)
                {
                    ::RegCloseKey(key);
                    fullFontName += str;
                    return fullFontName;
                }
            }

            fullFontName += "Arial.ttf";
            return fullFontName;
#elif defined (ANDROID)
            // fullFontName = L"/system/fonts/DroidSans.ttf";
			// return fullFontName;
			 fullFontName = L"/system/fonts/";
			 fullFontName += pFont->getFace();
			 fullFontName += L".ttf";
			 return fullFontName;
#endif
        }


        void CGraphicsAgg::create_font(const FontPtr pFont, bool& customDecoration)
        {
            std::string  fullFontName = get_font_path(pFont, customDecoration);
            m_font_engine.load_font(fullFontName.c_str(), 0, agg::glyph_ren_outline);
            agg::trans_affine mtx;
            mtx *= agg::trans_affine_rotation(-DEG2RAD(pFont->GetOrientation())); //radians
            mtx.flip_y();
            if((pFont->GetStyle() & FontStyleBold) && customDecoration)
                mtx *= agg::trans_affine_scaling(1.05, 1);
            if((pFont->GetStyle() & FontStyleItalic) && customDecoration)
                mtx *= agg::trans_affine_skewing(-0.3, 0);
            //font_engine_.width(font_engine_.width() * 2);
            m_font_engine.transform(mtx);
            m_font_engine.height(pFont->GetSize());
            //double actualHeight = font_engine_.ascender() - font_engine_.descender();
            //font_engine_.height(font_engine_.height() * (font->size_ / actualHeight));
        }

        void CGraphicsAgg::QueryTextMetrics(const FontPtr pFont, GUnits* height, GUnits* baseLine, GUnits* lineSpacing)
        {
            double oldOrientation = pFont->GetOrientation();
            const_cast<CFont*>(pFont.get())->SetOrientation(0);
            bool customDecoration;
            create_font(pFont, customDecoration);

            if(height)
                *height = (GUnits)(m_font_engine.ascender() - m_font_engine.descender());//(GUnits)font_engine_.height();

            if(baseLine)
                *baseLine = (GUnits)m_font_engine.ascender();

            if(lineSpacing)
                *lineSpacing = (GUnits)m_font_engine.height();

            const_cast<CFont*>(pFont.get())->SetOrientation(oldOrientation);
        }
        void CGraphicsAgg::QueryTextMetrics(const FontPtr pFont, const wchar_t* text, int len, GUnits* width, GUnits* height, GUnits* baseLine)
        {
            if(len == 0)
            {
                *width = 0;
                *height = 0;
                *baseLine = 0;
                return;
            }

            double oldOrientation  = pFont->GetOrientation();
            const_cast<CFont*>(pFont.get())->SetOrientation(0);
            bool customDecoration;
            create_font(pFont, customDecoration);

            double x = 0.5;
            double y = 0.5;
            double dx = 0, dy = 0;
            for(const wchar_t * p = text; *p && len; p++, len--)
            {
                const agg::glyph_cache* glyph = m_font_manager.glyph(*p);
                if(glyph)
                {
                    m_font_manager.add_kerning(&x, &y);
                    m_font_manager.init_embedded_adaptors(glyph, x, y);
                    dx += ((pFont->GetStyle() & FontStyleBold) && customDecoration) ?  glyph->advance_x * 1.05 : glyph->advance_x; // increment pen position
                    dy += glyph->advance_y;
                }
            }

            if(width)
                *width = (GUnits)dx;

            if(height)
                *height = (GUnits)(m_font_engine.ascender() - m_font_engine.descender());

            if(baseLine)
                *baseLine = (GUnits)m_font_engine.ascender();

            double h = m_font_engine.height();
            double a = m_font_engine.ascender();
            double d = m_font_engine.descender();

            const_cast<CFont*>(pFont.get())->SetOrientation(oldOrientation);
        }

        void CGraphicsAgg::DrawUnderLine(const FontPtr pFont, double xNew, double yNew, double x, double y)
        {
            if(pFont->GetStyle() & FontStyleUnderline)
            {
                GPoint pts[2];
                pts[0].x = (GUnits)xNew;
                pts[0].y = (GUnits)yNew;
                pts[1].x = (GUnits)x;
                pts[1].y = (GUnits)y;
                if(pts[0].y == pts[1].y)
                    pts[0].y = pts[1].y = (GUnits)ceil(pts[0].y);
                int count = 2;
                this->m_vertex_src.Attach(pts, &count, 1);
                this->m_rasterizer.add_path(this->m_conv_stroke);
            }
        }

        void CGraphicsAgg::DrawText(const FontPtr pFont, const wchar_t* text, int len, const GPoint& point, int drawFlags)
        {
            GUnits layoutWidth = 0;
            GUnits layoutHeight = 0;
            GUnits layoutBaseLine = 0;
            bool customDecoration;
            QueryTextMetrics(pFont, text, len, &layoutWidth, &layoutHeight, &layoutBaseLine);

            create_font(pFont, customDecoration);

            double dy = 0;
            switch(pFont->GetTextVAlignment())
            {
                case TextVAlignmentTop      : dy = layoutBaseLine; break;
                case TextVAlignmentCenter   : dy = -(layoutHeight / 2.0 - layoutBaseLine); break;
                case TextVAlignmentBaseline : dy = 0; break;
                case TextVAlignmentBottom   : dy = -(layoutHeight - layoutBaseLine); break;
            }

            double dx = 0;
            switch(pFont->GetTextHAlignment())
            {
                case TextHAlignmentLeft	   :	dx = 0; break;
                case TextHAlignmentCenter   :	dx = - layoutWidth / 2.0; break;
                case TextHAlignmentRight    :	dx = - layoutWidth; break;
            }

            long _c = (long)agg::cos_long((long)pFont->GetOrientation());
            long _s = (long)agg::sin_long((long)pFont->GetOrientation());

            double xNew = point.x + (_c * dx - _s * dy)*1e-4;
            double yNew = point.y + (_s * dx + _c * dy)*1e-4;

            double xNew1 = xNew;
            double yNew1 = yNew;

            double x = xNew;
            double y = yNew;
            bool drawContour = true;
            bool halo = pFont->GetHaloSize() != 0.0;

            if(halo || ((pFont->GetStyle() & FontStyleBold) && customDecoration))
            {
                while(drawContour)
                {
                    xNew = xNew1;
                    yNew = yNew1;
                    x = xNew;
                    y = yNew;

                    this->m_rasterizer.reset();
                    int len_ = len;
                    if(halo)
                        this->m_contour.width(2 * pFont->GetHaloSize() + 1);
                    else
                        this->m_contour.width(0.95);

                    for(const wchar_t* p = text; *p && len_; p++, len_--)
                    {
                        if(*p == L'\r')
                        {
                            x = xNew;
                            y = yNew;
                            continue;
                        }
                        if(*p == L'\n')
                        {
                            xNew += (_c * dx - _s * (dy + m_font_engine.height()))*1e-4;
                            yNew += (_s * dx + _c * (dy + m_font_engine.height()))*1e-4;
                            x += (_c * dx - _s * (dy + m_font_engine.height()))*1e-4;
                            y += (_s * dx + _c * (dy + m_font_engine.height()))*1e-4;
                            continue;
                        }

                        const agg::glyph_cache* glyph = m_font_manager.glyph(*p);
                        if(glyph)
                        {
                            m_font_manager.add_kerning(&x, &y);
                            m_font_manager.init_embedded_adaptors(glyph, x, y);

                            if(glyph->data_type == agg::glyph_data_outline)
                                this->m_rasterizer.add_path(m_contour);

                            x += (halo || (pFont->GetStyle() & FontStyleBold)) ? glyph->advance_x : glyph->advance_x * 1.05; 			// increment pen position
                            y += glyph->advance_y;
                        }
                    }
                    if(halo)
                        this->m_renderer.color(agg::rgba8(pFont->GetBgColor().GetR(), pFont->GetBgColor().GetG(), pFont->GetBgColor().GetB(), pFont->GetBgColor().GetA()));
                    else
                        this->m_renderer.color(agg::rgba8(pFont->GetColor().GetR(), pFont->GetColor().GetG(), pFont->GetColor().GetB(), pFont->GetColor().GetA()));

                    agg::render_scanlines(this->m_rasterizer, this->m_scanline, this->m_renderer);

                    if(!halo || !((pFont->GetStyle() & FontStyleBold) && customDecoration))
                        drawContour = false;
                    else
                        halo = false;
                }
            }

            if(!((pFont->GetStyle() & FontStyleBold) && customDecoration))
            {

                this->m_rasterizer.reset();
                this->m_renderer.color(agg::rgba8(pFont->GetColor().GetR(), pFont->GetColor().GetG(), pFont->GetColor().GetB(), pFont->GetColor().GetA()));

                x = xNew;
                y = yNew;

                for(const wchar_t* p = text; *p && len; p++, len--)
                {
                    if(*p == L'\r')
                    {
                        DrawUnderLine(pFont, xNew, yNew, x, y);
                        x = xNew;
                        y = yNew;
                        continue;
                    }
                    if(*p == L'\n')
                    {
                        DrawUnderLine(pFont, xNew, yNew, x, y);
                        xNew += (_c * dx - _s * (dy + m_font_engine.height()))*1e-4;
                        yNew += (_s * dx + _c * (dy + m_font_engine.height()))*1e-4;
                        x += (_c * dx - _s * (dy + m_font_engine.height()))*1e-4;
                        y += (_s * dx + _c * (dy + m_font_engine.height()))*1e-4;
                        continue;
                    }

                    const agg::glyph_cache* glyph = m_font_manager.glyph(*p);
                    if(glyph)
                    {
                        m_font_manager.add_kerning(&x, &y);
                        m_font_manager.init_embedded_adaptors(glyph, x, y);

                        if(glyph->data_type == agg::glyph_data_outline)
                            this->m_rasterizer.add_path(m_curves);

                        x += glyph->advance_x; // increment pen position
                        y += glyph->advance_y;
                    }
                }
                DrawUnderLine(pFont, xNew, yNew, x, y);
                agg::render_scanlines(this->m_rasterizer, this->m_scanline, this->m_renderer);
            }
        }
        void CGraphicsAgg::DrawTextByLine(const FontPtr pFont, const wchar_t* text, int len, const GPoint* pPoints, int nNumPoints)
        {
            bool customDecorations;
            create_font(pFont, customDecorations);

            this->m_vertex_src.Attach(pPoints, &nNumPoints, 1);

            m_bspline.interpolation_step(1.0 / 100);

            m_tcurve.reset();
            m_tcurve.add_path(m_bspline);
            m_tcurve.preserve_x_scale(true);

            m_fsegm.approximation_scale(3.0);
            m_curves.approximation_scale(2.0);

            if(pFont->GetHaloSize() > 0)
            {
                double x = 0.0;
                double y = 0.0;
                const wchar_t* p = text;
                int l = len;
                m_fcontour.width(pFont->GetHaloSize() * 2 + 1);
                this->m_renderer.color(agg::rgba8(pFont->GetBgColor().GetR(), pFont->GetBgColor().GetG(), pFont->GetBgColor().GetB(), pFont->GetBgColor().GetA()));

                while(l--)
                {
                    const agg::glyph_cache* glyph = m_font_manager.glyph(*p);
                    if(glyph)
                    {
                        if(x > m_tcurve.total_length()) break;

                        m_font_manager.add_kerning(&x, &y);
                        m_font_manager.init_embedded_adaptors(glyph, x, y);

                        if(glyph->data_type == agg::glyph_data_outline)
                        {
                            //rasterizer_.reset();
                            this->m_rasterizer.add_path(m_fcontour);
                            //agg::render_scanlines(rasterizer_, scanline_, renderer_);
                        }

                        // increment pen position
                        x += glyph->advance_x;
                        y += glyph->advance_y;
                    }
                    ++p;
                }
                agg::render_scanlines(this->m_rasterizer, this->m_scanline, this->m_renderer);
            }

            {
                double x = 0.0;
                double y = 0.0;
                const wchar_t* p = text;
                int l = len;
                this->m_renderer.color(agg::rgba8(pFont->GetColor().GetR(), pFont->GetColor().GetG(), pFont->GetColor().GetB(), pFont->GetColor().GetA()));

                while(l--)
                {
                    const agg::glyph_cache* glyph = m_font_manager.glyph(*p);
                    if(glyph)
                    {
                        if(x > m_tcurve.total_length()) break;

                        m_font_manager.add_kerning(&x, &y);
                        m_font_manager.init_embedded_adaptors(glyph, x, y);

                        if(glyph->data_type == agg::glyph_data_outline)
                            this->m_rasterizer.add_path(m_ftrans);

                        // increment pen position
                        x += glyph->advance_x;
                        y += glyph->advance_y;
                    }
                    ++p;
                }
                //rasterizer_.add_path(conv_stroke_);
                agg::render_scanlines(this->m_rasterizer, this->m_scanline, this->m_renderer);

            }
        }

        void CGraphicsAgg::DrawBitmap(const BitmapPtr bitmap, const GRect& dstRct, bool flip_y, unsigned char alpha)
        {
            if(bitmap->Bpp() != 32)
                return;

            if(alpha == 0)
                return;

            GRect dstRect = dstRct;
            dstRect.xMin += m_org.x;
            dstRect.xMax += m_org.x;
            dstRect.yMin += m_org.y;
            dstRect.yMax += m_org.y;

            agg::rendering_buffer rbuf;
            agg::pixfmt_bgra32 pixf(rbuf);
            rbuf.attach((agg::int8u*)bitmap->Bits(), (int)bitmap->Width(), (int)bitmap->Height(), (flip_y ? 1 : -1) * int(bitmap->Width() * 4));

            if(dstRect.Width() == bitmap->Width() && dstRect.Height() == bitmap->Height())
            {
                agg::rect_i r(0, 0, (int)bitmap->Width(), (int)bitmap->Height());
                this->m_renderer_base.blend_from(pixf, &r, (int)dstRect.xMin, (int)dstRect.yMin, alpha);
            }
            else
            {
                agg::trans_affine mtx2;
                mtx2 *= agg::trans_affine_translation(-(dstRect.xMin + dstRect.Width() /  2.0), -(dstRect.yMin + dstRect.Height() /  2.0));
                mtx2 *= agg::trans_affine_translation(dstRect.Width() /  2.0, dstRect.Height() /  2.0);
                if(dstRect.Width() != bitmap->Width() && dstRect.Height() != bitmap->Height())
                    mtx2 *= agg::trans_affine_scaling(bitmap->Width() / dstRect.Width(), bitmap->Height() / dstRect.Height());

                typedef agg::span_interpolator_linear<> interpolator_type;
                interpolator_type interpolator(mtx2);

                typedef agg::span_image_filter_rgba_bilinear_clip<agg::pixfmt_bgra32, interpolator_type> span_gen_type;
                span_gen_type sg(pixf, agg::rgba(0, 0, 0, 0), interpolator, alpha);

                this->m_rasterizer.move_to_d(dstRect.xMin, dstRect.yMin);
                this->m_rasterizer.line_to_d(dstRect.xMax, dstRect.yMin);
                this->m_rasterizer.line_to_d(dstRect.xMax, dstRect.yMax);
                this->m_rasterizer.line_to_d(dstRect.xMin, dstRect.yMax);
                this->m_rasterizer.line_to_d(dstRect.xMin, dstRect.yMin);

                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline, this->m_renderer_base, this->m_span_allocator, sg);
            }
        }
        void CGraphicsAgg::DrawRotatedBitmap(const BitmapPtr pBitmap, const GPoint& center, double angle, bool flip_y, unsigned char alpha, double scale_x , double scale_y, bool clip, bool onGrid)
        {
            if(pBitmap->Bpp() != 32)
                return;

            if(alpha == 0)
                return;

            GPoint c = center;
            c += m_org;

            agg::trans_affine mtx;
            mtx *= agg::trans_affine_translation(-c.x, -c.y);
            mtx *= agg::trans_affine_rotation(-DEG2RAD(angle));
            mtx *= agg::trans_affine_scaling(scale_x, scale_y);
            mtx *= agg::trans_affine_translation(c.x, c.y);

            agg::trans_affine mtx2;
            mtx2 *= agg::trans_affine_translation(-c.x, -c.y);
            mtx2 *= agg::trans_affine_rotation(DEG2RAD(angle));
            mtx2 *= agg::trans_affine_scaling(1 / scale_x, 1 / scale_y);
            mtx2 *= agg::trans_affine_translation((double)pBitmap->Width() / 2, (double)pBitmap->Height() / 2);

            typedef agg::span_interpolator_linear<> interpolator_type;
            interpolator_type interpolator(mtx2);

            double x[5], y[5];
            x[0] = c.x - pBitmap->Width() / 2;
            y[0] = c.y + pBitmap->Height() / 2;
            x[1] = c.x + pBitmap->Width() / 2;
            y[1] = c.y + pBitmap->Height() / 2;
            x[2] = c.x + pBitmap->Width() / 2;
            y[2] = c.y - pBitmap->Height() / 2;
            x[3] = c.x - pBitmap->Width() / 2;
            y[3] = c.y - pBitmap->Height() / 2;
            x[4] = c.x - pBitmap->Width() / 2;
            y[4] = c.y + pBitmap->Height() / 2;

            for(int i = 0; i < 5; i++)
            {
                mtx.transform(&x[i], &y[i]);
                if(onGrid)
                {
                    x[i] = int(x[i] + 0.5);
                    y[i] = int(y[i] + 0.5);
                }
            }
            this->m_rasterizer.move_to_d(x[0], y[0]);
            for(int i = 1; i < 5; i++)
                this->m_rasterizer.line_to_d(x[i], y[i]);

            agg::rendering_buffer rbuf;
            agg::pixfmt_bgra32 pixf(rbuf);
            rbuf.attach((agg::int8u*)pBitmap->Bits(), (int)pBitmap->Width(), (int)pBitmap->Height(), (flip_y ? 1 : -1) * int(pBitmap->Width() * 4));

            if(clip)
            {
                typedef agg::span_image_filter_rgba_bilinear_clip<agg::pixfmt_bgra32, interpolator_type> span_gen_type;
                span_gen_type sg(pixf, agg::rgba(0, 0, 0, 0), interpolator, alpha);

                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline, this->m_renderer_base, this->m_span_allocator, sg);
            }
            else
            {
                typedef agg::image_accessor_clone<agg::pixfmt_bgra32> img_source_type;
                img_source_type source(pixf);

                typedef agg::span_image_filter_rgba_bilinear<img_source_type, interpolator_type> span_gen_type;
                span_gen_type sg(source, interpolator, alpha);

                agg::render_scanlines_aa(this->m_rasterizer, this->m_scanline, this->m_renderer_base, this->m_span_allocator, sg);
            }



        }

        Color CGraphicsAgg::GetPixel(GUnits x, GUnits y)
        {
            agg::rgba8 pixel = this->m_rendering_buffer.pixel((int)x, (int)y);
            return Color(pixel.r, pixel.g, pixel.b, pixel.a);
        }

        GPoint CGraphicsAgg::GetViewportOrg() const
        {
            return m_org;
        }
        void   CGraphicsAgg::SetViewportOrg(const GPoint& org)
        {
            m_org = org;
        }

        GPoint CGraphicsAgg::GetBrushOrg() const
        {
            return m_brushOrg;
        }
        void   CGraphicsAgg::SetBrushOrg(const GPoint& org)
        {
            m_brushOrg = org;
        }

        const BitmapPtr CGraphicsAgg::GetSurface() const
        {
            return m_ptrSurface;
        }

        BitmapPtr CGraphicsAgg::GetSurface()
        {
            return m_ptrSurface;
        }

        size_t CGraphicsAgg::GetWidth() const
        {
            return (size_t)m_nWidth;
        }
        size_t CGraphicsAgg::GetHeight() const
        {
            return (size_t)m_nHeight;
        }

        void CGraphicsAgg::Lock()
        {
            m_mutex.lock();
        }
        void CGraphicsAgg::UnLock()
        {
            m_mutex.unlock();
        }

        IGraphicsPtr  IGraphics::CreateCGraphicsAgg(unsigned char* buf, GUnits  width, GUnits height, bool bRelease, bool flipY) {
            return std::make_shared<CGraphicsAgg>(buf, width,height, flipY);
        }

        IGraphicsPtr  IGraphics::CreateCGraphicsAgg( GUnits  width, GUnits height, bool flipY) {
            return std::make_shared<CGraphicsAgg>(width, height, flipY);

        }

    }
    }