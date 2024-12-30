#include "CommonLib.h"
#include "Rect.h"

namespace GraphEngine
{
	namespace Display
	{

		GRect::GRect()
		{
			Set(0, 0, -1, -1);
		}
		GRect::GRect(const GPoint& _min, const GPoint& _max)
		{
			Set(_min.x, _min.y, _max.x, _max.y);
		}
		GRect::GRect(GUnits minX, GUnits minY, GUnits maxX, GUnits maxY)
		{
			Set(minX, minY, maxX, maxY);
		}
		GRect::GRect(const GRect& rc)
		{
			Set(rc.xMin, rc.yMin, rc.xMax, rc.yMax);
		}


		bool GRect::operator==(const GRect& rc) const
		{
			return xMin == rc.xMin && xMax == rc.xMax && yMin == rc.yMin && yMax == rc.yMax;
		}
		bool GRect::operator!=(const GRect& rc) const
		{
			return xMin != rc.xMin || xMax != rc.xMax || yMin != rc.yMin || yMax != rc.yMax;
		}

		void GRect::Set(GUnits _xMin, GUnits _yMin, GUnits _xMax, GUnits _yMax)
		{
			xMin = _xMin;
			yMin = _yMin;
			xMax = _xMax;
			yMax = _yMax;
		}
		void GRect::Null()
		{
			Set(0, 0, 0, 0);
		}
		GRect& GRect::Offset(GUnits xOffset, GUnits yOffset)
		{
			xMin += xOffset;
			xMax += xOffset;
			yMin += yOffset;
			yMax += yOffset;
			return *this;
		}
		GRect& GRect::Offset(const GPoint& point)
		{
			return Offset(point.x, point.y);
		}
		void GRect::Inflate(GUnits dx, GUnits dy)
		{
			xMin -= dx;
			xMax += dx;
			yMin -= dy;
			yMax += dy;
		}

		void GRect::ExpandRect(const GRect &rc)
		{
			if (rc.IsEmpty())
				return;

			if (IsEmpty())
			{
				Set(rc.xMin, rc.yMin, rc.xMax, rc.yMax);
			}
			else
				Set(min(xMin, rc.xMin), min(yMin, rc.yMin), max(xMax, rc.xMax), max(yMax, rc.yMax));
		}
		void GRect::ExpandRect(const GPoint &p)
		{
			if (IsEmpty())
			{
				Set(p.x, p.y, p.x, p.y);
			}
			else
				Set(min(xMin, p.x), min(yMin, p.y), max(xMax, p.x), max(yMax, p.y));
		}
		GRect& GRect::Intersect(const GRect &rc)
		{
			if (this == &rc)
				return *this;

			if (IsEmpty() || rc.IsEmpty())
			{
				*this = GRect();
				return *this;
			}

			xMin = max(xMin, rc.xMin);
			yMin = max(yMin, rc.yMin);
			xMax = min(xMax, rc.xMax);
			yMax = min(yMax, rc.yMax);
			return *this;
		}
		bool GRect::IsIntersect(const GRect &rc) const
		{
			if (IsEmpty() || rc.IsEmpty())
				return false;

			if (xMin >= rc.xMax) return false;
			if (xMax <= rc.xMin) return false;
			if (yMin >= rc.yMax) return false;
			if (yMax <= rc.yMin) return false;
			return true;
		}
		bool GRect::IsInRect(const GRect &rc) const
		{
			if (IsEmpty() || rc.IsEmpty())
				return false;

			if (xMin > rc.xMin) return false;
			if (xMax < rc.xMax) return false;
			if (yMin > rc.yMin) return false;
			if (yMax < rc.yMax) return false;
			return true;
		}
		GUnits GRect::Width()  const
		{
			return xMax - xMin;
		}
		GUnits  GRect::Height() const
		{
			return yMax - yMin;
		}
		bool GRect::IsEmpty() const
		{
			return (xMax < xMin || (yMax < yMin));
		}
		bool GRect::IsNull() const
		{
			return (xMax == xMin) && (yMax == yMin);
		}
		GPoint GRect::CenterPoint() const
		{
			return GPoint((xMin + xMax) / 2, (yMin + yMax) / 2);
		}
		void GRect::Scale(double scl)
		{
			Scale(scl, scl);
		}
		void GRect::Scale(double sclx, double scly)
		{
			xMin = (GUnits)(xMin * sclx);
			xMax = (GUnits)(xMax * sclx);
			yMin = (GUnits)(yMin * scly);
			yMax = (GUnits)(yMax * scly);
		}

		bool GRect::PointInRect(const GPoint &p) const
		{
			return (p.x > xMin) && (p.y > yMin) &&
				(p.x < xMax) && (p.y < yMax);
		}

		bool GRect::PointInRectEQ(const GPoint &p) const
		{
			return (p.x >= xMin) && (p.y >= yMin) &&
				(p.x <= xMax) && (p.y <= yMax);
		}
	}
}