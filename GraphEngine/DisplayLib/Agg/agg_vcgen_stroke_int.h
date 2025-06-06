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

#ifndef AGG_VCGEN_STROKE_INTEGER_INCLUDED
#define AGG_VCGEN_STROKE_INTEGER_INCLUDED

#include "agg_math_stroke_int.h"
#include "agg_vertex_sequence_int.h"
#include "agg_math_stroke_int.h"

namespace agg
{

  //============================================================vcgen_stroke
  //
  // See Implementation agg_vcgen_stroke.cpp
  // Stroke generator
  //
  //------------------------------------------------------------------------
  class vcgen_stroke_int
  {
    enum status_e
    {
      initial,
      ready,
      cap1,
      cap2,
      outline1,
      close_first,
      outline2,
      out_vertices,
      end_poly1,
      end_poly2,
      stop
    };

  public:
    typedef vertex_sequence_int<vertex_dist_int, 6> vertex_storage;
    typedef pod_bvector<point_i, 6>         coord_storage;

    vcgen_stroke_int();

    void line_cap(line_cap_e lc)     { m_stroker.line_cap(lc); }
    void line_join(line_join_e lj)   { m_stroker.line_join(lj); }
    void inner_join(inner_join_e ij) { m_stroker.inner_join(ij); }

    line_cap_e   line_cap()   const { return m_stroker.line_cap(); }
    line_join_e  line_join()  const { return m_stroker.line_join(); }
    inner_join_e inner_join() const { return m_stroker.inner_join(); }

    void width(int w) { m_stroker.width(w); }
    void miter_limit(int ml) { m_stroker.miter_limit(ml); }
    void miter_limit_theta(int t) { m_stroker.miter_limit_theta(t); }
    void inner_miter_limit(int ml) { m_stroker.inner_miter_limit(ml); }
    void approximation_scale(int as) { m_stroker.approximation_scale(as); }

    double width() const { return m_stroker.width(); }
    double miter_limit() const { return m_stroker.miter_limit(); }
    double inner_miter_limit() const { return m_stroker.inner_miter_limit(); }
    double approximation_scale() const { return m_stroker.approximation_scale(); }

    void shorten(int s) { m_shorten = s; }
    double shorten() const { return m_shorten; }

    // Vertex Generator Interface
    void remove_all();
    void add_vertex(int x, int y, unsigned cmd);

    // Vertex Source Interface
    void     rewind(unsigned path_id);
    unsigned vertex(int* x, int* y);

  private:
    vcgen_stroke_int(const vcgen_stroke_int&);
    const vcgen_stroke_int& operator = (const vcgen_stroke_int &);

    math_stroke_int<coord_storage> m_stroker;
    vertex_storage             m_src_vertices;
    coord_storage              m_out_vertices;
    double                     m_shorten;
    unsigned                   m_closed;
    status_e                   m_status;
    status_e                   m_prev_status;
    unsigned                   m_src_vertex;
    unsigned                   m_out_vertex;
  };


}

#endif
