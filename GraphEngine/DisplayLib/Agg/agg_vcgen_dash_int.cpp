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

#include <math.h>
#include "agg_vcgen_dash_int.h"
#include "agg_shorten_path_int.h"

namespace agg
{

  //------------------------------------------------------------------------
 vcgen_dash_int::vcgen_dash_int() :
m_total_dash_len(0),
m_num_dashes(0),
m_dash_start(0),
m_shorten(0),
m_curr_dash_start(0),
m_curr_dash(0),
m_src_vertices(),
m_closed(0),
m_status(initial),
m_src_vertex(0)
{
}



//------------------------------------------------------------------------
void vcgen_dash_int::remove_all_dashes()
{
  m_total_dash_len = 0;
  m_num_dashes = 0;
  m_curr_dash_start = 0;
  m_curr_dash = 0;
}


//------------------------------------------------------------------------
void vcgen_dash_int::add_dash(int dash_len, int gap_len)
{
  if(m_num_dashes < max_dashes)
  {
    m_total_dash_len += dash_len + gap_len;
    m_dashes[m_num_dashes++] = dash_len;
    m_dashes[m_num_dashes++] = gap_len;
  }
}


//------------------------------------------------------------------------
void vcgen_dash_int::dash_start(int ds)
{
  m_dash_start = ds;
  calc_dash_start(abs(ds));
}


//------------------------------------------------------------------------
void vcgen_dash_int::calc_dash_start(int ds)
{
  m_curr_dash = 0;
  m_curr_dash_start = 0;
  while(ds > 0)
  {
    if(ds > m_dashes[m_curr_dash])
    {
      ds -= m_dashes[m_curr_dash];
      ++m_curr_dash;
      m_curr_dash_start = 0;
      if(m_curr_dash >= m_num_dashes) m_curr_dash = 0;
    }
    else
    {
      m_curr_dash_start = ds;
      ds = -1;
    }
  }
}


//------------------------------------------------------------------------
void vcgen_dash_int::remove_all()
{
  m_status = initial;
  m_src_vertices.remove_all();
  m_closed = 0;
}


//------------------------------------------------------------------------
void vcgen_dash_int::add_vertex(int x, int y, unsigned cmd)
{
  m_status = initial;
  if(is_move_to(cmd))
  {
    m_src_vertices.modify_last(vertex_dist_int(x, y));
  }
  else
  {
    if(is_vertex(cmd))
    {
      m_src_vertices.add(vertex_dist_int(x, y));
    }
    else
    {
      m_closed = get_close_flag(cmd);
    }
  }
}


//------------------------------------------------------------------------
void vcgen_dash_int::rewind(unsigned)
{
  if(m_status == initial)
  {
    m_src_vertices.close(m_closed != 0);
    shorten_path_int(m_src_vertices, m_shorten, m_closed);
  }
  m_status = ready;
  m_src_vertex = 0;
}


//------------------------------------------------------------------------
unsigned vcgen_dash_int::vertex(int* x, int* y)
{
  unsigned cmd = path_cmd_move_to;
  while(!is_stop(cmd))
  {
    switch(m_status)
    {
    case initial:
      rewind(0);

    case ready:
      if(m_num_dashes < 2 || m_src_vertices.size() < 2)
      {
        cmd = path_cmd_stop;
        break;
      }
      m_status = polyline;
      m_src_vertex = 1;
      m_v1 = &m_src_vertices[0];
      m_v2 = &m_src_vertices[1];
      m_curr_rest = m_v1->dist;
      *x = m_v1->x;
      *y = m_v1->y;
      if(m_dash_start >= 0) calc_dash_start(m_dash_start);
      return path_cmd_move_to;

    case polyline:
      {
        int dash_rest = m_dashes[m_curr_dash] - m_curr_dash_start;

        unsigned cmd = (m_curr_dash & 1) ? 
path_cmd_move_to : 
        path_cmd_line_to;

        if(m_curr_rest > dash_rest)
        {
          m_curr_rest -= dash_rest;
          ++m_curr_dash ;
          if(m_curr_dash >= m_num_dashes) m_curr_dash = 0;
          m_curr_dash_start = 0;
          *x = m_v2->x - (int64(m_v2->x - m_v1->x) * m_curr_rest) / m_v1->dist;
          *y = m_v2->y - (int64(m_v2->y - m_v1->y) * m_curr_rest) / m_v1->dist;
        }
        else
        {
          m_curr_dash_start += m_curr_rest;
          *x = m_v2->x;
          *y = m_v2->y;
          ++m_src_vertex;
          m_v1 = m_v2;
          m_curr_rest = m_v1->dist;
          if(m_closed)
          {
            if(m_src_vertex > m_src_vertices.size())
            {
              m_status = stop;
            }
            else
            {
              m_v2 = &m_src_vertices
                [
                  (m_src_vertex >= m_src_vertices.size()) ? 0 : 
                  m_src_vertex
                ];
            }
          }
          else
          {
            if(m_src_vertex >= m_src_vertices.size())
            {
              m_status = stop;
            }
            else
            {
              m_v2 = &m_src_vertices[m_src_vertex];
            }
          }
        }
        return cmd;
      }
      break;

    case stop:
      cmd = path_cmd_stop;
      break;
    }

  }
  return path_cmd_stop;
}


}

