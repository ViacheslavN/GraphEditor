#include "AST.h"
#include <sstream>
#include <cstdio>
#include <cctype>
#include <algorithm>

namespace bptreedb
{
	namespace sql
	{
		namespace
		{
			// the keywords of Grammar/SQLLexer.l: such identifiers are quoted
			const char* const Keywords[] = {
				"ADD", "ALL", "ALTER", "AND", "AS", "ASC", "BETWEEN", "BY", "CASE", "CAST", "COLUMN", "CREATE", "CROSS",
				"DEFAULT", "DELETE", "DESC", "DISTINCT", "DROP", "ELSE", "END", "ESCAPE", "EXCEPT", "EXISTS", "FALSE",
				"FROM", "FULL", "GROUP", "HAVING", "IF", "IN", "INDEX", "INNER", "INSERT", "INTERSECT", "INTO", "IS",
				"JOIN", "LEFT", "LIKE", "LIMIT", "NOT", "NULL", "OFFSET", "ON", "OR", "ORDER", "OUTER", "RIGHT",
				"SELECT", "SET", "SPATIAL", "TABLE", "THEN", "TRUE", "UNION", "UNIQUE", "UPDATE", "USING", "VALUES",
				"WHEN", "WHERE"
			};

			bool IsKeyword(const std::string& sName)
			{
				std::string sUpper(sName);
				std::transform(sUpper.begin(), sUpper.end(), sUpper.begin(), [](unsigned char c) { return (char)std::toupper(c); });
				for (const char* pKeyword : Keywords)
					if (sUpper == pKeyword)
						return true;
				return false;
			}

			template<class TList, class TFunc>
			void WriteList(std::ostream& stream, const TList& list, TFunc func)
			{
				bool bFirst = true;
				for (const auto& item : list)
				{
					if (!bFirst)
						stream << ", ";
					bFirst = false;
					func(item);
				}
			}

			void WriteExprList(std::ostream& stream, const TExprList& list)
			{
				WriteList(stream, list, [&](const SExprPtr& ptrExpr) { ptrExpr->ToSQL(stream); });
			}

			void WriteNames(std::ostream& stream, const std::vector<std::string>& names)
			{
				WriteList(stream, names, [&](const std::string& sName) { WriteIdentifier(stream, sName); });
			}

			void WriteDouble(std::ostream& stream, double dValue)
			{
				char buf[64];
				std::snprintf(buf, sizeof(buf), "%.17g", dValue);
				std::string s(buf);
				if (s.find_first_of(".eEn") == std::string::npos)      // keep it a double: 2 -> 2.0 (n: nan, inf)
					s += ".0";
				stream << s;
			}
		}

		std::string SNode::ToSQL() const
		{
			std::ostringstream stream;
			ToSQL(stream);
			return stream.str();
		}

		void WriteIdentifier(std::ostream& stream, const std::string& sName)
		{
			bool bPlain = !sName.empty() && (std::isalpha((unsigned char)sName[0]) || sName[0] == '_');
			for (char c : sName)
				bPlain = bPlain && (std::isalnum((unsigned char)c) || c == '_');
			if (bPlain && !IsKeyword(sName))
			{
				stream << sName;
				return;
			}

			stream << '"';
			for (char c : sName)
			{
				if (c == '"')
					stream << '"';
				stream << c;
			}
			stream << '"';
		}

		void WriteString(std::ostream& stream, const std::string& sValue)
		{
			stream << '\'';
			for (char c : sValue)
			{
				if (c == '\'')
					stream << '\'';
				stream << c;
			}
			stream << '\'';
		}

		const char* ToString(eUnaryOp op)
		{
			switch (op)
			{
			case eUnaryOp::Minus: return "-";
			case eUnaryOp::Plus: return "+";
			case eUnaryOp::Not: return "NOT ";
			}
			return "?";
		}

		const char* ToString(eBinaryOp op)
		{
			switch (op)
			{
			case eBinaryOp::Or: return "OR";
			case eBinaryOp::And: return "AND";
			case eBinaryOp::Equal: return "=";
			case eBinaryOp::NotEqual: return "<>";
			case eBinaryOp::Less: return "<";
			case eBinaryOp::LessOrEqual: return "<=";
			case eBinaryOp::Greater: return ">";
			case eBinaryOp::GreaterOrEqual: return ">=";
			case eBinaryOp::Add: return "+";
			case eBinaryOp::Subtract: return "-";
			case eBinaryOp::Multiply: return "*";
			case eBinaryOp::Divide: return "/";
			case eBinaryOp::Modulo: return "%";
			case eBinaryOp::Concat: return "||";
			}
			return "?";
		}

