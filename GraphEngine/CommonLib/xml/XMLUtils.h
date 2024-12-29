#pragma once

#include "CommonLib.h"
#include "../data/ByteArray.h"
namespace CommonLib
{

		namespace xml
		{
			class CXMLUtils
			{
			public:
				static void Blob2String(const byte_t *data, size_t size, astr& str);
				static void String2Blob(const astr& str, Data::TVecBuffer& data);
			};
		}

}