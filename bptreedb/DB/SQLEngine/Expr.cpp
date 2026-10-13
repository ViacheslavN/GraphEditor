#include "Expr.h"
#include "Operators.h"
#include "../Values.h"
#include <cmath>
#include <map>
#include <sstream>

namespace bptreedb
{
	namespace sql
	{
		namespace
		{
			std::string ChildKey(const CExprPtr& ptrExpr)
			{
				return ptrExpr.get() ? ptrExpr->Key() : std::string("_");
			}

			int Compare3(const TValue& left, const TValue& right, int& nCmp)
			{
				if (left.IsNull() || right.IsNull())
					return -1;
				nCmp = CompareValues(left, right);
				return 1;
			}

			// the scalar functions -------------------------------------------------------------

			size_t Utf8Length(const std::string& s)
			{
				size_t n = 0;
				for (unsigned char c : s)
					if ((c & 0xC0) != 0x80)
						++n;
				return n;
			}

			// the byte offset of the code point nIndex (0-based), s.size() past the end
			size_t Utf8Offset(const std::string& s, size_t nIndex)
			{
				size_t nCount = 0;
				for (size_t i = 0; i < s.size(); ++i)
				{
					if (((unsigned char)s[i] & 0xC0) != 0x80)
					{
						if (nCount == nIndex)
							return i;
						++nCount;
					}
				}
				return s.size();
			}

			TValue FuncAbs(const std::vector<TValue>& a)
			{
				TValue v = a[0];
				NormalizeInPlace(v);
				if (v.IsType<int64_t>())
				{
					int64_t n = v.Get<int64_t>();
					if (n == INT64_MIN)
						return TValue(-(double)n);
					return TValue(n < 0 ? -n : n);
				}
				return TValue(std::fabs(ToDouble(v)));
			}

			TValue FuncRound(const std::vector<TValue>& a)
			{
				double d = ToDouble(a[0]);
				int64_t nDigits = a.size() > 1 ? ToInt64(a[1]) : 0;
				double dScale = std::pow(10.0, (double)nDigits);
				double r = std::round(d * dScale) / dScale;
				return TValue(r);
			}

			template<double (*F)(double)>
			TValue IntOrDouble(const std::vector<TValue>& a)
			{
				TValue v = a[0];
				if (ValueKind(v) == eValueKind::Integer)
					return v;
				return TValue(F(ToDouble(v)));
			}

			double Floor(double d) { return std::floor(d); }
			double Ceil(double d) { return std::ceil(d); }

			template<double (*F)(double)>
			TValue Math1(const std::vector<TValue>& a)
			{
				return TValue(F(ToDouble(a[0])));
			}

			double Sqrt(double d) { return std::sqrt(d); }
			double Exp(double d) { return std::exp(d); }
			double Ln(double d) { return std::log(d); }
			double Log10(double d) { return std::log10(d); }
			double Sin(double d) { return std::sin(d); }
			double Cos(double d) { return std::cos(d); }
			double Tan(double d) { return std::tan(d); }
			double Atan(double d) { return std::atan(d); }

			TValue FuncPower(const std::vector<TValue>& a) { return TValue(std::pow(ToDouble(a[0]), ToDouble(a[1]))); }
			TValue FuncAtan2(const std::vector<TValue>& a) { return TValue(std::atan2(ToDouble(a[0]), ToDouble(a[1]))); }
			TValue FuncPi(const std::vector<TValue>&) { return TValue(3.14159265358979323846); }
			TValue FuncMod(const std::vector<TValue>& a) { return Arithmetic(eArithOp::Modulo, a[0], a[1]); }

			TValue FuncSign(const std::vector<TValue>& a)
			{
				int nCmp = CompareValues(a[0], TValue((int64_t)0));
				return TValue((int64_t)nCmp);
			}

			TValue FuncLength(const std::vector<TValue>& a)
			{
				switch (ValueKind(a[0]))
				{
				case eValueKind::Blob:
					return TValue((int64_t)a[0].Get<CommonLib::CBlobPtr_t>()->Size());
				case eValueKind::Geometry:
					return TValue((int64_t)a[0].Get<CommonLib::IGeoShapePtr>()->Size());
				default:
					return TValue((int64_t)Utf8Length(ToText(a[0])));
				}
			}

			TValue FuncUpper(const std::vector<TValue>& a)
			{
				std::string s = ToText(a[0]);
				for (char& c : s)
					if (c >= 'a' && c <= 'z')
						c = (char)(c - 'a' + 'A');
				return TValue(s);
			}

			TValue FuncLower(const std::vector<TValue>& a)
			{
				std::string s = ToText(a[0]);
				for (char& c : s)
					if (c >= 'A' && c <= 'Z')
						c = (char)(c - 'A' + 'a');
				return TValue(s);
			}

