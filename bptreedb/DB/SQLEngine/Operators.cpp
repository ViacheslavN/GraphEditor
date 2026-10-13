#include "Operators.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bptreedb
{
	namespace sql
	{
		namespace
		{
			bool IntRange(eDataTypes type, int64_t& nMin, uint64_t& nMax)
			{
				switch (type)
				{
				case dtInteger8: nMin = INT8_MIN; nMax = INT8_MAX; return true;
				case dtInteger16: nMin = INT16_MIN; nMax = INT16_MAX; return true;
				case dtInteger32: nMin = INT32_MIN; nMax = INT32_MAX; return true;
				case dtInteger64: case dtOid: nMin = INT64_MIN; nMax = INT64_MAX; return true;
				case dtUInteger8: nMin = 0; nMax = UINT8_MAX; return true;
				case dtUInteger16: nMin = 0; nMax = UINT16_MAX; return true;
				case dtUInteger32: nMin = 0; nMax = UINT32_MAX; return true;
				case dtUInteger64: nMin = 0; nMax = UINT64_MAX; return true;
				default: return false;
				}
			}
		}

		eKeyMatch KeyForField(const TValue& key, eDataTypes fieldType, OpType& op, TValue& fieldKey)
		{
			if (key.IsNull())
				return eKeyMatch::Empty;
			eValueKind kind = ValueKind(key);

			int64_t nMin = 0;
			uint64_t nMax = 0;
			if (IntRange(fieldType, nMin, nMax))
			{
				int64_t n = 0;
				if (kind == eValueKind::Integer)
					n = key.Get<int64_t>();
				else if (kind == eValueKind::Double)
				{
					double d = key.Get<double>();
					if (std::isnan(d))
						return eKeyMatch::ScanAll;
					if (d != std::floor(d))
					{
						// 1.5: = never matches, > 1.5 is >= 2, < 1.5 is <= 1
						switch (op)
						{
						case OpEqual: return eKeyMatch::Empty;
						case OpGreater: case OpGreaterOrEqual: d = std::ceil(d); op = OpGreaterOrEqual; break;
						case OpLess: case OpLessOrEqual: d = std::floor(d); op = OpLessOrEqual; break;
						default: return eKeyMatch::ScanAll;
						}
					}
					if (d < -9223372036854775808.0 || d >= 9223372036854775808.0)
					{
						bool bAbove = d > 0;
						if (op == OpEqual)
							return eKeyMatch::Empty;
						bool bGreater = op == OpGreater || op == OpGreaterOrEqual;
						return bGreater == bAbove ? eKeyMatch::Empty : eKeyMatch::ScanAll;
					}
					n = (int64_t)d;
				}
				else
					return eKeyMatch::ScanAll;

				// out of the range of the field
				bool bBelow = n < nMin;
				bool bAbove = n >= 0 && (uint64_t)n > nMax;
				if (bBelow || bAbove)
				{
					if (op == OpEqual)
						return eKeyMatch::Empty;
					bool bGreater = op == OpGreater || op == OpGreaterOrEqual;
					return bGreater == bAbove ? eKeyMatch::Empty : eKeyMatch::ScanAll;
				}
				fieldKey = n;
				return eKeyMatch::Use;
			}

			switch (fieldType)
			{
			case dtDouble:
				if (!IsNumberKind(kind))
					return eKeyMatch::ScanAll;
				fieldKey = ToDouble(key);
				return std::isnan(fieldKey.Get<double>()) ? eKeyMatch::ScanAll : eKeyMatch::Use;
			case dtFloat:
				{
					if (!IsNumberKind(kind))
						return eKeyMatch::ScanAll;
					double d = ToDouble(key);
					if (std::isnan(d) || (double)(float)d != d)
						return eKeyMatch::ScanAll;
					fieldKey = d;
					return eKeyMatch::Use;
				}
			case dtString:
				if (kind != eValueKind::String)
					return eKeyMatch::ScanAll;
				fieldKey = key;
				return eKeyMatch::Use;
			default:
				return eKeyMatch::ScanAll;
			}
		}

		std::string SAccessPath::ToString() const
		{
			switch (m_kind)
			{
			case Scan: return "scan";
			case Seek:
				{
					static const char* const ops[] = { "=", "<>", "<", "<=", ">", ">=" };
					return "index " + m_sField + " " + ops[(int)m_op];
				}
			case InList: return "index " + m_sField + " IN";
			case Spatial: return "spatial index " + m_sField;
			case RowIds: return "row ids";
			}
			return "?";
		}

		//------------------------------------------------------------------------------------------

		SEvalContext COperator::Ctx(const TRow& row) const
		{
			SEvalContext ctx;
			ctx.m_pRow = &row;
			ctx.m_pOuter = m_pOuter;
			ctx.m_pExec = m_pExec;
			return ctx;
		}

		bool COperator::Check(const std::vector<CExprPtr>& conditions, const TRow& row) const
		{
			if (conditions.empty())
				return true;
			SEvalContext ctx = Ctx(row);
			for (const CExprPtr& ptrCond : conditions)
				if (Truth(ptrCond->Eval(ctx)) != 1)
					return false;
			return true;
		}

		void COperator::Line(std::string& sText, int nDepth, const std::string& sLine)
		{
			sText.append((size_t)nDepth * 2, ' ');
			sText += sLine;
			sText += '\n';
		}

		namespace
		{
			const char* JoinName(eJoinType type)
			{
				switch (type)
				{
				case eJoinType::Inner: return "inner";
				case eJoinType::Left: return "left";
				case eJoinType::Right: return "right";
				case eJoinType::Full: return "full";
				default: return "cross";
				}
			}

			std::string Conditions(const std::vector<CExprPtr>& conditions)
			{
				return conditions.empty() ? std::string() : ", " + std::to_string(conditions.size()) + " condition(s)";
			}
		}

		void CSingleRowOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "one row");
		}

		void CTableScanOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "table " + m_sTable + ": " + m_path.ToString() + Conditions(m_filters));
		}

		void CSubquerySourceOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "subquery" + Conditions(m_filters));
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CFilterOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "filter" + Conditions(m_conditions));
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CJoinOpBase::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, std::string(Algorithm()) + " (" + JoinName(m_type) + ")" + Conditions(m_conditions));
			m_ptrLeft->Explain(sText, nDepth + 1);
			m_ptrRight->Explain(sText, nDepth + 1);
		}

		void CIndexJoinOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, std::string("index join (") + JoinName(m_type) + ")" + Conditions(m_conditions));
			m_ptrLeft->Explain(sText, nDepth + 1);
			m_ptrRight->Explain(sText, nDepth + 1);
		}

		void CAggregateOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "aggregate: " + std::to_string(m_groupExprs.size()) + " group key(s), " + std::to_string(m_aggregates.size()) + " aggregate(s)");
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CProjectOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "project: " + std::to_string(m_exprs.size()) + " column(s)");
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CDistinctOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "distinct");
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CSortOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "sort: " + std::to_string(m_keys.size()) + " key(s)");
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CLimitOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, "limit");
			m_ptrChild->Explain(sText, nDepth + 1);
		}

		void CCompoundOp::Explain(std::string& sText, int nDepth) const
		{
			Line(sText, nDepth, ToString(m_op));
			m_ptrLeft->Explain(sText, nDepth + 1);
			m_ptrRight->Explain(sText, nDepth + 1);
		}

		bool CSingleRowOp::Next()
		{
			if (m_bDone)
				return false;
			m_bDone = true;
			return true;
		}

		//------------------------------------------------------------------------------------------
		// table

		CTableScanOp::CTableScanOp(const std::string& sTable, const std::vector<SSlot>& slots, uint32_t nOffset, uint32_t nWidth,
			const SAccessPath& path, const std::vector<CExprPtr>& filters) :
			COperator(nWidth), m_sTable(sTable), m_slots(slots), m_nOffset(nOffset), m_path(path), m_filters(filters)
		{
			m_ptrFieldSet = IFieldSet::Create();
			for (const SSlot& slot : m_slots)
			{
				if (slot.m_sField.empty())
					m_cursorCols.push_back(-1);
				else
				{
					m_cursorCols.push_back(m_ptrFieldSet->GetCount());
					m_ptrFieldSet->Add(slot.m_sField);
				}
			}
		}

		void CTableScanOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			TRow empty;
			SEvalContext ctx = Ctx(empty);
			Start(ctx);
		}

		void CTableScanOp::OpenWithKeys(SExecContext& exec, const SEvalContext* pOuter, const TRow& keyRow)
		{
			SetContext(exec, pOuter);
			Start(Ctx(keyRow));
		}

		void CTableScanOp::Start(const SEvalContext& keyCtx)
		{
			m_ptrCursor.reset();
			m_seeks.clear();
			m_nSeek = 0;
			m_bEmpty = false;
			m_bScanAll = false;

			switch (m_path.m_kind)
			{
			case SAccessPath::Scan:
				m_bScanAll = true;
				break;
			case SAccessPath::Seek:
			case SAccessPath::InList:
				{
					std::vector<TValue> keys;
					for (const CExprPtr& ptrKey : m_path.m_keys)
					{
						OpType op = m_path.m_op;
						TValue fieldKey;
						switch (KeyForField(ptrKey->Eval(keyCtx), m_path.m_fieldType, op, fieldKey))
						{
						case eKeyMatch::Empty:
							break;
						case eKeyMatch::ScanAll:
							m_bScanAll = true;
							break;
						case eKeyMatch::Use:
							{
								bool bDuplicate = false;
								for (const auto& seek : m_seeks)
									bDuplicate = bDuplicate || (seek.second == op && GroupEqual(seek.first, fieldKey));
								if (!bDuplicate)
									m_seeks.emplace_back(fieldKey, op);
							}
							break;
						}
						if (m_bScanAll)
							break;
					}
					if (!m_bScanAll && m_seeks.empty())
						m_bEmpty = true;
				}
				break;
			case SAccessPath::RowIds:
				{
					std::vector<int64_t> rowIds;
					for (const CExprPtr& ptrKey : m_path.m_keys)
					{
						TValue key = ptrKey->Eval(keyCtx);
						OpType op = OpEqual;
						TValue fieldKey;
						if (KeyForField(key, dtInteger64, op, fieldKey) == eKeyMatch::Use)
							rowIds.push_back(fieldKey.Get<int64_t>());
					}
					std::sort(rowIds.begin(), rowIds.end());
					rowIds.erase(std::unique(rowIds.begin(), rowIds.end()), rowIds.end());
					if (rowIds.empty())
						m_bEmpty = true;
					else
						m_ptrCursor = m_pExec->m_ptrTran->executeSelectQuery(m_sTable, m_ptrFieldSet, rowIds);
				}
				break;
			case SAccessPath::Spatial:
				{
					CommonLib::bbox extent;
					const CSpatialExpr* pSpatial = static_cast<const CSpatialExpr*>(m_path.m_ptrSpatial.get());
					if (!pSpatial->Extent(keyCtx, extent))
						m_bEmpty = true;
					else
						m_ptrCursor = m_pExec->m_ptrTran->executeSpatialQuery(extent, m_sTable, m_path.m_sField, m_path.m_mode, m_ptrFieldSet);
				}
				break;
			}

			if (m_bScanAll)
			{
				m_seeks.clear();
				m_ptrCursor = m_pExec->m_ptrTran->executeSelectQuery(m_sTable, m_ptrFieldSet);
			}
		}

		ISelectCursorPtr CTableScanOp::Select(const TValue& key, OpType op)
		{
			return m_pExec->m_ptrTran->executeSelectQuery(m_sTable, m_ptrFieldSet, m_path.m_sField, key, op);
		}

		bool CTableScanOp::NextCursor()
		{
			for (;;)
			{
				if (m_ptrCursor.get() && m_ptrCursor->Next())
					return true;
				m_ptrCursor.reset();
				if (m_bEmpty || m_nSeek >= m_seeks.size())
					return false;
				const auto& seek = m_seeks[m_nSeek++];
				m_ptrCursor = Select(seek.first, seek.second);
			}
		}

		bool CTableScanOp::Next()
		{
			if (m_bEmpty)
				return false;
			while (NextCursor())
			{
				for (size_t i = 0; i < m_slots.size(); ++i)
				{
					TValue& value = m_row[m_nOffset + i];
					if (m_cursorCols[i] < 0)
						value = m_ptrCursor->GetRowID();
					else
					{
						value = m_ptrCursor->ReadValue(m_cursorCols[i]);
						NormalizeInPlace(value);
					}
				}
				if (Check(m_filters, m_row))
					return true;
			}
			return false;
		}

		void CTableScanOp::Close()
		{
			m_ptrCursor.reset();
			m_seeks.clear();
		}

		//------------------------------------------------------------------------------------------

		CSubquerySourceOp::CSubquerySourceOp(COperatorPtr ptrChild, uint32_t nColumns, uint32_t nOffset, uint32_t nWidth, const std::vector<CExprPtr>& filters) :
			COperator(nWidth), m_ptrChild(ptrChild), m_nColumns(nColumns), m_nOffset(nOffset), m_filters(filters)
		{
		}

		void CSubquerySourceOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_ptrChild->Open(exec, pOuter);
		}

		bool CSubquerySourceOp::Next()
		{
			while (m_ptrChild->Next())
			{
				const TRow& row = m_ptrChild->Row();
				for (uint32_t i = 0; i < m_nColumns; ++i)
					m_row[m_nOffset + i] = row[i];
				if (Check(m_filters, m_row))
					return true;
			}
			return false;
		}

		void CFilterOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_ptrChild->Open(exec, pOuter);
		}

		bool CFilterOp::Next()
		{
			while (m_ptrChild->Next())
				if (Check(m_conditions, m_ptrChild->Row()))
					return true;
			return false;
		}

		//------------------------------------------------------------------------------------------
		// joins

		CJoinOpBase::CJoinOpBase(COperatorPtr ptrLeft, COperatorPtr ptrRight, eJoinType type, const std::vector<CExprPtr>& conditions,
			uint32_t nLeftBegin, uint32_t nLeftEnd, uint32_t nRightBegin, uint32_t nRightEnd) :
			COperator(ptrLeft->Width()), m_ptrLeft(ptrLeft), m_ptrRight(ptrRight), m_type(type), m_conditions(conditions),
			m_nLeftBegin(nLeftBegin), m_nLeftEnd(nLeftEnd), m_nRightBegin(nRightBegin), m_nRightEnd(nRightEnd)
		{
		}

		void CJoinOpBase::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_rightRows.clear();
			m_matched.clear();
			m_ptrRight->Open(exec, pOuter);
			Build();
			m_ptrRight->Close();
			if (m_type == eJoinType::Right || m_type == eJoinType::Full)
				m_matched.assign(m_rightRows.size(), 0);
			m_ptrLeft->Open(exec, pOuter);
			m_bLeft = false;
			m_bTail = false;
			m_bDone = false;
			m_nPos = 0;
		}

		void CJoinOpBase::SetRight(const TRow& right)
		{
			std::copy(right.begin(), right.end(), m_row.begin() + m_nRightBegin);
		}

		void CJoinOpBase::SetRightNull()
		{
			for (uint32_t i = m_nRightBegin; i < m_nRightEnd; ++i)
				m_row[i] = TValue();
		}

		void CJoinOpBase::SetLeftNull()
		{
			for (uint32_t i = m_nLeftBegin; i < m_nLeftEnd; ++i)
				m_row[i] = TValue();
		}

		bool CJoinOpBase::Next()
		{
			if (m_bDone)
				return false;
			for (;;)
			{
				if (m_bTail)
				{
					while (m_nPos < m_rightRows.size())
					{
						size_t nIndex = m_nPos++;
						if (m_matched[nIndex])
							continue;
						SetLeftNull();
						SetRight(m_rightRows[nIndex]);
						return true;
					}
					m_bDone = true;
					return false;
				}

				if (!m_bLeft)
				{
					if (!m_ptrLeft->Next())
					{
						if (m_type == eJoinType::Right || m_type == eJoinType::Full)
						{
							m_bTail = true;
							m_nPos = 0;
							continue;
						}
						m_bDone = true;
						return false;
					}
					m_bLeft = true;
					m_bLeftMatched = false;
					m_nPos = 0;
					m_row = m_ptrLeft->Row();
					Candidates(m_row);
				}

				while (m_nPos < m_pCandidates->size())
				{
					size_t nIndex = (*m_pCandidates)[m_nPos++];
					SetRight(m_rightRows[nIndex]);
					if (Check(m_conditions, m_row))
					{
						m_bLeftMatched = true;
						if (!m_matched.empty())
							m_matched[nIndex] = 1;
						return true;
					}
				}

				m_bLeft = false;
				if (!m_bLeftMatched && (m_type == eJoinType::Left || m_type == eJoinType::Full))
				{
					SetRightNull();
					return true;
				}
			}
		}

		void CJoinOpBase::Close()
		{
			m_ptrLeft->Close();
			m_rightRows.clear();
			m_matched.clear();
		}

		void CNestedLoopJoinOp::Build()
		{
			while (m_ptrRight->Next())
			{
				const TRow& row = m_ptrRight->Row();
				m_rightRows.emplace_back(row.begin() + m_nRightBegin, row.begin() + m_nRightEnd);
			}
			m_all.resize(m_rightRows.size());
			for (size_t i = 0; i < m_all.size(); ++i)
				m_all[i] = i;
		}

		void CNestedLoopJoinOp::Candidates(const TRow&)
		{
			m_pCandidates = &m_all;
		}

		CHashJoinOp::CHashJoinOp(COperatorPtr ptrLeft, COperatorPtr ptrRight, eJoinType type, const std::vector<CExprPtr>& conditions,
			uint32_t nLeftBegin, uint32_t nLeftEnd, uint32_t nRightBegin, uint32_t nRightEnd,
			const std::vector<CExprPtr>& leftKeys, const std::vector<CExprPtr>& rightKeys) :
			CJoinOpBase(ptrLeft, ptrRight, type, conditions, nLeftBegin, nLeftEnd, nRightBegin, nRightEnd),
			m_leftKeys(leftKeys), m_rightKeys(rightKeys)
		{
		}

		void CHashJoinOp::Build()
		{
			m_hash.clear();
			while (m_ptrRight->Next())
			{
				const TRow& row = m_ptrRight->Row();
				SEvalContext ctx = Ctx(row);
				TRow key;
				bool bNull = false;
				for (const CExprPtr& ptrKey : m_rightKeys)
				{
					key.push_back(ptrKey->Eval(ctx));
					bNull = bNull || key.back().IsNull();
				}
				m_rightRows.emplace_back(row.begin() + m_nRightBegin, row.begin() + m_nRightEnd);
				if (!bNull)       // NULL never equals
					m_hash[key].push_back(m_rightRows.size() - 1);
			}
		}

		void CHashJoinOp::Candidates(const TRow& leftRow)
		{
			SEvalContext ctx = Ctx(leftRow);
			TRow key;
			for (const CExprPtr& ptrKey : m_leftKeys)
			{
				key.push_back(ptrKey->Eval(ctx));
				if (key.back().IsNull())
				{
					m_pCandidates = &m_none;
					return;
				}
			}
			auto it = m_hash.find(key);
			m_pCandidates = it == m_hash.end() ? &m_none : &it->second;
		}

		CIndexJoinOp::CIndexJoinOp(COperatorPtr ptrLeft, std::shared_ptr<CTableScanOp> ptrRight, eJoinType type, const std::vector<CExprPtr>& conditions,
			uint32_t nRightBegin, uint32_t nRightEnd) :
			COperator(ptrLeft->Width()), m_ptrLeft(ptrLeft), m_ptrRight(ptrRight), m_type(type), m_conditions(conditions),
			m_nRightBegin(nRightBegin), m_nRightEnd(nRightEnd)
		{
		}

		void CIndexJoinOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_ptrLeft->Open(exec, pOuter);
			m_bLeft = false;
		}

		bool CIndexJoinOp::Next()
		{
			for (;;)
			{
				if (!m_bLeft)
				{
					if (!m_ptrLeft->Next())
						return false;
					m_bLeft = true;
					m_bMatched = false;
					m_row = m_ptrLeft->Row();
					m_ptrRight->OpenWithKeys(*m_pExec, m_pOuter, m_row);
				}
				while (m_ptrRight->Next())
				{
					const TRow& right = m_ptrRight->Row();
					std::copy(right.begin() + m_nRightBegin, right.begin() + m_nRightEnd, m_row.begin() + m_nRightBegin);
					if (Check(m_conditions, m_row))
					{
						m_bMatched = true;
						return true;
					}
				}
				m_ptrRight->Close();
				m_bLeft = false;
				if (!m_bMatched && m_type == eJoinType::Left)
				{
					for (uint32_t i = m_nRightBegin; i < m_nRightEnd; ++i)
						m_row[i] = TValue();
					return true;
				}
			}
		}

		void CIndexJoinOp::Close()
		{
			m_ptrLeft->Close();
			m_ptrRight->Close();
		}

		//------------------------------------------------------------------------------------------
		// aggregation

		CAggregateOp::CAggregateOp(COperatorPtr ptrChild, const std::vector<CExprPtr>& groups, const std::vector<std::shared_ptr<CAggregateExpr> >& aggregates) :
			COperator((uint32_t)(groups.size() + aggregates.size())), m_ptrChild(ptrChild), m_groupExprs(groups), m_aggregates(aggregates)
		{
		}

		void CAggregateOp::Add(SState& state, const CAggregateExpr& agg, const TValue& value)
		{
			if (agg.m_ptrArg.get() == nullptr)       // COUNT(*)
			{
				++state.m_nCount;
				return;
			}
			if (value.IsNull())
				return;
			if (agg.m_bDistinct && !state.m_distinct.insert(value).second)
				return;

			++state.m_nCount;
			switch (agg.m_agg)
			{
			case eAggregate::Count:
				break;
			case eAggregate::Sum:
			case eAggregate::Avg:
				{
					eValueKind kind = ValueKind(value);
					if (kind == eValueKind::Integer && !state.m_bDouble)
					{
						int64_t n = value.Get<int64_t>(), r = 0;
						bool bOverflow = (n > 0 && state.m_nSum > INT64_MAX - n) || (n < 0 && state.m_nSum < INT64_MIN - n);
						if (!bOverflow)
						{
							state.m_nSum += n;
							break;
						}
						(void)r;
						state.m_bDouble = true;
						state.m_dSum = (double)state.m_nSum;
					}
					if (!state.m_bDouble)
					{
						state.m_bDouble = true;
						state.m_dSum = (double)state.m_nSum;
					}
					state.m_dSum += ToDouble(value);
				}
				break;
			case eAggregate::Min:
				if (state.m_value.IsNull() || CompareValues(value, state.m_value) < 0)
					state.m_value = value;
				break;
			case eAggregate::Max:
				if (state.m_value.IsNull() || CompareValues(value, state.m_value) > 0)
					state.m_value = value;
				break;
			}
		}

		TValue CAggregateOp::Result(const SState& state, const CAggregateExpr& agg) const
		{
			switch (agg.m_agg)
			{
			case eAggregate::Count:
				return TValue(state.m_nCount);
			case eAggregate::Sum:
				if (state.m_nCount == 0)
					return TValue();
				return state.m_bDouble ? TValue(state.m_dSum) : TValue(state.m_nSum);
			case eAggregate::Avg:
				if (state.m_nCount == 0)
					return TValue();
				return TValue((state.m_bDouble ? state.m_dSum : (double)state.m_nSum) / (double)state.m_nCount);
			case eAggregate::Min:
			case eAggregate::Max:
				return state.m_value;
			}
			return TValue();
		}

		void CAggregateOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_groups.clear();
			m_index.clear();
			m_nPos = 0;

			m_ptrChild->Open(exec, pOuter);
			while (m_ptrChild->Next())
			{
				SEvalContext ctx = Ctx(m_ptrChild->Row());
				TRow key;
				key.reserve(m_groupExprs.size());
				for (const CExprPtr& ptrGroup : m_groupExprs)
					key.push_back(ptrGroup->Eval(ctx));

				auto it = m_index.find(key);
				size_t nGroup = 0;
				if (it == m_index.end())
				{
					nGroup = m_groups.size();
					m_groups.emplace_back();
					m_groups.back().m_keys = key;
					m_groups.back().m_states.resize(m_aggregates.size());
					m_index.emplace(std::move(key), nGroup);
				}
				else
					nGroup = it->second;

				SGroup& group = m_groups[nGroup];
				for (size_t i = 0; i < m_aggregates.size(); ++i)
				{
					const CAggregateExpr& agg = *m_aggregates[i];
					TValue value = agg.m_ptrArg.get() ? agg.m_ptrArg->Eval(ctx) : TValue();
					Add(group.m_states[i], agg, value);
				}
			}
			m_ptrChild->Close();

			// aggregates without GROUP BY: one row even of no rows
			if (m_groups.empty() && m_groupExprs.empty())
			{
				m_groups.emplace_back();
				m_groups.back().m_states.resize(m_aggregates.size());
			}
		}

		bool CAggregateOp::Next()
		{
			if (m_nPos >= m_groups.size())
				return false;
			const SGroup& group = m_groups[m_nPos++];
			for (size_t i = 0; i < group.m_keys.size(); ++i)
				m_row[i] = group.m_keys[i];
			for (size_t i = 0; i < m_aggregates.size(); ++i)
				m_row[m_groupExprs.size() + i] = Result(group.m_states[i], *m_aggregates[i]);
			return true;
		}

		//------------------------------------------------------------------------------------------

		void CProjectOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_ptrChild->Open(exec, pOuter);
		}

		bool CProjectOp::Next()
		{
			if (!m_ptrChild->Next())
				return false;
			SEvalContext ctx = Ctx(m_ptrChild->Row());
			for (size_t i = 0; i < m_exprs.size(); ++i)
				m_row[i] = m_exprs[i]->Eval(ctx);
			return true;
		}

		void CDistinctOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_seen.clear();
			m_ptrChild->Open(exec, pOuter);
		}

		bool CDistinctOp::Next()
		{
			while (m_ptrChild->Next())
			{
				const TRow& row = m_ptrChild->Row();
				TRow key(row.begin(), row.begin() + m_nColumns);
				if (m_seen.insert(std::move(key)).second)
					return true;
			}
			return false;
		}

		void CSortOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_rows.clear();
			m_nPos = 0;
			m_ptrChild->Open(exec, pOuter);
			while (m_ptrChild->Next())
				m_rows.push_back(m_ptrChild->Row());
			m_ptrChild->Close();

			std::stable_sort(m_rows.begin(), m_rows.end(), [this](const TRow& a, const TRow& b)
			{
				for (const SKey& key : m_keys)
				{
					const TValue& va = a[key.m_nColumn];
					const TValue& vb = b[key.m_nColumn];
					int nCmp = 0;
					if (va.IsNull() || vb.IsNull())
						nCmp = va.IsNull() == vb.IsNull() ? 0 : (va.IsNull() ? -1 : 1);
					else
						nCmp = CompareValues(va, vb);
					if (nCmp != 0)
						return key.m_bDesc ? nCmp > 0 : nCmp < 0;
				}
				return false;
			});
		}

		bool CSortOp::Next()
		{
			if (m_nPos >= m_rows.size())
				return false;
			++m_nPos;
			return true;
		}

		void CLimitOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			TRow empty;
			SEvalContext ctx = Ctx(empty);
			m_nLimit = -1;
			int64_t nOffset = 0;
			if (m_ptrLimit.get())
			{
				TValue limit = m_ptrLimit->Eval(ctx);
				if (!limit.IsNull())
					m_nLimit = ToInt64(limit);
			}
			if (m_ptrOffset.get())
			{
				TValue offset = m_ptrOffset->Eval(ctx);
				if (!offset.IsNull())
					nOffset = (std::max<int64_t>)(0, ToInt64(offset));
			}
			m_nCount = 0;
			m_ptrChild->Open(exec, pOuter);
			for (int64_t i = 0; i < nOffset; ++i)
				if (!m_ptrChild->Next())
					break;
		}

		bool CLimitOp::Next()
		{
			if (m_nLimit >= 0 && m_nCount >= m_nLimit)
				return false;
			if (!m_ptrChild->Next())
				return false;
			++m_nCount;
			return true;
		}

		TRow CCompoundOp::Key(const TRow& row) const
		{
			return TRow(row.begin(), row.begin() + m_nColumns);
		}

		void CCompoundOp::Open(SExecContext& exec, const SEvalContext* pOuter)
		{
			SetContext(exec, pOuter);
			m_seen.clear();
			m_rightSet.clear();
			m_bRight = false;
			m_pRow = nullptr;
			if (m_op == eCompoundOp::Intersect || m_op == eCompoundOp::Except)
			{
				m_ptrRight->Open(exec, pOuter);
				while (m_ptrRight->Next())
					m_rightSet.insert(Key(m_ptrRight->Row()));
				m_ptrRight->Close();
			}
			m_ptrLeft->Open(exec, pOuter);
		}

		bool CCompoundOp::Next()
		{
			for (;;)
			{
				COperator& child = m_bRight ? *m_ptrRight : *m_ptrLeft;
				if (!child.Next())
				{
					child.Close();
					if (m_bRight || m_op == eCompoundOp::Intersect || m_op == eCompoundOp::Except)
						return false;
					m_bRight = true;
					m_ptrRight->Open(*m_pExec, m_pOuter);
					continue;
				}
				m_pRow = &child.Row();
				switch (m_op)
				{
				case eCompoundOp::UnionAll:
					return true;
				case eCompoundOp::Union:
					if (m_seen.insert(Key(*m_pRow)).second)
						return true;
					break;
				case eCompoundOp::Intersect:
				case eCompoundOp::Except:
					{
						TRow key = Key(*m_pRow);
						bool bInRight = m_rightSet.count(key) != 0;
						if (bInRight == (m_op == eCompoundOp::Intersect) && m_seen.insert(std::move(key)).second)
							return true;
					}
					break;
				default:
					return true;
				}
			}
		}

		void CCompoundOp::Close()
		{
			m_ptrLeft->Close();
			m_ptrRight->Close();
			m_seen.clear();
			m_rightSet.clear();
		}
	}
}
