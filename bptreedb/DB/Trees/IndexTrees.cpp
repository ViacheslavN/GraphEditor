// The index trees: floating point and string fields; the integers in IndexTreesSigned.cpp / IndexTreesUnsigned.cpp.

#include "IndexTree.h"

namespace bptreedb
{
	namespace db
	{
		using namespace trees;

		IIndexTreePtr CreateIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage, uint32_t nFormat)
		{
			try
			{
				switch (type)
				{
				case dtInteger8:
				case dtInteger16:
				case dtInteger32:
				case dtInteger64:
				case dtOid: return CreateSignedIndexTree(ptrContext, ptrPager, type, nInfoPage);
				case dtUInteger8:
				case dtUInteger16:
				case dtUInteger32:
				case dtUInteger64: return CreateUnsignedIndexTree(ptrContext, ptrPager, type, nInfoPage);
				case dtFloat: return std::make_shared<TIndexTree<float> >(ptrContext, ptrPager, nInfoPage);
				case dtDouble: return std::make_shared<TIndexTree<double> >(ptrContext, ptrPager, nInfoPage);
				case dtString:
					if (nFormat == 1)      // CIndex::FormatStringCut
						return std::make_shared<TIndexTree<StringValue, true> >(ptrContext, ptrPager, nInfoPage);
					return std::make_shared<TIndexTree<StringValue> >(ptrContext, ptrPager, nInfoPage);
				default:
					throw CommonLib::CExcBase("fields of type {0} can't be indexed", DataTypeName(type));
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the index tree of a {0} field, info page {1}", DataTypeName(type), nInfoPage, excSrc);
				throw;
			}
		}
	}
}
