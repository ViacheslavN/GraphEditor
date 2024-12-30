#pragma once


#include "../CommonLib/alloc/alloc.h"
#include "../CommonLib/data/PodVector.h"

#include <map>
namespace GraphEngine
{
	namespace Display
	{
		class CClipRectAlloc
		{
			typedef CommonLib::Data::TPodVector<int> TVecParts;
			typedef CommonLib::Data::TPodVector<GPoint> TVecGPoints;

			typedef std::map<uint32_t, TVecParts> TMapParts;
			typedef std::map<uint32_t, TVecGPoints> TMapPoints;
		public:
            CClipRectAlloc(CommonLib::IAllocPtr pAlloc);
			~CClipRectAlloc();
			int *getPartsBuf(uint32_t nCount, uint32_t nSlot);
			GPoint* getPointBuf(uint32_t nCount, uint32_t nSlot);

		private:
			CommonLib::IAllocPtr m_pAlloc;
			TMapParts m_mapParts;
			TMapPoints m_mapPoints;

		};
	}
}
