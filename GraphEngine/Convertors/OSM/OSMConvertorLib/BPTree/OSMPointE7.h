#pragma once

// The coordinates of an OSM node in the B+ trees of the conversion.

#include <cmath>
#include <cstdint>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the coordinates of a node: 1e-7 degrees (the OSM integers)
            struct SOSMPointE7
            {
                int32_t lat = 0;
                int32_t lon = 0;

                SOSMPointE7() {}
                SOSMPointE7(int32_t nLat, int32_t nLon) : lat(nLat), lon(nLon) {}

                static constexpr double Scale = 1e7;
                static SOSMPointE7 FromDegrees(double dLat, double dLon)
                {
                    return SOSMPointE7((int32_t)std::lround(dLat * Scale), (int32_t)std::lround(dLon * Scale));
                }
                double Lat() const { return lat / Scale; }
                double Lon() const { return lon / Scale; }

                bool operator == (const SOSMPointE7& other) const { return lat == other.lat && lon == other.lon; }
                bool operator != (const SOSMPointE7& other) const { return !(*this == other); }
            };
        }
    }
}