		const char* ToString(eJoinType type)
		{
			switch (type)
			{
			case eJoinType::Inner: return "JOIN";
			case eJoinType::Left: return "LEFT JOIN";
			case eJoinType::Right: return "RIGHT JOIN";
			case eJoinType::Full: return "FULL JOIN";
			case eJoinType::Cross: return "CROSS JOIN";
			}
			return "?";
		}

		const char* ToString(eCompoundOp op)
		{
			switch (op)
			{
			case eCompoundOp::None: return "";
			case eCompoundOp::Union: return "UNION";
			case eCompoundOp::UnionAll: return "UNION ALL";
			case eCompoundOp::Intersect: return "INTERSECT";
			case eCompoundOp::Except: return "EXCEPT";
			}
			return "?";
		}

		//------------------------------------------------------------------------------------------
		// expressions

		std::shared_ptr<SLiteral> SLiteral::MakeNull()
		{
			return std::make_shared<SLiteral>();
		}

		std::shared_ptr<SLiteral> SLiteral::MakeInteger(int64_t nValue)
		{
			auto ptr = std::make_shared<SLiteral>();
			ptr->m_kind = Integer;
			ptr->m_nValue = nValue;
			return ptr;
		}

		std::shared_ptr<SLiteral> SLiteral::MakeDouble(double dValue)
		{
			auto ptr = std::make_shared<SLiteral>();
			ptr->m_kind = Double;
			ptr->m_dValue = dValue;
			return ptr;
		}

		std::shared_ptr<SLiteral> SLiteral::MakeString(const std::string& sValue)
		{
			auto ptr = std::make_shared<SLiteral>();
			ptr->m_kind = String;
			ptr->m_sValue = sValue;
			return ptr;
		}

		std::shared_ptr<SLiteral> SLiteral::MakeBool(bool bValue)
		{
			auto ptr = std::make_shared<SLiteral>();
			ptr->m_kind = Bool;
			ptr->m_nValue = bValue ? 1 : 0;
			return ptr;
		}

		void SLiteral::ToSQL(std::ostream& stream) const
		{
			switch (m_kind)
			{
			case Null: stream << "NULL"; break;
			case Integer: stream << m_nValue; break;
			case Double: WriteDouble(stream, m_dValue); break;
			case String: WriteString(stream, m_sValue); break;
			case Bool: stream << (m_nValue ? "TRUE" : "FALSE"); break;
			}
		}

		void SParameter::ToSQL(std::ostream& stream) const
		{
			stream << '?';
		}

		void SColumnRef::ToSQL(std::ostream& stream) const
		{
			if (!m_sTable.empty())
			{
				WriteIdentifier(stream, m_sTable);
				stream << '.';
			}
			WriteIdentifier(stream, m_sColumn);
		}

		void SStar::ToSQL(std::ostream& stream) const
		{
			if (!m_sTable.empty())
			{
				WriteIdentifier(stream, m_sTable);
				stream << '.';
			}
			stream << '*';
		}

		void SUnaryExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(' << sql::ToString(m_op);
			m_ptrOperand->ToSQL(stream);
			stream << ')';
		}

		void SBinaryExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrLeft->ToSQL(stream);
			stream << ' ' << sql::ToString(m_op) << ' ';
			m_ptrRight->ToSQL(stream);
			stream << ')';
		}

		void SFunctionCall::ToSQL(std::ostream& stream) const
		{
			stream << m_sName << '(';
			if (m_bStar)
				stream << '*';
			else
			{
				if (m_bDistinct)
					stream << "DISTINCT ";
				WriteExprList(stream, m_args);
			}
			stream << ')';
		}

