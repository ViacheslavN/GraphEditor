//----------------------------------------------------------------------------
// Anti-Grain Geometry (AGG) - Version 2.5
// A high quality rendering engine for C++
// Copyright (C) 2002-2006 Maxim Shemanarev
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://antigrain.com
// 
// AGG is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// AGG is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with AGG; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
// MA 02110-1301, USA.
//----------------------------------------------------------------------------

#ifndef AGG_PIXFMT_AMASK_ADAPTOR_INCLUDED
#define AGG_PIXFMT_AMASK_ADAPTOR_INCLUDED


#include <string.h>
#include "agg_array.h"
#include "agg_rendering_buffer.h"


namespace agg
{
    //==================================================pixfmt_amask_adaptor
    template<class PixFmt, class AlphaMask> class pixfmt_amask_adaptor
    {
    public:
        typedef PixFmt pixfmt_type;
        typedef typename pixfmt_type::color_type color_type;
        typedef typename pixfmt_type::row_data row_data;
        typedef AlphaMask amask_type;
        typedef typename amask_type::cover_type cover_type;

    private:
        enum span_extra_tail_e { span_extra_tail = 256 };

        void realloc_span(unsigned len)
        {
            if(len > m_span.size())
            {
                m_span.resize(len + span_extra_tail);
            }
        }

        void init_span(unsigned len)
        {
            realloc_span(len);
            memset(&m_span[0], amask_type::cover_full, len * sizeof(cover_type));
        }

        void init_span(unsigned len, const cover_type* covers)
        {
            realloc_span(len);
            memcpy(&m_span[0], covers, len * sizeof(cover_type));
        }


    public:
        pixfmt_amask_adaptor(pixfmt_type& pixf, const amask_type& mask) :
            m_pixf(&pixf), m_mask(&mask), m_span()
        {}

        void attach_pixfmt(pixfmt_type& pixf)          { m_pixf = &pixf; }
        void attach_alpha_mask(const amask_type& mask) { m_mask = &mask; }

        //--------------------------------------------------------------------
        template<class PixFmt2>
        bool attach_pixfmt(PixFmt2& pixf, int x1, int y1, int x2, int y2)
        {
            return m_pixf->attach(pixf, x1, y1, x2, y2);
        }

        //--------------------------------------------------------------------
        unsigned width()  const { return m_pixf->width();  }
        unsigned height() const { return m_pixf->height(); }

        //--------------------------------------------------------------------
        color_type pixel(int x, int y)
        {
            return m_pixf->pixel(x, y);
        }

        //--------------------------------------------------------------------
        void copy_pixel(int x, int y, const color_type& c)
        {
            m_pixf->blend_pixel(x, y, c, m_mask->pixel(x, y));
        }

        //--------------------------------------------------------------------
        void blend_pixel(int x, int y, const color_type& c, cover_type cover)
        {
            m_pixf->blend_pixel(x, y, c, m_mask->combine_pixel(x, y, cover));
        }

        //--------------------------------------------------------------------
        void copy_hline(int x, int y, 
                        unsigned len, 
                        const color_type& c)
        {
            realloc_span(len);
            m_mask->fill_hspan(x, y, &m_span[0], len);
            m_pixf->blend_solid_hspan(x, y, len, c, &m_span[0]);
        }

        //--------------------------------------------------------------------
        void blend_hline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         cover_type cover)
        {
            init_span(len);
            m_mask->combine_hspan(x, y, &m_span[0], len);
            m_pixf->blend_solid_hspan(x, y, len, c, &m_span[0]);
        }

        //--------------------------------------------------------------------
        void copy_vline(int x, int y,
                        unsigned len, 
                        const color_type& c)
        {
            realloc_span(len);
            m_mask->fill_vspan(x, y, &m_span[0], len);
            m_pixf->blend_solid_vspan(x, y, len, c, &m_span[0]);
        }