			TValue Trim(const std::vector<TValue>& a, bool bLeft, bool bRight)
			{
				std::string s = ToText(a[0]);
				std::string sChars = a.size() > 1 ? ToText(a[1]) : std::string(" ");
				size_t nBegin = 0, nEnd = s.size();
				if (bLeft)
					while (nBegin < nEnd && sChars.find(s[nBegin]) != std::string::npos)
						++nBegin;
				if (bRight)
					while (nEnd > nBegin && sChars.find(s[nEnd - 1]) != std::string::npos)
						--nEnd;
				return TValue(s.substr(nBegin, nEnd - nBegin));
			}

			TValue FuncTrim(const std::vector<TValue>& a) { return Trim(a, true, true); }
			TValue FuncLTrim(const std::vector<TValue>& a) { return Trim(a, true, false); }
			TValue FuncRTrim(const std::vector<TValue>& a) { return Trim(a, false, true); }

			// SUBSTR(s, start [, len]): 1-based code points; a negative start counts from the end
			TValue FuncSubstr(const std::vector<TValue>& a)
			{
				std::string s = ToText(a[0]);
				int64_t nLen = (int64_t)Utf8Length(s);
				int64_t nStart = ToInt64(a[1]);
				int64_t nCount = a.size() > 2 ? ToInt64(a[2]) : nLen + 1;
				int64_t nFirst = nStart > 0 ? nStart - 1 : (nStart < 0 ? nLen + nStart : -1);
				int64_t nLast = nFirst + nCount;          // exclusive
				if (nStart == 0)
					nLast = nCount - 1;                   // as SQLite: position 0 is before the first character
				nFirst = (std::max<int64_t>)(0, nFirst);
				nLast = (std::min<int64_t>)(nLen, nLast);
				if (nLast <= nFirst)
					return TValue(std::string());
				size_t nB = Utf8Offset(s, (size_t)nFirst), nE = Utf8Offset(s, (size_t)nLast);
				return TValue(s.substr(nB, nE - nB));
			}

			TValue FuncReplace(const std::vector<TValue>& a)
			{
				std::string s = ToText(a[0]), sFrom = ToText(a[1]), sTo = ToText(a[2]);
				if (sFrom.empty())
					return TValue(s);
				std::string sResult;
				size_t nPos = 0;
				for (;;)
				{
					size_t nFound = s.find(sFrom, nPos);
					if (nFound == std::string::npos)
						break;
					sResult.append(s, nPos, nFound - nPos);
					sResult += sTo;
					nPos = nFound + sFrom.size();
				}
				sResult.append(s, nPos, std::string::npos);
				return TValue(sResult);
			}

			TValue FuncInstr(const std::vector<TValue>& a)
			{
				std::string s = ToText(a[0]), sSub = ToText(a[1]);
				size_t nFound = s.find(sSub);
				if (nFound == std::string::npos)
					return TValue((int64_t)0);
				return TValue((int64_t)Utf8Length(s.substr(0, nFound)) + 1);
			}

			TValue FuncConcat(const std::vector<TValue>& a)
			{
				std::string s;
				for (const TValue& v : a)
					if (!v.IsNull())
						s += ToText(v);
				return TValue(s);
			}

			TValue FuncLeast(const std::vector<TValue>& a)
			{
				TValue best = a[0];
				for (size_t i = 1; i < a.size(); ++i)
					if (CompareValues(a[i], best) < 0)
						best = a[i];
				return best;
			}

			TValue FuncGreatest(const std::vector<TValue>& a)
			{
				TValue best = a[0];
				for (size_t i = 1; i < a.size(); ++i)
					if (CompareValues(a[i], best) > 0)
						best = a[i];
				return best;
			}

			TValue FuncCoalesce(const std::vector<TValue>& a)
			{
				for (const TValue& v : a)
					if (!v.IsNull())
						return v;
				return TValue();
			}

			TValue FuncNullIf(const std::vector<TValue>& a)
			{
				if (!a[0].IsNull() && !a[1].IsNull() && CompareValues(a[0], a[1]) == 0)
					return TValue();
				return a[0];
			}

			TValue FuncTypeOf(const std::vector<TValue>& a)
			{
				switch (ValueKind(a[0]))
				{
				case eValueKind::Null: return TValue(std::string("null"));
				case eValueKind::Integer: return TValue(std::string("integer"));
				case eValueKind::Double: return TValue(std::string("real"));
				case eValueKind::String: return TValue(std::string("text"));
				case eValueKind::Blob: return TValue(std::string("blob"));
				case eValueKind::Geometry: return TValue(std::string("geometry"));
				default: return TValue(std::string("other"));
				}
			}

			template<int N>
			TValue FuncBBox(const std::vector<TValue>& a)
			{
				CommonLib::bbox bb;
				if (!ValueBBox(a[0], bb))
					return TValue();
				switch (N)
				{
				case 0: return TValue(bb.xMin);
				case 1: return TValue(bb.yMin);
				case 2: return TValue(bb.xMax);
				default: return TValue(bb.yMax);
				}
			}

			TValue FuncNumPoints(const std::vector<TValue>& a)
			{
				const CommonLib::IGeoShapePtr* pShape = a[0].GetPtr<CommonLib::IGeoShapePtr>();
				if (pShape == nullptr)
					throw CommonLib::CExcBase("ST_NUMPOINTS: not a geometry");
				return TValue((int64_t)(pShape->get() ? (*pShape)->GetPointCnt() : 0));
			}

