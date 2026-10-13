// The SQL front end: the text -> the syntax tree -> SQL again (ToSQL puts every operator in parentheses, so
// the expected text shows the grouping), syntax errors with their positions.

#include "../../SQLEngine/SQLDriver.h"
#include <iostream>
#include <string>
#include <exception>

using namespace bptreedb::sql;

namespace
{
	int g_nFailed = 0;
	int g_nPassed = 0;

	void CheckSQL(const std::string& sSQL, const std::string& sExpected)
	{
		try
		{
			CSQLDriver driver;
			TStatementList statements = driver.Parse(sSQL);
			std::string sResult;
			for (size_t i = 0; i < statements.size(); ++i)
				sResult += (i ? "; " : "") + statements[i]->ToSQL();
			if (sResult != sExpected)
			{
				std::cout << "FAILED: " << sSQL << "\n  expected: " << sExpected << "\n  result:   " << sResult << std::endl;
				++g_nFailed;
				return;
			}

			// the printed SQL is parsed to the same tree
			TStatementList again = driver.Parse(sResult);
			std::string sAgain;
			for (size_t i = 0; i < again.size(); ++i)
				sAgain += (i ? "; " : "") + again[i]->ToSQL();
			if (sAgain != sResult)
			{
				std::cout << "FAILED (round trip): " << sResult << "\n  result: " << sAgain << std::endl;
				++g_nFailed;
				return;
			}
			++g_nPassed;
		}
		catch (std::exception& e)
		{
			std::cout << "FAILED: " << sSQL << "\n  " << e.what() << std::endl;
			++g_nFailed;
		}
	}

	void CheckError(const std::string& sSQL, const std::string& sExpected)
	{
		try
		{
			CSQLDriver driver;
			driver.Parse(sSQL);
			std::cout << "FAILED (no error): " << sSQL << std::endl;
			++g_nFailed;
		}
		catch (std::exception& e)
		{
			std::string sMessage = e.what();
			if (sMessage.find(sExpected) == std::string::npos)
			{
				std::cout << "FAILED (error): " << sSQL << "\n  expected: " << sExpected << "\n  result:   " << sMessage << std::endl;
				++g_nFailed;
				return;
			}
			++g_nPassed;
		}
	}

	void TestSelect()
	{
		CheckSQL("select * from roads", "SELECT * FROM roads");
		CheckSQL("SELECT DISTINCT a, b AS bb, c cc FROM t", "SELECT DISTINCT a, b AS bb, c AS cc FROM t");
		CheckSQL("SELECT ALL t.*, t.a FROM t", "SELECT t.*, t.a FROM t");
		CheckSQL("SELECT 1, -2, 1.5, -0.25, 1e3, .5, 'it''s', NULL, TRUE, false",
			"SELECT 1, -2, 1.5, -0.25, 1000.0, 0.5, 'it''s', NULL, TRUE, FALSE");
		CheckSQL("SELECT 9223372036854775807, -9223372036854775808, 9223372036854775809",
			"SELECT 9223372036854775807, -9223372036854775808, 9.2233720368547758e+18");
		CheckSQL("SELECT a FROM t WHERE a = ? AND b > ?", "SELECT a FROM t WHERE ((a = ?) AND (b > ?))");
		CheckSQL("SELECT \"select\", [my col], `x``y` FROM \"My Table\"", "SELECT \"select\", \"my col\", \"x`y\" FROM \"My Table\"");
		CheckSQL("-- a comment\nSELECT /* inline \n comment */ a FROM t -- end", "SELECT a FROM t");
		CheckSQL("SELECT cls, COUNT(*), SUM(len) AS total, COUNT(DISTINCT name) FROM roads WHERE len > 10 GROUP BY cls HAVING COUNT(*) > 1",
			"SELECT cls, COUNT(*), SUM(len) AS total, COUNT(DISTINCT name) FROM roads WHERE (len > 10) GROUP BY cls HAVING (COUNT(*) > 1)");
		CheckSQL("SELECT a FROM t ORDER BY a DESC, b ASC, c LIMIT 10 OFFSET 20", "SELECT a FROM t ORDER BY a DESC, b, c LIMIT 10 OFFSET 20");
		CheckSQL("SELECT a FROM t LIMIT 20, 10", "SELECT a FROM t LIMIT 10 OFFSET 20");
		CheckSQL("SELECT a FROM t UNION SELECT a FROM u UNION ALL SELECT b FROM v EXCEPT SELECT c FROM w ORDER BY 1 LIMIT 5",
			"SELECT a FROM t UNION SELECT a FROM u UNION ALL SELECT b FROM v EXCEPT SELECT c FROM w ORDER BY 1 LIMIT 5");
		CheckSQL("SELECT now(), Upper(name) FROM t", "SELECT NOW(), UPPER(name) FROM t");
		CheckSQL("SELECT 1; ; SELECT 2;", "SELECT 1; SELECT 2");
		CheckSQL("", "");
	}

