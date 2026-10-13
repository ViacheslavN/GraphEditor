#pragma once

// Values of fields in CommonLib::CVariant: conversion to the type of a field, comparison, serialization.

#include "DBCommon.h"

namespace bptreedb
{
	namespace db
	{
		bool IsNumberType(eDataTypes type);

		// a CGeoShape from the shape blob
		CommonLib::IGeoShapePtr MakeShape(const byte_t* pData, uint32_t nSize);
		// the bounding box of a geometry value; false for NULL / an empty shape
		bool GetShapeBBox(const CommonLib::CVariant& value, CommonLib::bbox& bb);
		bool IsFixedSizeType(eDataTypes type);
		uint32_t FixedTypeSize(eDataTypes type);      // 0 for a string / blob

		// The value converted to the type of a field (dtOid - int64_t):
		//   numbers - any number variant (and bool), integers with a range check, a double -> integer only if
		//   it is an integer; strings - std::string or std::wstring (converted to UTF-8), the length checked
		//   if nMaxLen != 0; blobs - CBlobPtr_t or std::string. The empty variant stays empty (NULL).
		// Throws if the value can't be converted.
		CommonLib::CVariant ToFieldValue(const CommonLib::CVariant& value, eDataTypes type, uint32_t nMaxLen);

		// A number variant as T (range checked); throws for NULL / not a number.
		template <class T>
		T VariantToNumber(const CommonLib::CVariant& value);

		// Order of two non NULL values of a field type (already converted): <0, 0, >0. Numbers by TOrderedComp
		// (NaN is a value), strings by bytes. Blobs can't be compared.
		int CompareFieldValues(const CommonLib::CVariant& left, const CommonLib::CVariant& right, eDataTypes type);

		// Does "value op key" hold (both non NULL, of the field type).
		bool MatchOp(const CommonLib::CVariant& value, const CommonLib::CVariant& key, OpType op, eDataTypes type);

		// Size of a value in bytes (0 for NULL).
		int32_t ValueBytes(const CommonLib::CVariant& value);

		// Default value of a field in the schema.
		void WriteVariant(CommonLib::IWriteStream* pStream, const CommonLib::CVariant& value);
		void ReadVariant(CommonLib::IReadStream* pStream, CommonLib::CVariant& value);
	}
}