        //--------------------------------------------------------------------
        void blend_vline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         cover_type cover)
        {
            init_span(len);
            m_mask->combine_vspan(x, y, &m_span[0], len);
            m_pixf->blend_solid_vspan(x, y, len, c, &m_span[0]);
        }

        //--------------------------------------------------------------------
        void copy_from(const rendering_buffer& from, 
                       int xdst, int ydst,
                       int xsrc, int ysrc,
                       unsigned len)
        {
            m_pixf->copy_from(from, xdst, ydst, xsrc, ysrc, len);
        }


        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const cover_type* covers)
        {
            init_span(len, covers);
            m_mask->combine_hspan(x, y, &m_span[0], len);
            m_pixf->blend_solid_hspan(x, y, len, c, &m_span[0]);
        }


        //--------------------------------------------------------------------
        void blend_solid_vspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const cover_type* covers)
        {
            init_span(len, covers);
            m_mask->combine_vspan(x, y, &m_span[0], len);
            m_pixf->blend_solid_vspan(x, y, len, c, &m_span[0]);
        }


        //--------------------------------------------------------------------
        void copy_color_hspan(int x, int y, unsigned len, const color_type* colors)
        {
            realloc_span(len);
            m_mask->fill_hspan(x, y, &m_span[0], len);
            m_pixf->blend_color_hspan(x, y, len, colors, &m_span[0], cover_full);
        }

        //--------------------------------------------------------------------
        void copy_color_vspan(int x, int y, unsigned len, const color_type* colors)
        {
            realloc_span(len);
            m_mask->fill_vspan(x, y, &m_span[0], len);
            m_pixf->blend_color_vspan(x, y, len, colors, &m_span[0], cover_full);
        }

        //--------------------------------------------------------------------
        void blend_color_hspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const cover_type* covers,
                               cover_type cover = cover_full)
        {
            if(covers) 
            {
                init_span(len, covers);
                m_mask->combine_hspan(x, y, &m_span[0], len);
            }
            else
            {
                realloc_span(len);
                m_mask->fill_hspan(x, y, &m_span[0], len);
            }
            m_pixf->blend_color_hspan(x, y, len, colors, &m_span[0], cover);
        }


        //--------------------------------------------------------------------
        void blend_color_vspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const cover_type* covers,
                               cover_type cover = cover_full)
        {
            if(covers) 
            {
                init_span(len, covers);
                m_mask->combine_vspan(x, y, &m_span[0], len);
            }
            else
            {
                realloc_span(len);
                m_mask->fill_vspan(x, y, &m_span[0], len);
            }
            m_pixf->blend_color_vspan(x, y, len, colors, &m_span[0], cover);
        }

    private:
        pixfmt_type*          m_pixf;
        const amask_type*     m_mask;
        pod_array<cover_type> m_span;
    };


    template<class PixFmt, class AlphaPlane> class pixfmt_aplane_adaptor
    {
    public:
        typedef PixFmt pixfmt_type;
        typedef typename pixfmt_type::color_type color_type;
        typedef typename pixfmt_type::row_data row_data;
        typedef AlphaPlane aplane_type;
        typedef typename aplane_type::cover_type cover_type;

    private:
        enum span_extra_tail_e { span_extra_tail = 256 };

        void realloc_span(unsigned len)
        {
            if(len > m_span.size())
            {
                m_span.resize(len + span_extra_tail);
            }
        }

        void init_span(unsigned len)
        {
            realloc_span(len);
            memset(&m_span[0], aplane_type::cover_full, len * sizeof(cover_type));
        }

        void init_span(unsigned len, const cover_type* covers)
        {
            realloc_span(len);
            memcpy(&m_span[0], covers, len * sizeof(cover_type));
        }

      
    public:
        pixfmt_aplane_adaptor(pixfmt_type& pixf, aplane_type& mask) :
            m_pixf(&pixf), m_mask(&mask), m_span()
        {}

        void attach_pixfmt(pixfmt_type& pixf)          { m_pixf = &pixf; }
        void attach_alpha_plane(aplane_type& mask) { m_mask = &mask; }

        //--------------------------------------------------------------------
        template<class PixFmt2>
        bool attach_pixfmt(PixFmt2& pixf, int x1, int y1, int x2, int y2)
        {
            return m_pixf->attach(pixf, x1, y1, x2, y2);
        }
        void SetPolygonClipper(polygon_clipper *polyClip)
        {
          //polyClip_ = polyClip;
          m_pixf->SetPolygonClipper(polyClip);
          m_mask->SetPolygonClipper(polyClip);
        }
        //--------------------------------------------------------------------
        unsigned width()  const { return m_pixf->width();  }
        unsigned height() const { return m_pixf->height(); }

        //--------------------------------------------------------------------
        color_type pixel(int x, int y) const
        {
            //return m_pixf->pixel(x, y);
          color_type pix = m_pixf->pixel(x, y);
          return color_type(pix.r, pix.g, pix.b, m_mask->pixel(x, y));
        }

        //--------------------------------------------------------------------
        void copy_pixel(int x, int y, const color_type& c)
        {
            //m_pixf->blend_pixel(x, y, c, m_mask->pixel(x, y));
          m_pixf->copy_pixel(x, y, c);
          m_mask->copy_pixel(x, y, c.a);
        }

        //--------------------------------------------------------------------
        void blend_pixel(int x, int y, const color_type& c, cover_type cover)
        {
          cover_type alpha;
          agg::blender_gray8::blend_pix(&alpha, c.a, cover);
          m_pixf->blend_pixel(x, y, c, cover);
          m_mask->blend_pixel(x, y, alpha);
        }

        //--------------------------------------------------------------------
        void copy_hline(int x, int y, 
                        unsigned len, 
                        const color_type& c)
        {
            //realloc_span(len);
            //m_mask->fill_hspan(x, y, &m_span[0], len);
            //m_pixf->blend_solid_hspan(x, y, len, c, &m_span[0]);
          m_pixf->copy_hline(x, y, len, c);
          m_mask->copy_hline(x, y, len, c.a);
        }

        //--------------------------------------------------------------------
        void blend_hline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         cover_type cover)
        {
            //init_span(len);
            //m_mask->combine_hspan(x, y, &m_span[0], len);
            //m_pixf->blend_solid_hspan(x, y, len, c, &m_span[0]);
          do
          {
            blend_pixel(x++, y, c, cover);
          }
          while(--len);
        }

        //--------------------------------------------------------------------
        void copy_vline(int x, int y,
                        unsigned len, 
                        const color_type& c)
        {
            //realloc_span(len);
            //m_mask->fill_vspan(x, y, &m_span[0], len);
            //m_pixf->blend_solid_vspan(x, y, len, c, &m_span[0]);
          m_pixf->copy_vline(x, y, len, c);
          m_mask->copy_vline(x, y, len, c.a);
        }

        //--------------------------------------------------------------------
        void blend_vline(int x, int y,
                         unsigned len, 
                         const color_type& c,
                         cover_type cover)
        {
            //init_span(len);
            //m_mask->combine_vspan(x, y, &m_span[0], len);
            //m_pixf->blend_solid_vspan(x, y, len, c, &m_span[0]);
          do
          {
            blend_pixel(x, y++, c, cover);
          }
          while(--len);
        }

        //--------------------------------------------------------------------
        //void copy_from(const rendering_buffer& from, 
        //               int xdst, int ydst,
        //               int xsrc, int ysrc,
        //               unsigned len)
        //{
        //    m_pixf->copy_from(from, xdst, ydst, xsrc, ysrc, len);
        //}


        //--------------------------------------------------------------------
        void blend_solid_hspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const cover_type* covers)
        {
            //init_span(len, covers);
            //m_mask->combine_hspan(x, y, &m_span[0], len);
            //m_pixf->blend_solid_hspan(x, y, len, c, &m_span[0]);
          if(covers)
          {
            do
            {
              blend_pixel(x++, y, c, *covers);
              covers++;
            }
            while(--len);
          }
          else
          {
            blend_hline(x, y, len, c, 255);
          }
        }


        //--------------------------------------------------------------------
        void blend_solid_vspan(int x, int y,
                               unsigned len, 
                               const color_type& c,
                               const cover_type* covers)
        {
            //init_span(len, covers);
            //m_mask->combine_vspan(x, y, &m_span[0], len);
            //m_pixf->blend_solid_vspan(x, y, len, c, &m_span[0]);
          if(covers)
          {
            do
            {
              blend_pixel(x, y++, c, *covers);
              covers++;
            }
            while(--len);
          }
          else
          {
            blend_hline(x, y, len, c);
          }
        }


        //--------------------------------------------------------------------
        void copy_color_hspan(int x, int y, unsigned len, const color_type* colors)
        {
            //realloc_span(len);
            //m_mask->fill_hspan(x, y, &m_span[0], len);
            //m_pixf->blend_color_hspan(x, y, len, colors, &m_span[0], cover_full);
          m_pixf->copy_color_hspan(x, y, len, colors);
          do
          {
            m_mask->copy_pixel(x++, y, (*colors).a);
            colors++;
          }
          while(--len);
        }

        //--------------------------------------------------------------------
        void copy_color_vspan(int x, int y, unsigned len, const color_type* colors)
        {
            //realloc_span(len);
            //m_mask->fill_vspan(x, y, &m_span[0], len);
            //m_pixf->blend_color_vspan(x, y, len, colors, &m_span[0], cover_full);
          m_pixf->copy_color_vspan(x, y, len, colors);
          do
          {
            m_mask->copy_pixel(x, y++, (*colors).a);
            colors++;
          }
          while(--len);
        }

        //--------------------------------------------------------------------
        void blend_color_hspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const cover_type* covers,
                               cover_type cover = cover_full)
        {
            //if(covers) 
            //{
            //    init_span(len, covers);
            //    m_mask->combine_hspan(x, y, &m_span[0], len);
            //}
            //else
            //{
            //    realloc_span(len);
            //    m_mask->fill_hspan(x, y, &m_span[0], len);
            //}
            //m_pixf->blend_color_hspan(x, y, len, colors, &m_span[0], cover);
          if(covers)
          {
            do
            {
              cover_type alpha;
              agg::blender_gray8::blend_pix(&alpha, *covers, cover);
              blend_pixel(x, y, *colors, alpha);
              x++;
              colors++;
              covers++;
            }
            while(--len);
          }
          else
          {
            do
            {
              blend_pixel(x, y, *colors, cover);
              x++;
              colors++;
            }
            while(--len);
          }
        }


        //--------------------------------------------------------------------
        void blend_color_vspan(int x, int y,
                               unsigned len, 
                               const color_type* colors,
                               const cover_type* covers,
                               cover_type cover = cover_full)
        {
            //if(covers) 
            //{
            //    init_span(len, covers);
            //    m_mask->combine_vspan(x, y, &m_span[0], len);
            //}
            //else
            //{
            //    realloc_span(len);
            //    m_mask->fill_vspan(x, y, &m_span[0], len);
            //}
            //m_pixf->blend_color_vspan(x, y, len, colors, &m_span[0], cover);
          if(covers)
          {
            do
            {
#if !defined(__IPHONE_3_1) && !defined(ANDROID)//MD
			  aplane_type::cover_type cover2 =
#else
			  unsigned char cover2 = 
#endif
			  m_mask->combine_pixel(x, y, *covers);
              blend_pixel(x, y, *colors, cover2);
              y++;
              colors++;
              covers++;
            }
            while(--len);
          }
          else
          {
            do
            {
              blend_pixel(x, y, *colors, cover);
              y++;
              colors++;
            }
            while(--len);
          }
        }

        template<class SrcPixelFormatRenderer>
        void blend_from(const SrcPixelFormatRenderer& from, 
                        int xdst, int ydst,
                        int xsrc, int ysrc,
                        unsigned len,
                        int8u cover)
        {
          do
          {
            color_type c = from.pixel(xsrc, ysrc);
            blend_pixel(xdst, ydst, c, cover);
            xdst++;
            xsrc++;
          }
          while(--len);
        }

        row_data row(int y) const
        {
          return m_pixf->row(y);
        }

    private:
        pixfmt_type*          m_pixf;
        aplane_type*          m_mask;
        pod_array<cover_type> m_span;

    };

}

#endif

