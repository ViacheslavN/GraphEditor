#include "Values.h"
#include "../BPtreeLib/OrderedKey.h"
#include "../../CommonLib/str/StringEncoding.h"
#include "../../CommonLib/data/blob.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace bptreedb
{
	namespace db
	{
		namespace
		{
			// a number variant: integer (signed / unsigned) or floating point
			struct SNumber
			{
				enum EKind { eSigned, eUnsigned, eFloat } m_kind{ eSigned };
				int64_t m_s{ 0 };
				uint64_t m_u{ 0 };
				double m_d{ 0 };
			};

			bool GetNumber(const CommonLib::CVariant& v, SNumber& n)
			{
				switch (v.GetTypeID())
				{
				case CommonLib::varType_bool:     n.m_kind = SNumber::eUnsigned; n.m_u = v.Get<bool>() ? 1 : 0; return true;
				case CommonLib::varType_int8_t:   n.m_kind = SNumber::eSigned; n.m_s = v.Get<int8_t>(); return true;
				case CommonLib::varType_int16_t:  n.m_kind = SNumber::eSigned; n.m_s = v.Get<int16_t>(); return true;
				case CommonLib::varType_int32_t:  n.m_kind = SNumber::eSigned; n.m_s = v.Get<int32_t>(); return true;
				case CommonLib::varType_int64_t:  n.m_kind = SNumber::eSigned; n.m_s = v.Get<int64_t>(); return true;
				case CommonLib::varType_uint8_t:  n.m_kind = SNumber::eUnsigned; n.m_u = v.Get<uint8_t>(); return true;
				case CommonLib::varType_uint16_t: n.m_kind = SNumber::eUnsigned; n.m_u = v.Get<uint16_t>(); return true;
				case CommonLib::varType_uint32_t: n.m_kind = SNumber::eUnsigned; n.m_u = v.Get<uint32_t>(); return true;
				case CommonLib::varType_uint64_t: n.m_kind = SNumber::eUnsigned; n.m_u = v.Get<uint64_t>(); return true;
				case CommonLib::varType_float:    n.m_kind = SNumber::eFloat; n.m_d = v.Get<float>(); return true;
				case CommonLib::varType_double:   n.m_kind = SNumber::eFloat; n.m_d = v.Get<double>(); return true;
				default:
					return false;
				}
			}

			template <class T>
			T NumberTo(const SNumber& n)
			{
				typedef std::numeric_limits<T> L;
				if constexpr (std::is_floating_point<T>::value)
				{
					switch (n.m_kind)
					{
					case SNumber::eSigned: return (T)n.m_s;
					case SNumber::eUnsigned: return (T)n.m_u;
					default: return (T)n.m_d;
					}
				}
				else
				{
					switch (n.m_kind)
					{
					case SNumber::eSigned:
						if (std::is_signed<T>::value)
						{
							if (n.m_s < (int64_t)(L::min)() || n.m_s > (int64_t)(L::max)())
								throw CommonLib::CExcBase("value {0} is out of the range of the type", n.m_s);
						}
						else if (n.m_s < 0 || (uint64_t)n.m_s > (uint64_t)(L::max)())
							throw CommonLib::CExcBase("value {0} is out of the range of the type", n.m_s);
						return (T)n.m_s;
					case SNumber::eUnsigned:
						if (n.m_u > (uint64_t)(L::max)())
							throw CommonLib::CExcBase("value {0} is out of the range of the type", n.m_u);
						return (T)n.m_u;
					default:
						{
							double d = n.m_d;
							if (!std::isfinite(d) || std::floor(d) != d)
								throw CommonLib::CExcBase("value {0} isn't an integer", d);
							// 2^63, 2^64 are exact doubles; (max)() of a 64 bit type isn't
							double dMax = std::ldexp(1.0, (int)(sizeof(T) * 8 - (std::is_signed<T>::value ? 1 : 0)));
							double dMin = std::is_signed<T>::value ? -dMax : 0.0;
							if (d < dMin || d >= dMax)
								throw CommonLib::CExcBase("value {0} is out of the range of the type", d);
							return (T)d;
						}
					}
				}
			}

			template <class T>
			int CompareNumbers(const CommonLib::CVariant& l, const CommonLib::CVariant& r)
			{
				uint64_t a = TOrderedBits<T>::ToBits(l.Get<T>());
				uint64_t b = TOrderedBits<T>::ToBits(r.Get<T>());
				return a < b ? -1 : (a > b ? 1 : 0);
			}

			int CompareBytes(const std::string& l, const std::string& r)
			{
				size_t n = (std::min)(l.size(), r.size());
				int res = n ? memcmp(l.data(), r.data(), n) : 0;
				if (res != 0)
					return res;
				return l.size() < r.size() ? -1 : (l.size() > r.size() ? 1 : 0);
			}
		}

		template <class T>
		T VariantToNumber(const CommonLib::CVariant& value)
		{
			SNumber n;
			if (value.IsNull())
				throw CommonLib::CExcBase("the value is NULL");
			if (!GetNumber(value, n))
				throw CommonLib::CExcBase("the value isn't a number, variant type {0}", (int)value.GetTypeID());
			return NumberTo<T>(n);
		}

		template int8_t VariantToNumber<int8_t>(const CommonLib::CVariant&);
		template uint8_t VariantToNumber<uint8_t>(const CommonLib::CVariant&);
		template int16_t VariantToNumber<int16_t>(const CommonLib::CVariant&);
		template uint16_t VariantToNumber<uint16_t>(const CommonLib::CVariant&);
		template int32_t VariantToNumber<int32_t>(const CommonLib::CVariant&);
		template uint32_t VariantToNumber<uint32_t>(const CommonLib::CVariant&);
		template int64_t VariantToNumber<int64_t>(const CommonLib::CVariant&);
		template uint64_t VariantToNumber<uint64_t>(const CommonLib::CVariant&);
		template float VariantToNumber<float>(const CommonLib::CVariant&);
		template double VariantToNumber<double>(const CommonLib::CVariant&);

		CommonLib::IGeoShapePtr MakeShape(const byte_t* pData, uint32_t nSize)
		{
			std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
			if (nSize != 0)
				ptrShape->Import(pData, nSize);
			return ptrShape;
		}

		bool GetShapeBBox(const CommonLib::CVariant& value, CommonLib::bbox& bb)
		{
			const CommonLib::IGeoShapePtr* pShape = value.GetPtr<CommonLib::IGeoShapePtr>();
			if (pShape == nullptr || pShape->get() == nullptr || (*pShape)->GetPointCnt() == 0)
				return false;
			bb = (*pShape)->GetBB();
			return !(bb.xMax < bb.xMin) && !(bb.yMax < bb.yMin);
		}

		bool IsNumberType(eDataTypes type)
		{
			switch (type)
			{
			case dtInteger8: case dtInteger16: case dtInteger32: case dtInteger64:
			case dtUInteger8: case dtUInteger16: case dtUInteger32: case dtUInteger64:
			case dtOid: case dtFloat: case dtDouble:
				return true;
			default:
				return false;
			}
		}

		bool IsFixedSizeType(eDataTypes type)
		{
			return IsNumberType(type);
		}

		uint32_t FixedTypeSize(eDataTypes type)
		{
			switch (type)
			{
			case dtInteger8: case dtUInteger8: return 1;
			case dtInteger16: case dtUInteger16: return 2;
			case dtInteger32: case dtUInteger32: case dtFloat: return 4;
			case dtInteger64: case dtUInteger64: case dtOid: case dtDouble: return 8;
			default: return 0;
			}
		}

		CommonLib::CVariant ToFieldValue(const CommonLib::CVariant& value, eDataTypes type, uint32_t nMaxLen)
		{
			try
			{
				if (value.IsNull())
					return CommonLib::CVariant();

				switch (type)
				{
				case dtInteger8:   return CommonLib::CVariant(VariantToNumber<int8_t>(value));
				case dtInteger16:  return CommonLib::CVariant(VariantToNumber<int16_t>(value));
				case dtInteger32:  return CommonLib::CVariant(VariantToNumber<int32_t>(value));
				case dtInteger64:
				case dtOid:        return CommonLib::CVariant(VariantToNumber<int64_t>(value));
				case dtUInteger8:  return CommonLib::CVariant(VariantToNumber<uint8_t>(value));
				case dtUInteger16: return CommonLib::CVariant(VariantToNumber<uint16_t>(value));
				case dtUInteger32: return CommonLib::CVariant(VariantToNumber<uint32_t>(value));
				case dtUInteger64: return CommonLib::CVariant(VariantToNumber<uint64_t>(value));
				case dtFloat:      return CommonLib::CVariant(VariantToNumber<float>(value));
				case dtDouble:     return CommonLib::CVariant(VariantToNumber<double>(value));
				case dtString:
					{
						std::string s;
						if (value.IsType<CommonLib::astr_t>())
							s = value.Get<CommonLib::astr_t>();
						else if (value.IsType<CommonLib::wstr_t>())
							s = CommonLib::StringEncoding::str_w2utf8_safe(value.Get<CommonLib::wstr_t>());
						else
							throw CommonLib::CExcBase("a string is expected, variant type {0}", (int)value.GetTypeID());

						if (nMaxLen != 0 && s.size() > nMaxLen)
							throw CommonLib::CExcBase("string length {0} exceeds the field length {1}", (uint64_t)s.size(), nMaxLen);
						return CommonLib::CVariant(CommonLib::astr_t(s));
					}
				case dtBlob:
					{
						if (value.IsType<CommonLib::CBlobPtr_t>())
						{
							if (value.Get<CommonLib::CBlobPtr_t>().get() == nullptr)
								return CommonLib::CVariant();
							return value;
						}
						if (value.IsType<CommonLib::astr_t>())
						{
							const CommonLib::astr_t& s = value.Get<CommonLib::astr_t>();
							CommonLib::CBlobPtr_t ptrBlob = std::make_shared<CommonLib::Data::CBlob>();
							ptrBlob->Copy((const byte_t*)s.data(), (uint32_t)s.size());
							return CommonLib::CVariant(ptrBlob);
						}
						throw CommonLib::CExcBase("a blob is expected, variant type {0}", (int)value.GetTypeID());
					}
				case dtGeometry:
					{
						if (value.IsType<CommonLib::IGeoShapePtr>())
						{
							if (value.Get<CommonLib::IGeoShapePtr>().get() == nullptr)
								return CommonLib::CVariant();
							return value;
						}
						if (value.IsType<CommonLib::CBlobPtr_t>())
						{
							const CommonLib::CBlobPtr_t& ptrBlob = value.Get<CommonLib::CBlobPtr_t>();
							if (ptrBlob.get() == nullptr)
								return CommonLib::CVariant();
							return CommonLib::CVariant(MakeShape(ptrBlob->Buffer(), ptrBlob->Size()));
						}
						throw CommonLib::CExcBase("a shape is expected, variant type {0}", (int)value.GetTypeID());
					}
				default:
					throw CommonLib::CExcBase("unsupported field type {0}", (int)type);
				}
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("failed to convert a value to {0}", DataTypeName(type), excSrc);
				throw;
			}
		}

		int CompareFieldValues(const CommonLib::CVariant& left, const CommonLib::CVariant& right, eDataTypes type)
		{
			switch (type)
			{
			case dtInteger8:   return CompareNumbers<int8_t>(left, right);
			case dtInteger16:  return CompareNumbers<int16_t>(left, right);
			case dtInteger32:  return CompareNumbers<int32_t>(left, right);
			case dtInteger64:
			case dtOid:        return CompareNumbers<int64_t>(left, right);
			case dtUInteger8:  return CompareNumbers<uint8_t>(left, right);
			case dtUInteger16: return CompareNumbers<uint16_t>(left, right);
			case dtUInteger32: return CompareNumbers<uint32_t>(left, right);
			case dtUInteger64: return CompareNumbers<uint64_t>(left, right);
			case dtFloat:      return CompareNumbers<float>(left, right);
			case dtDouble:     return CompareNumbers<double>(left, right);
			case dtString:     return CompareBytes(left.Get<CommonLib::astr_t>(), right.Get<CommonLib::astr_t>());
			default:
				throw CommonLib::CExcBase("values of type {0} can't be compared", DataTypeName(type));
			}
		}

		bool MatchOp(const CommonLib::CVariant& value, const CommonLib::CVariant& key, OpType op, eDataTypes type)
		{
			int c = CompareFieldValues(value, key, type);
			switch (op)
			{
			case OpEqual: return c == 0;
			case OpNoEqual: return c != 0;
			case OpLess: return c < 0;
			case OpLessOrEqual: return c <= 0;
			case OpGreater: return c > 0;
			case OpGreaterOrEqual: return c >= 0;
			default:
				throw CommonLib::CExcBase("unknown operation {0}", (int)op);
			}
		}

		int32_t ValueBytes(const CommonLib::CVariant& value)
		{
			switch (value.GetTypeID())
			{
			case CommonLib::varType_CEmptyVariant: return 0;
			case CommonLib::varType_bool:
			case CommonLib::varType_int8_t:
			case CommonLib::varType_uint8_t: return 1;
			case CommonLib::varType_int16_t:
			case CommonLib::varType_uint16_t: return 2;
			case CommonLib::varType_int32_t:
			case CommonLib::varType_uint32_t:
			case CommonLib::varType_float: return 4;
			case CommonLib::varType_int64_t:
			case CommonLib::varType_uint64_t:
			case CommonLib::varType_double: return 8;
			case CommonLib::varType_astr_t: return (int32_t)value.Get<CommonLib::astr_t>().size();
			case CommonLib::varType_wstr_t: return (int32_t)(value.Get<CommonLib::wstr_t>().size() * sizeof(wchar_t));
			case CommonLib::varType_CBlobPtr_t:
				{
					const CommonLib::CBlobPtr_t& ptrBlob = value.Get<CommonLib::CBlobPtr_t>();
					return ptrBlob.get() ? (int32_t)ptrBlob->Size() : 0;
				}
			case CommonLib::varType_IGeoShapePtr:
				{
					const CommonLib::IGeoShapePtr& ptrShape = value.Get<CommonLib::IGeoShapePtr>();
					return ptrShape.get() ? (int32_t)ptrShape->Size() : 0;
				}
			default:
				return 0;
			}
		}

		void WriteVariant(CommonLib::IWriteStream* pStream, const CommonLib::CVariant& value)
		{
			pStream->Write((uint32_t)value.GetTypeID());
			switch (value.GetTypeID())
			{
			case CommonLib::varType_CEmptyVariant: break;
			case CommonLib::varType_bool: pStream->Write(value.Get<bool>()); break;
			case CommonLib::varType_int8_t: pStream->Write((uint8_t)value.Get<int8_t>()); break;
			case CommonLib::varType_uint8_t: pStream->Write(value.Get<uint8_t>()); break;
			case CommonLib::varType_int16_t: pStream->Write(value.Get<int16_t>()); break;
			case CommonLib::varType_uint16_t: pStream->Write(value.Get<uint16_t>()); break;
			case CommonLib::varType_int32_t: pStream->Write(value.Get<int32_t>()); break;
			case CommonLib::varType_uint32_t: pStream->Write(value.Get<uint32_t>()); break;
			case CommonLib::varType_int64_t: pStream->Write(value.Get<int64_t>()); break;
			case CommonLib::varType_uint64_t: pStream->Write(value.Get<uint64_t>()); break;
			case CommonLib::varType_float: pStream->Write(value.Get<float>()); break;
			case CommonLib::varType_double: pStream->Write(value.Get<double>()); break;
			case CommonLib::varType_astr_t:
				{
					const CommonLib::astr_t& s = value.Get<CommonLib::astr_t>();
					pStream->Write((uint32_t)s.size());
					if (!s.empty())
						pStream->Write((const byte_t*)s.data(), s.size());
					break;
				}
			case CommonLib::varType_CBlobPtr_t:
				{
					const CommonLib::CBlobPtr_t& ptrBlob = value.Get<CommonLib::CBlobPtr_t>();
					uint32_t nSize = ptrBlob.get() ? ptrBlob->Size() : 0;
					pStream->Write(nSize);
					if (nSize)
						pStream->Write(ptrBlob->Buffer(), nSize);
					break;
				}
			default:
				throw CommonLib::CExcBase("WriteVariant: unsupported variant type {0}", (int)value.GetTypeID());
			}
		}

		void ReadVariant(CommonLib::IReadStream* pStream, CommonLib::CVariant& value)
		{
			uint32_t nType = pStream->ReadIntu32();
			switch (nType)
			{
			case CommonLib::varType_CEmptyVariant: value = CommonLib::CVariant(); break;
			case CommonLib::varType_bool: value = pStream->ReadBool(); break;
			case CommonLib::varType_int8_t: value = (int8_t)pStream->ReadByte(); break;
			case CommonLib::varType_uint8_t: value = (uint8_t)pStream->ReadByte(); break;
			case CommonLib::varType_int16_t: value = pStream->Readint16(); break;
			case CommonLib::varType_uint16_t: value = pStream->Readintu16(); break;
			case CommonLib::varType_int32_t: value = pStream->ReadInt32(); break;
			case CommonLib::varType_uint32_t: value = pStream->ReadIntu32(); break;
			case CommonLib::varType_int64_t: value = pStream->ReadInt64(); break;
			case CommonLib::varType_uint64_t: value = pStream->ReadIntu64(); break;
			case CommonLib::varType_float: value = pStream->ReadFloat(); break;
			case CommonLib::varType_double: value = pStream->ReadDouble(); break;
			case CommonLib::varType_astr_t:
				{
					uint32_t nSize = pStream->ReadIntu32();
					CommonLib::astr_t s(nSize, '\0');
					if (nSize)
						pStream->Read((byte_t*)&s[0], nSize);
					value = s;
					break;
				}
			case CommonLib::varType_CBlobPtr_t:
				{
					uint32_t nSize = pStream->ReadIntu32();
					CommonLib::CBlobPtr_t ptrBlob = std::make_shared<CommonLib::Data::CBlob>(nSize);
					if (nSize)
						pStream->Read(ptrBlob->Buffer(), nSize);
					value = ptrBlob;
					break;
				}
			default:
				throw CommonLib::CExcBase("ReadVariant: unsupported variant type {0}", nType);
			}
		}

		//------------------------------------------------------------------------------------------------
		// DBCommon.h

		TCompressorParamsBasePtr SDBContext::MakeTreeParams(uint32_t nMaxLen) const
		{
			TCompressorParamsBasePtr ptrParams(new CompressorParamsBase());
			for (ECompressParams id : { eInnerKey, eInnerValue, eLeafKey, eLeafValue })
			{
				TCompressorParamsPtr p(new CompressorParams());
				if (nMaxLen != 0)
					p->SetIntParam("MaxLen", (int32_t)nMaxLen);
				p->SetIntParam("compressLevel", (int32_t)m_nCompressLevel);
				p->SetIntParam("compressRate", 3);
				ptrParams->AddCompressParams(p, id);
			}
			return ptrParams;
		}

		void CheckName(const std::string& sName, const char* pszWhat)
		{
			if (sName.empty())
				throw CommonLib::CExcBase("{0} name is empty", pszWhat);
			if (sName.size() > MaxNameLength)
				throw CommonLib::CExcBase("{0} name is longer than {1} bytes", pszWhat, MaxNameLength);
		}

		const char* DataTypeName(eDataTypes type)
		{
			switch (type)
			{
			case dtNull: return "null";
			case dtInteger8: return "int8";
			case dtInteger16: return "int16";
			case dtInteger32: return "int32";
			case dtInteger64: return "int64";
			case dtUInteger8: return "uint8";
			case dtUInteger16: return "uint16";
			case dtUInteger32: return "uint32";
			case dtUInteger64: return "uint64";
			case dtOid: return "oid";
			case dtFloat: return "float";
			case dtDouble: return "double";
			case dtString: return "string";
			case dtBlob: return "blob";
			case dtGeometry: return "geometry";
			default: return "unknown";
			}
		}

		const char* OpTypeName(OpType op)
		{
			switch (op)
			{
			case OpEqual: return "=";
			case OpNoEqual: return "!=";
			case OpLess: return "<";
			case OpLessOrEqual: return "<=";
			case OpGreater: return ">";
			case OpGreaterOrEqual: return ">=";
			default: return "?";
			}
		}
	}
}
