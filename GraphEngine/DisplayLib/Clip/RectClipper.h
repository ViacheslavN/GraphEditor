#ifndef GIS_ENGINE_DISPLAY_RECT_CLIP_H_
#define GIS_ENGINE_DISPLAY_RECT_CLIP_H_

#include "../DisplayLib.h"
#include "../GraphTypes/Point.h"
#include "Clip.h"
 
namespace GraphEngine
{
	namespace Display
	{
		class CClipRectAlloc;
		
		class CRectClipper : public IClip
		{
		public:
			 CRectClipper(CClipRectAlloc *pAlloc);
			 ~CRectClipper();
			 virtual int  clipLine(const GRect& clipper, GPoint* pBeg, GPoint* pEnd);
			 virtual void clipLine(const GRect& clipper, GPoint** ppPoints, int** ppnPointCounts, int* pCount);
			 virtual void clipRing(const GRect& clipper, GPoint** ppPoints, int* pPointCount);
			 virtual void clipPolygon(const GRect& clipper, GPoint** ppPoints, int** ppPointCounts, int* pCount);
		private:
			enum clip_bits
			{
				clip_left =   1,
				clip_right =  2,
				clip_top =    4,
				clip_bottom = 8
			};
			enum clip_res
			{
				clip_contains = 0,
				clip_begin = 1,
				clip_end = 2,
				clip_disjoint = 4
			};

			enum clip_memory_slots
			{
				in_points_slot,
				in_parts_slot,
				out_points_slot,
				out_parts_slot,

				base_slot = 100
			};


		private:
			int   getCode(const GRect& clipper, const GPoint& pnt);
			bool  all_points_in_rect(const GRect& clipper, GPoint* points, int* pointCounts, int count);
			bool  all_points_out_rect(const GRect& clipper, GPoint* points, int* pointCounts, int count);
			bool  has_poly_intersections(const GRect& clipper, GPoint* points, int count);
			int   total_points(GPoint* points, int* pointCounts, int count);
		private:
			CClipRectAlloc* m_pAlloc;
		};
	}
}


#endif