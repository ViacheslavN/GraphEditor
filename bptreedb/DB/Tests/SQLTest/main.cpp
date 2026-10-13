// SQL of bptreedb against an in-memory model of the data.
//   TestSQL [rows] [path]
// The same query through an index and through a scan (a copy of the column without an index) gives the same
// rows; joins, aggregates, subqueries, set operations and spatial queries are compared with the model.

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include "../../bptreedb.h"
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/SpatialData/GeoShape.h"

using namespace bptreedb;

namespace
{
	int g_nFailed = 0;
	int g_nChecks = 0;
	std::string g_path = "sqltest.btdb";

	void Check(bool bOk, const std::string& sWhat)
	{
		++g_nChecks;
		if (!bOk)
		{
			++g_nFailed;
			std::cout << "FAILED: " << sWhat << std::endl;
		}
	}

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	//------------------------------------------------------------------------------------------
	// the model

	const char* const Countries[] = { "CZ", "DE", "AT", "PL", "SK" };

	struct SCity
	{
		int64_t m_id;
		std::string m_name;
		bool m_bPopulation;
		int64_t m_population;
		std::string m_country;
	};

	struct SRoad
	{
		int64_t m_id;
		bool m_bName;
		std::string m_name;
		bool m_bCity;
		int64_t m_city;
		double m_len;
		int64_t m_cls;
		double m_x, m_y;
	};

	std::vector<SCity> g_cities;
	std::map<int64_t, SRoad> g_roads;     // by id

	CommonLib::IGeoShapePtr Point(double x, double y)
	{
		auto ptrShape = std::make_shared<CommonLib::CGeoShape>();
		ptrShape->Create(CommonLib::shape_type_point, 1);
		ptrShape->GetPoints()[0] = { x, y };
		ptrShape->CalcBB();
		return ptrShape;
	}

	CommonLib::IGeoShapePtr Rect(double x1, double y1, double x2, double y2)
	{
		auto ptrShape = std::make_shared<CommonLib::CGeoShape>();
		ptrShape->Create(CommonLib::shape_type_polyline, 2, 1);
		ptrShape->GetPoints()[0] = { x1, y1 };
		ptrShape->GetPoints()[1] = { x2, y2 };
		ptrShape->CalcBB();
		return ptrShape;
	}

	const SCity* FindCity(int64_t id)
	{
		if (id < 1 || id > (int64_t)g_cities.size())
			return nullptr;
		return &g_cities[id - 1];
	}

	//------------------------------------------------------------------------------------------
	// queries as text rows

