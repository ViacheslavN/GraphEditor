#include "LabelCollisionGrid.h"

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            CLabelCollisionGrid::CLabelCollisionGrid() : m_xMin(0.), m_yMin(0.), m_cellSize(32.), m_nx(0), m_ny(0), m_nQuery(0)
            {

            }

            void CLabelCollisionGrid::Setup(const Display::GRect& area, double cellSize)
            {
                Clear();
                m_cellSize = cellSize > 1. ? cellSize : 1.;
                m_xMin = area.xMin;
                m_yMin = area.yMin;
                m_nx = (std::max)(1, (int)ceil((area.xMax - area.xMin) / m_cellSize));
                m_ny = (std::max)(1, (int)ceil((area.yMax - area.yMin) / m_cellSize));
                m_cells.assign((size_t)m_nx * (size_t)m_ny, std::vector<uint32_t>());
            }

            void CLabelCollisionGrid::Clear()
            {
                m_cells.clear();
                m_boxes.clear();
                m_bounds.clear();
                m_stamps.clear();
                m_nQuery = 0;
                m_nx = m_ny = 0;
            }

            void CLabelCollisionGrid::CellRange(const Display::GRect& rect, int& x0, int& y0, int& x1, int& y1) const
            {
                // boxes outside the area are kept in the border cells, the test is exact anyway
                x0 = (int)floor((rect.xMin - m_xMin) / m_cellSize);
                y0 = (int)floor((rect.yMin - m_yMin) / m_cellSize);
                x1 = (int)floor((rect.xMax - m_xMin) / m_cellSize);
                y1 = (int)floor((rect.yMax - m_yMin) / m_cellSize);
                x0 = (std::max)(0, (std::min)(m_nx - 1, x0));
                x1 = (std::max)(0, (std::min)(m_nx - 1, x1));
                y0 = (std::max)(0, (std::min)(m_ny - 1, y0));
                y1 = (std::max)(0, (std::min)(m_ny - 1, y1));
            }

            bool CLabelCollisionGrid::Intersects(const SLabelBox& box) const
            {
                if(m_boxes.empty() || m_cells.empty())
                    return false;

                if(++m_nQuery == 0)
                {
                    std::fill(m_stamps.begin(), m_stamps.end(), 0);
                    m_nQuery = 1;
                }

                Display::GRect bounds = box.Bounds();
                int x0, y0, x1, y1;
                CellRange(bounds, x0, y0, x1, y1);
                for(int y = y0; y <= y1; ++y)
                {
                    for(int x = x0; x <= x1; ++x)
                    {
                        const std::vector<uint32_t>& cell = m_cells[(size_t)y * m_nx + x];
                        for(size_t i = 0; i < cell.size(); ++i)
                        {
                            uint32_t idx = cell[i];
                            if(m_stamps[idx] == m_nQuery)
                                continue;
                            m_stamps[idx] = m_nQuery;

                            const Display::GRect& other = m_bounds[idx];
                            if(other.xMin >= bounds.xMax || other.xMax <= bounds.xMin || other.yMin >= bounds.yMax || other.yMax <= bounds.yMin)
                                continue;
                            if(m_boxes[idx].Intersects(box))
                                return true;
                        }
                    }
                }
                return false;
            }

            void CLabelCollisionGrid::Insert(const SLabelBox& box)
            {
                if(m_cells.empty())
                    return;

                uint32_t idx = (uint32_t)m_boxes.size();
                Display::GRect bounds = box.Bounds();
                m_boxes.push_back(box);
                m_bounds.push_back(bounds);
                m_stamps.push_back(0);

                int x0, y0, x1, y1;
                CellRange(bounds, x0, y0, x1, y1);
                for(int y = y0; y <= y1; ++y)
                {
                    for(int x = x0; x <= x1; ++x)
                        m_cells[(size_t)y * m_nx + x].push_back(idx);
                }
            }
        }
    }
}