			struct SFunctionDef
			{
				size_t m_nMinArgs;
				size_t m_nMaxArgs;
				TScalarFunction m_func;
				bool m_bNullInNullOut;
			};

			const std::map<std::string, SFunctionDef>& Functions()
			{
				static const std::map<std::string, SFunctionDef> functions = {
					{ "ABS", { 1, 1, FuncAbs, true } },
					{ "ROUND", { 1, 2, FuncRound, true } },
					{ "FLOOR", { 1, 1, IntOrDouble<Floor>, true } },
					{ "CEIL", { 1, 1, IntOrDouble<Ceil>, true } },
					{ "CEILING", { 1, 1, IntOrDouble<Ceil>, true } },
					{ "SQRT", { 1, 1, Math1<Sqrt>, true } },
					{ "EXP", { 1, 1, Math1<Exp>, true } },
					{ "LN", { 1, 1, Math1<Ln>, true } },
					{ "LOG", { 1, 1, Math1<Ln>, true } },
					{ "LOG10", { 1, 1, Math1<Log10>, true } },
					{ "SIN", { 1, 1, Math1<Sin>, true } },
					{ "COS", { 1, 1, Math1<Cos>, true } },
					{ "TAN", { 1, 1, Math1<Tan>, true } },
					{ "ATAN", { 1, 1, Math1<Atan>, true } },
					{ "ATAN2", { 2, 2, FuncAtan2, true } },
					{ "POWER", { 2, 2, FuncPower, true } },
					{ "POW", { 2, 2, FuncPower, true } },
					{ "PI", { 0, 0, FuncPi, true } },
					{ "MOD", { 2, 2, FuncMod, true } },
					{ "SIGN", { 1, 1, FuncSign, true } },
					{ "LENGTH", { 1, 1, FuncLength, true } },
					{ "UPPER", { 1, 1, FuncUpper, true } },
					{ "LOWER", { 1, 1, FuncLower, true } },
					{ "TRIM", { 1, 2, FuncTrim, true } },
					{ "LTRIM", { 1, 2, FuncLTrim, true } },
					{ "RTRIM", { 1, 2, FuncRTrim, true } },
					{ "SUBSTR", { 2, 3, FuncSubstr, true } },
					{ "SUBSTRING", { 2, 3, FuncSubstr, true } },
					{ "REPLACE", { 3, 3, FuncReplace, true } },
					{ "INSTR", { 2, 2, FuncInstr, true } },
					{ "CONCAT", { 1, 1000, FuncConcat, false } },
					{ "LEAST", { 2, 1000, FuncLeast, true } },
					{ "GREATEST", { 2, 1000, FuncGreatest, true } },
					{ "COALESCE", { 1, 1000, FuncCoalesce, false } },
					{ "IFNULL", { 2, 2, FuncCoalesce, false } },
					{ "NULLIF", { 2, 2, FuncNullIf, false } },
					{ "TYPEOF", { 1, 1, FuncTypeOf, false } },
					{ "ST_XMIN", { 1, 1, FuncBBox<0>, true } },
					{ "ST_YMIN", { 1, 1, FuncBBox<1>, true } },
					{ "ST_XMAX", { 1, 1, FuncBBox<2>, true } },
					{ "ST_YMAX", { 1, 1, FuncBBox<3>, true } },
					{ "ST_NUMPOINTS", { 1, 1, FuncNumPoints, true } }
				};
				return functions;
			}
		}

		//------------------------------------------------------------------------------------------

		CConstExpr::CConstExpr(const TValue& value) : CExpr(eBoundExpr::Const), m_value(value)
		{
			m_type = ValueType(value);
		}

		std::string CConstExpr::Key() const
		{
			const char* pKind = "";
			switch (ValueKind(m_value))
			{
			case eValueKind::Integer: pKind = "i"; break;
			case eValueKind::Double: pKind = "d"; break;
			case eValueKind::String: pKind = "s"; break;
			default: break;
			}
			return std::string("const:") + pKind + ValueToSQL(m_value);
		}

		TValue CParamExpr::Eval(const SEvalContext& ctx) const
		{
			if (ctx.m_pExec == nullptr || m_nIndex >= ctx.m_pExec->m_params.size())
				throw CommonLib::CExcBase("parameter {0} isn't bound", m_nIndex + 1);
			return ctx.m_pExec->m_params[m_nIndex];
		}

		std::string CColumnExpr::Key() const
		{
			if (m_nSource < 0)
				return "row:" + std::to_string(m_nIndex);
			std::ostringstream stream;
			stream << "col:" << m_pScope << ":" << m_nSource << "." << m_nSlot;
			return stream.str();
		}

		TValue COuterColumnExpr::Eval(const SEvalContext& ctx) const
		{
			const SEvalContext* pCtx = &ctx;
			for (uint32_t i = 0; i < m_nDepth; ++i)
			{
				pCtx = pCtx->m_pOuter;
				if (pCtx == nullptr)
					throw CommonLib::CExcBase("no row of the outer query");
			}
			return (*pCtx->m_pRow)[m_ptrTarget->m_nIndex];
		}

