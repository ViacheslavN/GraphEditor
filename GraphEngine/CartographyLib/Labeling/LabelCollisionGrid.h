#pragma once
#include "Geom/LabelGeometry.h"
#include <cstdint>

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            // Occupied areas of the placed labels: uniform grid of cells with the boxes, exact test of the oriented boxes.
            // Replaces the bit matrix of the reference engine (LMatrix), which sampled rotated boxes with 5px cells:
            // the samples left gaps in rotated boxes and the cells made the boxes bigger.
            class CLabelCollisionGrid
            {
            public:
                CLabelCollisionGrid();

                void Setup(const Display::GRect& area, double cellSize);
                void Clear();

                bool Intersects(const SLabelBox& box) const;
                void Insert(const SLabelBox& box);
                size_t GetBoxCount() const { return m_boxes.size(); }

            private:
                void CellRange(const Display::GRect& rect, int& x0, int& y0, int& x1, int& y1) const;

            private:
                double m_xMin;
                double m_yMin;
                double m_cellSize;
                int    m_nx;
                int    m_ny;
                std::vector<std::vector<uint32_t> > m_cells;
                std::vector<SLabelBox>       m_boxes;
                std::vector<Display::GRect>  m_bounds;
                mutable std::vector<uint32_t> m_stamps;   // a box in several cells is tested once per query
                mutable uint32_t m_nQuery;
            };
        }
    }
}
