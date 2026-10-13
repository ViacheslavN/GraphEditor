#pragma once

// Bound expressions: the syntax tree of an expression with the names resolved (CQueryCompiler) and ready to
// evaluate on a row. A column is an index in the row of the operator that evaluates the expression;
// a column of an outer query (a correlated subquery) is an index in the row of an outer evaluation context.

#include "SQLValue.h"
#include "AST.h"
#include <functional>
#include <unordered_set>

namespace bptreedb
{
	namespace sql
	{
		class COperator;
		typedef std::shared_ptr<COperator> COperatorPtr;

		// one execution of a statement
		struct SExecContext
		{
			ITransactionPtr m_ptrTran;
			std::vector<TValue> m_params;
			uint64_t m_nExecution{ 0 };      // the caches of the uncorrelated subqueries are of one execution
		};

		// where an expression is evaluated: the row, the context of the outer query (correlated subqueries)
		struct SEvalContext
		{
			const TRow* m_pRow{ nullptr };
			const SEvalContext* m_pOuter{ nullptr };
			SExecContext* m_pExec{ nullptr };
		};

		enum class eBoundExpr
		{
			Const, Param, Column, OuterColumn, Unary, Binary, IsNull, Between, InList, InSubquery, Exists,
			ScalarSubquery, Like, Case, Cast, Function, Aggregate, Spatial
		};

		class CExpr;
		typedef std::shared_ptr<CExpr> CExprPtr;

		class CExpr
		{
		public:
			explicit CExpr(eBoundExpr kind) : m_kind(kind) {}
			virtual ~CExpr() {}

			virtual TValue Eval(const SEvalContext& ctx) const = 0;
			// the sub-expressions (the compiler walks and replaces them)
			virtual void Children(std::vector<CExprPtr*>& children) {}
			// the structure: equal keys - the same expression (GROUP BY matching, aggregate dedup)
			virtual std::string Key() const = 0;

			const eBoundExpr m_kind;
			eDataTypes m_type{ dtUnknown };      // the type of the result if known
			SSourcePos m_pos;
		};

		class CConstExpr : public CExpr
		{
		public:
			explicit CConstExpr(const TValue& value);
			TValue Eval(const SEvalContext& ctx) const override { return m_value; }
			std::string Key() const override;
			TValue m_value;
		};

		class CParamExpr : public CExpr
		{
		public:
			explicit CParamExpr(uint32_t nIndex) : CExpr(eBoundExpr::Param), m_nIndex(nIndex) {}
			TValue Eval(const SEvalContext& ctx) const override;
			std::string Key() const override { return "?" + std::to_string(m_nIndex); }
			uint32_t m_nIndex;
		};

		// a column of a source of the query (m_nSource >= 0, the slot in the source; m_nIndex is assigned when the
		// sources have their offsets), or a column of the row of an aggregation (m_nSource == -1)
		class CColumnExpr : public CExpr
		{
		public:
			CColumnExpr(const void* pScope, int32_t nSource, int32_t nSlot) : CExpr(eBoundExpr::Column), m_pScope(pScope), m_nSource(nSource), m_nSlot(nSlot) {}
			TValue Eval(const SEvalContext& ctx) const override { return (*ctx.m_pRow)[m_nIndex]; }
			std::string Key() const override;

			const void* m_pScope;
			int32_t m_nSource;
			int32_t m_nSlot;
			int32_t m_nIndex{ -1 };
		};
		typedef std::shared_ptr<CColumnExpr> CColumnExprPtr;

		class COuterColumnExpr : public CExpr
		{
		public:
			COuterColumnExpr(uint32_t nDepth, CColumnExprPtr ptrTarget) : CExpr(eBoundExpr::OuterColumn), m_nDepth(nDepth), m_ptrTarget(ptrTarget) {}
			TValue Eval(const SEvalContext& ctx) const override;
			std::string Key() const override { return "outer" + std::to_string(m_nDepth) + ":" + m_ptrTarget->Key(); }
			uint32_t m_nDepth;
			CColumnExprPtr m_ptrTarget;
		};

		class CUnaryExpr : public CExpr
		{
		public:
			CUnaryExpr(eUnaryOp op, CExprPtr ptrOperand) : CExpr(eBoundExpr::Unary), m_op(op), m_ptrOperand(ptrOperand) {}
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override { children.push_back(&m_ptrOperand); }
			std::string Key() const override;
			eUnaryOp m_op;
			CExprPtr m_ptrOperand;
		};