		TValue CUnaryExpr::Eval(const SEvalContext& ctx) const
		{
			TValue value = m_ptrOperand->Eval(ctx);
			switch (m_op)
			{
			case eUnaryOp::Minus:
				return Negate(value);
			case eUnaryOp::Plus:
				if (!value.IsNull() && !IsNumberKind(ValueKind(value)))
					return TValue(ToDouble(value));
				return value;
			case eUnaryOp::Not:
				{
					int n = Truth(value);
					return MakeTruth(n < 0 ? -1 : 1 - n);
				}
			}
			return TValue();
		}

		std::string CUnaryExpr::Key() const
		{
			return std::string("u") + sql::ToString(m_op) + "(" + ChildKey(m_ptrOperand) + ")";
		}

		bool CBinaryExpr::IsComparison() const
		{
			switch (m_op)
			{
			case eBinaryOp::Equal: case eBinaryOp::NotEqual: case eBinaryOp::Less:
			case eBinaryOp::LessOrEqual: case eBinaryOp::Greater: case eBinaryOp::GreaterOrEqual:
				return true;
			default:
				return false;
			}
		}

		TValue CBinaryExpr::Eval(const SEvalContext& ctx) const
		{
			switch (m_op)
			{
			case eBinaryOp::And:
				{
					int l = Truth(m_ptrLeft->Eval(ctx));
					if (l == 0)
						return MakeTruth(0);
					int r = Truth(m_ptrRight->Eval(ctx));
					if (r == 0)
						return MakeTruth(0);
					return MakeTruth(l < 0 || r < 0 ? -1 : 1);
				}
			case eBinaryOp::Or:
				{
					int l = Truth(m_ptrLeft->Eval(ctx));
					if (l == 1)
						return MakeTruth(1);
					int r = Truth(m_ptrRight->Eval(ctx));
					if (r == 1)
						return MakeTruth(1);
					return MakeTruth(l < 0 || r < 0 ? -1 : 0);
				}
			default:
				break;
			}

			TValue left = m_ptrLeft->Eval(ctx);
			TValue right = m_ptrRight->Eval(ctx);
			int nCmp = 0;
			switch (m_op)
			{
			case eBinaryOp::Equal:
				return MakeTruth(Compare3(left, right, nCmp) < 0 ? -1 : (nCmp == 0 ? 1 : 0));
			case eBinaryOp::NotEqual:
				return MakeTruth(Compare3(left, right, nCmp) < 0 ? -1 : (nCmp != 0 ? 1 : 0));
			case eBinaryOp::Less:
				return MakeTruth(Compare3(left, right, nCmp) < 0 ? -1 : (nCmp < 0 ? 1 : 0));
			case eBinaryOp::LessOrEqual:
				return MakeTruth(Compare3(left, right, nCmp) < 0 ? -1 : (nCmp <= 0 ? 1 : 0));
			case eBinaryOp::Greater:
				return MakeTruth(Compare3(left, right, nCmp) < 0 ? -1 : (nCmp > 0 ? 1 : 0));
			case eBinaryOp::GreaterOrEqual:
				return MakeTruth(Compare3(left, right, nCmp) < 0 ? -1 : (nCmp >= 0 ? 1 : 0));
			case eBinaryOp::Add: return Arithmetic(eArithOp::Add, left, right);
			case eBinaryOp::Subtract: return Arithmetic(eArithOp::Subtract, left, right);
			case eBinaryOp::Multiply: return Arithmetic(eArithOp::Multiply, left, right);
			case eBinaryOp::Divide: return Arithmetic(eArithOp::Divide, left, right);
			case eBinaryOp::Modulo: return Arithmetic(eArithOp::Modulo, left, right);
			case eBinaryOp::Concat:
				if (left.IsNull() || right.IsNull())
					return TValue();
				return TValue(ToText(left) + ToText(right));
			default:
				return TValue();
			}
		}

		std::string CBinaryExpr::Key() const
		{
			return std::string("b") + sql::ToString(m_op) + "(" + ChildKey(m_ptrLeft) + "," + ChildKey(m_ptrRight) + ")";
		}

		TValue CIsNullExpr::Eval(const SEvalContext& ctx) const
		{
			bool bNull = m_ptrOperand->Eval(ctx).IsNull();
			return MakeTruth(bNull != m_bNot ? 1 : 0);
		}

		std::string CIsNullExpr::Key() const
		{
			return std::string(m_bNot ? "notnull(" : "isnull(") + ChildKey(m_ptrOperand) + ")";
		}

		TValue CBetweenExpr::Eval(const SEvalContext& ctx) const
		{
			TValue value = m_ptrOperand->Eval(ctx);
			TValue low = m_ptrLow->Eval(ctx);
			TValue high = m_ptrHigh->Eval(ctx);
			int nCmp = 0;
			int nLow = Compare3(value, low, nCmp) < 0 ? -1 : (nCmp >= 0 ? 1 : 0);
			int nHigh = Compare3(value, high, nCmp) < 0 ? -1 : (nCmp <= 0 ? 1 : 0);
			int nResult = (nLow == 0 || nHigh == 0) ? 0 : ((nLow < 0 || nHigh < 0) ? -1 : 1);
			if (m_bNot && nResult >= 0)
				nResult = 1 - nResult;
			return MakeTruth(nResult);
		}

