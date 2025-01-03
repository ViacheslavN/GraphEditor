#include "CommonLib.h"
#include "Point.h"

namespace GraphEngine
{
	namespace Display
	{
		GPoint::GPoint() : x(0), y(0)
		{}
		GPoint::GPoint(GUnits X, GUnits Y) : x(X), y(Y)
		{}
		GPoint::GPoint(const GPoint& gp) : x(gp.x), y(gp.y)
		{}

		bool    GPoint::operator==(const GPoint& gp) const
		{
			return x == gp.x && y == gp.y;
		}
		bool    GPoint::operator!=(const GPoint& gp) const
		{
			return x != gp.x || y != gp.y;
		}
		const GPoint & GPoint::operator+=(const GPoint& gp)
		{
			x += gp.x;
			y += gp.y;
			return *this;
		}
		const GPoint &GPoint::operator-=(const GPoint& gp)
		{
			x -= gp.x;
			y -= gp.y;
			return *this;
		}
		GPoint GPoint::operator+(const GPoint& p) const
		{
			return GPoint(x + p.x, y + p.y);
		}
		GPoint GPoint::operator-(const GPoint& p) const
		{
			return GPoint(x - p.x, y - p.y);
		}
	}
}