	std::string Text(const CommonLib::CVariant& value)
	{
		if (value.IsNull())
			return "NULL";
		if (value.IsType<int64_t>()) return std::to_string(value.Get<int64_t>());
		if (value.IsType<int32_t>()) return std::to_string(value.Get<int32_t>());
		if (value.IsType<double>())
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%.6g", value.Get<double>());
			return buf;
		}
		if (value.IsType<CommonLib::astr_t>()) return value.Get<CommonLib::astr_t>();
		if (value.IsType<CommonLib::IGeoShapePtr>()) return "<geometry>";
		return "<?>";
	}

	std::vector<std::string> Rows(ISelectCursorPtr ptrCursor, bool bSort = true)
	{
		std::vector<std::string> rows;
		while (ptrCursor->Next())
		{
			std::string sRow;
			for (int32_t i = 0; i < ptrCursor->ColumnCount(); ++i)
				sRow += (i ? "|" : "") + Text(ptrCursor->ReadValue(i));
			rows.push_back(sRow);
		}
		if (bSort)
			std::sort(rows.begin(), rows.end());
		return rows;
	}

	std::vector<std::string> Query(ITransactionPtr tran, const std::string& sSQL, bool bSort = true)
	{
		return Rows(tran->executeQuery(sSQL), bSort);
	}

	std::vector<std::string> Query(ITransactionPtr tran, const std::string& sSQL, const std::vector<CommonLib::CVariant>& params, bool bSort = true)
	{
		IStatementPtr ptrStatement = tran->prepare(sSQL);
		for (size_t i = 0; i < params.size(); ++i)
			ptrStatement->BindParameter((uint32_t)i, params[i]);
		return Rows(ptrStatement->executeQuery(), bSort);
	}

	std::string One(ITransactionPtr tran, const std::string& sSQL)
	{
		std::vector<std::string> rows = Query(tran, sSQL, false);
		if (rows.size() != 1)
			return "<" + std::to_string(rows.size()) + " rows>";
		return rows[0];
	}

	void CheckRows(const std::vector<std::string>& result, std::vector<std::string> expected, const std::string& sWhat, bool bSort = true)
	{
		if (bSort)
			std::sort(expected.begin(), expected.end());
		bool bOk = result == expected;
		Check(bOk, sWhat + ": " + std::to_string(result.size()) + " rows, expected " + std::to_string(expected.size()));
		if (!bOk)
		{
			for (size_t i = 0; i < (std::min<size_t>)(5, (std::max)(result.size(), expected.size())); ++i)
				std::cout << "   " << (i < result.size() ? result[i] : "-") << "   vs   " << (i < expected.size() ? expected[i] : "-") << std::endl;
		}
	}

	void CheckError(ITransactionPtr tran, const std::string& sSQL, const std::string& sExpected)
	{
		try
		{
			tran->execute(sSQL);
			Check(false, "no error: " + sSQL);
		}
		catch (std::exception& e)
		{
			std::string sMessage = e.what();
			bool bOk = sMessage.find(sExpected) != std::string::npos;
			Check(bOk, "error of '" + sSQL + "'");
			if (!bOk)
				std::cout << "   " << sMessage << std::endl;
		}
	}

	std::string Num(double d)
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.6g", d);
		return buf;
	}

	//------------------------------------------------------------------------------------------

	void Load(IDatabasePtr db, int nRoads)
	{
		std::mt19937_64 rng(7);
		std::uniform_real_distribution<double> u(0.0, 1.0);

		ITransactionPtr tran = db->StartTransaction(eTT_DDL);
		tran->execute(
			"CREATE TABLE cities (id INT NOT NULL UNIQUE, id2 INT, name VARCHAR(64), population BIGINT, country VARCHAR(8));"
			"CREATE TABLE roads (id BIGINT NOT NULL, name TEXT, city_id INT, len DOUBLE, cls INT DEFAULT 1, cls2 INT, "
			"  geom GEOMETRY, geom2 GEOMETRY);"
			"CREATE INDEX ON roads (cls);"
			"CREATE UNIQUE INDEX ON roads (id);"
			"CREATE SPATIAL INDEX ON roads (geom) USING RTREE;"
			"CREATE TABLE zones (name TEXT, geom GEOMETRY);"
			"CREATE TABLE t1 (k INT, a TEXT); CREATE TABLE t2 (k INT, b TEXT);"
			"CREATE TABLE empty (x INT)");
		tran->Commit();

		tran = db->StartTransaction(eTT_MODIFY);
		IStatementPtr ptrInsert = tran->prepare("INSERT INTO cities (id, id2, name, population, country) VALUES (?, ?, ?, ?, ?)");
		for (int64_t i = 1; i <= 200; ++i)
		{
			SCity city{ i, "City" + std::to_string(i), u(rng) > 0.1, (int64_t)(u(rng) * 1000000), Countries[i % 5] };
			g_cities.push_back(city);
			ptrInsert->BindParameter(0, CommonLib::CVariant(i));
			ptrInsert->BindParameter(1, CommonLib::CVariant(i));
			ptrInsert->BindParameter(2, CommonLib::CVariant(city.m_name));
			ptrInsert->BindParameter(3, city.m_bPopulation ? CommonLib::CVariant(city.m_population) : CommonLib::CVariant());
			ptrInsert->BindParameter(4, CommonLib::CVariant(city.m_country));
			ptrInsert->execute();
		}

		auto start = std::chrono::steady_clock::now();
		ptrInsert = tran->prepare("INSERT INTO roads (id, name, city_id, len, cls, cls2, geom, geom2) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
		for (int64_t i = 1; i <= nRoads; ++i)
		{
			SRoad road;
			road.m_id = i;
			road.m_bName = u(rng) > 0.1;
			road.m_name = "Road" + std::to_string(i % 500);
			road.m_bCity = u(rng) > 0.05;
			road.m_city = 1 + (int64_t)(u(rng) * 230);      // 201..230: no such city
			road.m_len = std::floor(u(rng) * 100000) / 100;
			road.m_cls = (int64_t)(u(rng) * 9);              // 0..8
			road.m_x = std::floor(u(rng) * 10000) / 10;
			road.m_y = std::floor(u(rng) * 10000) / 10;
			g_roads[i] = road;

			CommonLib::IGeoShapePtr ptrPoint = Point(road.m_x, road.m_y);
			ptrInsert->BindParameter(0, CommonLib::CVariant(i));
			ptrInsert->BindParameter(1, road.m_bName ? CommonLib::CVariant(road.m_name) : CommonLib::CVariant());
			ptrInsert->BindParameter(2, road.m_bCity ? CommonLib::CVariant(road.m_city) : CommonLib::CVariant());
			ptrInsert->BindParameter(3, CommonLib::CVariant(road.m_len));
			ptrInsert->BindParameter(4, CommonLib::CVariant(road.m_cls));
			ptrInsert->BindParameter(5, CommonLib::CVariant(road.m_cls));
			ptrInsert->BindParameter(6, CommonLib::CVariant(ptrPoint));
			ptrInsert->BindParameter(7, CommonLib::CVariant(ptrPoint));
			ptrInsert->execute();
		}
		std::cout << "  " << nRoads << " roads inserted by a prepared statement: " << Seconds(start) << " s" << std::endl;

		int64_t nRows = tran->execute(
			"INSERT INTO zones VALUES ('a', NULL), ('b', NULL);"
			"INSERT INTO t1 VALUES (1, 'x'), (2, 'y'), (3, 'z'), (NULL, 'n');"
			"INSERT INTO t2 (k, b) VALUES (2, 'B2'), (3, 'B3'), (3, 'B3b'), (4, 'B4')");
		Check(nRows == 10, "INSERT VALUES counts: " + std::to_string(nRows));

		IStatementPtr ptrZone = tran->prepare("UPDATE zones SET geom = ? WHERE name = ?");
		ptrZone->BindParameter(0, CommonLib::CVariant(Rect(100, 100, 300, 250)));
		ptrZone->BindParameter(1, CommonLib::CVariant(std::string("a")));
		Check(ptrZone->execute() == 1, "UPDATE of a geometry");
		ptrZone->BindParameter(0, CommonLib::CVariant(Rect(500, 500, 520, 900)));
		ptrZone->BindParameter(1, CommonLib::CVariant(std::string("b")));
		Check(ptrZone->execute() == 1, "UPDATE of a geometry");
		tran->Commit();
	}

	//------------------------------------------------------------------------------------------

	void TestExpressions(ITransactionPtr tran)
	{
		std::cout << "expressions" << std::endl;
		CheckRows(Query(tran, "SELECT 1 + 2 * 3, 7 / 2, 7 % 3, 7.0 / 2, -7 / 2, 'a' || 'b' || 1, 2 - -3", false),
			{ "7|3|1|3.5|-3|ab1|5" }, "arithmetic", false);
		CheckRows(Query(tran, "SELECT NULL + 1, 1 = NULL, NULL IS NULL, 1 IS NOT NULL, NULL AND 0, NULL OR 1, NOT NULL, 1 / 0", false),
			{ "NULL|NULL|1|1|0|1|NULL|NULL" }, "NULL logic", false);
		CheckRows(Query(tran, "SELECT CASE WHEN 1 > 2 THEN 'x' WHEN 2 > 1 THEN 'y' END, CASE 3 WHEN 1 THEN 'a' ELSE 'b' END, CASE WHEN 0 THEN 1 END", false),
			{ "y|b|NULL" }, "CASE", false);
		CheckRows(Query(tran, "SELECT CAST('12' AS INT) + 1, CAST(3.7 AS INTEGER), CAST(5 AS TEXT) || 'x', CAST('2.5' AS DOUBLE) * 2", false),
			{ "13|3|5x|5" }, "CAST", false);
		CheckRows(Query(tran, "SELECT UPPER('abc'), LOWER('ABC'), LENGTH('Praha'), LENGTH('Žluť'), SUBSTR('Hello', 2, 3), SUBSTR('Hello', -3), "
			"TRIM('  x  '), REPLACE('a-b-c', '-', '+'), INSTR('hello', 'll'), COALESCE(NULL, NULL, 5), NULLIF(1, 1), IFNULL(NULL, 'd')", false),
			{ "ABC|abc|5|4|ell|llo|x|a+b+c|3|5|NULL|d" }, "string functions", false);
		CheckRows(Query(tran, "SELECT ABS(-3), ABS(-2.5), ROUND(2.567, 2), FLOOR(2.7), CEIL(2.1), MOD(10, 3), POWER(2, 10), SQRT(16), "
			"MIN(3, 1, 2), MAX(3, 1, 2), SIGN(-4), TYPEOF(1), TYPEOF(1.5), TYPEOF('s'), TYPEOF(NULL)", false),
			{ "3|2.5|2.57|2|3|1|1024|4|1|3|-1|integer|real|text|null" }, "math functions", false);
		CheckRows(Query(tran, "SELECT 'abc' LIKE 'A%', 'abc' LIKE 'a_c', 'abc' NOT LIKE '%d', '50%' LIKE '50!%' ESCAPE '!', 'x' LIKE 'x%y'", false),
			{ "1|1|1|1|0" }, "LIKE", false);
		CheckRows(Query(tran, "SELECT 3 BETWEEN 1 AND 5, 3 NOT BETWEEN 1 AND 5, 2 IN (1, 2), 2 IN (1, NULL), 2 NOT IN (1, 3), '5' = 5, 2 = 2.0", false),
			{ "1|0|1|NULL|1|1|1" }, "predicates", false);
		CheckRows(Query(tran, "SELECT ? + 1, ? || 'x', ? IS NULL", { CommonLib::CVariant((int32_t)41), CommonLib::CVariant(std::string("a")), CommonLib::CVariant() }, false),
			{ "42|ax|1" }, "parameters", false);
		CheckRows(Query(tran, "SELECT 9223372036854775807 + 1 > 0, -9223372036854775808, 3000000000 * 4", false),
			{ "1|-9223372036854775808|12000000000" }, "integer overflow", false);
	}

	void TestSelect(ITransactionPtr tran)
	{
		std::cout << "select: index and scan paths" << std::endl;
		Check(One(tran, "SELECT COUNT(*) FROM roads") == std::to_string(g_roads.size()), "count");

		// the same condition on cls (index) and cls2 (scan) and in the model
		struct SCase
		{
			std::string m_sCondition;
			std::function<bool(const SRoad&)> m_match;
		};
		std::vector<SCase> cases = {
			{ "%s = 3", [](const SRoad& r) { return r.m_cls == 3; } },
			{ "%s = 3.0", [](const SRoad& r) { return r.m_cls == 3; } },
			{ "%s = 3.5", [](const SRoad&) { return false; } },
			{ "%s > 2.5 AND %s <= 5", [](const SRoad& r) { return r.m_cls > 2.5 && r.m_cls <= 5; } },
			{ "%s < 2", [](const SRoad& r) { return r.m_cls < 2; } },
			{ "%s >= 7", [](const SRoad& r) { return r.m_cls >= 7; } },
			{ "%s IN (1, 4, 4, 8, 99)", [](const SRoad& r) { return r.m_cls == 1 || r.m_cls == 4 || r.m_cls == 8; } },
			{ "%s BETWEEN 2 AND 4", [](const SRoad& r) { return r.m_cls >= 2 && r.m_cls <= 4; } },
			{ "%s = 3 AND len > 500", [](const SRoad& r) { return r.m_cls == 3 && r.m_len > 500; } },
			{ "5 > %s AND name LIKE 'Road1%'", [](const SRoad& r) { return r.m_cls < 5 && r.m_bName && r.m_name.compare(0, 5, "Road1") == 0; } },
			{ "%s = 3 OR %s = 4", [](const SRoad& r) { return r.m_cls == 3 || r.m_cls == 4; } },
			{ "%s = NULL", [](const SRoad&) { return false; } },
			{ "%s = '6'", [](const SRoad& r) { return r.m_cls == 6; } },
			{ "%s > 100000000000", [](const SRoad&) { return false; } },
			{ "%s > -100000000000", [](const SRoad&) { return true; } }
		};
		auto Replace = [](std::string s, const std::string& sColumn)
		{
			for (size_t nPos; (nPos = s.find("%s")) != std::string::npos;)
				s.replace(nPos, 2, sColumn);
			return s;
		};
		for (const SCase& c : cases)
		{
			std::vector<std::string> expected;
			for (const auto& item : g_roads)
				if (c.m_match(item.second))
					expected.push_back(std::to_string(item.first));
			CheckRows(Query(tran, "SELECT id FROM roads WHERE " + Replace(c.m_sCondition, "cls")), expected, "index: " + c.m_sCondition);
			CheckRows(Query(tran, "SELECT id FROM roads WHERE " + Replace(c.m_sCondition, "cls2")), expected, "scan: " + c.m_sCondition);
		}

		// the unique index, the row id, the cursor's row id
		const SRoad& road = g_roads[g_roads.size() / 2];
		CheckRows(Query(tran, "SELECT name, cls FROM roads WHERE id = ?", { CommonLib::CVariant(road.m_id) }),
			{ (road.m_bName ? road.m_name : "NULL") + "|" + std::to_string(road.m_cls) }, "unique index");
		ISelectCursorPtr c = tran->executeQuery("SELECT id FROM roads WHERE id = " + std::to_string(road.m_id));
		Check(c->Next() && c->GetRowID() > 0, "row id of the cursor");
		int64_t nRowId = c->GetRowID();
		CheckRows(Query(tran, "SELECT id FROM roads WHERE rowid = " + std::to_string(nRowId)), { std::to_string(road.m_id) }, "rowid =");
		CheckRows(Query(tran, "SELECT id FROM roads WHERE rowid IN (" + std::to_string(nRowId) + ", -5)"), { std::to_string(road.m_id) }, "rowid IN");
		Check(c->ColumnName(0) == "id" && c->GetColumnType(0) == dtInteger64, "column name / type");

		// ORDER BY / LIMIT / DISTINCT
		std::vector<std::pair<double, int64_t> > byLen;
		for (const auto& item : g_roads)
			byLen.emplace_back(item.second.m_len, item.first);
		std::stable_sort(byLen.begin(), byLen.end(), [](const std::pair<double, int64_t>& a, const std::pair<double, int64_t>& b)
		{
			return a.first != b.first ? a.first > b.first : a.second < b.second;
		});
		std::vector<std::string> expected;
		for (size_t i = 5; i < 15; ++i)
			expected.push_back(std::to_string(byLen[i].second));
		CheckRows(Query(tran, "SELECT id FROM roads ORDER BY len DESC, id LIMIT 10 OFFSET 5", false), expected, "ORDER BY LIMIT OFFSET", false);
		CheckRows(Query(tran, "SELECT id FROM roads ORDER BY len DESC, 1 LIMIT 5, 10", false), expected, "LIMIT offset, count", false);
		CheckRows(Query(tran, "SELECT id AS i, len FROM roads ORDER BY -len, i LIMIT 3", false),
			{ std::to_string(byLen[0].second) + "|" + Num(byLen[0].first), std::to_string(byLen[1].second) + "|" + Num(byLen[1].first),
			std::to_string(byLen[2].second) + "|" + Num(byLen[2].first) }, "ORDER BY an expression", false);

		std::set<int64_t> classes;
		for (const auto& item : g_roads)
			classes.insert(item.second.m_cls);
		expected.clear();
		for (int64_t cls : classes)
			expected.push_back(std::to_string(cls));
		CheckRows(Query(tran, "SELECT DISTINCT cls FROM roads ORDER BY cls", false), expected, "DISTINCT", false);

		// NULLs are the smallest
		std::vector<std::string> rows = Query(tran, "SELECT name FROM roads ORDER BY name LIMIT 1", false);
		Check(rows.size() == 1 && rows[0] == "NULL", "ORDER BY: NULL first");
		rows = Query(tran, "SELECT * FROM t1 ORDER BY k DESC", false);
		CheckRows(rows, { "3|z", "2|y", "1|x", "NULL|n" }, "SELECT *, DESC", false);
	}

	void TestAggregates(ITransactionPtr tran)
	{
		std::cout << "aggregates" << std::endl;
		struct SGroup
		{
			int64_t m_nCount{ 0 };
			double m_dSum{ 0 };
			int64_t m_nMin{ INT64_MAX }, m_nMax{ INT64_MIN };
			std::set<int64_t> m_cities;
		};
		std::map<int64_t, SGroup> groups;
		for (const auto& item : g_roads)
		{
			SGroup& g = groups[item.second.m_cls];
			++g.m_nCount;
			g.m_dSum += item.second.m_len;
			g.m_nMin = (std::min)(g.m_nMin, item.first);
			g.m_nMax = (std::max)(g.m_nMax, item.first);
			if (item.second.m_bCity)
				g.m_cities.insert(item.second.m_city);
		}

		// SUM / AVG of doubles: compared as numbers
		ISelectCursorPtr c = tran->executeQuery("SELECT cls, COUNT(*), SUM(len), MIN(id), MAX(id), AVG(len), COUNT(DISTINCT city_id) "
			"FROM roads GROUP BY cls ORDER BY cls");
		size_t nGroups = 0;
		bool bOk = true;
		while (c->Next())
		{
			++nGroups;
			const SGroup& g = groups[c->ReadInt64(0)];
			bOk = bOk && c->ReadInt64(1) == g.m_nCount && std::fabs(c->ReadDouble(2) - g.m_dSum) < 1e-6 * (std::max)(1.0, g.m_dSum) &&
				c->ReadInt64(3) == g.m_nMin && c->ReadInt64(4) == g.m_nMax &&
				std::fabs(c->ReadDouble(5) - g.m_dSum / g.m_nCount) < 1e-6 && c->ReadInt64(6) == (int64_t)g.m_cities.size();
		}
		Check(bOk && nGroups == groups.size(), "GROUP BY cls: COUNT, SUM, MIN, MAX, AVG, COUNT(DISTINCT)");

		std::vector<std::string> expected;
		for (const auto& item : groups)
			if (item.second.m_nCount > (int64_t)g_roads.size() / 9)
				expected.push_back(std::to_string(item.first) + "|" + std::to_string(item.second.m_nCount));
		CheckRows(Query(tran, "SELECT cls, COUNT(*) AS n FROM roads GROUP BY cls HAVING COUNT(*) > " + std::to_string(g_roads.size() / 9)),
			expected, "HAVING");
		CheckRows(Query(tran, "SELECT cls + 1, COUNT(*) FROM roads GROUP BY cls + 1 HAVING cls + 1 = 4"),
			{ "4|" + std::to_string(groups[3].m_nCount) }, "GROUP BY an expression");
		CheckRows(Query(tran, "SELECT cls % 2 AS parity, COUNT(*) FROM roads GROUP BY 1 ORDER BY 2 DESC LIMIT 1", false).size() == 1 ?
			std::vector<std::string>{ "ok" } : std::vector<std::string>{}, { "ok" }, "GROUP BY a column number");
		CheckRows(Query(tran, "SELECT COUNT(*), SUM(len), MIN(name), COUNT(name) FROM roads WHERE id < 0", false), { "0|NULL|NULL|0" }, "aggregates of no rows", false);
		CheckRows(Query(tran, "SELECT COUNT(*) FROM empty GROUP BY x", false), {}, "GROUP BY of no rows", false);
		CheckRows(Query(tran, "SELECT SUM(k), AVG(k), MAX(a), COUNT(k), COUNT(*) FROM t1", false), { "6|2|z|3|4" }, "aggregates with NULL", false);
	}

	void TestJoins(ITransactionPtr tran)
	{
		std::cout << "joins" << std::endl;
		std::vector<std::string> inner, left;
		std::set<int64_t> usedCities;
		for (const auto& item : g_roads)
		{
			const SRoad& r = item.second;
			const SCity* pCity = r.m_bCity ? FindCity(r.m_city) : nullptr;
			if (pCity)
			{
				inner.push_back(std::to_string(r.m_id) + "|" + pCity->m_name);
				usedCities.insert(pCity->m_id);
			}
			left.push_back(std::to_string(r.m_id) + "|" + (pCity ? pCity->m_name : "NULL"));
		}
		std::vector<std::string> right = inner, full = left;
		for (const SCity& city : g_cities)
			if (!usedCities.count(city.m_id))
			{
				right.push_back("NULL|" + city.m_name);
				full.push_back("NULL|" + city.m_name);
			}

		auto start = std::chrono::steady_clock::now();
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r JOIN cities c ON r.city_id = c.id"), inner, "inner join (index)");
		double dIndex = Seconds(start);
		start = std::chrono::steady_clock::now();
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r JOIN cities c ON c.id2 = r.city_id"), inner, "inner join (hash)");
		double dHash = Seconds(start);
		std::cout << "  join of " << g_roads.size() << " roads: index " << dIndex << " s, hash " << dHash << " s" << std::endl;
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r, cities c WHERE r.city_id = c.id"), inner, "comma join with WHERE");
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r LEFT JOIN cities c ON r.city_id = c.id"), left, "left join (index)");
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r LEFT OUTER JOIN cities c ON r.city_id = c.id2"), left, "left join (hash)");
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r RIGHT JOIN cities c ON r.city_id = c.id"), right, "right join");
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r FULL JOIN cities c ON r.city_id = c.id"), full, "full join");
		CheckRows(Query(tran, "SELECT r.id, c.name FROM cities c RIGHT JOIN roads r ON r.city_id = c.id"), left, "right join = left join swapped");

		// WHERE of the NULL side of an outer join: after the join
		std::vector<std::string> expected;
		for (const auto& item : g_roads)
			if (!(item.second.m_bCity && FindCity(item.second.m_city)))
				expected.push_back(std::to_string(item.first));
		CheckRows(Query(tran, "SELECT r.id FROM roads r LEFT JOIN cities c ON r.city_id = c.id WHERE c.id IS NULL"), expected, "anti join by LEFT JOIN");
		// ON of the right side of a left join: a filter before the join, every left row stays
		expected.clear();
		for (const auto& item : g_roads)
		{
			const SCity* pCity = item.second.m_bCity ? FindCity(item.second.m_city) : nullptr;
			expected.push_back(std::to_string(item.first) + "|" + (pCity && pCity->m_country == "CZ" ? pCity->m_name : "NULL"));
		}
		CheckRows(Query(tran, "SELECT r.id, c.name FROM roads r LEFT JOIN cities c ON r.city_id = c.id AND c.country = 'CZ'"), expected, "left join, ON of the right side");

		// non equality: nested loop
		int64_t nLess = 0;
		for (const SCity& a : g_cities)
			for (const SCity& b : g_cities)
				if (a.m_bPopulation && b.m_bPopulation && a.m_population < b.m_population)
					++nLess;
		Check(One(tran, "SELECT COUNT(*) FROM cities a JOIN cities b ON a.population < b.population") == std::to_string(nLess), "nested loop join");
		Check(One(tran, "SELECT COUNT(*) FROM cities a CROSS JOIN cities b") == std::to_string(g_cities.size() * g_cities.size()), "cross join");
		Check(One(tran, "SELECT COUNT(*) FROM cities a JOIN cities b ON a.id = b.id JOIN cities c ON c.id = b.id2") == std::to_string(g_cities.size()), "join of three");

		CheckRows(Query(tran, "SELECT k, a, b FROM t1 JOIN t2 USING (k)"), { "2|y|B2", "3|z|B3", "3|z|B3b" }, "USING");
		CheckRows(Query(tran, "SELECT t1.k, a, t2.k, b FROM t1 FULL JOIN t2 ON t1.k = t2.k"),
			{ "1|x|NULL|NULL", "2|y|2|B2", "3|z|3|B3", "3|z|3|B3b", "NULL|n|NULL|NULL", "NULL|NULL|4|B4" }, "full join with NULL keys");
		CheckRows(Query(tran, "SELECT a, b FROM t1 JOIN t2 ON t1.k < t2.k AND t2.b LIKE '%b'"), { "x|B3b", "y|B3b" }, "join: non equality and a filter");

		// join + GROUP BY
		std::map<std::string, int64_t> byCountry;
		for (const auto& item : g_roads)
		{
			const SCity* pCity = item.second.m_bCity ? FindCity(item.second.m_city) : nullptr;
			if (pCity)
				++byCountry[pCity->m_country];
		}
		expected.clear();
		for (const auto& item : byCountry)
			expected.push_back(item.first + "|" + std::to_string(item.second));
		CheckRows(Query(tran, "SELECT c.country, COUNT(*) FROM roads r JOIN cities c ON r.city_id = c.id GROUP BY c.country"), expected, "join + GROUP BY");
	}

	void TestSubqueries(ITransactionPtr tran)
	{
		std::cout << "subqueries, UNION" << std::endl;
		int64_t nCZ = 0;
		std::map<int64_t, int64_t> roadsOfCity;
		double dSum = 0;
		for (const auto& item : g_roads)
		{
			const SCity* pCity = item.second.m_bCity ? FindCity(item.second.m_city) : nullptr;
			if (pCity && pCity->m_country == "CZ")
				++nCZ;
			if (pCity)
				++roadsOfCity[pCity->m_id];
			dSum += item.second.m_len;
		}
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE city_id IN (SELECT id FROM cities WHERE country = 'CZ')") == std::to_string(nCZ), "IN (SELECT)");
		Check(One(tran, "SELECT COUNT(*) FROM cities c WHERE EXISTS (SELECT 1 FROM roads r WHERE r.city_id = c.id)") == std::to_string(roadsOfCity.size()), "correlated EXISTS");
		Check(One(tran, "SELECT COUNT(*) FROM cities c WHERE NOT EXISTS (SELECT 1 FROM roads r WHERE r.city_id = c.id)") == std::to_string(g_cities.size() - roadsOfCity.size()), "NOT EXISTS");

		std::vector<std::string> expected;
		for (const SCity& city : g_cities)
			if (city.m_id <= 20)
				expected.push_back(std::to_string(city.m_id) + "|" + std::to_string(roadsOfCity.count(city.m_id) ? roadsOfCity[city.m_id] : 0));
		CheckRows(Query(tran, "SELECT c.id, (SELECT COUNT(*) FROM roads r WHERE r.city_id = c.id) FROM cities c WHERE c.id <= 20 ORDER BY c.id", false),
			expected, "correlated scalar subquery", false);

		int64_t nLonger = 0;
		for (const auto& item : g_roads)
			if (item.second.m_len > dSum / g_roads.size())
				++nLonger;
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE len > (SELECT AVG(len) FROM roads)") == std::to_string(nLonger), "scalar subquery");

		std::map<int64_t, int64_t> perClass;
		for (const auto& item : g_roads)
			++perClass[item.second.m_cls];
		expected.clear();
		for (const auto& item : perClass)
			if (item.first >= 4)
				expected.push_back(std::to_string(item.first) + "|" + std::to_string(item.second));
		CheckRows(Query(tran, "SELECT s.cls, s.n FROM (SELECT cls, COUNT(*) AS n FROM roads GROUP BY cls) AS s WHERE s.cls >= 4"), expected, "subquery in FROM");
		CheckRows(Query(tran, "SELECT s.k, c.name FROM (SELECT k FROM t1 WHERE k IS NOT NULL) s JOIN cities c ON c.id = s.k"),
			{ "1|City1", "2|City2", "3|City3" }, "join of a subquery and a table");

		CheckRows(Query(tran, "SELECT k FROM t1 UNION SELECT k FROM t2 ORDER BY 1", false), { "NULL", "1", "2", "3", "4" }, "UNION", false);
		CheckRows(Query(tran, "SELECT k FROM t1 UNION ALL SELECT k FROM t2"), { "1", "2", "3", "NULL", "2", "3", "3", "4" }, "UNION ALL");
		CheckRows(Query(tran, "SELECT k FROM t1 INTERSECT SELECT k FROM t2"), { "2", "3" }, "INTERSECT");
		CheckRows(Query(tran, "SELECT k FROM t1 EXCEPT SELECT k FROM t2 ORDER BY k DESC", false), { "1", "NULL" }, "EXCEPT", false);
		CheckRows(Query(tran, "SELECT a FROM t1 WHERE k = 1 UNION SELECT b FROM t2 WHERE k = 4 UNION SELECT 'lit' ORDER BY a LIMIT 2", false),
			{ "B4", "lit" }, "UNION of three, ORDER BY name, LIMIT", false);
	}

	void TestSpatial(ITransactionPtr tran)
	{
		std::cout << "spatial" << std::endl;
		struct SBox
		{
			double x1, y1, x2, y2;
		};
		for (const SBox& box : { SBox{ 100, 100, 300, 250 }, SBox{ 0, 0, 50, 1000 }, SBox{ 999, 999, 2000, 2000 } })
		{
			std::vector<std::string> expected;
			for (const auto& item : g_roads)
				if (item.second.m_x >= box.x1 && item.second.m_x <= box.x2 && item.second.m_y >= box.y1 && item.second.m_y <= box.y2)
					expected.push_back(std::to_string(item.first));
			std::string sEnvelope = "ST_MakeEnvelope(" + Num(box.x1) + ", " + Num(box.y1) + ", " + Num(box.x2) + ", " + Num(box.y2) + ")";
			CheckRows(Query(tran, "SELECT id FROM roads WHERE ST_Intersects(geom, " + sEnvelope + ")"), expected, "spatial index " + sEnvelope);
			CheckRows(Query(tran, "SELECT id FROM roads WHERE ST_Intersects(geom2, " + sEnvelope + ")"), expected, "spatial scan " + sEnvelope);
			CheckRows(Query(tran, "SELECT id FROM roads WHERE ST_Within(geom, " + sEnvelope + ")"), expected, "ST_Within");
			CheckRows(Query(tran, "SELECT id FROM roads WHERE ST_Contains(" + sEnvelope + ", geom) AND cls >= 0"), expected, "ST_Contains(envelope, geom)");
		}
		CheckRows(Query(tran, "SELECT id FROM roads WHERE ST_Intersects(geom, ST_MakeEnvelope(?, ?, ?, ?)) AND id = ?",
			{ CommonLib::CVariant(0.0), CommonLib::CVariant(0.0), CommonLib::CVariant(2000.0), CommonLib::CVariant(2000.0), CommonLib::CVariant((int64_t)1) }),
			{ "1" }, "spatial with parameters");

		// a spatial join: the R-tree of roads for every zone
		std::vector<std::string> expected;
		for (const auto& item : g_roads)
		{
			const SRoad& r = item.second;
			if (r.m_x >= 100 && r.m_x <= 300 && r.m_y >= 100 && r.m_y <= 250)
				expected.push_back("a|" + std::to_string(r.m_id));
			if (r.m_x >= 500 && r.m_x <= 520 && r.m_y >= 500 && r.m_y <= 900)
				expected.push_back("b|" + std::to_string(r.m_id));
		}
		auto start = std::chrono::steady_clock::now();
		CheckRows(Query(tran, "SELECT z.name, r.id FROM zones z JOIN roads r ON ST_Intersects(r.geom, z.geom)"), expected, "spatial join (index)");
		double dIndex = Seconds(start);
		start = std::chrono::steady_clock::now();
		CheckRows(Query(tran, "SELECT z.name, r.id FROM zones z JOIN roads r ON ST_Intersects(z.geom, r.geom2)"), expected, "spatial join (nested loop)");
		std::cout << "  spatial join: index " << dIndex << " s, nested loop " << Seconds(start) << " s" << std::endl;

		ISelectCursorPtr c = tran->executeQuery("SELECT ST_XMin(geom), ST_YMax(geom), ST_NumPoints(geom), geom FROM zones WHERE name = 'a'");
		Check(c->Next() && c->ReadDouble(0) == 100 && c->ReadDouble(1) == 250 && c->ReadInt32(2) == 2 && c->ReadShape(3).get() != nullptr, "geometry functions");
	}

	void TestModify(IDatabasePtr db)
	{
		std::cout << "update, delete, insert select" << std::endl;
		int64_t nCls3 = 0, nCls8 = 0, nCls1 = 0;
		for (const auto& item : g_roads)
		{
			nCls3 += item.second.m_cls == 3;
			nCls8 += item.second.m_cls == 8;
			nCls1 += item.second.m_cls == 1;
		}

		// rolled back
		ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
		int64_t nUpdated = tran->execute("UPDATE roads SET len = len * 2, name = 'upd', cls = 103 WHERE cls = 3");
		Check(nUpdated == nCls3, "UPDATE count");
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE cls = 103 AND name = 'upd'") == std::to_string(nCls3), "UPDATE seen by its transaction (index)");
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE cls = 3") == "0", "UPDATE: the old index key is gone");
		{
			ITransactionPtr reader = db->StartTransaction(eTT_SELECT);
			Check(One(reader, "SELECT COUNT(*) FROM roads WHERE cls = 103") == "0", "a reader doesn't see the uncommitted UPDATE");
			reader->Commit();
		}
		tran->Rollback();
		tran = db->StartTransaction(eTT_SELECT);
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE cls = 3") == std::to_string(nCls3), "rolled back");
		tran->Commit();

		// committed
		tran = db->StartTransaction(eTT_MODIFY);
		Check(tran->execute("DELETE FROM roads WHERE cls = 8") == nCls8, "DELETE count");
		Check(tran->execute("INSERT INTO roads (id, name, cls, cls2, len) SELECT id + 1000000, name, cls, cls2, len FROM roads WHERE cls = 1") == nCls1, "INSERT SELECT of the same table");
		Check(tran->execute("UPDATE roads SET cls2 = cls2 + 100 WHERE id > 1000000") == nCls1, "UPDATE of the inserted rows");
		Check(tran->execute("UPDATE roads SET name = NULL WHERE rowid = -1") == 0, "UPDATE of no rows");
		tran->Commit();

		tran = db->StartTransaction(eTT_SELECT, eTTDirectRead);
		Check(One(tran, "SELECT COUNT(*) FROM roads") == std::to_string(g_roads.size() - nCls8 + nCls1), "the count after the commit");
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE cls = 8") == "0", "deleted (index)");
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE cls = 1") == std::to_string(2 * nCls1), "inserted (index)");
		Check(One(tran, "SELECT COUNT(*) FROM roads WHERE cls2 = 101") == std::to_string(nCls1), "updated");
		Check(One(tran, "SELECT MIN(cls) FROM roads WHERE id > 1000000 AND geom IS NULL") == "1", "INSERT SELECT: the columns not inserted are NULL");
		tran->Commit();
	}

	void TestDDL(IDatabasePtr db)
	{
		std::cout << "DDL" << std::endl;
		ITransactionPtr tran = db->StartTransaction(eTT_DDL);
		tran->execute("CREATE TABLE IF NOT EXISTS roads (x INT)");      // exists: nothing
		tran->execute("CREATE TABLE notes (id INT UNIQUE, text VARCHAR(20) NOT NULL DEFAULT 'none', score DOUBLE DEFAULT -1.5)");
		tran->execute("INSERT INTO notes (id) VALUES (1); INSERT INTO notes (id, text) VALUES (2, 'two')");
		CheckRows(Query(tran, "SELECT id, text, score FROM notes ORDER BY id", false), { "1|none|-1.5", "2|two|-1.5" }, "DEFAULT values", false);
		tran->execute("ALTER TABLE notes ADD COLUMN flag INT DEFAULT 7");
		CheckRows(Query(tran, "SELECT id, flag FROM notes ORDER BY id", false), { "1|7", "2|7" }, "ALTER TABLE ADD COLUMN of a filled table", false);
		tran->execute("ALTER TABLE notes DROP COLUMN score; CREATE INDEX ON notes (flag); DROP INDEX ON notes (flag); CREATE INDEX IF NOT EXISTS ON notes (id)");
		CheckRows(Query(tran, "SELECT * FROM notes WHERE id = 2", false), { "2|two|7" }, "DROP COLUMN", false);
		tran->execute("CREATE INDEX z ON zones (geom) USING ZORDER(0, 0, 1000, 1000)");
		CheckRows(Query(tran, "SELECT name FROM zones WHERE ST_Intersects(geom, ST_MakeEnvelope(510, 510, 511, 511))", false), { "b" }, "Z-order index", false);
		tran->execute("DROP TABLE notes; DROP TABLE IF EXISTS notes");
		Check(db->GetSchema()->GetTable("notes").get() == nullptr, "DROP TABLE");
		tran->Commit();
	}

	void CheckPlan(ITransactionPtr tran, const std::string& sSQL, const std::vector<std::string>& parts)
	{
		std::string sPlan = tran->prepare(sSQL)->GetPlan();
		bool bOk = true;
		for (const std::string& sPart : parts)
			bOk = bOk && sPlan.find(sPart) != std::string::npos;
		Check(bOk, "plan of '" + sSQL + "'");
		if (!bOk)
			std::cout << sPlan;
	}

	void TestPlans(IDatabasePtr db)
	{
		std::cout << "plans" << std::endl;
		ITransactionPtr tran = db->StartTransaction(eTT_SELECT);
		CheckPlan(tran, "SELECT id FROM roads WHERE cls = 3", { "table roads: index cls =" });
		CheckPlan(tran, "SELECT id FROM roads WHERE 3 < cls AND len > 5", { "table roads: index cls >, 2 condition(s)" });
		CheckPlan(tran, "SELECT id FROM roads WHERE cls = 3 AND id = 5", { "table roads: index id =" });       // the unique index
		CheckPlan(tran, "SELECT id FROM roads WHERE cls IN (1, 2)", { "index cls IN" });
		CheckPlan(tran, "SELECT id FROM roads WHERE rowid = ?", { "table roads: row ids" });
		CheckPlan(tran, "SELECT id FROM roads WHERE cls2 = 3 OR cls = 4", { "table roads: scan" });
		CheckPlan(tran, "SELECT id FROM roads WHERE ST_Intersects(geom, ST_MakeEnvelope(0, 0, 1, 1))", { "spatial index geom" });
		CheckPlan(tran, "SELECT r.id FROM roads r JOIN cities c ON r.city_id = c.id WHERE c.country = 'CZ' AND r.cls = 2",
			{ "index join (inner)", "table roads: index cls =", "table cities: index id =, 1 condition(s)" });
		CheckPlan(tran, "SELECT r.id FROM roads r JOIN cities c ON r.city_id = c.id2", { "hash join (inner)" });
		CheckPlan(tran, "SELECT r.id FROM cities c JOIN roads r ON r.cls = c.id", { "index join (inner)", "table roads: index cls =" });
		CheckPlan(tran, "SELECT r.id FROM roads r LEFT JOIN cities c ON r.city_id = c.id WHERE c.id IS NULL", { "filter, 1 condition(s)", "index join (left)" });
		CheckPlan(tran, "SELECT 1 FROM cities a JOIN cities b ON a.population < b.population", { "nested loop join (inner), 1 condition(s)" });
		CheckPlan(tran, "SELECT z.name FROM zones z JOIN roads r ON ST_Intersects(z.geom, r.geom)", { "index join (inner)", "table roads: spatial index geom" });
		CheckPlan(tran, "SELECT cls, COUNT(*) FROM roads GROUP BY cls ORDER BY 2 LIMIT 3", { "limit", "sort: 1 key(s)", "aggregate: 1 group key(s), 1 aggregate(s)" });
		CheckPlan(tran, "SELECT k FROM t1 UNION SELECT k FROM t2", { "UNION" });
		tran->Commit();
	}

	void TestStatements(IDatabasePtr db)
	{
		std::cout << "prepared statements, errors" << std::endl;
		ITransactionPtr tran = db->StartTransaction(eTT_SELECT);
		IStatementPtr ptrStatement = tran->prepare("SELECT COUNT(*) FROM roads WHERE cls = ? AND len >= ?");
		Check(ptrStatement->GetParameterCount() == 2 && ptrStatement->IsQuery(), "parameters of a statement");
		std::map<int64_t, int64_t> perClass;
		for (const auto& item : g_roads)
			if (item.second.m_len >= 100)
				++perClass[item.second.m_cls];
		bool bOk = true;
		for (int64_t cls = 0; cls < 8; ++cls)
		{
			if (cls == 1 || cls == 3)
				continue;     // changed by TestModify
			ptrStatement->BindParameter(0, CommonLib::CVariant(cls));
			ptrStatement->BindParameter(1, CommonLib::CVariant(100));
			std::vector<std::string> rows = Rows(ptrStatement->executeQuery());
			bOk = bOk && rows.size() == 1 && rows[0] == std::to_string(perClass[cls]);
		}
		Check(bOk, "a statement executed with other parameters");

		ISelectCursorPtr c1 = ptrStatement->executeQuery();
		ISelectCursorPtr c2 = ptrStatement->executeQuery();
		bool bThrown = false;
		try
		{
			c1->Next();
		}
		catch (std::exception&)
		{
			bThrown = true;
		}
		Check(bThrown && c2->Next(), "the cursor of the previous execution is invalid");

		ptrStatement->ClearParameters();
		bThrown = false;
		try
		{
			ptrStatement->executeQuery();
		}
		catch (std::exception& e)
		{
			bThrown = std::string(e.what()).find("isn't bound") != std::string::npos;
		}
		Check(bThrown, "an unbound parameter");

		CheckError(tran, "SELECT * FROM nothing", "no table 'nothing'");
		CheckError(tran, "SELECT nothing FROM roads", "line 1, column 8: no column 'nothing'");
		CheckError(tran, "SELECT k FROM t1, t2", "column 'k' is ambiguous");
		CheckError(tran, "SELECT x.k FROM t1", "no table or alias 'x'");
		CheckError(tran, "SELECT a, COUNT(*) FROM t1", "has to be in GROUP BY");
		CheckError(tran, "SELECT k FROM t1 WHERE COUNT(*) > 1", "aggregate COUNT isn't allowed here");
		CheckError(tran, "SELECT SUM(COUNT(k)) FROM t1", "aggregate COUNT isn't allowed here");
		CheckError(tran, "SELECT k FROM t1 UNION SELECT k, b FROM t2", "different numbers of columns");
		CheckError(tran, "SELECT * FROM t1 WHERE k IN (SELECT k, b FROM t2)", "has to return one column");
		CheckError(tran, "SELECT (SELECT k FROM t2)", "more than one row");
		CheckError(tran, "SELECT FOO(1)", "unknown function FOO");
		CheckError(tran, "SELECT ABS(1, 2)", "ABS takes 1 argument");
		CheckError(tran, "SELECT DISTINCT k FROM t1 ORDER BY a", "ORDER BY of SELECT DISTINCT");
		CheckError(tran, "SELECT * FROM t1 a JOIN t2 a ON 1 = 1", "is used by two tables");
		CheckError(tran, "SELECT 1 FROM t1 JOIN t2 ON t1.k = t3.k JOIN t1 t3 ON 1 = 1", "no table or alias 't3'");
		CheckError(tran, "INSERT INTO t1 VALUES (1)", "INSERT has 2 columns");
		CheckError(tran, "SELECT 'a' + 1", "'a' isn't a number");
		CheckError(tran, "SELECT ST_Intersects(1, ST_MakeEnvelope(1, 2, 3))", "four arguments");
		CheckError(tran, "SELECT a FROM t1 WHERE", "syntax error");
		CheckError(tran, "UPDATE t1 SET a = 'q'", "");          // a read transaction can't write
		bThrown = false;
		try
		{
			tran->executeQuery("DELETE FROM t1");
		}
		catch (std::exception& e)
		{
			bThrown = std::string(e.what()).find("use execute") != std::string::npos;
		}
		Check(bThrown, "executeQuery of DELETE");
		tran->Commit();

		tran = db->StartTransaction(eTT_MODIFY);
		CheckError(tran, "CREATE TABLE x (a INT)", "");                     // not a DDL transaction
		CheckError(tran, "INSERT INTO cities (id, name) VALUES (1, 'dup')", "");   // unique id
		CheckError(tran, "INSERT INTO cities (name) VALUES ('no id')", "");        // NOT NULL
		CheckError(tran, "INSERT INTO cities (id, name) VALUES (5000, 'a name longer than sixty four bytes ....................................')", "");
		tran->Rollback();
	}
}

int main(int argc, char** argv)
{
	int nRoads = argc > 1 ? std::atoi(argv[1]) : 5000;
	if (argc > 2)
		g_path = argv[2];
	try
	{
		for (const char* pSuffix : { "", "-redo", "-tran" })
			std::filesystem::remove(g_path + pSuffix);
		IDatabasePtr db = CreateDatabase();
		db->Create(g_path);
		std::cout << "load" << std::endl;
		Load(db, nRoads);

		ITransactionPtr tran = db->StartTransaction(eTT_SELECT);
		TestExpressions(tran);
		TestSelect(tran);
		TestAggregates(tran);
		TestJoins(tran);
		TestSubqueries(tran);
		TestSpatial(tran);
		tran->Commit();
		TestPlans(db);

		TestModify(db);
		TestDDL(db);
		TestStatements(db);
		db->Close();
	}
	catch (std::exception& e)
	{
		std::cout << "EXCEPTION: " << e.what() << std::endl;
		++g_nFailed;
	}
	std::cout << g_nChecks << " checks, " << g_nFailed << " failed" << std::endl;
	return g_nFailed == 0 ? 0 : 1;
}
