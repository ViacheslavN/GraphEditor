// The value trees of the signed integer fields.

#include "ValueField.h"

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			IValueFieldPtr CreateSignedValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage)
			{
				switch (type)
				{
				case dtInteger8: return MakeNumberValueField<int8_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtInteger16: return MakeNumberValueField<int16_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtInteger32: return MakeNumberValueField<int32_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtInteger64:
				case dtOid: return MakeNumberValueField<int64_t>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				default:
					throw CommonLib::CExcBase("type {0} isn't a signed integer", DataTypeName(type));
				}
			}
		}
	}
}
