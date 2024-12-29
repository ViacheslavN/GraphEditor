#pragma once
#include "Point.h"

namespace GraphEngine
{
	namespace Display
	{
		class GRect
		{
		public:
			GRect();
			GRect(const GPoint& _min, const GPoint& _max);
			GRect(GUnits minX, GUnits minY, GUnits maxX, GUnits maxY);
			GRect(const GRect&);


			bool operator==(const GRect&) const;
			bool operator!=(const GRect&) const;

			void Set(GUnits xMin, GUnits yMin, GUnits xMax, GUnits yMax);
			GRect& Offset(GUnits xOffset, GUnits yOffset);
			GRect& Offset(const GPoint& point);
			void Inflate(GUnits dx, GUnits dy);
			void Null();
			void ExpandRect(const GRect &rc);
			void ExpandRect(const GPoint &p);
			GRect& Intersect(const GRect &rc);
			bool IsIntersect(const GRect &rc) const;
			bool IsInRect(const GRect &rc) const;
			GUnits  Width()  const;
			GUnits  Height() const;
			bool IsEmpty() const;
			bool IsNull() const;
			GPoint CenterPoint() const;
			void Scale(double scl);
			void Scale(double sclx, double scly);
			bool PointInRect(const GPoint &p) const;
			bool PointInRectEQ(const GPoint &p) const;

		public:
			GUnits xMin;
			GUnits yMin;
			GUnits xMax;
			GUnits yMax;
		};
	}
}