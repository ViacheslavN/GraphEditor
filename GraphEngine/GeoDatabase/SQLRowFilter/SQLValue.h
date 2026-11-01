#pragma once

#include <cstdint>
#include <string>

namespace GraphEngine {
    namespace GeoDatabase {
        namespace SQLFilter {

            // three-valued logic of SQL: a comparison with NULL is Unknown
            enum class eTruth
            {
                False,
                True,
                Unknown
            };

            // value of an expression: NULL, integer, real, text (as SQLite: a boolean is the integer 0 / 1)
            class CSQLValue
            {
            public:
                enum eType
                {
                    Null = 0,
                    Integer,
                    Real,
                    Text
                };

                CSQLValue() {}

                static CSQLValue MakeInteger(int64_t nValue);
                static CSQLValue MakeReal(double dValue);
                static CSQLValue MakeText(std::string sValue);
                static CSQLValue MakeBool(bool bValue) { return MakeInteger(bValue ? 1 : 0); }
                static CSQLValue FromTruth(eTruth truth);

                eType GetType() const { return m_type; }
                bool  IsNull() const { return m_type == Null; }
                bool  IsNumeric() const { return m_type == Integer || m_type == Real; }

                int64_t            GetInteger() const { return m_nInteger; }
                double             GetReal() const { return m_dReal; }
                const std::string& GetText() const { return m_sText; }

                // the number of the value: text - its leading number ('12abc' -> 12, 'abc' -> 0), NULL stays NULL
                CSQLValue   ToNumeric() const;
                double      ToReal() const;
                int64_t     ToInteger() const;
                std::string ToText() const;
                eTruth      ToTruth() const;

                // the whole text is a number (spaces around are allowed)
                static bool ParseNumber(const std::string& sText, CSQLValue& value);

            private:
                eType       m_type = Null;
                int64_t     m_nInteger = 0;
                double      m_dReal = 0.;
                std::string m_sText;
            };

            // compares two values: false - the result is unknown (a NULL), otherwise nResult is <0, 0, >0.
            // numbers by value; texts byte by byte (UTF-8); a number with a text: the text is compared as a number
            // if it is a number, otherwise a number is less than a text
            bool CompareValues(const CSQLValue& a, const CSQLValue& b, int& nResult);

            eTruth TruthNot(eTruth a);
            eTruth TruthAnd(eTruth a, eTruth b);
            eTruth TruthOr(eTruth a, eTruth b);
        }
    }
}
