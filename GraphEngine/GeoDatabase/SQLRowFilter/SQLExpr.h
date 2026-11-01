#pragma once

#include "SQLValue.h"
#include <memory>
#include <stdexcept>
#include <vector>

namespace GraphEngine {
    namespace GeoDatabase {
        namespace SQLFilter {

            // a parse error: the text and the position (0-based byte offset) in the expression
            class CSQLExprError : public std::runtime_error
            {
            public:
                CSQLExprError(const std::string& sMessage, size_t nPos) : std::runtime_error(sMessage), m_nPos(nPos) {}
                size_t GetPosition() const { return m_nPos; }

            private:
                size_t m_nPos;
            };

            // the values of the fields referenced by an expression, by the index of the field in the expression
            class IFieldValues
            {
            public:
                virtual ~IFieldValues() {}
                virtual CSQLValue GetFieldValue(size_t nField) const = 0;
            };

            class CSQLExpr;
            typedef std::shared_ptr<CSQLExpr> CSQLExprPtr;
            typedef std::vector<CSQLExprPtr>  TExprList;

            class CSQLExpr
            {
            public:
                virtual ~CSQLExpr() {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const = 0;

                eTruth EvaluateTruth(const IFieldValues& values) const { return Evaluate(values).ToTruth(); }
            };

            class CLiteralExpr : public CSQLExpr
            {
            public:
                explicit CLiteralExpr(CSQLValue value) : m_value(std::move(value)) {}
                virtual CSQLValue Evaluate(const IFieldValues&) const { return m_value; }

            private:
                CSQLValue m_value;
            };

            class CFieldExpr : public CSQLExpr
            {
            public:
                explicit CFieldExpr(size_t nField) : m_nField(nField) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const { return values.GetFieldValue(m_nField); }

            private:
                size_t m_nField;
            };

            class CUnaryExpr : public CSQLExpr
            {
            public:
                enum eOp
                {
                    Negate,
                    Plus,
                    Not
                };

                CUnaryExpr(eOp op, CSQLExprPtr ptrExpr) : m_op(op), m_ptrExpr(std::move(ptrExpr)) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                eOp         m_op;
                CSQLExprPtr m_ptrExpr;
            };

            class CBinaryExpr : public CSQLExpr
            {
            public:
                enum eOp
                {
                    Add,
                    Subtract,
                    Multiply,
                    Divide,
                    Modulo,
                    Concat,
                    Equal,
                    NotEqual,
                    Less,
                    LessOrEqual,
                    Greater,
                    GreaterOrEqual,
                    And,
                    Or
                };

                CBinaryExpr(eOp op, CSQLExprPtr ptrLeft, CSQLExprPtr ptrRight) : m_op(op), m_ptrLeft(std::move(ptrLeft)), m_ptrRight(std::move(ptrRight)) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                eOp         m_op;
                CSQLExprPtr m_ptrLeft;
                CSQLExprPtr m_ptrRight;
            };

            // expr IS [NOT] NULL
            class CIsNullExpr : public CSQLExpr
            {
            public:
                CIsNullExpr(CSQLExprPtr ptrExpr, bool bNot) : m_ptrExpr(std::move(ptrExpr)), m_bNot(bNot) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                CSQLExprPtr m_ptrExpr;
                bool        m_bNot;
            };

            // expr [NOT] LIKE pattern [ESCAPE char]: % - any text, _ - one character; ASCII letters ignore case (as SQLite)
            class CLikeExpr : public CSQLExpr
            {
            public:
                CLikeExpr(CSQLExprPtr ptrExpr, CSQLExprPtr ptrPattern, CSQLExprPtr ptrEscape, bool bNot) :
                        m_ptrExpr(std::move(ptrExpr)), m_ptrPattern(std::move(ptrPattern)), m_ptrEscape(std::move(ptrEscape)), m_bNot(bNot) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

                static bool Match(const std::string& sText, const std::string& sPattern, int nEscape);

            private:
                CSQLExprPtr m_ptrExpr;
                CSQLExprPtr m_ptrPattern;
                CSQLExprPtr m_ptrEscape;   // may be empty
                bool        m_bNot;
            };

            // expr [NOT] BETWEEN low AND high
            class CBetweenExpr : public CSQLExpr
            {
            public:
                CBetweenExpr(CSQLExprPtr ptrExpr, CSQLExprPtr ptrLow, CSQLExprPtr ptrHigh, bool bNot) :
                        m_ptrExpr(std::move(ptrExpr)), m_ptrLow(std::move(ptrLow)), m_ptrHigh(std::move(ptrHigh)), m_bNot(bNot) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                CSQLExprPtr m_ptrExpr;
                CSQLExprPtr m_ptrLow;
                CSQLExprPtr m_ptrHigh;
                bool        m_bNot;
            };

            // expr [NOT] IN (list)
            class CInExpr : public CSQLExpr
            {
            public:
                CInExpr(CSQLExprPtr ptrExpr, TExprList list, bool bNot) : m_ptrExpr(std::move(ptrExpr)), m_list(std::move(list)), m_bNot(bNot) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                CSQLExprPtr m_ptrExpr;
                TExprList   m_list;
                bool        m_bNot;
            };

            // CASE [operand] WHEN ... THEN ... [ELSE ...] END
            class CCaseExpr : public CSQLExpr
            {
            public:
                struct SWhen
                {
                    CSQLExprPtr ptrWhen;
                    CSQLExprPtr ptrThen;
                };

                CCaseExpr(CSQLExprPtr ptrOperand, std::vector<SWhen> vecWhens, CSQLExprPtr ptrElse) :
                        m_ptrOperand(std::move(ptrOperand)), m_vecWhens(std::move(vecWhens)), m_ptrElse(std::move(ptrElse)) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                CSQLExprPtr        m_ptrOperand;   // may be empty
                std::vector<SWhen> m_vecWhens;
                CSQLExprPtr        m_ptrElse;      // may be empty
            };

            // CAST(expr AS INTEGER | REAL | TEXT)
            class CCastExpr : public CSQLExpr
            {
            public:
                CCastExpr(CSQLExprPtr ptrExpr, CSQLValue::eType type) : m_ptrExpr(std::move(ptrExpr)), m_type(type) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

            private:
                CSQLExprPtr      m_ptrExpr;
                CSQLValue::eType m_type;
            };

            // the scalar functions: UPPER, LOWER, LENGTH, ABS, ROUND, SUBSTR, TRIM, LTRIM, RTRIM, COALESCE, IFNULL, NULLIF,
            // REPLACE, INSTR, MIN, MAX (of the arguments)
            class CFunctionExpr : public CSQLExpr
            {
            public:
                enum eFunction
                {
                    Upper,
                    Lower,
                    Length,
                    Abs,
                    Round,
                    Substr,
                    Trim,
                    LTrim,
                    RTrim,
                    Coalesce,
                    IfNull,
                    NullIf,
                    Replace,
                    Instr,
                    Min,
                    Max
                };

                CFunctionExpr(eFunction func, TExprList args) : m_func(func), m_args(std::move(args)) {}
                virtual CSQLValue Evaluate(const IFieldValues& values) const;

                // false - unknown name; sError - the wrong number of arguments
                static bool Resolve(const std::string& sName, size_t nArgs, eFunction& func, std::string& sError);

            private:
                eFunction m_func;
                TExprList m_args;
            };
        }
    }
}
