#pragma once

// Values of the SQL engine: CommonLib::CVariant in a normalized form
//   NULL - the empty variant; integers - int64_t; floating point - double; strings - std::string (UTF-8);
//   blobs - CBlobPtr_t; geometries - IGeoShapePtr.
// The values of the fields are normalized when they are read (an int32 field gives int64_t). There is no
// boolean type: conditions are integers 1 / 0, NULL is unknown (three-valued logic).

#include "../DBCommon.h"
#include <vector>

namespace bptreedb
{
	namespace sql
	{
		typedef CommonLib::CVariant TValue;
		typedef std::vector<CommonLib::CVariant> TRow;

		enum class eValueKind { Null, Integer, Double, String, Blob, Geometry, Other };

		eValueKind ValueKind(const TValue& value);
		inline bool IsNumberKind(eValueKind kind) { return kind == eValueKind::Integer || kind == eValueKind::Double; }

		TValue NormalizeValue(const TValue& value);
		void NormalizeInPlace(TValue& value);

		// the type of a normalized value (dtUnknown for NULL)
		eDataTypes ValueType(const TValue& value);
		bool IsIntegerType(eDataTypes type);

		// numbers (a string that is a number is converted); throws for other values
		int64_t ToInt64(const TValue& value);
		double ToDouble(const TValue& value);
		// a number as text (strings as they are); throws for blobs / geometries
		std::string ToText(const TValue& value);
		// "12", "-1.5e3" -> a number; false if the whole string isn't a number
		bool ParseNumber(const std::string& s, TValue& value);

		// Order of two non NULL values: numbers by value (an integer and a double exactly), strings by bytes,
		// blobs by bytes. A number and a string: the string as a number if it is one, else number < string.
		// Throws for geometries and for values of different kinds that can't be ordered.
		int CompareValues(const TValue& left, const TValue& right);

		// Equality of grouping / DISTINCT / hash joins: NULL equals NULL here; a value of another kind isn't equal.
		bool GroupEqual(const TValue& left, const TValue& right);
		size_t HashValue(const TValue& value);

		struct SValueHash
		{
			size_t operator()(const TValue& value) const { return HashValue(value); }
		};

		struct SValueEqual
		{
			bool operator()(const TValue& left, const TValue& right) const { return GroupEqual(left, right); }
		};

		struct SRowHash
		{
			size_t operator()(const TRow& row) const;
		};

		struct SRowEqual
		{
			bool operator()(const TRow& left, const TRow& right) const;
		};

		// three-valued logic: -1 unknown (NULL), 0 false, 1 true
		int Truth(const TValue& value);
		TValue MakeTruth(int nTruth);

		// arithmetic: integers stay integers (a double on overflow), a division by zero is NULL, NULL gives NULL
		enum class eArithOp { Add, Subtract, Multiply, Divide, Modulo };
		TValue Arithmetic(eArithOp op, const TValue& left, const TValue& right);
		TValue Negate(const TValue& value);

		// LIKE: % any sequence, _ one character (UTF-8), ASCII letters case-insensitive; nEscape < 0 - no escape
		bool LikeMatch(const std::string& sText, const std::string& sPattern, int nEscape);

		// a value as SQL text for messages
		std::string ValueToSQL(const TValue& value);
	}
}
