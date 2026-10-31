#pragma once

// CRingBuilder: the polygon of a multipolygon (boundary) relation from the points of its member ways (1e-7 degrees,
// integers: the joining compares the points exactly).
//   - a closed way is a ring; the others are joined by their end points (reversed if needed) into rings, the ways which
//     don't close a ring are dropped (Unclosed: an extract cuts the relations at its border);
//   - the nesting decides the outer rings and the holes (the roles aren't trusted): a ring inside an even number of rings
//     is outer, inside an odd number - a hole of the smallest ring containing it;
//   - the polygon: every outer ring (clockwise) followed by its holes (counterclockwise), the rings closed.

#include "OSMPointE7.h"

#include <cstdint>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            class CRingBuilder
            {
            public:
                void Clear();
                // the points of a member way (at least 2)
                void AddWay(const SOSMPointE7* pPoints, size_t nPoints);
                // false - no ring; xs - the lon, ys - the lat, parts - the point counts of the rings
                bool Build(std::vector<int64_t>& xs, std::vector<int64_t>& ys, std::vector<uint32_t>& parts);

                uint32_t Rings() const { return m_nRings; }        // of the last Build
                uint32_t Holes() const { return m_nHoles; }
                uint32_t Unclosed() const { return m_nUnclosed; }  // the ways not in a closed ring

            private:
                typedef std::vector<SOSMPointE7> TRing;

                struct SRingInfo
                {
                    double  dArea = 0.;      // signed (the lon - x, the lat - y)
                    int32_t minLon = 0, minLat = 0, maxLon = 0, maxLat = 0;
                    int     nParent = -1;    // the smallest ring containing it
                    int     nDepth = 0;
                };

                void JoinWays();
                static double SignedArea(const TRing& ring);
                // 1 - inside, 0 - outside, -1 - on a vertex
                static int PointInRing(const SOSMPointE7& point, const TRing& ring);
                // ring a is inside ring b (a vertex of a which isn't a vertex of b decides)
                static bool Inside(const TRing& a, const TRing& b);

            private:
                std::vector<TRing> m_ways;     // the open ways
                std::vector<TRing> m_rings;
                uint32_t m_nRings = 0;
                uint32_t m_nHoles = 0;
                uint32_t m_nUnclosed = 0;
            };
        }
    }
}
