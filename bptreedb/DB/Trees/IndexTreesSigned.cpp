// The index trees of the signed integer fields.

#include "IndexTree.h"

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			IIndexTreePtr CreateSignedIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage)
			{
				switch (type)
				{
				case dtInteger8: return std::make_shared<TIndexTree<int8_t> >(ptrContext, ptrPager, nInfoPage);
				case dtInteger16: return std::make_shared<TIndexTree<int16_t> >(ptrContext, ptrPager, nInfoPage);
				case dtInteger32: return std::make_shared<TIndexTree<int32_t> >(ptrContext, ptrPager, nInfoPage);
				case dtInteger64:
				case dtOid: return std::make_shared<TIndexTree<int64_t> >(ptrContext, ptrPager, nInfoPage);
				default:
					throw CommonLib::CExcBase("type {0} isn't a signed integer", DataTypeName(type));
				}
			}
		}
	}
}
