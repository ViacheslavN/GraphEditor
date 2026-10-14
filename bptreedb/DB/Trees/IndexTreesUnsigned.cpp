// The index trees of the unsigned integer fields.

#include "IndexTree.h"

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			IIndexTreePtr CreateUnsignedIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage)
			{
				switch (type)
				{
				case dtUInteger8: return std::make_shared<TIndexTree<uint8_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger16: return std::make_shared<TIndexTree<uint16_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger32: return std::make_shared<TIndexTree<uint32_t> >(ptrContext, ptrPager, nInfoPage);
				case dtUInteger64: return std::make_shared<TIndexTree<uint64_t> >(ptrContext, ptrPager, nInfoPage);
				default:
					throw CommonLib::CExcBase("type {0} isn't an unsigned integer", DataTypeName(type));
				}
			}
		}
	}
}
