#pragma once
#include "../Cartography.h"
#include "Geom/LabelGeometry.h"
#include "LabelStyle.h"

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            enum eLabelGeometryType
            {
                LabelGeometryPoint,
                LabelGeometryLine,
                LabelGeometryPolygon
            };

            // part of the label text drawn by one call: the whole text of a straight label, a character of a curved one
            struct SGlyphRun
            {
                int       start;   // first character
                int       count;
                double    x;       // start of the baseline
                double    y;
                double    angle;   // degrees, clockwise on the screen
                SLabelBox box;     // occupied area (without the halo)
            };

            struct SLabelPlacement
            {
                std::vector<SGlyphRun> runs;

                Display::GRect Bounds() const
                {
                    Display::GRect bounds;
                    for(size_t i = 0; i < runs.size(); ++i)
                    {
                        Display::GRect rc = runs[i].box.Bounds();
                        if(i == 0)
                            bounds = rc;
                        else
                            bounds.ExpandRect(rc);
                    }
                    return bounds;
                }
            };

            // label collected by AddLabel, the geometry is already in device coordinates (clipped by the view)
            struct SLabelItem
            {
                std::wstring              text;
                CLabelStylePtr            style;
                SLabelingOptions          options;
                int                       classIndex;
                double                    weight;   // length of a line, area of a polygon: bigger features are labeled first
                double                    width;    // text width
                eLabelGeometryType        type;
                std::vector<TLabelPoints> parts;    // point: one part with the points, line: the visible parts,
                                                    // polygon: the outer ring and its holes

                SLabelItem() : classIndex(0), weight(0.), width(0.), type(LabelGeometryPoint) {}
            };

            // device values of the placement settings
            struct SLabelStrategyParams
            {
                double step;           // distance between the positions tried along a line / across a polygon
                double offset;         // gap between the feature and the label
                double maxCharAngle;   // curved labels

                SLabelStrategyParams() : step(10.), offset(3.), maxCharAngle(30.) {}
            };
        }
    }
}