	void TestExpressions()
	{
		// precedence: * before +, + before comparisons, comparisons before NOT, NOT before AND, AND before OR
		CheckSQL("SELECT a + b * c - d / e % f", "SELECT ((a + (b * c)) - ((d / e) % f))");
		CheckSQL("SELECT a || b || c + 1", "SELECT (((a || b) || c) + 1)");
		CheckSQL("SELECT -a * -(b + 1), +c, - -1", "SELECT ((-a) * (-(b + 1))), (+c), 1");
		CheckSQL("SELECT * FROM t WHERE a = 1 OR b = 2 AND NOT c < 3", "SELECT * FROM t WHERE ((a = 1) OR ((b = 2) AND (NOT (c < 3))))");
		CheckSQL("SELECT * FROM t WHERE a + 1 >= b * 2 AND c <> d AND e != f AND g == h",
			"SELECT * FROM t WHERE (((((a + 1) >= (b * 2)) AND (c <> d)) AND (e <> f)) AND (g = h))");
		CheckSQL("SELECT * FROM t WHERE (a = 1 OR b = 2) AND c = 3", "SELECT * FROM t WHERE (((a = 1) OR (b = 2)) AND (c = 3))");
		CheckSQL("SELECT * FROM t WHERE a = b < c", "SELECT * FROM t WHERE (a = (b < c))");
		CheckSQL("SELECT * FROM t WHERE a IS NULL OR b IS NOT NULL", "SELECT * FROM t WHERE ((a IS NULL) OR (b IS NOT NULL))");
		CheckSQL("SELECT * FROM t WHERE a BETWEEN 1 AND 5 AND b NOT BETWEEN x - 1 AND x + 1",
			"SELECT * FROM t WHERE ((a BETWEEN 1 AND 5) AND (b NOT BETWEEN (x - 1) AND (x + 1)))");
		CheckSQL("SELECT * FROM t WHERE name LIKE 'A%' AND note NOT LIKE '%x!%%' ESCAPE '!'",
			"SELECT * FROM t WHERE ((name LIKE 'A%') AND (note NOT LIKE '%x!%%' ESCAPE '!'))");
		CheckSQL("SELECT * FROM t WHERE name LIKE 'a' || b", "SELECT * FROM t WHERE (name LIKE ('a' || b))");
		CheckSQL("SELECT * FROM t WHERE a IN (1, 2, 3) AND b NOT IN ('x') OR c IN (SELECT id FROM u WHERE u.k = t.k)",
			"SELECT * FROM t WHERE (((a IN (1, 2, 3)) AND (b NOT IN ('x'))) OR (c IN (SELECT id FROM u WHERE (u.k = t.k))))");
		CheckSQL("SELECT * FROM t WHERE NOT EXISTS (SELECT 1 FROM u WHERE u.id = t.id) AND a > (SELECT MAX(b) FROM u)",
			"SELECT * FROM t WHERE ((NOT EXISTS (SELECT 1 FROM u WHERE (u.id = t.id))) AND (a > (SELECT MAX(b) FROM u)))");
		CheckSQL("SELECT CASE WHEN a > 0 THEN 'pos' WHEN a < 0 THEN 'neg' ELSE 'zero' END, CASE cls WHEN 1 THEN 'a' END FROM t",
			"SELECT CASE WHEN (a > 0) THEN 'pos' WHEN (a < 0) THEN 'neg' ELSE 'zero' END, CASE cls WHEN 1 THEN 'a' END FROM t");
		CheckSQL("SELECT CAST(a AS varchar(20)), CAST(b AS double) FROM t", "SELECT CAST(a AS VARCHAR(20)), CAST(b AS DOUBLE) FROM t");
		CheckSQL("SELECT ST_Intersects(geom, ST_MakeEnvelope(0, 0, 10, 10)) FROM roads",
			"SELECT ST_INTERSECTS(geom, ST_MAKEENVELOPE(0, 0, 10, 10)) FROM roads");
	}