		class CBinaryExpr : public CExpr
		{
		public:
			CBinaryExpr(eBinaryOp op, CExprPtr ptrLeft, CExprPtr ptrRight) : CExpr(eBoundExpr::Binary), m_op(op), m_ptrLeft(ptrLeft), m_ptrRight(ptrRight) {}
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override { children.push_back(&m_ptrLeft); children.push_back(&m_ptrRight); }
			std::string Key() const override;
			bool IsComparison() const;
			eBinaryOp m_op;
			CExprPtr m_ptrLeft;
			CExprPtr m_ptrRight;
		};

		class CIsNullExpr : public CExpr
		{
		public:
			CIsNullExpr(CExprPtr ptrOperand, bool bNot) : CExpr(eBoundExpr::IsNull), m_ptrOperand(ptrOperand), m_bNot(bNot) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override { children.push_back(&m_ptrOperand); }
			std::string Key() const override;
			CExprPtr m_ptrOperand;
			bool m_bNot;
		};

		class CBetweenExpr : public CExpr
		{
		public:
			CBetweenExpr(CExprPtr ptrOperand, CExprPtr ptrLow, CExprPtr ptrHigh, bool bNot) : CExpr(eBoundExpr::Between),
				m_ptrOperand(ptrOperand), m_ptrLow(ptrLow), m_ptrHigh(ptrHigh), m_bNot(bNot) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			CExprPtr m_ptrOperand;
			CExprPtr m_ptrLow;
			CExprPtr m_ptrHigh;
			bool m_bNot;
		};

		class CInListExpr : public CExpr
		{
		public:
			CInListExpr(CExprPtr ptrOperand, const std::vector<CExprPtr>& list, bool bNot) : CExpr(eBoundExpr::InList),
				m_ptrOperand(ptrOperand), m_list(list), m_bNot(bNot) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			CExprPtr m_ptrOperand;
			std::vector<CExprPtr> m_list;
			bool m_bNot;
		};

		// the plan of a subquery; outer references: the sources of the enclosing queries it reads
		struct SSubqueryPlan
		{
			COperatorPtr m_ptrRoot;
			uint32_t m_nColumns{ 0 };
			bool m_bCorrelated{ false };
			std::vector<std::pair<const void*, int32_t> > m_outerRefs;     // (scope, source)
		};
		typedef std::shared_ptr<SSubqueryPlan> SSubqueryPlanPtr;

		class CSubqueryExprBase : public CExpr
		{
		public:
			CSubqueryExprBase(eBoundExpr kind, SSubqueryPlanPtr ptrPlan) : CExpr(kind), m_ptrPlan(ptrPlan) {}
			std::string Key() const override;
			SSubqueryPlanPtr m_ptrPlan;

		protected:
			// an uncorrelated subquery runs once per execution of the statement
			bool Cached(const SEvalContext& ctx) const;
			void SetCached(const SEvalContext& ctx) const;
			mutable uint64_t m_nCachedExecution{ ~uint64_t(0) };
		};

		class CScalarSubqueryExpr : public CSubqueryExprBase
		{
		public:
			explicit CScalarSubqueryExpr(SSubqueryPlanPtr ptrPlan) : CSubqueryExprBase(eBoundExpr::ScalarSubquery, ptrPlan) {}
			TValue Eval(const SEvalContext& ctx) const override;
		private:
			mutable TValue m_cached;
		};

		class CExistsExpr : public CSubqueryExprBase
		{
		public:
			explicit CExistsExpr(SSubqueryPlanPtr ptrPlan) : CSubqueryExprBase(eBoundExpr::Exists, ptrPlan) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
		private:
			mutable bool m_bCached{ false };
		};

		class CInSubqueryExpr : public CSubqueryExprBase
		{
		public:
			CInSubqueryExpr(CExprPtr ptrOperand, SSubqueryPlanPtr ptrPlan, bool bNot) : CSubqueryExprBase(eBoundExpr::InSubquery, ptrPlan),
				m_ptrOperand(ptrOperand), m_bNot(bNot) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override { children.push_back(&m_ptrOperand); }
			std::string Key() const override;
			CExprPtr m_ptrOperand;
			bool m_bNot;
		private:
			void Load(const SEvalContext& ctx) const;
			mutable std::unordered_set<TValue, SValueHash, SValueEqual> m_values;
			mutable bool m_bHasNull{ false };
		};