		void SIsNullExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrOperand->ToSQL(stream);
			stream << (m_bNot ? " IS NOT NULL)" : " IS NULL)");
		}

		void SBetweenExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrOperand->ToSQL(stream);
			stream << (m_bNot ? " NOT BETWEEN " : " BETWEEN ");
			m_ptrLow->ToSQL(stream);
			stream << " AND ";
			m_ptrHigh->ToSQL(stream);
			stream << ')';
		}

		void SInListExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrOperand->ToSQL(stream);
			stream << (m_bNot ? " NOT IN (" : " IN (");
			WriteExprList(stream, m_list);
			stream << "))";
		}

		void SInSelectExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrOperand->ToSQL(stream);
			stream << (m_bNot ? " NOT IN (" : " IN (");
			m_ptrSelect->ToSQL(stream);
			stream << "))";
		}

		void SLikeExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrOperand->ToSQL(stream);
			stream << (m_bNot ? " NOT LIKE " : " LIKE ");
			m_ptrPattern->ToSQL(stream);
			if (m_ptrEscape.get())
			{
				stream << " ESCAPE ";
				m_ptrEscape->ToSQL(stream);
			}
			stream << ')';
		}

		void SCaseExpr::ToSQL(std::ostream& stream) const
		{
			stream << "CASE";
			if (m_ptrOperand.get())
			{
				stream << ' ';
				m_ptrOperand->ToSQL(stream);
			}
			for (const SWhenClause& when : m_whens)
			{
				stream << " WHEN ";
				when.m_ptrWhen->ToSQL(stream);
				stream << " THEN ";
				when.m_ptrThen->ToSQL(stream);
			}
			if (m_ptrElse.get())
			{
				stream << " ELSE ";
				m_ptrElse->ToSQL(stream);
			}
			stream << " END";
		}

		void STypeName::ToSQL(std::ostream& stream) const
		{
			stream << m_sName;
			if (!m_params.empty())
			{
				stream << '(';
				WriteList(stream, m_params, [&](int64_t n) { stream << n; });
				stream << ')';
			}
		}

		void SCastExpr::ToSQL(std::ostream& stream) const
		{
			stream << "CAST(";
			m_ptrOperand->ToSQL(stream);
			stream << " AS ";
			m_type.ToSQL(stream);
			stream << ')';
		}

		void SExistsExpr::ToSQL(std::ostream& stream) const
		{
			stream << "EXISTS (";
			m_ptrSelect->ToSQL(stream);
			stream << ')';
		}

		void SSubqueryExpr::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrSelect->ToSQL(stream);
			stream << ')';
		}

		//------------------------------------------------------------------------------------------
		// FROM

		void STableName::ToSQL(std::ostream& stream) const
		{
			WriteIdentifier(stream, m_sName);
			if (!m_sAlias.empty())
			{
				stream << " AS ";
				WriteIdentifier(stream, m_sAlias);
			}
		}

		void SSubqueryRef::ToSQL(std::ostream& stream) const
		{
			stream << '(';
			m_ptrSelect->ToSQL(stream);
			stream << ')';
			if (!m_sAlias.empty())
			{
				stream << " AS ";
				WriteIdentifier(stream, m_sAlias);
			}
		}

		void SJoinRef::ToSQL(std::ostream& stream) const
		{
			m_ptrLeft->ToSQL(stream);
			stream << ' ' << sql::ToString(m_joinType) << ' ';
			// a join on the right (it isn't produced by the grammar, the joins are left deep) in parentheses
			bool bParen = m_ptrRight->m_type == eTableRefType::Join;
			if (bParen)
				stream << '(';
			m_ptrRight->ToSQL(stream);
			if (bParen)
				stream << ')';
			if (m_ptrCondition.get())
			{
				stream << " ON ";
				m_ptrCondition->ToSQL(stream);
			}
			else if (!m_using.empty())
			{
				stream << " USING (";
				WriteNames(stream, m_using);
				stream << ')';
			}
		}

		//------------------------------------------------------------------------------------------
		// statements

		void SSelectCore::ToSQL(std::ostream& stream) const
		{
			stream << "SELECT ";
			if (m_bDistinct)
				stream << "DISTINCT ";
			WriteList(stream, m_items, [&](const SSelectItem& item)
			{
				item.m_ptrExpr->ToSQL(stream);
				if (!item.m_sAlias.empty())
				{
					stream << " AS ";
					WriteIdentifier(stream, item.m_sAlias);
				}
			});
			if (!m_from.empty())
			{
				stream << " FROM ";
				WriteList(stream, m_from, [&](const STableRefPtr& ptrRef) { ptrRef->ToSQL(stream); });
			}
			if (m_ptrWhere.get())
			{
				stream << " WHERE ";
				m_ptrWhere->ToSQL(stream);
			}
			if (!m_groupBy.empty())
			{
				stream << " GROUP BY ";
				WriteExprList(stream, m_groupBy);
			}
			if (m_ptrHaving.get())
			{
				stream << " HAVING ";
				m_ptrHaving->ToSQL(stream);
			}
		}

		void SSelectStmt::ToSQL(std::ostream& stream) const
		{
			for (const SPart& part : m_parts)
			{
				if (part.m_op != eCompoundOp::None)
					stream << ' ' << sql::ToString(part.m_op) << ' ';
				part.m_core.ToSQL(stream);
			}
			if (!m_orderBy.empty())
			{
				stream << " ORDER BY ";
				WriteList(stream, m_orderBy, [&](const SOrderItem& item)
				{
					item.m_ptrExpr->ToSQL(stream);
					if (item.m_bDesc)
						stream << " DESC";
				});
			}
			if (m_ptrLimit.get())
			{
				stream << " LIMIT ";
				m_ptrLimit->ToSQL(stream);
			}
			if (m_ptrOffset.get())
			{
				stream << " OFFSET ";
				m_ptrOffset->ToSQL(stream);
			}
		}

		void SInsertStmt::ToSQL(std::ostream& stream) const
		{
			stream << "INSERT INTO ";
			WriteIdentifier(stream, m_sTable);
			if (!m_columns.empty())
			{
				stream << " (";
				WriteNames(stream, m_columns);
				stream << ')';
			}
			if (m_ptrSelect.get())
			{
				stream << ' ';
				m_ptrSelect->ToSQL(stream);
				return;
			}
			stream << " VALUES ";
			WriteList(stream, m_rows, [&](const TExprList& row)
			{
				stream << '(';
				WriteExprList(stream, row);
				stream << ')';
			});
		}

		void SUpdateStmt::ToSQL(std::ostream& stream) const
		{
			stream << "UPDATE ";
			WriteIdentifier(stream, m_sTable);
			stream << " SET ";
			WriteList(stream, m_set, [&](const SSetClause& set)
			{
				WriteIdentifier(stream, set.m_sColumn);
				stream << " = ";
				set.m_ptrValue->ToSQL(stream);
			});
			if (m_ptrWhere.get())
			{
				stream << " WHERE ";
				m_ptrWhere->ToSQL(stream);
			}
		}

		void SDeleteStmt::ToSQL(std::ostream& stream) const
		{
			stream << "DELETE FROM ";
			WriteIdentifier(stream, m_sTable);
			if (m_ptrWhere.get())
			{
				stream << " WHERE ";
				m_ptrWhere->ToSQL(stream);
			}
		}

		void SColumnDef::ToSQL(std::ostream& stream) const
		{
			WriteIdentifier(stream, m_sName);
			stream << ' ';
			m_type.ToSQL(stream);
			if (m_bNotNull)
				stream << " NOT NULL";
			if (m_bUnique)
				stream << " UNIQUE";
			if (m_ptrDefault.get())
			{
				stream << " DEFAULT ";
				m_ptrDefault->ToSQL(stream);
			}
		}

		void SCreateTableStmt::ToSQL(std::ostream& stream) const
		{
			stream << "CREATE TABLE ";
			if (m_bIfNotExists)
				stream << "IF NOT EXISTS ";
			WriteIdentifier(stream, m_sTable);
			stream << " (";
			WriteList(stream, m_columns, [&](const SColumnDef& column) { column.ToSQL(stream); });
			stream << ')';
		}

		void SDropTableStmt::ToSQL(std::ostream& stream) const
		{
			stream << "DROP TABLE ";
			if (m_bIfExists)
				stream << "IF EXISTS ";
			WriteIdentifier(stream, m_sTable);
		}

		void SAlterTableStmt::ToSQL(std::ostream& stream) const
		{
			stream << "ALTER TABLE ";
			WriteIdentifier(stream, m_sTable);
			if (m_action == AddColumn)
			{
				stream << " ADD COLUMN ";
				m_column.ToSQL(stream);
			}
			else
			{
				stream << " DROP COLUMN ";
				WriteIdentifier(stream, m_sColumn);
			}
		}

		void SCreateIndexStmt::ToSQL(std::ostream& stream) const
		{
			stream << "CREATE ";
			if (m_kind == Unique)
				stream << "UNIQUE ";
			else if (m_kind == Spatial)
				stream << "SPATIAL ";
			stream << "INDEX ";
			if (m_bIfNotExists)
				stream << "IF NOT EXISTS ";
			if (!m_sName.empty())
			{
				WriteIdentifier(stream, m_sName);
				stream << ' ';
			}
			stream << "ON ";
			WriteIdentifier(stream, m_sTable);
			stream << " (";
			WriteNames(stream, m_columns);
			stream << ')';
			if (!m_sMethod.empty())
			{
				stream << " USING " << m_sMethod;
				if (!m_methodParams.empty())
				{
					stream << '(';
					WriteList(stream, m_methodParams, [&](double d) { WriteDouble(stream, d); });
					stream << ')';
				}
			}
		}

		void SDropIndexStmt::ToSQL(std::ostream& stream) const
		{
			stream << "DROP INDEX ";
			if (m_bIfExists)
				stream << "IF EXISTS ";
			if (!m_sName.empty())
			{
				WriteIdentifier(stream, m_sName);
				stream << ' ';
			}
			stream << "ON ";
			WriteIdentifier(stream, m_sTable);
			stream << " (";
			WriteNames(stream, m_columns);
			stream << ')';
		}
	}
}