		void CBetweenExpr::Children(std::vector<CExprPtr*>& children)
		{
			children.push_back(&m_ptrOperand);
			children.push_back(&m_ptrLow);
			children.push_back(&m_ptrHigh);
		}

		std::string CBetweenExpr::Key() const
		{
			return std::string(m_bNot ? "notbetween(" : "between(") + ChildKey(m_ptrOperand) + "," + ChildKey(m_ptrLow) + "," + ChildKey(m_ptrHigh) + ")";
		}

		TValue CInListExpr::Eval(const SEvalContext& ctx) const
		{
			TValue value = m_ptrOperand->Eval(ctx);
			if (value.IsNull())
				return TValue();
			bool bNull = false;
			for (const CExprPtr& ptrItem : m_list)
			{
				TValue item = ptrItem->Eval(ctx);
				if (item.IsNull())
				{
					bNull = true;
					continue;
				}
				if (CompareValues(value, item) == 0)
					return MakeTruth(m_bNot ? 0 : 1);
			}
			if (bNull)
				return TValue();
			return MakeTruth(m_bNot ? 1 : 0);
		}

		void CInListExpr::Children(std::vector<CExprPtr*>& children)
		{
			children.push_back(&m_ptrOperand);
			for (CExprPtr& ptrItem : m_list)
				children.push_back(&ptrItem);
		}

		std::string CInListExpr::Key() const
		{
			std::string s = std::string(m_bNot ? "notin(" : "in(") + ChildKey(m_ptrOperand);
			for (const CExprPtr& ptrItem : m_list)
				s += "," + ChildKey(ptrItem);
			return s + ")";
		}

		//------------------------------------------------------------------------------------------
		// subqueries

		std::string CSubqueryExprBase::Key() const
		{
			std::ostringstream stream;
			stream << "subquery:" << m_ptrPlan.get();
			return stream.str();
		}

		bool CSubqueryExprBase::Cached(const SEvalContext& ctx) const
		{
			return !m_ptrPlan->m_bCorrelated && m_nCachedExecution == ctx.m_pExec->m_nExecution;
		}

		void CSubqueryExprBase::SetCached(const SEvalContext& ctx) const
		{
			m_nCachedExecution = ctx.m_pExec->m_nExecution;
		}

		TValue CScalarSubqueryExpr::Eval(const SEvalContext& ctx) const
		{
			if (Cached(ctx))
				return m_cached;
			COperator& root = *m_ptrPlan->m_ptrRoot;
			root.Open(*ctx.m_pExec, &ctx);
			TValue value;
			if (root.Next())
			{
				value = root.Row()[0];
				if (root.Next())
				{
					root.Close();
					throw CommonLib::CExcBase("a scalar subquery returned more than one row");
				}
			}
			root.Close();
			m_cached = value;
			SetCached(ctx);
			return value;
		}

		TValue CExistsExpr::Eval(const SEvalContext& ctx) const
		{
			if (Cached(ctx))
				return MakeTruth(m_bCached ? 1 : 0);
			COperator& root = *m_ptrPlan->m_ptrRoot;
			root.Open(*ctx.m_pExec, &ctx);
			bool bExists = root.Next();
			root.Close();
			m_bCached = bExists;
			SetCached(ctx);
			return MakeTruth(bExists ? 1 : 0);
		}

		void CInSubqueryExpr::Load(const SEvalContext& ctx) const
		{
			m_values.clear();
			m_bHasNull = false;
			COperator& root = *m_ptrPlan->m_ptrRoot;
			root.Open(*ctx.m_pExec, &ctx);
			while (root.Next())
			{
				const TValue& value = root.Row()[0];
				if (value.IsNull())
					m_bHasNull = true;
				else
					m_values.insert(value);
			}
			root.Close();
		}

		TValue CInSubqueryExpr::Eval(const SEvalContext& ctx) const
		{
			TValue value = m_ptrOperand->Eval(ctx);
			if (!Cached(ctx))
			{
				Load(ctx);
				SetCached(ctx);
			}
			if (value.IsNull())
				return TValue();

			bool bFound = m_values.count(value) != 0;
			if (!bFound && ValueKind(value) == eValueKind::String)
			{
				// a string compared with numbers: as a number
				TValue number;
				if (ParseNumber(value.Get<CommonLib::astr_t>(), number))
					bFound = m_values.count(number) != 0;
			}
			if (!bFound && IsNumberKind(ValueKind(value)))
			{
				for (const TValue& item : m_values)
					if (ValueKind(item) == eValueKind::String && CompareValues(value, item) == 0)
					{
						bFound = true;
						break;
					}
			}
			if (bFound)
				return MakeTruth(m_bNot ? 0 : 1);
			if (m_bHasNull)
				return TValue();
			return MakeTruth(m_bNot ? 1 : 0);
		}

