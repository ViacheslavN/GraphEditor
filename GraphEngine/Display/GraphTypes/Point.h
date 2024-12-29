#pragma once
#include "GraphTypes.h"

namespace GraphEngine
{
	namespace Display
	{
		class GPoint
		{
		public:
			GPoint();
			GPoint(GUnits X, GUnits Y);
			GPoint(const GPoint& gp);

			bool  operator==(const GPoint&) const;
			bool  operator!=(const GPoint&) const;
			const GPoint &operator+=(const GPoint &);
			const GPoint &operator-=(const GPoint &);
			GPoint operator+(const GPoint& p) const;
			GPoint operator-(const GPoint& p) const;
		public:
			GUnits x, y;

			static double CalcDistance(const GPoint& pt0, const GPoint& pt1)
			{
				return CDisplayMath::CalcDistance(pt0.x, pt0.y, pt1.x, pt1.y);
			}

			static double CrossProduct(const GPoint& pt1, const GPoint& pt2, const GPoint& pt3)
			{
				return (pt3.x - pt2.x) * (pt2.y - pt1.y) - (pt3.y - pt2.y) * (pt2.x - pt1.x);
			}
		};

		typedef std::vector<GPoint> TVecPoints;

	
	}
}