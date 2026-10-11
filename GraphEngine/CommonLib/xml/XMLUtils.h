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
				static void Blob2String(const byte_t *data, size_t size, std::string& str);
				static void String2Blob(const std::string& str, Data::TVecBuffer& data);

				// the text for an attribute value: & < > " ' and the control characters as entities
				static std::string EscapeAttribute(const std::string& str);
				// the text of a node: & < >
				static std::string EscapeText(const std::string& str);
				// the entities back: &amp; &lt; &gt; &quot; &apos; &#N; &#xH; (UTF-8); an unknown '&' stays as it is
				// (the files saved without the escaping)
				static std::string Unescape(const char* psz, size_t nSize);
			};
		}

}