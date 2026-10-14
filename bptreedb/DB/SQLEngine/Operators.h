#pragma once

// Operators of a query plan (the iterator model): Open starts (or restarts) the operator, Next moves to the
// next row, Row is the current row. The plan is built once by CQueryCompiler and opened by every execution.
//
// The rows of the FROM part have the width of all the sources of the query: a source fills its range
// [offset, offset + width) of the row, a join combines the ranges of its sides. The operators above
// (aggregation, projection) make their own rows.

#include "Expr.h"
#include <unordered_map>

namespace bptreedb
{
	namespace sql
	{
		class COperator
		{
		public:
			explicit COperator(uint32_t nWidth) : m_row(nWidth) {}
			virtual ~COperator() {}

			// pOuter: the context of the outer query (a correlated subquery), may be null
			virtual void Open(SExecContext& exec, const SEvalContext* pOuter) = 0;
			virtual bool Next() = 0;
			virtual void Close() {}
			virtual const TRow& Row() const { return m_row; }
			uint32_t Width() const { return (uint32_t)m_row.size(); }

			// the plan as text: one line per operator, the children indented
			virtual void Explain(std::string& sText, int nDepth) const = 0;

		protected:
			static void Line(std::string& sText, int nDepth, const std::string& sLine);
			void SetContext(SExecContext& exec, const SEvalContext* pOuter) { m_pExec = &exec; m_pOuter = pOuter; }
			SEvalContext Ctx(const TRow& row) const;
			bool Check(const std::vector<CExprPtr>& conditions, const TRow& row) const;    // all are true

			TRow m_row;
			SExecContext* m_pExec{ nullptr };
			const SEvalContext* m_pOuter{ nullptr };
		};

		// SELECT without FROM: one row
		class CSingleRowOp : public COperator
		{
		public:
			explicit CSingleRowOp(uint32_t nWidth) : COperator(nWidth) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override { SetContext(exec, pOuter); m_bDone = false; }
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
		private:
			bool m_bDone{ false };
		};

		// how a table is read
		struct SAccessPath
		{
			enum eKind { Scan, Seek, InList, Spatial, RowIds };

			eKind m_kind{ Scan };
			std::string m_sField;
			eDataTypes m_fieldType{ dtUnknown };
			OpType m_op{ OpEqual };                // Seek
			std::vector<CExprPtr> m_keys;          // Seek: one, InList / RowIds: the values
			CExprPtr m_ptrSpatial;                 // Spatial: a CSpatialExpr, its extent is the query
			eSpatialQueryMode m_mode{ sqmIntersect };
			// the keys are evaluated on the row of the left side of an index join (OpenWithKeys)
			bool m_bKeysFromRow{ false };

			std::string ToString() const;
		};

		// A table: the cursor of the access path, the values of the used fields (and the row id) into the row,
		// then the filters (the conditions of the table pushed down).
		class CTableScanOp : public COperator
		{
		public:
			struct SSlot
			{
				std::string m_sField;      // empty: the row id
			};

			CTableScanOp(const std::string& sTable, const std::vector<SSlot>& slots, uint32_t nOffset, uint32_t nWidth,
				const SAccessPath& path, const std::vector<CExprPtr>& filters);

			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			// an index join: the keys of the path from the row of the left side
			void OpenWithKeys(SExecContext& exec, const SEvalContext* pOuter, const TRow& keyRow);
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override;

			const SAccessPath& Path() const { return m_path; }
			const std::string& Table() const { return m_sTable; }

		private:
			void Start(const SEvalContext& keyCtx);
			bool NextCursor();
			ISelectCursorPtr Select(const TValue& key, OpType op);

			std::string m_sTable;
			std::vector<SSlot> m_slots;
			std::vector<int32_t> m_cursorCols;     // the column of the cursor of a slot, -1 the row id
			IFieldSetPtr m_ptrFieldSet;
			uint32_t m_nOffset;
			SAccessPath m_path;
			std::vector<CExprPtr> m_filters;

			ISelectCursorPtr m_ptrCursor;
			// the seeks to do (InList: one per value; Seek: one)
			std::vector<std::pair<TValue, OpType> > m_seeks;
			size_t m_nSeek{ 0 };
			bool m_bEmpty{ false };
			bool m_bScanAll{ false };
		};

		// The system tables of the space statistics (ITransaction::GetSpaceStatistics), read in FROM like tables:
		//   bptreedb_space       the pages of the trees counted from the inner nodes (fast)
		//   bptreedb_space_full  also the stream pages of the big blobs (the leaves of the blob trees are read)
		// A row per tree (the rows, the values of a field, an index), then the rows of the file: 'file' (the pages of
		// the file) and 'other' (the pages not in the trees: the schema, the pages a tree doesn't use any more and,
		// without _full, the big blobs).
		class CSpaceStatsOp : public COperator
		{
		public:
			explicit CSpaceStatsOp(bool bBlobs);