	void TestJoins()
	{
		CheckSQL("SELECT r.name, c.name FROM roads r JOIN cities AS c ON r.city_id = c.id",
			"SELECT r.name, c.name FROM roads AS r JOIN cities AS c ON (r.city_id = c.id)");
		CheckSQL("SELECT * FROM a INNER JOIN b ON a.id = b.id LEFT OUTER JOIN c ON c.id = b.cid RIGHT JOIN d USING (k, m) FULL JOIN e ON e.x = a.x CROSS JOIN f",
			"SELECT * FROM a JOIN b ON (a.id = b.id) LEFT JOIN c ON (c.id = b.cid) RIGHT JOIN d USING (k, m) FULL JOIN e ON (e.x = a.x) CROSS JOIN f");
		CheckSQL("SELECT * FROM a, b, c WHERE a.id = b.id", "SELECT * FROM a, b, c WHERE (a.id = b.id)");
		CheckSQL("SELECT * FROM a JOIN b", "SELECT * FROM a JOIN b");
		CheckSQL("SELECT s.cnt FROM (SELECT cls, COUNT(*) AS cnt FROM roads GROUP BY cls) s JOIN classes k ON k.id = s.cls",
			"SELECT s.cnt FROM (SELECT cls, COUNT(*) AS cnt FROM roads GROUP BY cls) AS s JOIN classes AS k ON (k.id = s.cls)");

		// the tree: left deep
		CSQLDriver driver;
		auto ptrSelect = std::static_pointer_cast<SSelectStmt>(driver.ParseStatement("SELECT * FROM a JOIN b ON x = 1 LEFT JOIN c ON y = 2"));
		const STableRefPtr& ptrFrom = ptrSelect->m_parts[0].m_core.m_from[0];
		bool bOk = ptrFrom->m_type == eTableRefType::Join;
		if (bOk)
		{
			auto pJoin = static_cast<const SJoinRef*>(ptrFrom.get());
			bOk = pJoin->m_joinType == eJoinType::Left && pJoin->m_ptrLeft->m_type == eTableRefType::Join &&
				pJoin->m_ptrRight->m_type == eTableRefType::Table;
		}
		if (!bOk)
		{
			std::cout << "FAILED: the join tree isn't left deep" << std::endl;
			++g_nFailed;
		}
		else
			++g_nPassed;
	}

	void TestDML()
	{
		CheckSQL("INSERT INTO roads (name, len) VALUES ('A1', 10.5), ('A2', ?)", "INSERT INTO roads (name, len) VALUES ('A1', 10.5), ('A2', ?)");
		CheckSQL("insert into t values (1, null, -3)", "INSERT INTO t VALUES (1, NULL, -3)");
		CheckSQL("INSERT INTO t (a) SELECT b FROM u WHERE c = 1", "INSERT INTO t (a) SELECT b FROM u WHERE (c = 1)");
		CheckSQL("UPDATE roads SET len = len * 2, name = 'x' WHERE id = 5", "UPDATE roads SET len = (len * 2), name = 'x' WHERE (id = 5)");
		CheckSQL("UPDATE t SET a = NULL", "UPDATE t SET a = NULL");
		CheckSQL("DELETE FROM roads WHERE cls IN (1, 2)", "DELETE FROM roads WHERE (cls IN (1, 2))");
		CheckSQL("DELETE FROM roads", "DELETE FROM roads");
	}

