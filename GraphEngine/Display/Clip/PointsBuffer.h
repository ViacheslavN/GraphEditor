#pragma once
#include "../CommonLib/alloc/alloc.h"
#include "../GraphTypes/Point.h"
 

namespace GraphEngine
{
	namespace Display
	{
		class CPointBuffer
		{
		public:
			CPointBuffer(CommonLib::IAllocPtr pAlloc);
			~CPointBuffer();

			GPoint *GetPoints();
			const GPoint *GetPoints() const;
			uint32_t GetSize() const;
			void Reserve(uint32_t nSize);
			void push_back(const GPoint& point);
		private:
			CommonLib::IAllocPtr m_pAlloc;
		};
	}
}