			// the system table of the name (case-insensitive), false - not a system table
			static bool IsSpaceTable(const std::string& sName, bool& bBlobs);
			static const std::vector<std::string>& ColumnNames();
			static const std::vector<eDataTypes>& ColumnTypes();

			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;

		private:
			bool m_bBlobs;
			std::vector<TRow> m_rows;
			size_t m_nPos{ 0 };
		};

		// a subquery in FROM: its visible columns into the range of the row
		class CSubquerySourceOp : public COperator
		{
		public:
			CSubquerySourceOp(COperatorPtr ptrChild, uint32_t nColumns, uint32_t nOffset, uint32_t nWidth, const std::vector<CExprPtr>& filters);
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_ptrChild->Close(); }
		private:
			COperatorPtr m_ptrChild;
			uint32_t m_nColumns;
			uint32_t m_nOffset;
			std::vector<CExprPtr> m_filters;
		};

		class CFilterOp : public COperator
		{
		public:
			CFilterOp(COperatorPtr ptrChild, const std::vector<CExprPtr>& conditions) : COperator(0), m_ptrChild(ptrChild), m_conditions(conditions) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_ptrChild->Close(); }
			const TRow& Row() const override { return m_ptrChild->Row(); }
		private:
			COperatorPtr m_ptrChild;
			std::vector<CExprPtr> m_conditions;
		};

		// The joins: the right side is read into memory (nested loop: all rows, hash: by the keys), the left side
		// streams. RIGHT / FULL emit the right rows without a match after the left side.
		class CJoinOpBase : public COperator
		{
		public:
			CJoinOpBase(COperatorPtr ptrLeft, COperatorPtr ptrRight, eJoinType type, const std::vector<CExprPtr>& conditions,
				uint32_t nLeftBegin, uint32_t nLeftEnd, uint32_t nRightBegin, uint32_t nRightEnd);
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override;

		protected:
			virtual const char* Algorithm() const = 0;
			virtual void Build() = 0;                                   // reads the right side
			virtual void Candidates(const TRow& leftRow) = 0;           // m_candidates for a left row

			void SetRight(const TRow& right);
			void SetRightNull();
			void SetLeftNull();

			COperatorPtr m_ptrLeft;
			COperatorPtr m_ptrRight;
			eJoinType m_type;
			std::vector<CExprPtr> m_conditions;
			uint32_t m_nLeftBegin, m_nLeftEnd, m_nRightBegin, m_nRightEnd;

			std::vector<TRow> m_rightRows;          // the range of the right side
			std::vector<char> m_matched;            // RIGHT / FULL
			const std::vector<size_t>* m_pCandidates{ nullptr };
			std::vector<size_t> m_all;

		private:
			bool m_bLeft{ false };
			bool m_bLeftMatched{ false };
			size_t m_nPos{ 0 };
			bool m_bTail{ false };                  // the unmatched right rows
			bool m_bDone{ false };
		};

		class CNestedLoopJoinOp : public CJoinOpBase
		{
		public:
			using CJoinOpBase::CJoinOpBase;
		protected:
			const char* Algorithm() const override { return "nested loop join"; }
			void Build() override;
			void Candidates(const TRow& leftRow) override;
		};

		class CHashJoinOp : public CJoinOpBase
		{
		public:
			CHashJoinOp(COperatorPtr ptrLeft, COperatorPtr ptrRight, eJoinType type, const std::vector<CExprPtr>& conditions,
				uint32_t nLeftBegin, uint32_t nLeftEnd, uint32_t nRightBegin, uint32_t nRightEnd,
				const std::vector<CExprPtr>& leftKeys, const std::vector<CExprPtr>& rightKeys);
		protected:
			const char* Algorithm() const override { return "hash join"; }
			void Build() override;
			void Candidates(const TRow& leftRow) override;
		private:
			std::vector<CExprPtr> m_leftKeys;
			std::vector<CExprPtr> m_rightKeys;
			std::unordered_map<TRow, std::vector<size_t>, SRowHash, SRowEqual> m_hash;
			std::vector<size_t> m_none;
		};

		// INNER / LEFT join where the right side is a table read by its index with keys of the left row
		class CIndexJoinOp : public COperator
		{
		public:
			CIndexJoinOp(COperatorPtr ptrLeft, std::shared_ptr<CTableScanOp> ptrRight, eJoinType type, const std::vector<CExprPtr>& conditions,
				uint32_t nRightBegin, uint32_t nRightEnd);
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override;
		private:
			COperatorPtr m_ptrLeft;
			std::shared_ptr<CTableScanOp> m_ptrRight;
			eJoinType m_type;
			std::vector<CExprPtr> m_conditions;
			uint32_t m_nRightBegin, m_nRightEnd;
			bool m_bLeft{ false };
			bool m_bMatched{ false };
		};

		// GROUP BY / aggregates: the row is the group keys, then the aggregates
		class CAggregateOp : public COperator
		{
		public:
			CAggregateOp(COperatorPtr ptrChild, const std::vector<CExprPtr>& groups, const std::vector<std::shared_ptr<CAggregateExpr> >& aggregates);
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_groups.clear(); m_index.clear(); }

		private:
			struct SState
			{
				int64_t m_nCount{ 0 };
				bool m_bDouble{ false };
				int64_t m_nSum{ 0 };
				double m_dSum{ 0 };
				TValue m_value;          // MIN / MAX
				std::unordered_set<TValue, SValueHash, SValueEqual> m_distinct;
			};
			struct SGroup
			{
				TRow m_keys;
				std::vector<SState> m_states;
			};

			void Add(SState& state, const CAggregateExpr& agg, const TValue& value);
			TValue Result(const SState& state, const CAggregateExpr& agg) const;

			COperatorPtr m_ptrChild;
			std::vector<CExprPtr> m_groupExprs;
			std::vector<std::shared_ptr<CAggregateExpr> > m_aggregates;
			std::vector<SGroup> m_groups;
			std::unordered_map<TRow, size_t, SRowHash, SRowEqual> m_index;
			size_t m_nPos{ 0 };
		};

		class CProjectOp : public COperator
		{
		public:
			CProjectOp(COperatorPtr ptrChild, const std::vector<CExprPtr>& exprs) : COperator((uint32_t)exprs.size()), m_ptrChild(ptrChild), m_exprs(exprs) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_ptrChild->Close(); }
		private:
			COperatorPtr m_ptrChild;
			std::vector<CExprPtr> m_exprs;
		};

		// the rows of the child whose first nColumns values weren't seen
		class CDistinctOp : public COperator
		{
		public:
			CDistinctOp(COperatorPtr ptrChild, uint32_t nColumns) : COperator(0), m_ptrChild(ptrChild), m_nColumns(nColumns) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_ptrChild->Close(); m_seen.clear(); }
			const TRow& Row() const override { return m_ptrChild->Row(); }
		private:
			COperatorPtr m_ptrChild;
			uint32_t m_nColumns;
			std::unordered_set<TRow, SRowHash, SRowEqual> m_seen;
		};

		// ORDER BY: NULL is the smallest value
		class CSortOp : public COperator
		{
		public:
			struct SKey
			{
				uint32_t m_nColumn;
				bool m_bDesc;
			};
			CSortOp(COperatorPtr ptrChild, const std::vector<SKey>& keys) : COperator(0), m_ptrChild(ptrChild), m_keys(keys) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_rows.clear(); }
			const TRow& Row() const override { return m_rows[m_nPos - 1]; }
		private:
			COperatorPtr m_ptrChild;
			std::vector<SKey> m_keys;
			std::vector<TRow> m_rows;
			size_t m_nPos{ 0 };
		};

		class CLimitOp : public COperator
		{
		public:
			CLimitOp(COperatorPtr ptrChild, CExprPtr ptrLimit, CExprPtr ptrOffset) : COperator(0), m_ptrChild(ptrChild), m_ptrLimit(ptrLimit), m_ptrOffset(ptrOffset) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override { m_ptrChild->Close(); }
			const TRow& Row() const override { return m_ptrChild->Row(); }
		private:
			COperatorPtr m_ptrChild;
			CExprPtr m_ptrLimit;
			CExprPtr m_ptrOffset;
			int64_t m_nLimit{ -1 };
			int64_t m_nCount{ 0 };
		};

		// UNION [ALL] / INTERSECT / EXCEPT of the first nColumns columns
		class CCompoundOp : public COperator
		{
		public:
			CCompoundOp(COperatorPtr ptrLeft, COperatorPtr ptrRight, eCompoundOp op, uint32_t nColumns) : COperator(0),
				m_ptrLeft(ptrLeft), m_ptrRight(ptrRight), m_op(op), m_nColumns(nColumns) {}
			void Open(SExecContext& exec, const SEvalContext* pOuter) override;
			bool Next() override;
			void Explain(std::string& sText, int nDepth) const override;
			void Close() override;
			const TRow& Row() const override { return *m_pRow; }
		private:
			TRow Key(const TRow& row) const;

			COperatorPtr m_ptrLeft;
			COperatorPtr m_ptrRight;
			eCompoundOp m_op;
			uint32_t m_nColumns;
			bool m_bRight{ false };
			const TRow* m_pRow{ nullptr };
			std::unordered_set<TRow, SRowHash, SRowEqual> m_seen;
			std::unordered_set<TRow, SRowHash, SRowEqual> m_rightSet;
		};

		// the key of a seek converted to the type of the indexed field
		enum class eKeyMatch { Use, Empty, ScanAll };
		eKeyMatch KeyForField(const TValue& key, eDataTypes fieldType, OpType& op, TValue& fieldKey);
	}
}
