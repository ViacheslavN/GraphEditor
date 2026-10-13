// Test of the database engine (DB/bptreedb.h) against an in-memory model:
//   schema (tables, fields of all types, unique / regular / R-tree / Z-order indexes) and its reopening,
//   insert / select (full scan, by index, by a field without index, spatial) / update / delete,
//   NOT NULL, unique and type checks (a failed check doesn't break the transaction),
//   rollback of data and of DDL, recovery from the journal after a "crash", parallel read transactions.
//
// usage: TestDB [rows] [database file]
//   rows          - rows of the main table, default 20000
//   database file - default F:\DBTest.btdb (Windows), /tmp/DBTest.btdb (other)

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "../../bptreedb.h"
#include "../../Database.h"          // the test hook of the commit (recovery test)
#include "../../../../CommonLib/exception/exc_base.h"
#include "../../../../CommonLib/SpatialData/GeoShape.h"
#include "../../../../CommonLib/data/blob.h"

using namespace bptreedb;

namespace
{
	std::string g_path;
	int g_nFailed = 0;

	double Seconds(std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	void Check(bool b, const std::string& msg)
	{
		if (!b)
			throw CommonLib::CExcBase("check failed: {0}", msg);
	}

	template <class TFunc>
	bool Throws(TFunc func)
	{
		try { func(); }
		catch (std::exception&) { return true; }
		return false;
	}

	//--------------------------------------------------------------------------------------------
	// the model of the table "roads"

	struct SRoad
	{
		int64_t m_id{ 0 };                      // unique index
		bool m_bName{ false };
		std::string m_name;                     // string(64), regular index, NULL sometimes
		double m_len{ 0 };                      // no index
		uint8_t m_cls{ 0 };                     // regular index, NOT NULL, default 1
		std::string m_note;                     // unlimited string (blob storage)
		std::vector<byte_t> m_data;             // blob, NULL if empty
		CommonLib::bbox m_bb;                   // geometry (R-tree): a point or a rect
		bool m_bPoint{ true };
	};
	typedef std::map<int64_t, SRoad> TModel;    // by row id

	CommonLib::IGeoShapePtr MakeShape(const SRoad& r)
	{
		std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
		if (r.m_bPoint)
		{
			ptrShape->Create(CommonLib::shape_type_point, 1);
			ptrShape->GetPoints()[0] = { r.m_bb.xMin, r.m_bb.yMin };
		}
		else
		{
			ptrShape->Create(CommonLib::shape_type_polyline, 2, 1);
			ptrShape->GetPoints()[0] = { r.m_bb.xMin, r.m_bb.yMin };
			ptrShape->GetPoints()[1] = { r.m_bb.xMax, r.m_bb.yMax };
		}
		ptrShape->CalcBB();
		return ptrShape;
	}

	SRoad RandomRoad(std::mt19937_64& rng, int64_t id)
	{
		static const char* words[] = { "Main", "North", "River", "Park", "Lake", "Praha", "Brno", "Station", "School", "Garden" };
		std::uniform_real_distribution<double> u(0.0, 1.0);
		SRoad r;
		r.m_id = id;
		r.m_bName = u(rng) > 0.1;
		if (r.m_bName)
			r.m_name = std::string(words[rng() % 10]) + " " + std::to_string(rng() % 500);
		r.m_len = std::floor(u(rng) * 100000) / 100;
		r.m_cls = (uint8_t)(rng() % 8);
		size_t nNote = u(rng) < 0.05 ? 3000 + rng() % 20000 : rng() % 40;    // a few long notes go to stream pages
		for (size_t i = 0; i < nNote; ++i)
			r.m_note += (char)('a' + rng() % 26);
		if (u(rng) < 0.3)
		{
			r.m_data.resize(rng() % 200 + 1);
			for (byte_t& b : r.m_data)
				b = (byte_t)rng();
		}
		r.m_bPoint = u(rng) < 0.6;
		double x = u(rng) * 1000, y = u(rng) * 1000;
		r.m_bb.xMin = x; r.m_bb.yMin = y;
		r.m_bb.xMax = r.m_bPoint ? x : (std::min)(1000.0, x + u(rng) * 20);
		r.m_bb.yMax = r.m_bPoint ? y : (std::min)(1000.0, y + u(rng) * 20);
		return r;
	}

	IFieldSetPtr RoadFields()
	{
		return IFieldSet::Create({ "id", "name", "len", "cls", "note", "data", "geom" });
	}

	void BindRoad(IInsertCursorPtr ptrCursor, const SRoad& r)
	{
		ptrCursor->BindInt32(0, (int32_t)r.m_id);      // converted to int64
		if (r.m_bName)
			ptrCursor->BindText(1, r.m_name, true);
		else
			ptrCursor->BindNull(1);
		ptrCursor->BindDouble(2, r.m_len);
		ptrCursor->BindUInt8(3, r.m_cls);
		ptrCursor->BindText(4, r.m_note, true);
		if (!r.m_data.empty())
			ptrCursor->BindBlob(5, r.m_data.data(), (int32_t)r.m_data.size(), true);
		ptrCursor->BindShape(6, MakeShape(r));
	}

	// a row of a cursor with RoadFields()
	void CheckRoad(ISelectCursorPtr c, const SRoad& r)
	{
		std::string sRow = "row " + std::to_string(c->GetRowID());
		Check(c->ReadInt64(0) == r.m_id, sRow + " id");
		Check(c->ColumnIsNull(1) == !r.m_bName, sRow + " name null");
		if (r.m_bName)
			Check(c->ReadText(1) == r.m_name, sRow + " name");
		Check(c->ReadDouble(2) == r.m_len, sRow + " len");
		Check(c->ReadUInt8(3) == r.m_cls, sRow + " cls");
		Check(c->ReadText(4) == r.m_note, sRow + " note");
		Check(c->ColumnIsNull(5) == r.m_data.empty(), sRow + " data null");
		if (!r.m_data.empty())
		{
			const byte_t* p = nullptr;
			int32_t n = 0;
			c->ReadBlob(5, &p, n);
			Check(n == (int32_t)r.m_data.size() && memcmp(p, r.m_data.data(), n) == 0, sRow + " data");
		}
		CommonLib::bbox bb = c->ReadShape(6)->GetBB();
		Check(bb.xMin == r.m_bb.xMin && bb.yMin == r.m_bb.yMin && bb.xMax == r.m_bb.xMax && bb.yMax == r.m_bb.yMax, sRow + " geom");
	}

	// all rows, by index and by field queries against the model
	// every second check through a direct read transaction (the trees the table shares)
	void CheckTable(IDatabasePtr db, const TModel& model, std::mt19937_64& rng)
	{
		static int nCheck = 0;
		ITransactionPtr tran = db->StartTransaction(eTT_SELECT, (nCheck++ % 2) ? eTTDirectRead : eTT2PL);
		ITablePtr table = db->GetSchema()->GetTable("roads");
		Check(table->GetRowCount() == (int64_t)model.size(), "row count " + std::to_string(table->GetRowCount()) + " model " + std::to_string(model.size()));

		// full scan: row id order
		ISelectCursorPtr c = tran->executeSelectQuery("roads", RoadFields());
		auto it = model.begin();
		size_t n = 0;
		while (c->Next())
		{
			Check(it != model.end() && c->GetRowID() == it->first, "scan row " + std::to_string(c->GetRowID()));
			CheckRoad(c, it->second);
			++it, ++n;
		}
		Check(it == model.end(), "scan: " + std::to_string(n) + " rows of " + std::to_string(model.size()));

		std::vector<SRoad> rows;
		for (const auto& kv : model)
			rows.push_back(kv.second);

		// queries: the found row ids against the model
		auto query = [&](const std::string& sField, const CommonLib::CVariant& key, OpType op, const std::function<bool(const SRoad&)>& pred)
		{
			std::set<int64_t> expected, found;
			for (const auto& kv : model)
				if (pred(kv.second))
					expected.insert(kv.first);
			ISelectCursorPtr q = tran->executeSelectQuery("roads", RoadFields(), sField, key, op);
			while (q->Next())
			{
				Check(found.insert(q->GetRowID()).second, "query " + sField + ": row twice");
				CheckRoad(q, model.at(q->GetRowID()));
			}
			Check(found == expected, "query " + sField + " " + std::to_string((int)op) + ": " + std::to_string(found.size()) + " rows, expected " + std::to_string(expected.size()));
		};

		for (int i = 0; i < 6 && !rows.empty(); ++i)
		{
			const SRoad& r = rows[rng() % rows.size()];
			OpType op = (OpType)(rng() % 6);
			auto cmp = [op](auto a, auto b)
			{
				switch (op)
				{
				case OpEqual: return a == b;
				case OpNoEqual: return a != b;
				case OpLess: return a < b;
				case OpLessOrEqual: return a <= b;
				case OpGreater: return a > b;
				default: return a >= b;
				}
			};
			query("id", CommonLib::CVariant(r.m_id), op, [&](const SRoad& x) { return cmp(x.m_id, r.m_id); });
			query("cls", CommonLib::CVariant((int32_t)r.m_cls), op, [&](const SRoad& x) { return cmp(x.m_cls, r.m_cls); });
			query("len", CommonLib::CVariant(r.m_len), op, [&](const SRoad& x) { return cmp(x.m_len, r.m_len); });
			if (r.m_bName)
				query("name", CommonLib::CVariant(r.m_name), op, [&](const SRoad& x) { return x.m_bName && cmp(x.m_name, r.m_name); });
		}

		// spatial: R-tree, all modes, against the bounding boxes
		for (int i = 0; i < 10; ++i)
		{
			std::uniform_real_distribution<double> u(0.0, 1000.0);
			CommonLib::bbox q;
			q.xMin = u(rng); q.yMin = u(rng);
			double d = i < 5 ? 30 : 0.5;
			q.xMax = q.xMin + d; q.yMax = q.yMin + d;
			for (eSpatialQueryMode mode : { sqmIntersect, sqmInside, sqmOver })
			{
				std::set<int64_t> expected, found;
				for (const auto& kv : model)
				{
					const CommonLib::bbox& b = kv.second.m_bb;
					bool bMatch = mode == sqmIntersect ? !(b.xMax < q.xMin || q.xMax < b.xMin || b.yMax < q.yMin || q.yMax < b.yMin)
						: mode == sqmInside ? (q.xMin <= b.xMin && q.yMin <= b.yMin && b.xMax <= q.xMax && b.yMax <= q.yMax)
						: (b.xMin <= q.xMin && b.yMin <= q.yMin && q.xMax <= b.xMax && q.yMax <= b.yMax);
					if (bMatch)
						expected.insert(kv.first);
				}
				ISelectCursorPtr s = tran->executeSpatialQuery(q, "roads", "geom", mode, IFieldSet::Create({ "id" }));
				while (s->Next())
				{
					found.insert(s->GetRowID());
					Check(s->ReadInt64(0) == model.at(s->GetRowID()).m_id, "spatial id");
				}
				Check(found == expected, "spatial query mode " + std::to_string((int)mode) + ": " + std::to_string(found.size()) + " rows, expected " + std::to_string(expected.size()));
			}
		}
		tran->Commit();
	}

	//--------------------------------------------------------------------------------------------

	void CreateSchema(IDatabasePtr db)
	{
		ITransactionPtr tran = db->StartTransaction(eTT_DDL);
		ISchemaPtr schema = db->GetSchema();
		ITablePtr roads = schema->CreateTable("roads", tran);

		SFieldProp id("id", dtInteger64);
		id.m_bUnique = true;
		id.m_bNotNull = true;
		roads->CreateField(id, tran);
		roads->CreateField(SFieldProp("name", dtString, 64), tran);
		roads->CreateField(SFieldProp("len", dtDouble), tran);
		SFieldProp cls("cls", dtUInteger8);
		cls.m_bNotNull = true;
		cls.m_defaultValue = CommonLib::CVariant((int32_t)1);
		roads->CreateField(cls, tran);
		roads->CreateField(SFieldProp("note", dtString), tran);
		roads->CreateField(SFieldProp("data", dtBlob), tran);
		roads->CreateField(SFieldProp("geom", dtGeometry), tran);

		roads->CreateIndex("name", SIndexProp(itMultiRegular), tran);
		roads->CreateIndex("cls", SIndexProp(itMultiRegular), tran);
		roads->CreateIndex("geom", SIndexProp(itSpatialRTree), tran);

		// wrong DDL: the transaction stays usable
		Check(Throws([&] { schema->CreateTable("roads", tran); }), "duplicate table");
		Check(Throws([&] { roads->CreateField(SFieldProp("len", dtFloat), tran); }), "duplicate field");
		Check(Throws([&] { roads->CreateIndex("data", SIndexProp(itMultiRegular), tran); }), "blob index");
		Check(Throws([&] { roads->CreateIndex("note", SIndexProp(itMultiRegular), tran); }), "unlimited string index");
		Check(Throws([&] { roads->CreateIndex("len", SIndexProp(itSpatialRTree), tran); }), "spatial index of a double");
		Check(Throws([&] { roads->CreateIndex("geom", SIndexProp(itSpatialZOrder), tran); }), "second index");

		// points: geometry with a Z-order index (the grid from the query precision: 1 unit cells)
		ITablePtr points = schema->CreateTable("points", tran);
		points->CreateField(SFieldProp("geom", dtGeometry), tran);
		points->CreateField(SFieldProp("v", dtInteger32), tran);
		SIndexProp zprop(itSpatialZOrder);
		zprop.m_extent.xMin = 0; zprop.m_extent.yMin = 0; zprop.m_extent.xMax = 1000; zprop.m_extent.yMax = 1000;
		zprop.m_dCellSize = 1.0;
		points->CreateIndex("geom", zprop, tran);
		tran->Commit();
	}

	void TestMain(size_t nRows)
	{
		std::cout << "main table, " << nRows << " rows" << std::endl;
		std::mt19937_64 rng(42);
		TModel model;
		SDatabaseParams params;
		params.m_nPageCacheSize = 200;
		params.m_nTreeCacheSize = 32;

		{
			IDatabasePtr db = CreateDatabase();
			db->Create(g_path, params);
			CreateSchema(db);

			auto start = std::chrono::steady_clock::now();
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IInsertCursorPtr ins = tran->createInsertCursor("roads", RoadFields(), true);   // ids ascending
			for (size_t i = 0; i < nRows; ++i)
			{
				SRoad r = RandomRoad(rng, (int64_t)i * 10);
				BindRoad(ins, r);
				int64_t nRowId = ins->Next();
				model[nRowId] = r;
			}

			// checks that don't change anything: the transaction stays usable
			SRoad dup = RandomRoad(rng, 0);
			BindRoad(ins, dup);
			Check(Throws([&] { ins->Next(); }), "unique id");
			SRoad r = RandomRoad(rng, -5);
			BindRoad(ins, r);
			ins->BindNull(3);
			Check(Throws([&] { ins->Next(); }), "NOT NULL cls");
			ins->BindValue(3, CommonLib::CVariant((int32_t)300));
			Check(Throws([&] { ins->Next(); }), "uint8 range");
			ins->BindText(1, std::string(65, 'x'), true);
			ins->BindUInt8(3, 2);
			Check(Throws([&] { ins->Next(); }), "string length");
			ins->BindText(1, "ok", true);
			ins->BindText(3, "two", true);
			Check(Throws([&] { ins->Next(); }), "text into uint8");

			// default of cls
			IInsertCursorPtr ins2 = tran->createInsertCursor("roads", IFieldSet::Create({ "id", "note", "geom" }));
			r = RandomRoad(rng, -7);
			r.m_bName = false; r.m_name.clear(); r.m_len = 0; r.m_cls = 1; r.m_data.clear();
			ins2->BindInt64(0, r.m_id);
			ins2->BindText(1, r.m_note, true);
			ins2->BindShape(2, MakeShape(r));
			int64_t nRowId = ins2->Next();
			model[nRowId] = r;
			tran->Commit();
			std::cout << "  insert: " << Seconds(start) << " s, file " << std::filesystem::file_size(g_path) / 1024 << " KB" << std::endl;

			// "len" has no value in that row: NULL, not 0
			ITransactionPtr rt = db->StartTransaction(eTT_SELECT);
			ISelectCursorPtr c = rt->executeSelectQuery("roads", IFieldSet::Create({ "len", "cls" }), "id", CommonLib::CVariant((int64_t)-7), OpEqual);
			Check(c->Next() && c->ColumnIsNull(0) && c->ReadInt32(1) == 1 && !c->Next(), "default / NULL");
			Check(Throws([&] { rt->createInsertCursor("roads"); }), "insert in a read transaction");
			rt->Commit();
			model[nRowId].m_len = 0;
			db->Close();
		}

		// reopen: the schema and the data
		IDatabasePtr db = CreateDatabase();
		db->Open(g_path, params);
		{
			ITablePtr roads = db->GetSchema()->GetTable("roads");
			Check(roads.get() && roads->GetFields()->GetFieldCount() == 7 && roads->GetIndexCount() == 4, "schema after reopen");
			Check(roads->GetField("id")->GetIsUnique() && roads->GetIndex("geom")->GetType() == itSpatialRTree, "field properties");
		}
		// the row without len: compare as NULL
		for (auto& kv : model)
			if (kv.second.m_id == -7)
			{
				ITransactionPtr t = db->StartTransaction(eTT_MODIFY);
				IUpdateCursorPtr u = t->createUpdateCursor("roads", IFieldSet::Create({ "len" }), "id", CommonLib::CVariant((int64_t)-7), OpEqual);
				Check(u->Next(), "update -7");
				u->UpdateValue(0, CommonLib::CVariant(0.0));
				t->Commit();
			}

		auto start = std::chrono::steady_clock::now();
		CheckTable(db, model, rng);
		std::cout << "  check: " << Seconds(start) << " s" << std::endl;

		// update: cls 3 -> len -1, name "updated N", geometry moved
		{
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IUpdateCursorPtr u = tran->createUpdateCursor("roads", IFieldSet::Create({ "len", "name", "geom", "cls" }), "cls", CommonLib::CVariant((int32_t)3), OpEqual);
			size_t n = 0;
			while (u->Next())
			{
				SRoad& r = model.at(u->GetRowID());
				r.m_len = -1;
				r.m_bName = true;
				r.m_name = "updated " + std::to_string(n++);
				r.m_bb.xMin = r.m_bb.xMax = 500; r.m_bb.yMin = r.m_bb.yMax = 500 + (double)n / 1000;
				r.m_bPoint = true;
				r.m_cls = 4;
				IRowPtr row = u->CreateRow();
				row->SetValue(0, CommonLib::CVariant(-1.0));
				row->SetValue(1, CommonLib::CVariant(r.m_name));
				row->SetValue(2, CommonLib::CVariant(MakeShape(r)));
				row->SetValue(3, CommonLib::CVariant((int32_t)4));   // the cursor still visits all selected rows
				u->UpdateRow(row);
				Check(u->ReadDouble(0) == -1 && u->ReadText(1) == r.m_name, "updated values");
			}
			Check(n > 0, "rows updated");
			// unique violation by an update
			IUpdateCursorPtr u2 = tran->createUpdateCursor("roads", IFieldSet::Create({ "id" }));
			Check(u2->Next(), "u2");
			Check(Throws([&] { u2->UpdateValue(0, CommonLib::CVariant((int64_t)10)); }), "unique by update");
			tran->Commit();
			std::cout << "  updated " << n << " rows" << std::endl;
		}

		// delete every 3rd row
		{
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IDeleteCursorPtr d = tran->createDeleteCursor("roads");
			size_t n = 0;
			for (auto it = model.begin(); it != model.end();)
			{
				if (n++ % 3 == 0)
				{
					Check(d->DeleteRow(it->first), "delete");
					it = model.erase(it);
				}
				else
					++it;
			}
			Check(!d->DeleteRow(-100), "delete of a missing row");
			tran->Commit();
		}
		CheckTable(db, model, rng);
		std::cout << "  update / delete ok, file " << std::filesystem::file_size(g_path) / 1024 << " KB" << std::endl;

		// rollback of data changes
		{
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IInsertCursorPtr ins = tran->createInsertCursor("roads", RoadFields());
			for (int i = 0; i < 3000; ++i)
			{
				BindRoad(ins, RandomRoad(rng, 100000000 + i));
				ins->Next();
			}
			IDeleteCursorPtr d = tran->createDeleteCursor("roads");
			int n = 0;
			for (const auto& kv : model)
				if (n++ % 2 == 0)
					d->DeleteRow(kv.first);
			IUpdateCursorPtr u = tran->createUpdateCursor("roads", IFieldSet::Create({ "note" }));
			while (u->Next())
				u->UpdateValue(0, CommonLib::CVariant(std::string("rolled back")));
			tran->Rollback();
			Check(Throws([&] { ins->Next(); }), "cursor after rollback");
		}
		CheckTable(db, model, rng);
		// an active transaction released without Commit is rolled back
		{
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IDeleteCursorPtr d = tran->createDeleteCursor("roads");
			d->DeleteRow(model.begin()->first);
		}
		CheckTable(db, model, rng);
		std::cout << "  rollback ok" << std::endl;

		// rollback of DDL: a new table, a new field, a dropped table come back
		{
			ITablePtr points = db->GetSchema()->GetTable("points");
			ITransactionPtr tran = db->StartTransaction(eTT_DDL);
			ITablePtr t = db->GetSchema()->CreateTable("tmp", tran);
			t->CreateField(SFieldProp("a", dtInteger32), tran);
			db->GetSchema()->GetTable("roads")->CreateField(SFieldProp("extra", dtFloat), tran);
			db->GetSchema()->DropTable("points", tran);
			Check(db->GetSchema()->GetTable("points").get() == nullptr, "dropped");
			tran->Rollback();
			Check(db->GetSchema()->GetTable("tmp").get() == nullptr, "tmp rolled back");
			Check(db->GetSchema()->GetTable("roads")->GetField("extra").get() == nullptr, "field rolled back");
			Check(db->GetSchema()->GetTable("points") == points, "dropped table back (the same object)");
			Check(Throws([&] { t->GetRowCount(); ITransactionPtr tr = db->StartTransaction(eTT_DDL); t->CreateField(SFieldProp("b", dtInteger32), tr); }), "a rolled back table can't be used");
		}
		CheckTable(db, model, rng);
		std::cout << "  DDL rollback ok" << std::endl;

		// a new field on a table with rows (default fills them), an index on it built from the rows
		{
			ITransactionPtr tran = db->StartTransaction(eTT_DDL);
			ITablePtr roads = db->GetSchema()->GetTable("roads");
			SFieldProp f("rank", dtInteger16);
			f.m_defaultValue = CommonLib::CVariant((int32_t)7);
			roads->CreateField(f, tran);
			roads->CreateIndex("rank", SIndexProp(itMultiRegular), tran);
			SFieldProp bad("must", dtInteger16);
			bad.m_bNotNull = true;
			Check(Throws([&] { roads->CreateField(bad, tran); }), "NOT NULL without default on rows");
			tran->Commit();

			ITransactionPtr rt = db->StartTransaction(eTT_SELECT);
			ISelectCursorPtr c = rt->executeSelectQuery("roads", IFieldSet::Create({ "rank" }), "rank", CommonLib::CVariant((int64_t)7), OpEqual);
			size_t n = 0;
			while (c->Next())
				++n;
			Check(n == model.size(), "rank default index: " + std::to_string(n));
			rt->Commit();
		}
		std::cout << "  field on a filled table ok" << std::endl;

		// a shorter schema after a longer one: the schema stream reuses its pages, the old tail stays after the end
		{
			ITransactionPtr tran = db->StartTransaction(eTT_DDL);
			ITablePtr big = db->GetSchema()->CreateTable("big", tran);
			for (int i = 0; i < 300; ++i)
				big->CreateField(SFieldProp("a_long_field_name_to_make_the_schema_span_pages_" + std::to_string(i), dtInteger32), tran);
			tran->Commit();
			tran = db->StartTransaction(eTT_DDL);
			db->GetSchema()->DropTable("big", tran);
			tran->Commit();
			db->Close();
			db->Open(g_path, params);
			Check(db->GetSchema()->GetTableCount() == 2 && db->GetSchema()->GetTable("big").get() == nullptr, "shorter schema after reopen");
			Check(db->GetSchema()->GetTable("roads")->GetField("rank").get() != nullptr, "roads after reopen");
		}
		CheckTable(db, model, rng);
		std::cout << "  shorter schema ok" << std::endl;
		db->Close();
	}

	//--------------------------------------------------------------------------------------------
	// indexes created on a filled table: built at once (sorted keys / STR), compared with a model

	void TestBuildIndexes(size_t nRows)
	{
		std::cout << "indexes built on a filled table, " << nRows << " rows" << std::endl;
		std::mt19937_64 rng(5);
		IDatabasePtr db = CreateDatabase();
		db->Open(g_path);

		struct SItem { int64_t id; std::string name; int32_t dup; CommonLib::bbox bb; };
		std::map<int64_t, SItem> model;    // by row id
		{
			ITransactionPtr tran = db->StartTransaction(eTT_DDL);
			ITablePtr t = db->GetSchema()->CreateTable("load", tran);
			t->CreateField(SFieldProp("id", dtInteger64), tran);
			t->CreateField(SFieldProp("name", dtString, 32), tran);
			t->CreateField(SFieldProp("dup", dtInteger32), tran);
			t->CreateField(SFieldProp("geom", dtGeometry), tran);
			t->CreateField(SFieldProp("geom2", dtGeometry), tran);

			auto start = std::chrono::steady_clock::now();
			IInsertCursorPtr ins = tran->createInsertCursor("load");
			std::vector<int64_t> ids(nRows);
			for (size_t i = 0; i < nRows; ++i)
				ids[i] = (int64_t)i * 3 - 1000;
			std::shuffle(ids.begin(), ids.end(), rng);
			for (size_t i = 0; i < nRows; ++i)
			{
				SItem item;
				item.id = ids[i];
				item.name = "street " + std::to_string(rng() % 3000);
				item.dup = (int32_t)(rng() % 100);
				double x = (double)(rng() % 100000) / 100, y = (double)(rng() % 100000) / 100;
				item.bb.xMin = x; item.bb.yMin = y; item.bb.xMax = x; item.bb.yMax = y;
				SRoad r;
				r.m_bb = item.bb;
				ins->BindInt64(0, item.id);
				ins->BindText(1, item.name, true);
				ins->BindInt32(2, item.dup);
				ins->BindShape(3, MakeShape(r));
				ins->BindShape(4, MakeShape(r));
				model[ins->Next()] = item;
			}
			std::cout << "  load: " << Seconds(start) << " s" << std::endl;

			start = std::chrono::steady_clock::now();
			SIndexProp unique(itUnique);
			t->CreateIndex("id", unique, tran);
			t->CreateIndex("name", SIndexProp(itMultiRegular), tran);
			t->CreateIndex("geom", SIndexProp(itSpatialRTree), tran);
			SIndexProp z(itSpatialZOrder);
			z.m_extent.xMin = 0; z.m_extent.yMin = 0; z.m_extent.xMax = 1000; z.m_extent.yMax = 1000;
			z.m_dCellSize = 0.5;
			t->CreateIndex("geom2", z, tran);
			std::cout << "  4 indexes built: " << Seconds(start) << " s" << std::endl;

			// failed builds don't break the transaction
			Check(Throws([&] { t->CreateIndex("dup", unique, tran); }), "unique index on duplicates");
			Check(t->GetIndex("dup").get() == nullptr, "no index after the failed build");
			t->CreateIndex("dup", SIndexProp(itMultiRegular), tran);
			tran->Commit();
		}
		{
			// a Z-order extent that doesn't cover the data
			ITransactionPtr tran = db->StartTransaction(eTT_DDL);
			ITablePtr t = db->GetSchema()->GetTable("load");
			t->DropIndex("geom2", tran);
			SIndexProp z(itSpatialZOrder);
			z.m_extent.xMin = 0; z.m_extent.yMin = 0; z.m_extent.xMax = 500; z.m_extent.yMax = 500;
			z.m_dCellSize = 1;
			Check(Throws([&] { t->CreateIndex("geom2", z, tran); }), "Z-order extent");
			tran->Rollback();      // geom2 keeps its index
		}

		ITransactionPtr tran = db->StartTransaction(eTT_SELECT);
		Check(db->GetSchema()->GetTable("load")->GetIndexCount() == 5, "indexes of load");
		auto query = [&](const std::string& sField, const CommonLib::CVariant& key, OpType op, const std::function<bool(const SItem&)>& pred)
		{
			std::set<int64_t> expected, found;
			for (const auto& kv : model)
				if (pred(kv.second))
					expected.insert(kv.first);
			ISelectCursorPtr c = tran->executeSelectQuery("load", IFieldSet::Create({ "id" }), sField, key, op);
			while (c->Next())
			{
				Check(found.insert(c->GetRowID()).second, "row twice");
				Check(c->ReadInt64(0) == model.at(c->GetRowID()).id, "id value");
			}
			Check(found == expected, "query " + sField + ": " + std::to_string(found.size()) + " rows, expected " + std::to_string(expected.size()));
		};
		for (int i = 0; i < 5; ++i)
		{
			const SItem& r = std::next(model.begin(), (long)(rng() % model.size()))->second;
			query("id", CommonLib::CVariant(r.id), OpEqual, [&](const SItem& x) { return x.id == r.id; });
			query("id", CommonLib::CVariant(r.id), OpLess, [&](const SItem& x) { return x.id < r.id; });
			query("name", CommonLib::CVariant(r.name), OpEqual, [&](const SItem& x) { return x.name == r.name; });
			query("name", CommonLib::CVariant(r.name), OpGreaterOrEqual, [&](const SItem& x) { return strcmp(x.name.c_str(), r.name.c_str()) >= 0; });
			query("dup", CommonLib::CVariant(r.dup), OpEqual, [&](const SItem& x) { return x.dup == r.dup; });
		}
		for (const char* pszField : { "geom", "geom2" })
		{
			for (int i = 0; i < 10; ++i)
			{
				CommonLib::bbox q;
				q.xMin = (double)(rng() % 1000); q.yMin = (double)(rng() % 1000);
				q.xMax = q.xMin + (i < 5 ? 50 : 3); q.yMax = q.yMin + (i < 5 ? 50 : 3);
				std::set<int64_t> expected, found;
				for (const auto& kv : model)
				{
					const CommonLib::bbox& b = kv.second.bb;
					if (!(b.xMax < q.xMin || q.xMax < b.xMin || b.yMax < q.yMin || q.yMax < b.yMin))
						expected.insert(kv.first);
				}
				ISelectCursorPtr c = tran->executeSpatialQuery(q, "load", pszField, sqmIntersect, IFieldSet::Create({ "id" }));
				while (c->Next())
					found.insert(c->GetRowID());
				Check(found == expected, std::string("spatial ") + pszField + ": " + std::to_string(found.size()) + " rows, expected " + std::to_string(expected.size()));
			}
		}
		tran->Commit();

		// the built indexes take inserts / deletes afterwards
		{
			ITransactionPtr w = db->StartTransaction(eTT_MODIFY);
			IInsertCursorPtr ins = w->createInsertCursor("load", IFieldSet::Create({ "id", "name" }));
			ins->BindInt64(0, model.begin()->second.id);
			ins->BindText(1, "dup id", true);
			Check(Throws([&] { ins->Next(); }), "unique id after the build");
			ins->BindInt64(0, -77777);
			int64_t nRowId = ins->Next();
			IDeleteCursorPtr d = w->createDeleteCursor("load");
			Check(d->DeleteRow(model.begin()->first), "delete");
			w->Commit();
			ITransactionPtr r = db->StartTransaction(eTT_SELECT);
			ISelectCursorPtr c = r->executeSelectQuery("load", IFieldSet::Create({ "id" }), "id", CommonLib::CVariant((int64_t)-77777), OpEqual);
			Check(c->Next() && c->GetRowID() == nRowId && !c->Next(), "new row by the built index");
			r->Commit();
		}
		db->Close();
		std::cout << "  ok" << std::endl;
	}

	//--------------------------------------------------------------------------------------------
	// Z-order spatial index

	void TestZOrder(size_t nRows)
	{
		std::cout << "Z-order spatial index, " << nRows << " points / rects" << std::endl;
		std::mt19937_64 rng(7);
		IDatabasePtr db = CreateDatabase();
		db->Open(g_path);
		std::map<int64_t, SRoad> model;
		{
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IInsertCursorPtr ins = tran->createInsertCursor("points");
			for (size_t i = 0; i < nRows; ++i)
			{
				SRoad r = RandomRoad(rng, (int64_t)i);
				ins->BindShape(0, MakeShape(r));
				ins->BindInt32(1, (int32_t)i);
				model[ins->Next()] = r;
			}
			// outside the extent of the grid
			SRoad out = RandomRoad(rng, 0);
			out.m_bPoint = true;
			out.m_bb.xMin = out.m_bb.xMax = 2000;
			ins->BindShape(0, MakeShape(out));
			Check(Throws([&] { ins->Next(); }), "outside the Z-order extent");
			tran->Commit();
		}
		ITransactionPtr tran = db->StartTransaction(eTT_SELECT);
		for (int i = 0; i < 30; ++i)
		{
			std::uniform_real_distribution<double> u(-50.0, 1050.0);
			CommonLib::bbox q;
			q.xMin = u(rng); q.yMin = u(rng);
			double d = i % 3 == 0 ? 100 : (i % 3 == 1 ? 10 : 0.3);
			q.xMax = q.xMin + d; q.yMax = q.yMin + d;
			for (eSpatialQueryMode mode : { sqmIntersect, sqmInside, sqmOver })
			{
				std::set<int64_t> expected, found;
				for (const auto& kv : model)
				{
					const CommonLib::bbox& b = kv.second.m_bb;
					bool bMatch = mode == sqmIntersect ? !(b.xMax < q.xMin || q.xMax < b.xMin || b.yMax < q.yMin || q.yMax < b.yMin)
						: mode == sqmInside ? (q.xMin <= b.xMin && q.yMin <= b.yMin && b.xMax <= q.xMax && b.yMax <= q.yMax)
						: (b.xMin <= q.xMin && b.yMin <= q.yMin && q.xMax <= b.xMax && q.yMax <= b.yMax);
					if (bMatch)
						expected.insert(kv.first);
				}
				ISelectCursorPtr s = tran->executeSpatialQuery(q, "points", "geom", mode);
				while (s->Next())
				{
					found.insert(s->GetRowID());
					Check(s->ReadInt32(1) == (int32_t)model.at(s->GetRowID()).m_id, "z value");
				}
				Check(found == expected, "z-order mode " + std::to_string((int)mode) + ": " + std::to_string(found.size()) + " rows, expected " + std::to_string(expected.size()));
			}
		}
		tran->Commit();
		db->Close();
		std::cout << "  ok" << std::endl;
	}

	//--------------------------------------------------------------------------------------------
	// recovery: a crash after the redo log of a commit was written, before its pages were applied
	// (the test hook of the commit copies the files at that moment). The copy with the complete log opens with
	// the commit, the copy with a cut log without it. A small memory limit spills the private pages to the file.

	std::map<int64_t, double> ReadLen(IDatabasePtr db)
	{
		std::map<int64_t, double> values;
		ITransactionPtr t = db->StartTransaction(eTT_SELECT);
		ISelectCursorPtr c = t->executeSelectQuery("roads", IFieldSet::Create({ "len" }));
		while (c->Next())
			values[c->GetRowID()] = c->ColumnIsNull(0) ? -1e300 : c->ReadDouble(0);
		t->Commit();
		return values;
	}

	void TestRecovery()
	{
		std::cout << "recovery from the redo log" << std::endl;
		std::string sComplete = g_path + ".crash1", sCut = g_path + ".crash2";
		std::mt19937_64 rng(99);

		SDatabaseParams params;
		params.m_nTranMemoryPages = 16;        // the private pages spill to "-tran"
		IDatabasePtr db = CreateDatabase();
		db->Open(g_path, params);
		std::map<int64_t, double> before = ReadLen(db);

		bool bCopied = false;
		std::dynamic_pointer_cast<bptreedb::db::CDatabase>(db)->SetAfterRedoLogHook([&]
		{
			try
			{
				namespace fs = std::filesystem;
				fs::copy_file(g_path, sComplete, fs::copy_options::overwrite_existing);
				fs::copy_file(g_path + "-redo", sComplete + "-redo", fs::copy_options::overwrite_existing);
				fs::copy_file(g_path, sCut, fs::copy_options::overwrite_existing);
				fs::copy_file(g_path + "-redo", sCut + "-redo", fs::copy_options::overwrite_existing);
				fs::resize_file(sCut + "-redo", fs::file_size(sCut + "-redo") - 10);    // the commit record is cut
				bCopied = true;
			}
			catch (std::exception& e)
			{
				std::cout << "  skipped: can't copy the open files (" << e.what() << ")" << std::endl;
			}
		});

		ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
		IUpdateCursorPtr u = tran->createUpdateCursor("roads", IFieldSet::Create({ "len", "note" }));
		while (u->Next())
		{
			u->UpdateValue(0, CommonLib::CVariant(-12345.0));
			u->UpdateValue(1, CommonLib::CVariant(std::string(200, 'z')));
		}
		IInsertCursorPtr ins = tran->createInsertCursor("roads", RoadFields());
		for (int i = 0; i < 2000; ++i)
		{
			BindRoad(ins, RandomRoad(rng, 200000000 + i));
			ins->Next();
		}
		Check(ReadLen(db) == before, "a reader doesn't see the uncommitted changes");
		tran->Commit();
		std::dynamic_pointer_cast<bptreedb::db::CDatabase>(db)->SetAfterRedoLogHook(nullptr);
		std::map<int64_t, double> after = ReadLen(db);
		Check(after.size() == before.size() + 2000, "rows after the commit");
		db->Close();
		Check(!std::filesystem::exists(g_path + "-redo") && !std::filesystem::exists(g_path + "-tran"), "no redo log / temporary file after the commit");
		if (!bCopied)
			return;

		IDatabasePtr db2 = CreateDatabase();
		db2->Open(sComplete, params);
		Check(!std::filesystem::exists(sComplete + "-redo"), "the redo log is deleted after the recovery");
		Check(ReadLen(db2) == after, "the complete redo log is replayed");
		db2->Close();

		IDatabasePtr db3 = CreateDatabase();
		db3->Open(sCut, params);
		Check(!std::filesystem::exists(sCut + "-redo"), "the cut redo log is deleted");
		Check(ReadLen(db3) == before, "the cut redo log isn't replayed");
		db3->Close();
		std::filesystem::remove(sComplete);
		std::filesystem::remove(sCut);
		std::cout << "  ok" << std::endl;
	}

	//--------------------------------------------------------------------------------------------
	// parallel read transactions; a write transaction waits for them

	void TestParallelReads()
	{
		std::cout << "parallel read transactions" << std::endl;
		IDatabasePtr db = CreateDatabase();
		db->Open(g_path);
		size_t nRows = (size_t)db->GetSchema()->GetTable("roads")->GetRowCount();

		std::atomic<int> nErrors{ 0 };
		std::vector<std::thread> threads;
		for (int t = 0; t < 4; ++t)
		{
			threads.emplace_back([&, t]
			{
				try
				{
					for (int k = 0; k < 3; ++k)
					{
						ITransactionPtr tran = db->StartTransaction(eTT_SELECT);
						ISelectCursorPtr c = (t % 2) ? tran->executeSelectQuery("roads", IFieldSet::Create({ "id", "name", "note" }))
							: tran->executeSelectQuery("roads", IFieldSet::Create({ "id", "len" }), "cls", CommonLib::CVariant((int32_t)0), OpGreaterOrEqual);
						size_t n = 0;
						int64_t nPrevId = (std::numeric_limits<int64_t>::min)();
						while (c->Next())
						{
							int64_t id = c->ReadInt64(0);
							if (t % 2 && id <= nPrevId && id >= 0)
								throw CommonLib::CExcBase("ids out of order");
							nPrevId = id;
							++n;
						}
						if (n != nRows)
							throw CommonLib::CExcBase("thread {0}: {1} rows of {2}", t, (uint64_t)n, (uint64_t)nRows);
						tran->Commit();
					}
				}
				catch (std::exception& e)
				{
					std::cout << "  " << e.what() << std::endl;
					++nErrors;
				}
			});
		}
		// a writer between the readers: they don't wait for it until its commit
		{
			ITransactionPtr tran = db->StartTransaction(eTT_MODIFY);
			IUpdateCursorPtr u = tran->createUpdateCursor("roads", IFieldSet::Create({ "len" }), "cls", CommonLib::CVariant((int32_t)0), OpEqual);
			size_t nUpdated = 0;
			while (u->Next())
			{
				u->UpdateValue(0, CommonLib::CVariant(77777.0));
				++nUpdated;
			}

			// a reader thread finishes while the writer is open, and doesn't see its values
			for (eTransactionType tranType : { eTT2PL, eTTDirectRead })
			{
				std::atomic<int> nSeen{ -1 };
				std::thread reader([&]
				{
					try
					{
						ITransactionPtr r = db->StartTransaction(eTT_SELECT, tranType);
						ISelectCursorPtr c = r->executeSelectQuery("roads", IFieldSet::Create({ "len" }), "len", CommonLib::CVariant(77777.0), OpEqual);
						int n = 0;
						while (c->Next())
							++n;
						r->Commit();
						nSeen = n;
					}
					catch (std::exception& e)
					{
						std::cout << "  " << e.what() << std::endl;
						nSeen = -2;
					}
				});
				reader.join();      // would hang if the reader waited for the writer
				Check(nSeen == 0, "a reader during the writer sees " + std::to_string(nSeen));
			}
			tran->Commit();

			ITransactionPtr r = db->StartTransaction(eTT_SELECT, eTTDirectRead);
			ISelectCursorPtr c = r->executeSelectQuery("roads", IFieldSet::Create({ "len" }), "len", CommonLib::CVariant(77777.0), OpEqual);
			size_t n = 0;
			while (c->Next())
				++n;
			r->Commit();
			Check(n == nUpdated, "a direct reader after the commit sees " + std::to_string(n) + " of " + std::to_string(nUpdated));
		}
		for (std::thread& th : threads)
			th.join();
		Check(nErrors == 0, "parallel reads");
		Check(Throws([&] { ITransactionPtr t = db->StartTransaction(eTT_SELECT); db->Close(); }), "close with an active transaction");
		db->Close();
		std::cout << "  ok" << std::endl;
	}

	template <class TFunc>
	void Run(const char* pszName, TFunc func)
	{
		try
		{
			func();
		}
		catch (std::exception& e)
		{
			++g_nFailed;
			std::cout << "FAILED " << pszName << ": " << e.what() << std::endl;
		}
	}
}

int main(int argc, char* argv[])
{
	size_t nRows = argc > 1 ? (size_t)std::strtoul(argv[1], nullptr, 10) : 20000;
#ifdef _WIN32
	g_path = "F:\\DBTest.btdb";
#else
	g_path = "/tmp/DBTest.btdb";
#endif
	if (argc > 2)
		g_path = argv[2];

	Run("main", [&] { TestMain(nRows); });
	if (g_nFailed == 0)
	{
		Run("build indexes", [&] { TestBuildIndexes(nRows); });
		Run("z-order", [&] { TestZOrder(nRows); });
		Run("recovery", [&] { TestRecovery(); });
		Run("parallel reads", [&] { TestParallelReads(); });
	}

	if (g_nFailed != 0)
	{
		std::cout << g_nFailed << " test(s) failed" << std::endl;
		return 1;
	}
	std::cout << "all tests passed" << std::endl;
	return 0;
}