		class CLikeExpr : public CExpr
		{
		public:
			CLikeExpr(CExprPtr ptrOperand, CExprPtr ptrPattern, CExprPtr ptrEscape, bool bNot) : CExpr(eBoundExpr::Like),
				m_ptrOperand(ptrOperand), m_ptrPattern(ptrPattern), m_ptrEscape(ptrEscape), m_bNot(bNot) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			CExprPtr m_ptrOperand;
			CExprPtr m_ptrPattern;
			CExprPtr m_ptrEscape;
			bool m_bNot;
		};

		class CCaseExpr : public CExpr
		{
		public:
			CCaseExpr() : CExpr(eBoundExpr::Case) {}
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			CExprPtr m_ptrOperand;
			std::vector<std::pair<CExprPtr, CExprPtr> > m_whens;
			CExprPtr m_ptrElse;
		};

		class CCastExpr : public CExpr
		{
		public:
			CCastExpr(CExprPtr ptrOperand, eDataTypes type) : CExpr(eBoundExpr::Cast), m_ptrOperand(ptrOperand) { m_type = type; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override { children.push_back(&m_ptrOperand); }
			std::string Key() const override;
			CExprPtr m_ptrOperand;
		};

		typedef std::function<TValue(const std::vector<TValue>&)> TScalarFunction;

		class CFunctionExpr : public CExpr
		{
		public:
			CFunctionExpr(const std::string& sName, TScalarFunction func, bool bNullInNullOut) : CExpr(eBoundExpr::Function),
				m_sName(sName), m_func(func), m_bNullInNullOut(bNullInNullOut) {}
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			std::string m_sName;
			TScalarFunction m_func;
			bool m_bNullInNullOut;
			std::vector<CExprPtr> m_args;
		};

		enum class eAggregate { Count, Sum, Avg, Min, Max };

		// an aggregate call: computed by CAggregateOp, replaced by a column of its row in the expressions above it
		class CAggregateExpr : public CExpr
		{
		public:
			CAggregateExpr(eAggregate agg, CExprPtr ptrArg, bool bDistinct) : CExpr(eBoundExpr::Aggregate), m_agg(agg), m_ptrArg(ptrArg), m_bDistinct(bDistinct) {}
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			eAggregate m_agg;
			CExprPtr m_ptrArg;      // null: COUNT(*)
			bool m_bDistinct;
		};

		// ST_Intersects / ST_Within / ST_Contains of the bounding boxes: a geometry and an envelope
		// (ST_MakeEnvelope(xmin, ymin, xmax, ymax)) or another geometry
		class CSpatialExpr : public CExpr
		{
		public:
			CSpatialExpr(eSpatialQueryMode mode) : CExpr(eBoundExpr::Spatial), m_mode(mode) { m_type = dtInteger64; }
			TValue Eval(const SEvalContext& ctx) const override;
			void Children(std::vector<CExprPtr*>& children) override;
			std::string Key() const override;
			// the extent: false - NULL / an empty geometry
			bool Extent(const SEvalContext& ctx, CommonLib::bbox& extent) const;

			eSpatialQueryMode m_mode;        // of m_ptrGeom against the extent
			CExprPtr m_ptrGeom;
			CExprPtr m_ptrOther;             // the other geometry, or the envelope:
			std::vector<CExprPtr> m_envelope;
		};

		// the bounding box of a geometry value, false for NULL / an empty one
		bool ValueBBox(const TValue& value, CommonLib::bbox& bb);
		bool MatchBBox(const CommonLib::bbox& bb, const CommonLib::bbox& extent, eSpatialQueryMode mode);

		// the scalar functions: false if there is no such function; throws for a wrong count of arguments
		bool FindScalarFunction(const std::string& sName, size_t nArgs, TScalarFunction& func, bool& bNullInNullOut);
		eDataTypes FunctionType(const std::string& sName, const std::vector<CExprPtr>& args);
		bool IsAggregateName(const std::string& sName, eAggregate& agg);

		// a type of CREATE TABLE / CAST: false if unknown
		bool MapTypeName(const STypeName& type, eDataTypes& dataType, uint32_t& nLength);
		TValue CastValue(const TValue& value, eDataTypes type);

		// every node of the tree (pre-order)
		void VisitExpr(const CExprPtr& ptrExpr, const std::function<void(const CExprPtr&)>& visit);
		bool ContainsKind(const CExprPtr& ptrExpr, eBoundExpr kind);
	}
}
