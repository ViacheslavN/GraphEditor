// The value trees of the fields: floating point, strings, blobs (geometries); the integers in ValueFieldsSigned.cpp / ValueFieldsUnsigned.cpp.

#include "ValueField.h"

namespace bptreedb
{
	namespace db
	{
		using namespace trees;

		IValueFieldPtr CreateValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage)
		{
			try
			{
				switch (storageType)
				{
				case evsStringCut: return std::make_shared<TValueField<TStringValues<true> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case evsString: return std::make_shared<TValueField<TStringValues<false> > >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case evsBlob: return std::make_shared<TValueField<TBlobValues> >(ptrContext, ptrPager, type, nLength, nInfoPage);
				case evsNumber:
				case evsNumberNumLen:
					break;
				default:
					throw CommonLib::CExcBase("unknown value storage {0}", (int)storageType);
				}

				switch (type)
				{
				case dtInteger8:
				case dtInteger16:
				case dtInteger32:
				case dtInteger64:
				case dtOid: return CreateSignedValueField(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtUInteger8:
				case dtUInteger16:
				case dtUInteger32:
				case dtUInteger64: return CreateUnsignedValueField(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtFloat: return MakeNumberValueField<float>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				case dtDouble: return MakeNumberValueField<double>(ptrContext, ptrPager, type, storageType, nLength, nInfoPage);
				default:
					throw CommonLib::CExcBase("type {0} isn't a number", DataTypeName(type));
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to open the value tree of a {0} field, info page {1}", DataTypeName(type), nInfoPage, excSrc);
				throw;
			}
		}
	}
}
