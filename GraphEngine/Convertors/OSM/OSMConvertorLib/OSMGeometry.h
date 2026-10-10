#pragma once
#include "OSMConvertorLib.h"

namespace GraphEngine {
    namespace Convertors {

        typedef std::vector<CommonLib::GisXYPoint> TOSMPoints;

        // longitude / latitude -> the coordinates of the result (Web Mercator or longitude / latitude)
        class COSMProjection
        {
        public:
            explicit COSMProjection(bool bWebMercator = true) : m_bWebMercator(bWebMercator) {}

            bool IsWebMercator() const { return m_bWebMercator; }
            void Project(double dLon, double dLat, double& x, double& y) const;
            Geometry::ISpatialReferencePtr CreateSpatialReference() const;
            CommonLib::Units GetUnits() const;

        private:
            bool m_bWebMercator;
        };

        class COSMGeometry
        {
        public:
            static CommonLib::IGeoShapePtr CreatePoint(double x, double y);
            // parts with less than 2 points are skipped, null - no part
            static CommonLib::IGeoShapePtr CreatePolyline(const std::vector<TOSMPoints>& parts);
            // the rings are oriented: outer - clockwise, holes - counter clockwise (y goes up, as in shapefiles)
            static CommonLib::IGeoShapePtr CreatePolygon(const std::vector<TOSMPoints>& rings, const std::vector<bool>& outer);

            // positive - counter clockwise
            static double SignedArea(const TOSMPoints& ring);
            static bool PointInRing(const TOSMPoints& ring, double x, double y);

            // Multipolygon from its member ways: the ways are joined into closed rings (by the end points, any direction),
            // the outer rings and the holes are found by nesting (the roles of the members aren't reliable).
            // Rings which can't be closed are skipped. Returns false if there is no valid ring.
            static bool BuildMultipolygon(const std::vector<TOSMPoints>& ways, std::vector<TOSMPoints>& rings, std::vector<bool>& outer);
        };
    }
}