		std::string CInSubqueryExpr::Key() const
		{
			return std::string(m_bNot ? "notin(" : "in(") + ChildKey(m_ptrOperand) + "," + CSubqueryExprBase::Key() + ")";
		}

		//------------------------------------------------------------------------------------------

		TValue CLikeExpr::Eval(const SEvalContext& ctx) const
		{
			TValue value = m_ptrOperand->Eval(ctx);
			TValue pattern = m_ptrPattern->Eval(ctx);
			if (value.IsNull() || pattern.IsNull())
				return TValue();
			int nEscape = -1;
			if (m_ptrEscape.get())
			{
				TValue escape = m_ptrEscape->Eval(ctx);
				if (escape.IsNull())
					return TValue();
				std::string sEscape = ToText(escape);
				if (sEscape.size() != 1)
					throw CommonLib::CExcBase("ESCAPE has to be one character");
				nEscape = (unsigned char)sEscape[0];
			}
			bool bMatch = LikeMatch(ToText(value), ToText(pattern), nEscape);
			return MakeTruth(bMatch != m_bNot ? 1 : 0);
		}

		void CLikeExpr::Children(std::vector<CExprPtr*>& children)
		{
			children.push_back(&m_ptrOperand);
			children.push_back(&m_ptrPattern);
			if (m_ptrEscape.get())
				children.push_back(&m_ptrEscape);
		}

		std::string CLikeExpr::Key() const
		{
			return std::string(m_bNot ? "notlike(" : "like(") + ChildKey(m_ptrOperand) + "," + ChildKey(m_ptrPattern) + "," + ChildKey(m_ptrEscape) + ")";
		}

		TValue CCaseExpr::Eval(const SEvalContext& ctx) const
		{
			if (m_ptrOperand.get())
			{
				TValue value = m_ptrOperand->Eval(ctx);
				for (const auto& when : m_whens)
				{
					TValue whenValue = when.first->Eval(ctx);
					int nCmp = 0;
					if (Compare3(value, whenValue, nCmp) > 0 && nCmp == 0)
						return when.second->Eval(ctx);
				}
			}
			else
			{
				for (const auto& when : m_whens)
					if (Truth(when.first->Eval(ctx)) == 1)
						return when.second->Eval(ctx);
			}
			return m_ptrElse.get() ? m_ptrElse->Eval(ctx) : TValue();
		}

		void CCaseExpr::Children(std::vector<CExprPtr*>& children)
		{
			if (m_ptrOperand.get())
				children.push_back(&m_ptrOperand);
			for (auto& when : m_whens)
			{
				children.push_back(&when.first);
				children.push_back(&when.second);
			}
			if (m_ptrElse.get())
				children.push_back(&m_ptrElse);
		}

		std::string CCaseExpr::Key() const
		{
			std::string s = "case(" + ChildKey(m_ptrOperand);
			for (const auto& when : m_whens)
				s += ";" + ChildKey(when.first) + ":" + ChildKey(when.second);
			return s + ";" + ChildKey(m_ptrElse) + ")";
		}

		TValue CCastExpr::Eval(const SEvalContext& ctx) const
		{
			return CastValue(m_ptrOperand->Eval(ctx), m_type);
		}

		std::string CCastExpr::Key() const
		{
			return "cast" + std::to_string((int)m_type) + "(" + ChildKey(m_ptrOperand) + ")";
		}

		TValue CFunctionExpr::Eval(const SEvalContext& ctx) const
		{
			std::vector<TValue> args;
			args.reserve(m_args.size());
			for (const CExprPtr& ptrArg : m_args)
			{
				args.push_back(ptrArg->Eval(ctx));
				if (m_bNullInNullOut && args.back().IsNull())
					return TValue();
			}
			try
			{
				return m_func(args);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("function {0}", m_sName, excSrc);
				throw;
			}
		}

		void CFunctionExpr::Children(std::vector<CExprPtr*>& children)
		{
			for (CExprPtr& ptrArg : m_args)
				children.push_back(&ptrArg);
		}

		std::string CFunctionExpr::Key() const
		{
			std::string s = m_sName + "(";
			for (size_t i = 0; i < m_args.size(); ++i)
				s += (i ? "," : "") + ChildKey(m_args[i]);
			return s + ")";
		}

		TValue CAggregateExpr::Eval(const SEvalContext&) const
		{
			throw CommonLib::CExcBase("an aggregate is evaluated outside of GROUP BY");
		}

		void CAggregateExpr::Children(std::vector<CExprPtr*>& children)
		{
			if (m_ptrArg.get())
				children.push_back(&m_ptrArg);
		}

		std::string CAggregateExpr::Key() const
		{
			static const char* const names[] = { "COUNT", "SUM", "AVG", "MIN", "MAX" };
			return std::string("agg:") + names[(int)m_agg] + "(" + (m_bDistinct ? "DISTINCT " : "") + ChildKey(m_ptrArg) + ")";
		}

		//------------------------------------------------------------------------------------------
		// spatial

		bool ValueBBox(const TValue& value, CommonLib::bbox& bb)
		{
			return db::GetShapeBBox(value, bb);
		}