	void TestDDL()
	{
		CheckSQL("CREATE TABLE IF NOT EXISTS roads (id bigint NOT NULL UNIQUE, name varchar(64) DEFAULT 'none', len double DEFAULT -1, geom geometry, note text NULL)",
			"CREATE TABLE IF NOT EXISTS roads (id BIGINT NOT NULL UNIQUE, name VARCHAR(64) DEFAULT 'none', len DOUBLE DEFAULT -1, geom GEOMETRY, note TEXT)");
		CheckSQL("DROP TABLE roads; DROP TABLE IF EXISTS x", "DROP TABLE roads; DROP TABLE IF EXISTS x");
		CheckSQL("ALTER TABLE roads ADD COLUMN speed int DEFAULT 50", "ALTER TABLE roads ADD COLUMN speed INT DEFAULT 50");
		CheckSQL("ALTER TABLE roads ADD speed int", "ALTER TABLE roads ADD COLUMN speed INT");
		CheckSQL("ALTER TABLE roads DROP COLUMN speed; ALTER TABLE roads DROP note", "ALTER TABLE roads DROP COLUMN speed; ALTER TABLE roads DROP COLUMN note");
		CheckSQL("CREATE INDEX idx_name ON roads (name)", "CREATE INDEX idx_name ON roads (name)");
		CheckSQL("CREATE UNIQUE INDEX IF NOT EXISTS ON roads (id)", "CREATE UNIQUE INDEX IF NOT EXISTS ON roads (id)");
		CheckSQL("CREATE SPATIAL INDEX ON roads (geom) USING rtree", "CREATE SPATIAL INDEX ON roads (geom) USING RTREE");
		CheckSQL("CREATE SPATIAL INDEX g ON roads (geom) USING ZOrder(-180, -90, 180, 90, 0.001)",
			"CREATE SPATIAL INDEX g ON roads (geom) USING ZORDER(-180.0, -90.0, 180.0, 90.0, 0.001)");
		CheckSQL("DROP INDEX ON roads (name); DROP INDEX IF EXISTS idx ON roads (geom)", "DROP INDEX ON roads (name); DROP INDEX IF EXISTS idx ON roads (geom)");
	}

	void TestParameters()
	{
		CSQLDriver driver;
		auto statements = driver.Parse("SELECT * FROM t WHERE a = ? AND b IN (?, ?); UPDATE t SET c = ?");
		bool bOk = driver.GetParameterCount() == 4 && statements.size() == 2;
		if (bOk)
		{
			auto ptrUpdate = std::static_pointer_cast<SUpdateStmt>(statements[1]);
			const SExprPtr& ptrValue = ptrUpdate->m_set[0].m_ptrValue;
			bOk = ptrValue->m_type == eExprType::Parameter && static_cast<const SParameter*>(ptrValue.get())->m_nIndex == 3;
		}
		if (!bOk)
		{
			std::cout << "FAILED: parameters" << std::endl;
			++g_nFailed;
		}
		else
			++g_nPassed;

		// positions
		auto ptrSelect = std::static_pointer_cast<SSelectStmt>(driver.ParseStatement("SELECT a,\n  bcd + 1\nFROM t"));
		const SExprPtr& ptrExpr = ptrSelect->m_parts[0].m_core.m_items[1].m_ptrExpr;
		if (ptrExpr->m_pos.m_nLine != 2 || ptrExpr->m_pos.m_nColumn != 3)
		{
			std::cout << "FAILED: position " << ptrExpr->m_pos.m_nLine << ":" << ptrExpr->m_pos.m_nColumn << std::endl;
			++g_nFailed;
		}
		else
			++g_nPassed;
	}

	void TestErrors()
	{
		CheckError("SELECT FROM t", "line 1, column 8: syntax error, unexpected FROM");
		CheckError("SELECT a FROM t WHERE", "line 1, column 22: syntax error, unexpected end of input");
		CheckError("SELECT a\nFROM t\nWHERE a = = 1", "line 3, column 11");
		CheckError("SELECT 'abc", "the string isn't closed");
		CheckError("SELECT \"abc", "the quoted identifier isn't closed");
		CheckError("SELECT a /* comment", "the comment isn't closed");
		CheckError("SELECT a # b", "line 1, column 10: unexpected character '#'");
		CheckError("SELECT * FROM a LEFT JOIN b", "an outer join needs ON or USING");
		CheckError("SELECT * FROM a CROSS JOIN b ON a.x = b.x", "CROSS JOIN has no ON / USING");
		CheckError("SELECT a = b = c", "syntax error");
		CheckError("SELECT 1 SELECT 2", "unexpected SELECT");

		try
		{
			CSQLDriver driver;
			driver.ParseStatement("SELECT 1; SELECT 2");
			std::cout << "FAILED: ParseStatement of 2 statements" << std::endl;
			++g_nFailed;
		}
		catch (std::exception&)
		{
			++g_nPassed;
		}
	}
}

int main()
{
	TestSelect();
	TestExpressions();
	TestJoins();
	TestDML();
	TestDDL();
	TestParameters();
	TestErrors();

	std::cout << g_nPassed << " passed, " << g_nFailed << " failed" << std::endl;
	return g_nFailed == 0 ? 0 : 1;
}
