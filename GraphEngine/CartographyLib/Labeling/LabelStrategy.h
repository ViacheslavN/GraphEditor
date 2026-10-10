#pragma once
#include "LabelTypes.h"
#include <functional>

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            // receives the label positions in the order of preference, returns true to stop
            typedef std::function<bool(const SLabelPlacement& placement)> TLabelCandidateFunc;

            // Positions of a label (ported from UniGIS / UGLite LLabelStrategy, reworked):
            //  - point: around the point by the priorities of the positions;
            //  - line: horizontal, parallel, curved (character by character) or perpendicular,
            //          from the middle of the longest visible part to its ends, then the other parts;
            //  - polygon: the pole of inaccessibility first, then horizontal bands from it to the borders,
            //             the text is inside the polygon (holes are taken into account);
            //             straight placement does the same along the main axis of the polygon.
            class CLabelStrategy
            {
            public:
                static void GenerateCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func);

                // straight text run with the box center at (cx, cy)
                static SGlyphRun MakeStraightRun(const SLabelItem& item, double cx, double cy, double angle);

            private:
                static bool PointCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func);
                static bool LineCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func);
                static bool PolygonCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func);

                static bool HorizontalLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params, const std::vector<double>& shifts, const TLabelCandidateFunc& func);
                static bool ParallelLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params, const std::vector<double>& shifts, const TLabelCandidateFunc& func);
                static bool CurvedLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params, const std::vector<double>& shifts, const TLabelCandidateFunc& func);
                static bool PerpendicularLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params, const TLabelCandidateFunc& func);
                static bool MakeCurvedPlacement(const SLabelItem& item, const CLabelPath& path, double start, double shift, double maxCharAngle, SLabelPlacement& placement);

                // horizontal placement in the frame rotated by angle around (cx, cy)
                static bool PolygonInFrame(const SLabelItem& item, const CLabelPolygon& polygon, double angle, double cx, double cy,
                                           const SLabelStrategyParams& params, const TLabelCandidateFunc& func);

                static void PositionsFromMiddle(double from, double to, double step, std::vector<double>& positions);
            };
        }
    }
}
