#include "stdafx.h"
#include "XMLUtils.h"

namespace CommonLib
	{
		namespace xml
		{
			void CXMLUtils::Blob2String(const byte_t *pData, size_t size, std::string& result)
			{
	 
				if (!pData || size == 0)
					return;

				result.resize(size * 2 + 1, 0);
				for (size_t i = 0; i < size; ++i)
				{
					unsigned char sym = pData[i];
					unsigned char left = sym >> 4;
					unsigned char right = sym & 0xF;
					result[(int)i * 2 + 0] = left <= 9 ? (left + '0') : ((left - 10) + 'A');
					result[(int)i * 2 + 1] = right <= 9 ? (right + '0') : ((right - 10) + 'A');
				}
			}

			void CXMLUtils::String2Blob(const std::string& str, CommonLib::Data::TVecBuffer& blob)
			{
				int len = (int)str.length() / 2;
				if (len == 0)
					return;
				blob.resize(len);

				for (int i = 0; i < len; ++i)
				{
					wchar_t left = str[i * 2 + 0];
					wchar_t right = str[i * 2 + 1];
					int leftVal;
					int rightVal;

					if (left >= '0' && left <= '9')
						leftVal = left - '0';
					else if (left >= 'A' && left <= 'F')
						leftVal = left - 'A' + 10;
					else
						throw CommonLib::CExcBase("CXMLUtils::String2Blob invalid symbol");
			

					if (right >= '0' && right <= '9')
						rightVal = right - '0';
					else if (right >= 'A' && right <= 'F')
						rightVal = right - 'A' + 10;
					else
						throw CommonLib::CExcBase("CXMLUtils::String2Blob invalid symbol");
	
					blob[i] = (unsigned char)((leftVal & 0xF) << 4 | (rightVal & 0xF));
				}
			}

			namespace
			{
				std::string Escape(const std::string& str, bool bAttribute)
				{
					std::string result;
					result.reserve(str.size() + 16);
					for (unsigned char ch : str)
					{
						switch (ch)
						{
						case '&': result += "&amp;"; break;
						case '<': result += "&lt;"; break;
						case '>': result += "&gt;"; break;
						case '"': if (bAttribute) result += "&quot;"; else result += (char)ch; break;
						case '\'': if (bAttribute) result += "&apos;"; else result += (char)ch; break;
						case '\n': if (bAttribute) result += "&#10;"; else result += (char)ch; break;
						case '\r': if (bAttribute) result += "&#13;"; else result += (char)ch; break;
						case '\t': if (bAttribute) result += "&#9;"; else result += (char)ch; break;
						default: result += (char)ch; break;
						}
					}
					return result;
				}

				void AppendUtf8(std::string& result, uint32_t nCode)
				{
					if (nCode < 0x80)
						result += (char)nCode;
					else if (nCode < 0x800)
					{
						result += (char)(0xC0 | (nCode >> 6));
						result += (char)(0x80 | (nCode & 0x3F));
					}
					else if (nCode < 0x10000)
					{
						result += (char)(0xE0 | (nCode >> 12));
						result += (char)(0x80 | ((nCode >> 6) & 0x3F));
						result += (char)(0x80 | (nCode & 0x3F));
					}
					else
					{
						result += (char)(0xF0 | (nCode >> 18));
						result += (char)(0x80 | ((nCode >> 12) & 0x3F));
						result += (char)(0x80 | ((nCode >> 6) & 0x3F));
						result += (char)(0x80 | (nCode & 0x3F));
					}
				}
			}

			std::string CXMLUtils::EscapeAttribute(const std::string& str)
			{
				return Escape(str, true);
			}

			std::string CXMLUtils::EscapeText(const std::string& str)
			{
				return Escape(str, false);
			}

			std::string CXMLUtils::Unescape(const char* psz, size_t nSize)
			{
				std::string result;
				result.reserve(nSize);
				for (size_t i = 0; i < nSize; ++i)
				{
					if (psz[i] != '&')
					{
						result += psz[i];
						continue;
					}
					// the entity up to ';' (at most 10 characters)
					size_t nEnd = i + 1;
					while (nEnd < nSize && nEnd - i <= 10 && psz[nEnd] != ';' && psz[nEnd] != '&')
						++nEnd;
					if (nEnd >= nSize || psz[nEnd] != ';')
					{
						result += '&';
						continue;
					}
					std::string sEntity(psz + i + 1, nEnd - i - 1);
					bool bKnown = true;
					if (sEntity == "amp")
						result += '&';
					else if (sEntity == "lt")
						result += '<';
					else if (sEntity == "gt")
						result += '>';
					else if (sEntity == "quot")
						result += '"';
					else if (sEntity == "apos")
						result += '\'';
					else if (sEntity.size() > 1 && sEntity[0] == '#')
					{
						bool bHex = sEntity[1] == 'x' || sEntity[1] == 'X';
						const char* pDigits = sEntity.c_str() + (bHex ? 2 : 1);
						char* pStop = nullptr;
						unsigned long nCode = strtoul(pDigits, &pStop, bHex ? 16 : 10);
						if (*pDigits && pStop && *pStop == 0 && nCode > 0 && nCode <= 0x10FFFF)
							AppendUtf8(result, (uint32_t)nCode);
						else
							bKnown = false;
					}
					else
						bKnown = false;

					if (bKnown)
						i = nEnd;
					else
						result += '&';
				}
				return result;
			}
		}

}