#include "CommonLib.h"
#include "../GraphTypes/Point.h"
#include "ClipRectAlloc.h"

namespace GraphEngine
{
	namespace Display
	{
		CClipRectAlloc::CClipRectAlloc(CommonLib::IAllocPtr pAlloc) : m_pAlloc(pAlloc)
		{
			if(m_pAlloc.get() == nullptr)
				m_pAlloc = std::shared_ptr<CommonLib::CSimpleAlloc>();
		}
		CClipRectAlloc::~CClipRectAlloc()
		{

		}

		template< typename TVal, typename TMap>
		TVal* getBuf(uint32_t nSlot, uint32_t nCount, TMap& map)
		{
			typename TMap::iterator it = map.find(nSlot);
			if(it != map.end())
			{
				if(it->second.size() < nCount)
					it->second.reserve(nCount);
				return it->second.begin();
			}
			else
			{
				if(nCount < 8192)
					nCount = 8192;
				map[nSlot].reserve(nCount);
				return map[nSlot].begin();
			}
		}


		int* CClipRectAlloc::getPartsBuf(uint32_t nCount, uint32_t nSlot)
		{
		    return getBuf<int, TMapParts>(nSlot, nCount, m_mapParts);
		}

		GPoint* CClipRectAlloc::getPointBuf(uint32_t nCount, uint32_t nSlot)
		{			 
			return getBuf<GPoint, TMapPoints>(nSlot, nCount, m_mapPoints);
		}
	}
}