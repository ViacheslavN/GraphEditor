#include "Compiler.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace bptreedb
{
	namespace sql
	{
		namespace
		{
			const size_t MaxSources = 64;

			std::string Upper(std::string s)
			{
				std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
				return s;
			}

			bool EqualNoCase(const std::string& a, const std::string& b)
			{
				if (a.size() != b.size())
					return false;
				for (size_t i = 0; i < a.size(); ++i)
					if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i]))
						return false;
				return true;
			}

			eDataTypes NormType(eDataTypes type)
			{
				if (IsIntegerType(type))
					return dtInteger64;
				if (type == dtFloat)
					return dtDouble;
				return type;
			}

			OpType ToOp(eBinaryOp op)
			{
				switch (op)
				{
				case eBinaryOp::Equal: return OpEqual;
				case eBinaryOp::NotEqual: return OpNoEqual;
				case eBinaryOp::Less: return OpLess;
				case eBinaryOp::LessOrEqual: return OpLessOrEqual;
				case eBinaryOp::Greater: return OpGreater;
				default: return OpGreaterOrEqual;
				}
			}

			// a < b is b > a
			OpType Swap(OpType op)
			{
				switch (op)
				{
				case OpLess: return OpGreater;
				case OpLessOrEqual: return OpGreaterOrEqual;
				case OpGreater: return OpLess;
				case OpGreaterOrEqual: return OpLessOrEqual;
				default: return op;
				}
			}

			eSpatialQueryMode FlipMode(eSpatialQueryMode mode)
			{
				if (mode == sqmInside)
					return sqmOver;
				if (mode == sqmOver)
					return sqmInside;
				return mode;
			}

			bool IsSubqueryKind(eBoundExpr kind)
			{
				return kind == eBoundExpr::InSubquery || kind == eBoundExpr::Exists || kind == eBoundExpr::ScalarSubquery;
			}

			bool IsBTreeIndex(eIndexTypes type)
			{
				return type == itUnique || type == itMultiRegular;
			}

			bool IsSpatialIndex(eIndexTypes type)
			{
				return type == itSpatialRTree || type == itSpatialZOrder;
			}

			CColumnExprPtr RowColumn(uint32_t nIndex, eDataTypes type)
			{
				auto ptrColumn = std::make_shared<CColumnExpr>(nullptr, -1, -1);
				ptrColumn->m_nIndex = (int32_t)nIndex;
				ptrColumn->m_type = type;
				return ptrColumn;
			}
		}

		void ThrowAt(const SSourcePos& pos, const std::string& sMessage)
		{
			if (pos.m_nLine == 0)
				throw CommonLib::CExcBase(sMessage);
			throw CommonLib::CExcBase("line " + std::to_string(pos.m_nLine) + ", column " + std::to_string(pos.m_nColumn) + ": " + sMessage);
		}

		//------------------------------------------------------------------------------------------
		// sources and scopes

		struct SSource
		{
			enum eKind { Table, Subquery };

			eKind m_kind{ Table };
			std::string m_sTable;
			std::string m_sAlias;                   // the name of the source in the query
			ITablePtr m_ptrTable;
			IFieldsPtr m_ptrFields;
			SSelectPlan m_plan;                     // Subquery

			std::vector<std::string> m_slots;       // Table: the fields read ("" - the row id)
			std::vector<eDataTypes> m_slotTypes;
			uint32_t m_nOffset{ 0 };
			std::vector<CExprPtr> m_filters;

			uint32_t Width() const { return m_kind == Table ? (uint32_t)m_slots.size() : m_plan.m_nColumns; }

			int32_t Use(const std::string& sField, eDataTypes type)
			{
				for (size_t i = 0; i < m_slots.size(); ++i)
					if (m_slots[i] == sField)
						return (int32_t)i;
				m_slots.push_back(sField);
				m_slotTypes.push_back(type);
				return (int32_t)m_slots.size() - 1;
			}

			// a column by name: the slot (a table: the field is added to the read ones), -1 if there is none
			int32_t Find(const std::string& sName, eDataTypes& type, std::string& sCanonical)
			{
				if (m_kind == Subquery)
				{
					int32_t nFound = -1;
					for (size_t i = 0; i < m_plan.m_nColumns; ++i)
					{
						if (m_plan.m_names[i] == sName)
						{
							nFound = (int32_t)i;
							break;
						}
						if (nFound < 0 && EqualNoCase(m_plan.m_names[i], sName))
							nFound = (int32_t)i;
					}
					if (nFound >= 0)
					{
						type = m_plan.m_types[nFound];
						sCanonical = m_plan.m_names[nFound];
					}
					return nFound;
				}

				int32_t nField = m_ptrFields->FindField(sName);
				if (nField < 0)
				{
					for (int32_t i = 0; i < m_ptrFields->GetFieldCount(); ++i)
						if (EqualNoCase(m_ptrFields->GetField(i)->GetName(), sName))
						{
							nField = i;
							break;
						}
				}
				if (nField >= 0)
				{
					IFieldPtr ptrField = m_ptrFields->GetField(nField);
					type = ptrField->GetType();
					sCanonical = ptrField->GetName();
					return Use(sCanonical, type);
				}
				if (EqualNoCase(sName, "ROWID"))
				{
					type = dtInteger64;
					sCanonical = "rowid";
					return Use(std::string(), dtInteger64);
				}
				return -1;
			}

			bool Has(const std::string& sName) const
			{
				if (m_kind == Subquery)
				{
					for (size_t i = 0; i < m_plan.m_nColumns; ++i)
						if (EqualNoCase(m_plan.m_names[i], sName))
							return true;
					return false;
				}
				if (m_ptrFields->FindField(sName) >= 0 || EqualNoCase(sName, "ROWID"))
					return true;
				for (int32_t i = 0; i < m_ptrFields->GetFieldCount(); ++i)
					if (EqualNoCase(m_ptrFields->GetField(i)->GetName(), sName))
						return true;
				return false;
			}
		};

		class CScope
		{
		public:
			explicit CScope(CScope* pParent) : m_pParent(pParent), m_nLevel(pParent ? pParent->m_nLevel + 1 : 0) {}

			CColumnExprPtr MakeColumn(int32_t nSource, int32_t nSlot, eDataTypes type)
			{
				auto ptrColumn = std::make_shared<CColumnExpr>(this, nSource, nSlot);
				ptrColumn->m_type = type;
				m_columns.push_back(ptrColumn);
				return ptrColumn;
			}

			// the offsets of the sources in the row, the indexes of the columns
			void Finalize()
			{
				m_nWidth = 0;
				for (SSource& source : m_sources)
				{
					source.m_nOffset = m_nWidth;
					m_nWidth += source.Width();
				}
				for (const CColumnExprPtr& ptrColumn : m_columns)
					ptrColumn->m_nIndex = (int32_t)(m_sources[ptrColumn->m_nSource].m_nOffset + ptrColumn->m_nSlot);
			}

			uint32_t RangeBegin(uint32_t nFirst) const { return m_sources[nFirst].m_nOffset; }
			uint32_t RangeEnd(uint32_t nLast) const { return m_sources[nLast].m_nOffset + m_sources[nLast].Width(); }

			CScope* m_pParent;
			uint32_t m_nLevel;
			std::vector<SSource> m_sources;
			std::vector<CColumnExprPtr> m_columns;
			std::set<std::string> m_using;          // the names of USING (upper case): not ambiguous
			bool m_bAggregate{ false };
			uint32_t m_nWidth{ 0 };
		};

		struct CQueryCompiler::SFromNode
		{
			bool m_bLeaf{ true };
			int32_t m_nSource{ -1 };
			eJoinType m_type{ eJoinType::Inner };
			std::unique_ptr<SFromNode> m_ptrLeft;
			std::unique_ptr<SFromNode> m_ptrRight;
			std::vector<CExprPtr> m_conditions;
			std::vector<CExprPtr> m_filtersAbove;   // WHERE conditions above an outer join
			uint64_t m_nMask{ 0 };
			uint32_t m_nFirst{ 0 };                 // the sources of the subtree
			uint32_t m_nLast{ 0 };
		};

		namespace
		{
			const CColumnExpr* ColumnOf(const CExprPtr& ptrExpr, const CScope& scope, int32_t nSource)
			{
				if (ptrExpr.get() == nullptr || ptrExpr->m_kind != eBoundExpr::Column)
					return nullptr;
				const CColumnExpr* pColumn = static_cast<const CColumnExpr*>(ptrExpr.get());
				if (pColumn->m_pScope != &scope || pColumn->m_nSource != nSource)
					return nullptr;
				return pColumn;
			}
		}

		//------------------------------------------------------------------------------------------

		ITablePtr CQueryCompiler::FindTable(const std::string& sName) const
		{
			ITablePtr ptrTable = m_ptrSchema->GetTable(sName);
			if (ptrTable.get())
				return ptrTable;
			for (int32_t i = 0; i < m_ptrSchema->GetTableCount(); ++i)
			{
				ITablePtr ptr = m_ptrSchema->GetTable(i);
				if (EqualNoCase(ptr->GetName(), sName))
					return ptr;
			}
			return ITablePtr();
		}

		ITablePtr CQueryCompiler::GetTable(const std::string& sName, const SSourcePos& pos) const
		{
			ITablePtr ptrTable = FindTable(sName);
			if (ptrTable.get() == nullptr)
				ThrowAt(pos, "no table '" + sName + "'");
			return ptrTable;
		}

		uint64_t CQueryCompiler::MaskOf(const CExprPtr& ptrExpr, const CScope& scope) const
		{
			uint64_t nMask = 0;
			VisitExpr(ptrExpr, [&](const CExprPtr& ptr)
			{
				if (ptr->m_kind == eBoundExpr::Column)
				{
					const CColumnExpr* pColumn = static_cast<const CColumnExpr*>(ptr.get());
					if (pColumn->m_pScope == &scope && pColumn->m_nSource >= 0)
						nMask |= uint64_t(1) << pColumn->m_nSource;
				}
				else if (IsSubqueryKind(ptr->m_kind))
				{
					const CSubqueryExprBase* pSub = static_cast<const CSubqueryExprBase*>(ptr.get());
					for (const auto& ref : pSub->m_ptrPlan->m_outerRefs)
						if (ref.first == &scope)
							nMask |= uint64_t(1) << ref.second;
				}
			});
			return nMask;
		}

		void CQueryCompiler::SplitAnd(const CExprPtr& ptrExpr, std::vector<CExprPtr>& conjuncts)
		{
			if (ptrExpr->m_kind == eBoundExpr::Binary)
			{
				const CBinaryExpr* pBinary = static_cast<const CBinaryExpr*>(ptrExpr.get());
				if (pBinary->m_op == eBinaryOp::And)
				{
					SplitAnd(pBinary->m_ptrLeft, conjuncts);
					SplitAnd(pBinary->m_ptrRight, conjuncts);
					return;
				}
			}
			conjuncts.push_back(ptrExpr);
		}

		//------------------------------------------------------------------------------------------
		// expressions

		CExprPtr CQueryCompiler::Column(const std::string& sTable, const std::string& sColumn, CScope& scope, const SSourcePos& pos)
		{
			uint32_t nDepth = 0;
			for (CScope* pScope = &scope; pScope; pScope = pScope->m_pParent, ++nDepth)
			{
				std::vector<int32_t> candidates;
				bool bTableFound = false;
				for (size_t i = 0; i < pScope->m_sources.size(); ++i)
				{
					SSource& source = pScope->m_sources[i];
					if (!sTable.empty() && !EqualNoCase(source.m_sAlias, sTable))
						continue;
					bTableFound = true;
					if (source.Has(sColumn))
						candidates.push_back((int32_t)i);
				}
				if (!sTable.empty() && bTableFound && candidates.empty())
					ThrowAt(pos, "table '" + sTable + "' has no column '" + sColumn + "'");
				if (candidates.empty())
					continue;
				if (candidates.size() > 1 && !(sTable.empty() && pScope->m_using.count(Upper(sColumn))))
					ThrowAt(pos, "column '" + sColumn + "' is ambiguous");

				int32_t nSource = candidates[0];
				eDataTypes type = dtUnknown;
				std::string sCanonical;
				int32_t nSlot = pScope->m_sources[nSource].Find(sColumn, type, sCanonical);
				CColumnExprPtr ptrColumn = pScope->MakeColumn(nSource, nSlot, type);
				ptrColumn->m_pos = pos;
				if (nDepth == 0)
					return ptrColumn;

				// a column of an outer query: the subqueries being compiled read it
				for (const SCollector& collector : m_collectors)
				{
					collector.m_pPlan->m_outerRefs.emplace_back(pScope, nSource);
					if (pScope->m_nLevel <= collector.m_nLevel)
						collector.m_pPlan->m_bCorrelated = true;
				}
				auto ptrOuter = std::make_shared<COuterColumnExpr>(nDepth, ptrColumn);
				ptrOuter->m_type = type;
				ptrOuter->m_pos = pos;
				return ptrOuter;
			}
			if (!sTable.empty())
				ThrowAt(pos, "no table or alias '" + sTable + "' for column '" + sColumn + "'");
			ThrowAt(pos, "no column '" + sColumn + "'");
		}

		SSubqueryPlanPtr CQueryCompiler::CompileSubquery(const SSelectStmt& stmt, CScope& scope, bool bOneColumn)
		{
			auto ptrPlan = std::make_shared<SSubqueryPlan>();
			m_collectors.push_back(SCollector{ ptrPlan.get(), scope.m_nLevel });
			SSelectPlan plan;
			try
			{
				plan = CompileSelect(stmt, &scope);
			}
			catch (...)
			{
				m_collectors.pop_back();
				throw;
			}
			m_collectors.pop_back();

			if (bOneColumn && plan.m_nColumns != 1)
				ThrowAt(stmt.m_pos, "the subquery has to return one column, it has " + std::to_string(plan.m_nColumns));
			ptrPlan->m_ptrRoot = plan.m_ptrRoot;
			ptrPlan->m_nColumns = plan.m_nColumns;
			return ptrPlan;
		}

		CExprPtr CQueryCompiler::Expr(const SExprPtr& ptrAst, CScope& scope, bool bAggregates)
		{
			CExprPtr ptrResult;
			const SExpr& ast = *ptrAst;
			switch (ast.m_type)
			{
			case eExprType::Literal:
				{
					const SLiteral& literal = static_cast<const SLiteral&>(ast);
					switch (literal.m_kind)
					{
					case SLiteral::Null: ptrResult = std::make_shared<CConstExpr>(TValue()); break;
					case SLiteral::Integer: ptrResult = std::make_shared<CConstExpr>(TValue(literal.m_nValue)); break;
					case SLiteral::Double: ptrResult = std::make_shared<CConstExpr>(TValue(literal.m_dValue)); break;
					case SLiteral::String: ptrResult = std::make_shared<CConstExpr>(TValue(literal.m_sValue)); break;
					case SLiteral::Bool: ptrResult = std::make_shared<CConstExpr>(TValue(literal.m_nValue)); break;
					}
				}
				break;
			case eExprType::Parameter:
				ptrResult = std::make_shared<CParamExpr>(static_cast<const SParameter&>(ast).m_nIndex);
				break;
			case eExprType::Column:
				{
					const SColumnRef& ref = static_cast<const SColumnRef&>(ast);
					ptrResult = Column(ref.m_sTable, ref.m_sColumn, scope, ast.m_pos);
				}
				break;
			case eExprType::Star:
				ThrowAt(ast.m_pos, "* is allowed only in the select list and COUNT(*)");
			case eExprType::Unary:
				{
					const SUnaryExpr& unary = static_cast<const SUnaryExpr&>(ast);
					CExprPtr ptrOperand = Expr(unary.m_ptrOperand, scope, bAggregates);
					ptrResult = std::make_shared<CUnaryExpr>(unary.m_op, ptrOperand);
					ptrResult->m_type = unary.m_op == eUnaryOp::Not ? dtInteger64 : NormType(ptrOperand->m_type);
				}
				break;
			case eExprType::Binary:
				{
					const SBinaryExpr& binary = static_cast<const SBinaryExpr&>(ast);
					CExprPtr ptrLeft = Expr(binary.m_ptrLeft, scope, bAggregates);
					CExprPtr ptrRight = Expr(binary.m_ptrRight, scope, bAggregates);
					auto ptrBinary = std::make_shared<CBinaryExpr>(binary.m_op, ptrLeft, ptrRight);
					eDataTypes left = NormType(ptrLeft->m_type), right = NormType(ptrRight->m_type);
					switch (binary.m_op)
					{
					case eBinaryOp::Concat:
						ptrBinary->m_type = dtString;
						break;
					case eBinaryOp::Add: case eBinaryOp::Subtract: case eBinaryOp::Multiply: case eBinaryOp::Divide: case eBinaryOp::Modulo:
						if (left == dtInteger64 && right == dtInteger64)
							ptrBinary->m_type = dtInteger64;
						else if ((left == dtDouble || left == dtInteger64) && (right == dtDouble || right == dtInteger64))
							ptrBinary->m_type = dtDouble;
						break;
					default:
						ptrBinary->m_type = dtInteger64;
						break;
					}
					ptrResult = ptrBinary;
				}
				break;
			case eExprType::Function:
				ptrResult = Function(static_cast<const SFunctionCall&>(ast), scope, bAggregates);
				break;
			case eExprType::IsNull:
				{
					const SIsNullExpr& isNull = static_cast<const SIsNullExpr&>(ast);
					ptrResult = std::make_shared<CIsNullExpr>(Expr(isNull.m_ptrOperand, scope, bAggregates), isNull.m_bNot);
				}
				break;
			case eExprType::Between:
				{
					const SBetweenExpr& between = static_cast<const SBetweenExpr&>(ast);
					ptrResult = std::make_shared<CBetweenExpr>(Expr(between.m_ptrOperand, scope, bAggregates), Expr(between.m_ptrLow, scope, bAggregates),
						Expr(between.m_ptrHigh, scope, bAggregates), between.m_bNot);
				}
				break;
			case eExprType::InList:
				{
					const SInListExpr& in = static_cast<const SInListExpr&>(ast);
					std::vector<CExprPtr> list;
					for (const SExprPtr& ptrItem : in.m_list)
						list.push_back(Expr(ptrItem, scope, bAggregates));
					ptrResult = std::make_shared<CInListExpr>(Expr(in.m_ptrOperand, scope, bAggregates), list, in.m_bNot);
				}
				break;
			case eExprType::InSelect:
				{
					const SInSelectExpr& in = static_cast<const SInSelectExpr&>(ast);
					CExprPtr ptrOperand = Expr(in.m_ptrOperand, scope, bAggregates);
					ptrResult = std::make_shared<CInSubqueryExpr>(ptrOperand, CompileSubquery(*in.m_ptrSelect, scope, true), in.m_bNot);
				}
				break;
			case eExprType::Like:
				{
					const SLikeExpr& like = static_cast<const SLikeExpr&>(ast);
					ptrResult = std::make_shared<CLikeExpr>(Expr(like.m_ptrOperand, scope, bAggregates), Expr(like.m_ptrPattern, scope, bAggregates),
						like.m_ptrEscape.get() ? Expr(like.m_ptrEscape, scope, bAggregates) : CExprPtr(), like.m_bNot);
				}
				break;
			case eExprType::Case:
				{
					const SCaseExpr& caseAst = static_cast<const SCaseExpr&>(ast);
					auto ptrCase = std::make_shared<CCaseExpr>();
					if (caseAst.m_ptrOperand.get())
						ptrCase->m_ptrOperand = Expr(caseAst.m_ptrOperand, scope, bAggregates);
					eDataTypes type = dtUnknown;
					bool bFirst = true;
					for (const SWhenClause& when : caseAst.m_whens)
					{
						CExprPtr ptrThen = Expr(when.m_ptrThen, scope, bAggregates);
						eDataTypes thenType = NormType(ptrThen->m_type);
						type = bFirst ? thenType : (type == thenType ? type : dtUnknown);
						bFirst = false;
						ptrCase->m_whens.emplace_back(Expr(when.m_ptrWhen, scope, bAggregates), ptrThen);
					}
					if (caseAst.m_ptrElse.get())
					{
						ptrCase->m_ptrElse = Expr(caseAst.m_ptrElse, scope, bAggregates);
						if (NormType(ptrCase->m_ptrElse->m_type) != type)
							type = dtUnknown;
					}
					ptrCase->m_type = type;
					ptrResult = ptrCase;
				}
				break;
			case eExprType::Cast:
				{
					const SCastExpr& cast = static_cast<const SCastExpr&>(ast);
					eDataTypes type = dtUnknown;
					uint32_t nLength = 0;
					bool bKnown = false;
					try
					{
						bKnown = MapTypeName(cast.m_type, type, nLength);
					}
					catch (std::exception& e)
					{
						ThrowAt(ast.m_pos, e.what());
					}
					if (!bKnown)
						ThrowAt(ast.m_pos, "unknown type " + cast.m_type.m_sName);
					ptrResult = std::make_shared<CCastExpr>(Expr(cast.m_ptrOperand, scope, bAggregates), NormType(type));
				}
				break;
			case eExprType::Exists:
				ptrResult = std::make_shared<CExistsExpr>(CompileSubquery(*static_cast<const SExistsExpr&>(ast).m_ptrSelect, scope, false));
				break;
			case eExprType::Subquery:
				{
					const SSelectStmt& select = *static_cast<const SSubqueryExpr&>(ast).m_ptrSelect;
					ptrResult = std::make_shared<CScalarSubqueryExpr>(CompileSubquery(select, scope, true));
				}
				break;
			}
			ptrResult->m_pos = ast.m_pos;
			return ptrResult;
		}

		CExprPtr CQueryCompiler::Function(const SFunctionCall& call, CScope& scope, bool bAggregates)
		{
			const std::string& sName = call.m_sName;
			eAggregate agg = eAggregate::Count;
			bool bScalarMinMax = (sName == "MIN" || sName == "MAX") && call.m_args.size() > 1;
			if (IsAggregateName(sName, agg) && !bScalarMinMax)
			{
				if (!bAggregates)
					ThrowAt(call.m_pos, "aggregate " + sName + " isn't allowed here");
				CExprPtr ptrArg;
				if (call.m_bStar)
				{
					if (agg != eAggregate::Count)
						ThrowAt(call.m_pos, sName + "(*) isn't allowed");
				}
				else
				{
					if (call.m_args.size() != 1)
						ThrowAt(call.m_pos, sName + " takes one argument");
					ptrArg = Expr(call.m_args[0], scope, false);
				}
				scope.m_bAggregate = true;
				auto ptrAgg = std::make_shared<CAggregateExpr>(agg, ptrArg, call.m_bDistinct);
				eDataTypes argType = ptrArg.get() ? NormType(ptrArg->m_type) : dtUnknown;
				switch (agg)
				{
				case eAggregate::Count: ptrAgg->m_type = dtInteger64; break;
				case eAggregate::Sum: ptrAgg->m_type = argType == dtInteger64 ? dtInteger64 : (argType == dtDouble ? dtDouble : dtUnknown); break;
				case eAggregate::Avg: ptrAgg->m_type = dtDouble; break;
				default: ptrAgg->m_type = argType; break;
				}
				return ptrAgg;
			}

			if (sName == "ST_INTERSECTS" || sName == "ST_WITHIN" || sName == "ST_CONTAINS")
				return Spatial(call, scope, bAggregates);
			if (sName == "ST_MAKEENVELOPE")
				ThrowAt(call.m_pos, "ST_MakeEnvelope is an argument of ST_Intersects, ST_Within or ST_Contains only");
			if (call.m_bStar)
				ThrowAt(call.m_pos, sName + "(*) isn't allowed");
			if (call.m_bDistinct)
				ThrowAt(call.m_pos, "DISTINCT is allowed in aggregates only");

			TScalarFunction func;
			bool bNullInNullOut = true;
			std::string sLookup = bScalarMinMax ? (sName == "MIN" ? "LEAST" : "GREATEST") : sName;
			bool bFound = false;
			try
			{
				bFound = FindScalarFunction(sLookup, call.m_args.size(), func, bNullInNullOut);
			}
			catch (std::exception& e)
			{
				ThrowAt(call.m_pos, e.what());
			}
			if (!bFound)
				ThrowAt(call.m_pos, "unknown function " + sName);

			auto ptrFunc = std::make_shared<CFunctionExpr>(sName, func, bNullInNullOut);
			for (const SExprPtr& ptrArg : call.m_args)
				ptrFunc->m_args.push_back(Expr(ptrArg, scope, bAggregates));
			ptrFunc->m_type = FunctionType(sLookup, ptrFunc->m_args);
			return ptrFunc;
		}

		CExprPtr CQueryCompiler::Spatial(const SFunctionCall& call, CScope& scope, bool bAggregates)
		{
			if (call.m_args.size() != 2 || call.m_bStar || call.m_bDistinct)
				ThrowAt(call.m_pos, call.m_sName + " takes two arguments");

			auto IsEnvelope = [](const SExprPtr& ptrArg)
			{
				return ptrArg->m_type == eExprType::Function && static_cast<const SFunctionCall&>(*ptrArg).m_sName == "ST_MAKEENVELOPE";
			};

			eSpatialQueryMode mode = call.m_sName == "ST_INTERSECTS" ? sqmIntersect : (call.m_sName == "ST_WITHIN" ? sqmInside : sqmOver);
			bool bEnvelope0 = IsEnvelope(call.m_args[0]), bEnvelope1 = IsEnvelope(call.m_args[1]);
			if (bEnvelope0 && bEnvelope1)
				ThrowAt(call.m_pos, call.m_sName + " of two envelopes");

			SExprPtr ptrGeomAst = call.m_args[0], ptrOtherAst = call.m_args[1];
			if (bEnvelope0)
			{
				std::swap(ptrGeomAst, ptrOtherAst);
				mode = FlipMode(mode);
			}

			auto ptrSpatial = std::make_shared<CSpatialExpr>(mode);
			ptrSpatial->m_ptrGeom = Expr(ptrGeomAst, scope, bAggregates);
			if (IsEnvelope(ptrOtherAst))
			{
				const SFunctionCall& envelope = static_cast<const SFunctionCall&>(*ptrOtherAst);
				if (envelope.m_args.size() != 4)
					ThrowAt(envelope.m_pos, "ST_MakeEnvelope takes four arguments: xmin, ymin, xmax, ymax");
				for (const SExprPtr& ptrArg : envelope.m_args)
					ptrSpatial->m_envelope.push_back(Expr(ptrArg, scope, bAggregates));
			}
			else
				ptrSpatial->m_ptrOther = Expr(ptrOtherAst, scope, bAggregates);
			return ptrSpatial;
		}

		CExprPtr CQueryCompiler::CompileValue(const SExprPtr& ptrExpr)
		{
			CScope scope(nullptr);
			return Expr(ptrExpr, scope, false);
		}

		//------------------------------------------------------------------------------------------
		// FROM

		std::unique_ptr<CQueryCompiler::SFromNode> CQueryCompiler::CompileFrom(const STableRefPtr& ptrRef, CScope& scope)
		{
			std::unique_ptr<SFromNode> ptrNode(new SFromNode());
			switch (ptrRef->m_type)
			{
			case eTableRefType::Table:
			case eTableRefType::Subquery:
				{
					SSource source;
					if (ptrRef->m_type == eTableRefType::Table)
					{
						const STableName& name = static_cast<const STableName&>(*ptrRef);
						source.m_ptrTable = GetTable(name.m_sName, name.m_pos);
						source.m_ptrFields = source.m_ptrTable->GetFields();
						source.m_sTable = source.m_ptrTable->GetName();
						source.m_sAlias = name.m_sAlias.empty() ? source.m_sTable : name.m_sAlias;
						m_tablesRead.insert(source.m_sTable);
					}
					else
					{
						const SSubqueryRef& sub = static_cast<const SSubqueryRef&>(*ptrRef);
						source.m_kind = SSource::Subquery;
						// a subquery in FROM doesn't see the other sources of the query (no LATERAL)
						source.m_plan = CompileSelect(*sub.m_ptrSelect, scope.m_pParent);
						source.m_sAlias = sub.m_sAlias;
					}
					for (const SSource& other : scope.m_sources)
						if (!source.m_sAlias.empty() && EqualNoCase(other.m_sAlias, source.m_sAlias))
							ThrowAt(ptrRef->m_pos, "the name '" + source.m_sAlias + "' is used by two tables of FROM: use an alias");
					if (scope.m_sources.size() >= MaxSources)
						ThrowAt(ptrRef->m_pos, "a query can have up to 64 tables");

					ptrNode->m_nSource = (int32_t)scope.m_sources.size();
					ptrNode->m_nMask = uint64_t(1) << ptrNode->m_nSource;
					ptrNode->m_nFirst = ptrNode->m_nLast = (uint32_t)ptrNode->m_nSource;
					scope.m_sources.push_back(std::move(source));
				}
				break;
			case eTableRefType::Join:
				{
					const SJoinRef& join = static_cast<const SJoinRef&>(*ptrRef);
					ptrNode->m_bLeaf = false;
					ptrNode->m_type = join.m_joinType;
					ptrNode->m_ptrLeft = CompileFrom(join.m_ptrLeft, scope);
					ptrNode->m_ptrRight = CompileFrom(join.m_ptrRight, scope);
					ptrNode->m_nMask = ptrNode->m_ptrLeft->m_nMask | ptrNode->m_ptrRight->m_nMask;
					ptrNode->m_nFirst = ptrNode->m_ptrLeft->m_nFirst;
					ptrNode->m_nLast = ptrNode->m_ptrRight->m_nLast;

					if (join.m_ptrCondition.get())
					{
						std::vector<CExprPtr> conjuncts;
						SplitAnd(Expr(join.m_ptrCondition, scope, false), conjuncts);
						for (const CExprPtr& ptrCond : conjuncts)
							PlaceOn(ptrCond, *ptrNode, scope);
					}

					// USING (a, b): left.a = right.a AND left.b = right.b
					for (const std::string& sName : join.m_using)
					{
						CExprPtr sides[2];
						const SFromNode* nodes[2] = { ptrNode->m_ptrLeft.get(), ptrNode->m_ptrRight.get() };
						for (int nSide = 0; nSide < 2; ++nSide)
						{
							int32_t nFound = -1;
							for (uint32_t i = nodes[nSide]->m_nFirst; i <= nodes[nSide]->m_nLast; ++i)
							{
								if (!scope.m_sources[i].Has(sName))
									continue;
								if (nFound >= 0)
									ThrowAt(join.m_pos, "USING column '" + sName + "' is in two tables of a side of the join");
								nFound = (int32_t)i;
							}
							if (nFound < 0)
								ThrowAt(join.m_pos, "USING column '" + sName + "' isn't in a side of the join");
							eDataTypes type = dtUnknown;
							std::string sCanonical;
							int32_t nSlot = scope.m_sources[nFound].Find(sName, type, sCanonical);
							sides[nSide] = scope.MakeColumn(nFound, nSlot, type);
						}
						auto ptrEqual = std::make_shared<CBinaryExpr>(eBinaryOp::Equal, sides[0], sides[1]);
						ptrEqual->m_type = dtInteger64;
						ptrEqual->m_pos = join.m_pos;
						PlaceOn(ptrEqual, *ptrNode, scope);
						scope.m_using.insert(Upper(sName));
					}
				}
				break;
			}
			return ptrNode;
		}

		void CQueryCompiler::PlaceOn(const CExprPtr& ptrCond, SFromNode& node, CScope& scope)
		{
			uint64_t nMask = MaskOf(ptrCond, scope);
			if (nMask & ~node.m_nMask)
				ThrowAt(ptrCond->m_pos, "the ON condition uses a table that isn't in its join");
			uint64_t nLeft = node.m_ptrLeft->m_nMask, nRight = node.m_ptrRight->m_nMask;
			switch (node.m_type)
			{
			case eJoinType::Inner:
			case eJoinType::Cross:
				if (nMask == 0)
					node.m_conditions.push_back(ptrCond);
				else
					Place(ptrCond, node, scope);
				break;
			case eJoinType::Left:
				// a condition of the right side filters it before the join
				if (nMask != 0 && (nMask & ~nRight) == 0)
					Place(ptrCond, *node.m_ptrRight, scope);
				else
					node.m_conditions.push_back(ptrCond);
				break;
			case eJoinType::Right:
				if (nMask != 0 && (nMask & ~nLeft) == 0)
					Place(ptrCond, *node.m_ptrLeft, scope);
				else
					node.m_conditions.push_back(ptrCond);
				break;
			case eJoinType::Full:
				node.m_conditions.push_back(ptrCond);
				break;
			}
		}

		// a condition (of WHERE, or of ON of an inner join) as low as possible: into a side of an inner join, into
		// the preserved side of an outer join, else at the join (an inner join) or above it (an outer join)
		void CQueryCompiler::Place(const CExprPtr& ptrCond, SFromNode& node, CScope& scope)
		{
			if (node.m_bLeaf)
			{
				scope.m_sources[node.m_nSource].m_filters.push_back(ptrCond);
				return;
			}
			uint64_t nMask = MaskOf(ptrCond, scope);
			bool bInLeft = nMask != 0 && (nMask & ~node.m_ptrLeft->m_nMask) == 0;
			bool bInRight = nMask != 0 && (nMask & ~node.m_ptrRight->m_nMask) == 0;
			switch (node.m_type)
			{
			case eJoinType::Inner:
			case eJoinType::Cross:
				if (bInLeft)
					Place(ptrCond, *node.m_ptrLeft, scope);
				else if (bInRight)
					Place(ptrCond, *node.m_ptrRight, scope);
				else
				{
					node.m_conditions.push_back(ptrCond);
					node.m_type = eJoinType::Inner;
				}
				break;
			case eJoinType::Left:
				if (bInLeft)
					Place(ptrCond, *node.m_ptrLeft, scope);
				else
					node.m_filtersAbove.push_back(ptrCond);
				break;
			case eJoinType::Right:
				if (bInRight)
					Place(ptrCond, *node.m_ptrRight, scope);
				else
					node.m_filtersAbove.push_back(ptrCond);
				break;
			case eJoinType::Full:
				node.m_filtersAbove.push_back(ptrCond);
				break;
			}
		}

		// the conditions of a table with a value that doesn't depend on the rows of the query: the best index
		SAccessPath CQueryCompiler::ChooseAccessPath(CScope& scope, int32_t nSource)
		{
			SSource& source = scope.m_sources[nSource];
			SAccessPath best;
			int nBest = 100;

			auto Index = [&](const std::string& sField, eIndexTypes& type) -> bool
			{
				if (sField.empty())
					return false;
				IIndexPtr ptrIndex = source.m_ptrTable->GetIndex(sField);
				if (ptrIndex.get() == nullptr)
					return false;
				type = ptrIndex->GetType();
				return true;
			};
			auto Consider = [&](int nPriority, const SAccessPath& path)
			{
				if (nPriority < nBest)
				{
					nBest = nPriority;
					best = path;
				}
			};

			for (const CExprPtr& ptrCond : source.m_filters)
			{
				switch (ptrCond->m_kind)
				{
				case eBoundExpr::Binary:
					{
						const CBinaryExpr& binary = static_cast<const CBinaryExpr&>(*ptrCond);
						if (!binary.IsComparison() || binary.m_op == eBinaryOp::NotEqual)
							break;
						OpType op = ToOp(binary.m_op);
						const CColumnExpr* pColumn = ColumnOf(binary.m_ptrLeft, scope, nSource);
						CExprPtr ptrKey = binary.m_ptrRight;
						if (pColumn == nullptr || MaskOf(ptrKey, scope) != 0)
						{
							pColumn = ColumnOf(binary.m_ptrRight, scope, nSource);
							ptrKey = binary.m_ptrLeft;
							op = Swap(op);
						}
						if (pColumn == nullptr || MaskOf(ptrKey, scope) != 0)
							break;

						const std::string& sField = source.m_slots[pColumn->m_nSlot];
						SAccessPath path;
						path.m_keys.push_back(ptrKey);
						if (sField.empty())
						{
							if (op == OpEqual)
							{
								path.m_kind = SAccessPath::RowIds;
								Consider(0, path);
							}
							break;
						}
						eIndexTypes type = itMultiRegular;
						if (!Index(sField, type) || !IsBTreeIndex(type))
							break;
						path.m_kind = SAccessPath::Seek;
						path.m_sField = sField;
						path.m_fieldType = source.m_slotTypes[pColumn->m_nSlot];
						path.m_op = op;
						Consider(op == OpEqual ? (type == itUnique ? 1 : 2) : 5, path);
					}
					break;
				case eBoundExpr::InList:
					{
						const CInListExpr& in = static_cast<const CInListExpr&>(*ptrCond);
						const CColumnExpr* pColumn = ColumnOf(in.m_ptrOperand, scope, nSource);
						if (in.m_bNot || pColumn == nullptr)
							break;
						bool bConst = true;
						for (const CExprPtr& ptrItem : in.m_list)
							bConst = bConst && MaskOf(ptrItem, scope) == 0;
						if (!bConst)
							break;
						const std::string& sField = source.m_slots[pColumn->m_nSlot];
						SAccessPath path;
						path.m_keys = in.m_list;
						if (sField.empty())
						{
							path.m_kind = SAccessPath::RowIds;
							Consider(0, path);
							break;
						}
						eIndexTypes type = itMultiRegular;
						if (!Index(sField, type) || !IsBTreeIndex(type))
							break;
						path.m_kind = SAccessPath::InList;
						path.m_sField = sField;
						path.m_fieldType = source.m_slotTypes[pColumn->m_nSlot];
						path.m_op = OpEqual;
						Consider(3, path);
					}
					break;
				case eBoundExpr::Between:
					{
						const CBetweenExpr& between = static_cast<const CBetweenExpr&>(*ptrCond);
						const CColumnExpr* pColumn = ColumnOf(between.m_ptrOperand, scope, nSource);
						if (between.m_bNot || pColumn == nullptr || MaskOf(between.m_ptrLow, scope) != 0)
							break;
						const std::string& sField = source.m_slots[pColumn->m_nSlot];
						eIndexTypes type = itMultiRegular;
						if (!Index(sField, type) || !IsBTreeIndex(type))
							break;
						SAccessPath path;
						path.m_kind = SAccessPath::Seek;
						path.m_sField = sField;
						path.m_fieldType = source.m_slotTypes[pColumn->m_nSlot];
						path.m_op = OpGreaterOrEqual;
						path.m_keys.push_back(between.m_ptrLow);
						Consider(5, path);
					}
					break;
				case eBoundExpr::Spatial:
					{
						const CSpatialExpr& spatial = static_cast<const CSpatialExpr&>(*ptrCond);
						const CColumnExpr* pColumn = ColumnOf(spatial.m_ptrGeom, scope, nSource);
						if (pColumn == nullptr)
							break;
						bool bConst = spatial.m_ptrOther.get() ? MaskOf(spatial.m_ptrOther, scope) == 0 : true;
						for (const CExprPtr& ptrExpr : spatial.m_envelope)
							bConst = bConst && MaskOf(ptrExpr, scope) == 0;
						const std::string& sField = source.m_slots[pColumn->m_nSlot];
						eIndexTypes type = itMultiRegular;
						if (!bConst || !Index(sField, type) || !IsSpatialIndex(type))
							break;
						SAccessPath path;
						path.m_kind = SAccessPath::Spatial;
						path.m_sField = sField;
						path.m_fieldType = dtGeometry;
						path.m_mode = spatial.m_mode;
						path.m_ptrSpatial = ptrCond;
						Consider(4, path);
					}
					break;
				default:
					break;
				}
			}
			return best;
		}

		COperatorPtr CQueryCompiler::Build(SFromNode& node, CScope& scope)
		{
			COperatorPtr ptrOp;
			if (node.m_bLeaf)
			{
				SSource& source = scope.m_sources[node.m_nSource];
				if (source.m_kind == SSource::Table)
				{
					std::vector<CTableScanOp::SSlot> slots;
					for (const std::string& sField : source.m_slots)
						slots.push_back(CTableScanOp::SSlot{ sField });
					ptrOp = std::make_shared<CTableScanOp>(source.m_sTable, slots, source.m_nOffset, scope.m_nWidth,
						ChooseAccessPath(scope, node.m_nSource), source.m_filters);
				}
				else
					ptrOp = std::make_shared<CSubquerySourceOp>(source.m_plan.m_ptrRoot, source.m_plan.m_nColumns, source.m_nOffset, scope.m_nWidth, source.m_filters);
				return ptrOp;
			}

			SFromNode& left = *node.m_ptrLeft;
			SFromNode& right = *node.m_ptrRight;
			COperatorPtr ptrLeft = Build(left, scope);
			uint32_t nLeftBegin = scope.RangeBegin(left.m_nFirst), nLeftEnd = scope.RangeEnd(left.m_nLast);
			uint32_t nRightBegin = scope.RangeBegin(right.m_nFirst), nRightEnd = scope.RangeEnd(right.m_nLast);

			// equalities of a left and a right expression
			std::vector<CExprPtr> leftKeys, rightKeys, others;
			for (const CExprPtr& ptrCond : node.m_conditions)
			{
				if (ptrCond->m_kind == eBoundExpr::Binary)
				{
					const CBinaryExpr& binary = static_cast<const CBinaryExpr&>(*ptrCond);
					if (binary.m_op == eBinaryOp::Equal)
					{
						uint64_t nL = MaskOf(binary.m_ptrLeft, scope), nR = MaskOf(binary.m_ptrRight, scope);
						if (nL && nR && (nL & ~left.m_nMask) == 0 && (nR & ~right.m_nMask) == 0)
						{
							leftKeys.push_back(binary.m_ptrLeft);
							rightKeys.push_back(binary.m_ptrRight);
							continue;
						}
						if (nL && nR && (nL & ~right.m_nMask) == 0 && (nR & ~left.m_nMask) == 0)
						{
							leftKeys.push_back(binary.m_ptrRight);
							rightKeys.push_back(binary.m_ptrLeft);
							continue;
						}
					}
				}
				others.push_back(ptrCond);
			}

			// an index join: the right side is a table with an index on the equality (or a spatial index)
			if (right.m_bLeaf && scope.m_sources[right.m_nSource].m_kind == SSource::Table &&
				(node.m_type == eJoinType::Inner || node.m_type == eJoinType::Left))
			{
				SSource& source = scope.m_sources[right.m_nSource];
				SAccessPath path;
				int nBest = 100;
				for (size_t i = 0; i < rightKeys.size(); ++i)
				{
					const CColumnExpr* pColumn = ColumnOf(rightKeys[i], scope, right.m_nSource);
					if (pColumn == nullptr)
						continue;
					const std::string& sField = source.m_slots[pColumn->m_nSlot];
					SAccessPath candidate;
					candidate.m_keys.push_back(leftKeys[i]);
					candidate.m_bKeysFromRow = true;
					int nPriority = 100;
					if (sField.empty())
					{
						candidate.m_kind = SAccessPath::RowIds;
						nPriority = 0;
					}
					else
					{
						IIndexPtr ptrIndex = source.m_ptrTable->GetIndex(sField);
						if (ptrIndex.get() == nullptr || !IsBTreeIndex(ptrIndex->GetType()))
							continue;
						candidate.m_kind = SAccessPath::Seek;
						candidate.m_sField = sField;
						candidate.m_fieldType = source.m_slotTypes[pColumn->m_nSlot];
						candidate.m_op = OpEqual;
						nPriority = ptrIndex->GetType() == itUnique ? 1 : 2;
					}
					if (nPriority < nBest)
					{
						nBest = nPriority;
						path = candidate;
					}
				}
				if (nBest == 100)
				{
					// ST_Intersects(left.geom, right.geom) and so on: the right geometry by its spatial index
					for (const CExprPtr& ptrCond : others)
					{
						if (ptrCond->m_kind != eBoundExpr::Spatial)
							continue;
						const CSpatialExpr& spatial = static_cast<const CSpatialExpr&>(*ptrCond);
						if (spatial.m_ptrOther.get() == nullptr)
							continue;
						std::shared_ptr<CSpatialExpr> ptrProbe;
						const CColumnExpr* pColumn = ColumnOf(spatial.m_ptrGeom, scope, right.m_nSource);
						if (pColumn && (MaskOf(spatial.m_ptrOther, scope) & ~left.m_nMask) == 0)
						{
							ptrProbe = std::make_shared<CSpatialExpr>(spatial.m_mode);
							ptrProbe->m_ptrGeom = spatial.m_ptrGeom;
							ptrProbe->m_ptrOther = spatial.m_ptrOther;
						}
						else
						{
							pColumn = ColumnOf(spatial.m_ptrOther, scope, right.m_nSource);
							if (pColumn && (MaskOf(spatial.m_ptrGeom, scope) & ~left.m_nMask) == 0)
							{
								ptrProbe = std::make_shared<CSpatialExpr>(FlipMode(spatial.m_mode));
								ptrProbe->m_ptrGeom = spatial.m_ptrOther;
								ptrProbe->m_ptrOther = spatial.m_ptrGeom;
							}
						}
						if (ptrProbe.get() == nullptr)
							continue;
						const std::string& sField = source.m_slots[pColumn->m_nSlot];
						IIndexPtr ptrIndex = sField.empty() ? IIndexPtr() : source.m_ptrTable->GetIndex(sField);
						if (ptrIndex.get() == nullptr || !IsSpatialIndex(ptrIndex->GetType()))
							continue;
						path.m_kind = SAccessPath::Spatial;
						path.m_sField = sField;
						path.m_fieldType = dtGeometry;
						path.m_mode = ptrProbe->m_mode;
						path.m_ptrSpatial = ptrProbe;
						path.m_bKeysFromRow = true;
						nBest = 3;
						break;
					}
				}
				if (nBest < 100)
				{
					std::vector<CTableScanOp::SSlot> slots;
					for (const std::string& sField : source.m_slots)
						slots.push_back(CTableScanOp::SSlot{ sField });
					auto ptrRight = std::make_shared<CTableScanOp>(source.m_sTable, slots, source.m_nOffset, scope.m_nWidth, path, source.m_filters);
					ptrOp = std::make_shared<CIndexJoinOp>(ptrLeft, ptrRight, node.m_type, node.m_conditions, nRightBegin, nRightEnd);
				}
			}

			if (ptrOp.get() == nullptr)
			{
				COperatorPtr ptrRight = Build(right, scope);
				if (!leftKeys.empty())
					ptrOp = std::make_shared<CHashJoinOp>(ptrLeft, ptrRight, node.m_type, others, nLeftBegin, nLeftEnd, nRightBegin, nRightEnd, leftKeys, rightKeys);
				else
					ptrOp = std::make_shared<CNestedLoopJoinOp>(ptrLeft, ptrRight, node.m_type, node.m_conditions, nLeftBegin, nLeftEnd, nRightBegin, nRightEnd);
			}
			if (!node.m_filtersAbove.empty())
				ptrOp = std::make_shared<CFilterOp>(ptrOp, node.m_filtersAbove);
			return ptrOp;
		}

		//------------------------------------------------------------------------------------------
		// SELECT

		SSelectPlan CQueryCompiler::CompileSelect(const SSelectStmt& stmt)
		{
			return CompileSelect(stmt, nullptr);
		}

		SSelectPlan CQueryCompiler::CompileSelect(const SSelectStmt& stmt, CScope* pParent)
		{
			if (stmt.m_parts.size() == 1)
				return CompileCore(stmt.m_parts[0].m_core, &stmt.m_orderBy, stmt.m_ptrLimit, stmt.m_ptrOffset, pParent);

			std::vector<SSelectPlan> plans;
			for (const SSelectStmt::SPart& part : stmt.m_parts)
			{
				plans.push_back(CompileCore(part.m_core, nullptr, SExprPtr(), SExprPtr(), pParent));
				if (plans.back().m_nColumns != plans[0].m_nColumns)
					ThrowAt(stmt.m_pos, "the parts of " + std::string(ToString(part.m_op)) + " have different numbers of columns");
			}

			SSelectPlan result;
			result.m_nColumns = plans[0].m_nColumns;
			result.m_names = plans[0].m_names;
			result.m_types = plans[0].m_types;
			COperatorPtr ptrOp = plans[0].m_ptrRoot;
			for (size_t i = 1; i < plans.size(); ++i)
			{
				ptrOp = std::make_shared<CCompoundOp>(ptrOp, plans[i].m_ptrRoot, stmt.m_parts[i].m_op, result.m_nColumns);
				for (size_t c = 0; c < result.m_nColumns; ++c)
					if (result.m_types[c] != plans[i].m_types[c])
						result.m_types[c] = dtUnknown;
			}

			// ORDER BY of the result: a column number or the name of a column
			std::vector<CSortOp::SKey> keys;
			for (const SOrderItem& item : stmt.m_orderBy)
			{
				int32_t nColumn = -1;
				const SExpr& ast = *item.m_ptrExpr;
				if (ast.m_type == eExprType::Literal && static_cast<const SLiteral&>(ast).m_kind == SLiteral::Integer)
				{
					int64_t n = static_cast<const SLiteral&>(ast).m_nValue;
					if (n < 1 || n > (int64_t)result.m_nColumns)
						ThrowAt(ast.m_pos, "ORDER BY column " + std::to_string(n) + " is out of the range");
					nColumn = (int32_t)n - 1;
				}
				else if (ast.m_type == eExprType::Column && static_cast<const SColumnRef&>(ast).m_sTable.empty())
				{
					const std::string& sName = static_cast<const SColumnRef&>(ast).m_sColumn;
					for (size_t c = 0; c < result.m_nColumns && nColumn < 0; ++c)
						if (EqualNoCase(result.m_names[c], sName))
							nColumn = (int32_t)c;
				}
				if (nColumn < 0)
					ThrowAt(ast.m_pos, "ORDER BY of a compound SELECT: a column number or a column name of the result");
				keys.push_back(CSortOp::SKey{ (uint32_t)nColumn, item.m_bDesc });
			}
			if (!keys.empty())
				ptrOp = std::make_shared<CSortOp>(ptrOp, keys);
			if (stmt.m_ptrLimit.get())
			{
				CScope limitScope(pParent);
				CExprPtr ptrLimit = Expr(stmt.m_ptrLimit, limitScope, false);
				CExprPtr ptrOffset = stmt.m_ptrOffset.get() ? Expr(stmt.m_ptrOffset, limitScope, false) : CExprPtr();
				ptrOp = std::make_shared<CLimitOp>(ptrOp, ptrLimit, ptrOffset);
			}
			result.m_ptrRoot = ptrOp;
			return result;
		}

		SSelectPlan CQueryCompiler::CompileCore(const SSelectCore& core, const std::vector<SOrderItem>* pOrder, const SExprPtr& ptrLimitAst,
			const SExprPtr& ptrOffsetAst, CScope* pParent)
		{
			CScope scope(pParent);

			// FROM: "a, b" is a cross join
			std::unique_ptr<SFromNode> ptrFrom;
			for (const STableRefPtr& ptrRef : core.m_from)
			{
				std::unique_ptr<SFromNode> ptrNode = CompileFrom(ptrRef, scope);
				if (ptrFrom.get() == nullptr)
				{
					ptrFrom = std::move(ptrNode);
					continue;
				}
				std::unique_ptr<SFromNode> ptrJoin(new SFromNode());
				ptrJoin->m_bLeaf = false;
				ptrJoin->m_type = eJoinType::Cross;
				ptrJoin->m_nMask = ptrFrom->m_nMask | ptrNode->m_nMask;
				ptrJoin->m_nFirst = ptrFrom->m_nFirst;
				ptrJoin->m_nLast = ptrNode->m_nLast;
				ptrJoin->m_ptrLeft = std::move(ptrFrom);
				ptrJoin->m_ptrRight = std::move(ptrNode);
				ptrFrom = std::move(ptrJoin);
			}

			std::vector<CExprPtr> where;
			if (core.m_ptrWhere.get())
				SplitAnd(Expr(core.m_ptrWhere, scope, false), where);

			// the select list
			std::vector<CExprPtr> outputs;
			std::vector<std::string> names;
			for (const SSelectItem& item : core.m_items)
			{
				if (item.m_ptrExpr->m_type == eExprType::Star)
				{
					const std::string& sTable = static_cast<const SStar&>(*item.m_ptrExpr).m_sTable;
					bool bFound = false;
					for (size_t i = 0; i < scope.m_sources.size(); ++i)
					{
						SSource& source = scope.m_sources[i];
						if (!sTable.empty() && !EqualNoCase(source.m_sAlias, sTable))
							continue;
						bFound = true;
						if (source.m_kind == SSource::Table)
						{
							for (int32_t f = 0; f < source.m_ptrFields->GetFieldCount(); ++f)
							{
								IFieldPtr ptrField = source.m_ptrFields->GetField(f);
								int32_t nSlot = source.Use(ptrField->GetName(), ptrField->GetType());
								outputs.push_back(scope.MakeColumn((int32_t)i, nSlot, ptrField->GetType()));
								names.push_back(ptrField->GetName());
							}
						}
						else
						{
							for (uint32_t c = 0; c < source.m_plan.m_nColumns; ++c)
							{
								outputs.push_back(scope.MakeColumn((int32_t)i, (int32_t)c, source.m_plan.m_types[c]));
								names.push_back(source.m_plan.m_names[c]);
							}
						}
					}
					if (!bFound)
						ThrowAt(item.m_ptrExpr->m_pos, sTable.empty() ? "SELECT * without FROM" : "no table or alias '" + sTable + "'");
					continue;
				}

				outputs.push_back(Expr(item.m_ptrExpr, scope, true));
				if (!item.m_sAlias.empty())
					names.push_back(item.m_sAlias);
				else if (item.m_ptrExpr->m_type == eExprType::Column)
					names.push_back(static_cast<const SColumnRef&>(*item.m_ptrExpr).m_sColumn);
				else
					names.push_back(item.m_ptrExpr->ToSQL());
			}
			const uint32_t nVisible = (uint32_t)outputs.size();

			// GROUP BY: expressions or the numbers of the columns
			std::vector<CExprPtr> groups;
			for (const SExprPtr& ptrGroup : core.m_groupBy)
			{
				if (ptrGroup->m_type == eExprType::Literal && static_cast<const SLiteral&>(*ptrGroup).m_kind == SLiteral::Integer)
				{
					int64_t n = static_cast<const SLiteral&>(*ptrGroup).m_nValue;
					if (n < 1 || n > (int64_t)nVisible)
						ThrowAt(ptrGroup->m_pos, "GROUP BY column " + std::to_string(n) + " is out of the range");
					if (ContainsKind(outputs[n - 1], eBoundExpr::Aggregate))
						ThrowAt(ptrGroup->m_pos, "GROUP BY of an aggregate");
					groups.push_back(outputs[n - 1]);
				}
				else
					groups.push_back(Expr(ptrGroup, scope, false));
			}
			CExprPtr ptrHaving = core.m_ptrHaving.get() ? Expr(core.m_ptrHaving, scope, true) : CExprPtr();

			// ORDER BY: a column number, the name of a column of the result, or an expression
			struct SOrderKey
			{
				int32_t m_nOutput;
				CExprPtr m_ptrExpr;
				bool m_bDesc;
			};
			std::vector<SOrderKey> orderKeys;
			if (pOrder)
			{
				for (const SOrderItem& item : *pOrder)
				{
					SOrderKey key{ -1, CExprPtr(), item.m_bDesc };
					const SExpr& ast = *item.m_ptrExpr;
					if (ast.m_type == eExprType::Literal && static_cast<const SLiteral&>(ast).m_kind == SLiteral::Integer)
					{
						int64_t n = static_cast<const SLiteral&>(ast).m_nValue;
						if (n < 1 || n > (int64_t)nVisible)
							ThrowAt(ast.m_pos, "ORDER BY column " + std::to_string(n) + " is out of the range");
						key.m_nOutput = (int32_t)n - 1;
					}
					else
					{
						if (ast.m_type == eExprType::Column && static_cast<const SColumnRef&>(ast).m_sTable.empty())
						{
							const std::string& sName = static_cast<const SColumnRef&>(ast).m_sColumn;
							for (size_t i = 0; i < core.m_items.size() && i < nVisible && key.m_nOutput < 0; ++i)
								if (!core.m_items[i].m_sAlias.empty() && EqualNoCase(core.m_items[i].m_sAlias, sName))
									key.m_nOutput = (int32_t)i;
						}
						if (key.m_nOutput < 0)
						{
							key.m_ptrExpr = Expr(item.m_ptrExpr, scope, true);
							std::string sKey = key.m_ptrExpr->Key();
							for (uint32_t i = 0; i < nVisible && key.m_nOutput < 0; ++i)
								if (outputs[i]->Key() == sKey)
									key.m_nOutput = (int32_t)i;
							if (key.m_nOutput >= 0)
								key.m_ptrExpr.reset();
						}
					}
					orderKeys.push_back(key);
				}
			}

			bool bAggregate = scope.m_bAggregate || !groups.empty() || ptrHaving.get();
			if (core.m_bDistinct)
				for (const SOrderKey& key : orderKeys)
					if (key.m_nOutput < 0)
						ThrowAt(key.m_ptrExpr->m_pos, "ORDER BY of SELECT DISTINCT has to use the selected columns");

			// the row id of a query of one table (ISelectCursor::GetRowID)
			CExprPtr ptrRowId;
			if (!bAggregate && !core.m_bDistinct && scope.m_sources.size() == 1 && scope.m_sources[0].m_kind == SSource::Table)
				ptrRowId = scope.MakeColumn(0, scope.m_sources[0].Use(std::string(), dtInteger64), dtInteger64);

			scope.Finalize();

			// WHERE: the conditions down to the tables / joins
			std::vector<CExprPtr> topFilters;
			for (const CExprPtr& ptrCond : where)
			{
				uint64_t nMask = MaskOf(ptrCond, scope);
				if (nMask == 0 || ptrFrom.get() == nullptr)
					topFilters.push_back(ptrCond);
				else
					Place(ptrCond, *ptrFrom, scope);
			}

			COperatorPtr ptrOp = ptrFrom.get() ? Build(*ptrFrom, scope) : COperatorPtr(std::make_shared<CSingleRowOp>(scope.m_nWidth));
			if (!topFilters.empty())
				ptrOp = std::make_shared<CFilterOp>(ptrOp, topFilters);

			if (bAggregate)
			{
				std::vector<std::string> groupKeys;
				for (const CExprPtr& ptrGroup : groups)
					groupKeys.push_back(ptrGroup->Key());
				std::vector<std::shared_ptr<CAggregateExpr> > aggregates;
				std::map<std::string, size_t> aggIndex;

				// the expressions over the row of the aggregation: the group keys, then the aggregates
				std::function<void(CExprPtr&)> Rewrite = [&](CExprPtr& ptrExpr)
				{
					std::string sKey = ptrExpr->Key();
					for (size_t i = 0; i < groupKeys.size(); ++i)
					{
						if (groupKeys[i] == sKey)
						{
							ptrExpr = RowColumn((uint32_t)i, NormType(ptrExpr->m_type));
							return;
						}
					}
					if (ptrExpr->m_kind == eBoundExpr::Aggregate)
					{
						auto it = aggIndex.find(sKey);
						size_t nIndex = 0;
						if (it == aggIndex.end())
						{
							nIndex = aggregates.size();
							aggregates.push_back(std::static_pointer_cast<CAggregateExpr>(ptrExpr));
							aggIndex[sKey] = nIndex;
						}
						else
							nIndex = it->second;
						ptrExpr = RowColumn((uint32_t)(groups.size() + nIndex), ptrExpr->m_type);
						return;
					}
					if (ptrExpr->m_kind == eBoundExpr::Column)
					{
						const CColumnExpr* pColumn = static_cast<const CColumnExpr*>(ptrExpr.get());
						if (pColumn->m_pScope == &scope && pColumn->m_nSource >= 0)
							ThrowAt(ptrExpr->m_pos, "a column has to be in GROUP BY or in an aggregate");
						return;
					}
					if (IsSubqueryKind(ptrExpr->m_kind) && MaskOf(ptrExpr, scope) != 0)
						ThrowAt(ptrExpr->m_pos, "a correlated subquery isn't supported with GROUP BY / aggregates");
					std::vector<CExprPtr*> children;
					ptrExpr->Children(children);
					for (CExprPtr* pChild : children)
						Rewrite(*pChild);
				};
				for (CExprPtr& ptrOutput : outputs)
					Rewrite(ptrOutput);
				if (ptrHaving.get())
					Rewrite(ptrHaving);
				for (SOrderKey& key : orderKeys)
					if (key.m_ptrExpr.get())
						Rewrite(key.m_ptrExpr);

				ptrOp = std::make_shared<CAggregateOp>(ptrOp, groups, aggregates);
				if (ptrHaving.get())
				{
					std::vector<CExprPtr> having;
					SplitAnd(ptrHaving, having);
					ptrOp = std::make_shared<CFilterOp>(ptrOp, having);
				}
			}

			// the projection: the visible columns, the hidden ORDER BY expressions, the row id
			SSelectPlan plan;
			plan.m_nColumns = nVisible;
			plan.m_names = names;
			for (uint32_t i = 0; i < nVisible; ++i)
				plan.m_types.push_back(NormType(outputs[i]->m_type));
			std::vector<CSortOp::SKey> sortKeys;
			for (SOrderKey& key : orderKeys)
			{
				if (key.m_nOutput < 0)
				{
					key.m_nOutput = (int32_t)outputs.size();
					outputs.push_back(key.m_ptrExpr);
				}
				sortKeys.push_back(CSortOp::SKey{ (uint32_t)key.m_nOutput, key.m_bDesc });
			}
			if (ptrRowId.get())
			{
				plan.m_nRowIdColumn = (int32_t)outputs.size();
				outputs.push_back(ptrRowId);
			}

			ptrOp = std::make_shared<CProjectOp>(ptrOp, outputs);
			if (core.m_bDistinct)
				ptrOp = std::make_shared<CDistinctOp>(ptrOp, nVisible);
			if (!sortKeys.empty())
				ptrOp = std::make_shared<CSortOp>(ptrOp, sortKeys);
			if (ptrLimitAst.get())
			{
				CScope limitScope(pParent);
				CExprPtr ptrLimit = Expr(ptrLimitAst, limitScope, false);
				CExprPtr ptrOffset = ptrOffsetAst.get() ? Expr(ptrOffsetAst, limitScope, false) : CExprPtr();
				ptrOp = std::make_shared<CLimitOp>(ptrOp, ptrLimit, ptrOffset);
			}
			plan.m_ptrRoot = ptrOp;
			return plan;
		}

		//------------------------------------------------------------------------------------------
		// UPDATE / DELETE

		namespace
		{
			std::string FieldName(const ITablePtr& ptrTable, const std::string& sName, const SSourcePos& pos)
			{
				IFieldPtr ptrField = ptrTable->GetField(sName);
				if (ptrField.get())
					return ptrField->GetName();
				IFieldsPtr ptrFields = ptrTable->GetFields();
				for (int32_t i = 0; i < ptrFields->GetFieldCount(); ++i)
					if (EqualNoCase(ptrFields->GetField(i)->GetName(), sName))
						return ptrFields->GetField(i)->GetName();
				ThrowAt(pos, "table '" + ptrTable->GetName() + "' has no column '" + sName + "'");
			}
		}

		SModifyPlan CQueryCompiler::CompileUpdate(const SUpdateStmt& stmt)
		{
			SModifyPlan plan;
			CScope scope(nullptr);
			auto ptrRef = std::make_shared<STableName>(stmt.m_sTable, std::string());
			ptrRef->m_pos = stmt.m_pos;
			std::unique_ptr<SFromNode> ptrFrom = CompileFrom(ptrRef, scope);
			SSource& source = scope.m_sources[0];
			plan.m_sTable = source.m_sTable;

			for (const SSetClause& set : stmt.m_set)
			{
				std::string sField = FieldName(source.m_ptrTable, set.m_sColumn, set.m_ptrValue->m_pos);
				if (std::find(plan.m_columns.begin(), plan.m_columns.end(), sField) != plan.m_columns.end())
					ThrowAt(set.m_ptrValue->m_pos, "column '" + sField + "' is set twice");
				plan.m_columns.push_back(sField);
				plan.m_values.push_back(Expr(set.m_ptrValue, scope, false));
			}

			std::vector<CExprPtr> where;
			if (stmt.m_ptrWhere.get())
				SplitAnd(Expr(stmt.m_ptrWhere, scope, false), where);
			CColumnExprPtr ptrRowId = scope.MakeColumn(0, source.Use(std::string(), dtInteger64), dtInteger64);
			scope.Finalize();

			std::vector<CExprPtr> topFilters;
			for (const CExprPtr& ptrCond : where)
			{
				if (MaskOf(ptrCond, scope) == 0)
					topFilters.push_back(ptrCond);
				else
					Place(ptrCond, *ptrFrom, scope);
			}
			plan.m_ptrRoot = Build(*ptrFrom, scope);
			if (!topFilters.empty())
				plan.m_ptrRoot = std::make_shared<CFilterOp>(plan.m_ptrRoot, topFilters);
			plan.m_nRowIdIndex = ptrRowId->m_nIndex;
			return plan;
		}

		SModifyPlan CQueryCompiler::CompileDelete(const SDeleteStmt& stmt)
		{
			SUpdateStmt update;
			update.m_sTable = stmt.m_sTable;
			update.m_ptrWhere = stmt.m_ptrWhere;
			update.m_pos = stmt.m_pos;
			return CompileUpdate(update);
		}
	}
}
