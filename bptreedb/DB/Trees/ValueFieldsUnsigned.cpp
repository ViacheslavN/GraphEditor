// The value trees of the unsigned integer fields.

#include "ValueField.h"

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			IValueFieldPtr CreateUnsignedValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage)
			{
				switch (type)
				{
				case dtUInteger8: return MakeNumberValueField<uint8_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtUInteger16: return MakeNumberValueField<uint16_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtUInteger32: return MakeNumberValueField<uint32_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtUInteger64: return MakeNumberValueField<uint64_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				default:
					throw CommonLib::CExcBase("type {0} isn't an unsigned integer", DataTypeName(type));
				}
			}
		}
	}
}