		bool MatchBBox(const CommonLib::bbox& bb, const CommonLib::bbox& extent, eSpatialQueryMode mode)
		{
			switch (mode)
			{
			case sqmIntersect:
				return !(bb.xMax < extent.xMin || bb.xMin > extent.xMax || bb.yMax < extent.yMin || bb.yMin > extent.yMax);
			case sqmOver:        // bb contains the extent
				return bb.xMin <= extent.xMin && bb.xMax >= extent.xMax && bb.yMin <= extent.yMin && bb.yMax >= extent.yMax;
			case sqmInside:      // bb inside the extent
				return bb.xMin >= extent.xMin && bb.xMax <= extent.xMax && bb.yMin >= extent.yMin && bb.yMax <= extent.yMax;
			default:
				throw CommonLib::CExcBase("the spatial mode {0} isn't supported", (int)mode);
			}
		}

		bool CSpatialExpr::Extent(const SEvalContext& ctx, CommonLib::bbox& extent) const
		{
			if (m_ptrOther.get())
				return ValueBBox(m_ptrOther->Eval(ctx), extent);

			double d[4];
			for (size_t i = 0; i < 4; ++i)
			{
				TValue value = m_envelope[i]->Eval(ctx);
				if (value.IsNull())
					return false;
				d[i] = ToDouble(value);
			}
			extent.xMin = (std::min)(d[0], d[2]);
			extent.yMin = (std::min)(d[1], d[3]);
			extent.xMax = (std::max)(d[0], d[2]);
			extent.yMax = (std::max)(d[1], d[3]);
			return true;
		}

		TValue CSpatialExpr::Eval(const SEvalContext& ctx) const
		{
			TValue geom = m_ptrGeom->Eval(ctx);
			if (geom.IsNull())
				return TValue();
			if (ValueKind(geom) != eValueKind::Geometry)
				throw CommonLib::CExcBase("a spatial predicate of a value that isn't a geometry");
			CommonLib::bbox bb, extent;
			if (!ValueBBox(geom, bb) || !Extent(ctx, extent))
				return MakeTruth(0);
			return MakeTruth(MatchBBox(bb, extent, m_mode) ? 1 : 0);
		}

		void CSpatialExpr::Children(std::vector<CExprPtr*>& children)
		{
			children.push_back(&m_ptrGeom);
			if (m_ptrOther.get())
				children.push_back(&m_ptrOther);
			for (CExprPtr& ptrExpr : m_envelope)
				children.push_back(&ptrExpr);
		}

		std::string CSpatialExpr::Key() const
		{
			std::string s = "spatial" + std::to_string((int)m_mode) + "(" + ChildKey(m_ptrGeom) + "," + ChildKey(m_ptrOther);
			for (const CExprPtr& ptrExpr : m_envelope)
				s += "," + ChildKey(ptrExpr);
			return s + ")";
		}

		//------------------------------------------------------------------------------------------

		bool FindScalarFunction(const std::string& sName, size_t nArgs, TScalarFunction& func, bool& bNullInNullOut)
		{
			auto it = Functions().find(sName);
			if (it == Functions().end())
				return false;
			if (nArgs < it->second.m_nMinArgs || nArgs > it->second.m_nMaxArgs)
			{
				if (it->second.m_nMinArgs == it->second.m_nMaxArgs)
					throw CommonLib::CExcBase("{0} takes {1} argument(s), not {2}", sName, it->second.m_nMinArgs, nArgs);
				throw CommonLib::CExcBase("{0} takes {1} to {2} arguments, not {3}", sName, it->second.m_nMinArgs, it->second.m_nMaxArgs, nArgs);
			}
			func = it->second.m_func;
			bNullInNullOut = it->second.m_bNullInNullOut;
			return true;
		}

		eDataTypes FunctionType(const std::string& sName, const std::vector<CExprPtr>& args)
		{
			if (sName == "LENGTH" || sName == "INSTR" || sName == "SIGN" || sName == "ST_NUMPOINTS")
				return dtInteger64;
			if (sName == "UPPER" || sName == "LOWER" || sName == "TRIM" || sName == "LTRIM" || sName == "RTRIM" || sName == "SUBSTR" ||
				sName == "SUBSTRING" || sName == "REPLACE" || sName == "CONCAT" || sName == "TYPEOF")
				return dtString;
			if (sName == "LEAST" || sName == "GREATEST" || sName == "COALESCE" || sName == "IFNULL")
			{
				eDataTypes type = dtUnknown;
				for (size_t i = 0; i < args.size(); ++i)
				{
					eDataTypes argType = IsIntegerType(args[i]->m_type) ? dtInteger64 : (args[i]->m_type == dtFloat ? dtDouble : args[i]->m_type);
					if (i == 0)
						type = argType;
					else if (type != argType)
					{
						bool bNumbers = (type == dtInteger64 || type == dtDouble) && (argType == dtInteger64 || argType == dtDouble);
						type = bNumbers ? dtUnknown : dtUnknown;
					}
				}
				return type;
			}
			if (sName == "ABS" || sName == "FLOOR" || sName == "CEIL" || sName == "CEILING" || sName == "COALESCE" || sName == "IFNULL" ||
				sName == "NULLIF" || sName == "MOD")
			{
				eDataTypes type = args.empty() ? dtUnknown : args[0]->m_type;
				if (IsIntegerType(type))
					return dtInteger64;
				if (type == dtFloat)
					return dtDouble;
				return type;
			}
			return dtDouble;
		}

		bool IsAggregateName(const std::string& sName, eAggregate& agg)
		{
			if (sName == "COUNT") agg = eAggregate::Count;
			else if (sName == "SUM") agg = eAggregate::Sum;
			else if (sName == "AVG") agg = eAggregate::Avg;
			else if (sName == "MIN") agg = eAggregate::Min;
			else if (sName == "MAX") agg = eAggregate::Max;
			else return false;
			return true;
		}

		bool MapTypeName(const STypeName& type, eDataTypes& dataType, uint32_t& nLength)
		{
			const std::string& s = type.m_sName;
			nLength = 0;
			if (s == "TINYINT" || s == "INT8" || s == "BOOL" || s == "BOOLEAN") dataType = dtInteger8;
			else if (s == "SMALLINT" || s == "INT16") dataType = dtInteger16;
			else if (s == "INT" || s == "INTEGER" || s == "INT32" || s == "MEDIUMINT") dataType = dtInteger32;
			else if (s == "BIGINT" || s == "INT64" || s == "LONG") dataType = dtInteger64;
			else if (s == "UTINYINT" || s == "UINT8" || s == "BYTE") dataType = dtUInteger8;
			else if (s == "USMALLINT" || s == "UINT16") dataType = dtUInteger16;
			else if (s == "UINT" || s == "UINTEGER" || s == "UINT32") dataType = dtUInteger32;
			else if (s == "UBIGINT" || s == "UINT64") dataType = dtUInteger64;
			else if (s == "OID") dataType = dtOid;
			else if (s == "FLOAT" || s == "REAL" || s == "FLOAT4") dataType = dtFloat;
			else if (s == "DOUBLE" || s == "FLOAT8" || s == "NUMERIC" || s == "DECIMAL") dataType = dtDouble;
			else if (s == "VARCHAR" || s == "CHAR" || s == "NVARCHAR" || s == "NCHAR" || s == "TEXT" || s == "STRING" || s == "CLOB")
			{
				dataType = dtString;
				if (!type.m_params.empty())
				{
					if (type.m_params.size() != 1 || type.m_params[0] <= 0 || type.m_params[0] > 0x7FFFFFFF)
						throw CommonLib::CExcBase("{0}: a wrong length", s);
					nLength = (uint32_t)type.m_params[0];
				}
				return true;
			}
			else if (s == "BLOB" || s == "BINARY" || s == "VARBINARY" || s == "BYTEA") dataType = dtBlob;
			else if (s == "GEOMETRY" || s == "SHAPE" || s == "POINT" || s == "LINESTRING" || s == "POLYGON" ||
				s == "MULTIPOINT" || s == "MULTILINESTRING" || s == "MULTIPOLYGON") dataType = dtGeometry;
			else
				return false;

			if (!type.m_params.empty() && !(dataType == dtDouble && (s == "NUMERIC" || s == "DECIMAL")))
				throw CommonLib::CExcBase("{0} has no parameters", s);
			return true;
		}

		TValue CastValue(const TValue& value, eDataTypes type)
		{
			if (value.IsNull())
				return TValue();
			if (IsIntegerType(type))
			{
				if (ValueKind(value) == eValueKind::String)
				{
					TValue number;
					if (!ParseNumber(value.Get<CommonLib::astr_t>(), number))
						throw CommonLib::CExcBase("CAST: '{0}' isn't a number", value.Get<CommonLib::astr_t>());
					return TValue(ToInt64(number));
				}
				return TValue(ToInt64(value));
			}
			switch (type)
			{
			case dtFloat:
			case dtDouble:
				return TValue(ToDouble(value));
			case dtString:
				return TValue(ToText(value));
			case dtBlob:
				if (ValueKind(value) == eValueKind::Blob)
					return value;
				break;
			case dtGeometry:
				if (ValueKind(value) == eValueKind::Geometry)
					return value;
				break;
			default:
				break;
			}
			throw CommonLib::CExcBase("CAST to {0} isn't supported for this value", db::DataTypeName(type));
		}

		void VisitExpr(const CExprPtr& ptrExpr, const std::function<void(const CExprPtr&)>& visit)
		{
			if (ptrExpr.get() == nullptr)
				return;
			visit(ptrExpr);
			std::vector<CExprPtr*> children;
			ptrExpr->Children(children);
			for (CExprPtr* pChild : children)
				VisitExpr(*pChild, visit);
		}

		bool ContainsKind(const CExprPtr& ptrExpr, eBoundExpr kind)
		{
			bool bFound = false;
			VisitExpr(ptrExpr, [&](const CExprPtr& ptr) { bFound = bFound || ptr->m_kind == kind; });
			return bFound;
		}
	}
}
