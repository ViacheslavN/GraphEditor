#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::GeoDatabase;
using namespace geodatabase_test;

TEST_CASE("Fields: add, find, remove, clone", "[geodatabase][fields]")
{
    IFieldsPtr ptrFields = std::make_shared<CFields>();
    ptrFields->AddField(CreateField("OID", dtInteger64, true));
    ptrFields->AddField(CreateField("Name", dtString));
    ptrFields->AddField(CreateField("Shape", dtGeometry));

    REQUIRE(ptrFields->GetFieldCount() == 3);
    REQUIRE(ptrFields->FindField("Name") == 1);
    REQUIRE(ptrFields->FindField("Missing") == -1);
    REQUIRE(ptrFields->FieldExists("Shape"));
    REQUIRE(ptrFields->GetField("Shape")->GetType() == dtGeometry);
    REQUIRE(ptrFields->GetField("Missing") == nullptr);

    IFieldsPtr ptrClone = ptrFields->Clone();
    ptrFields->RemoveField(1);
    REQUIRE(ptrFields->GetFieldCount() == 2);
    REQUIRE(ptrFields->FindField("Shape") == 1); // index map is rebuilt
    REQUIRE_FALSE(ptrFields->FieldExists("Name"));

    // the clone is independent
    REQUIRE(ptrClone->GetFieldCount() == 3);
    ptrClone->GetField(0)->SetName("ID");
    REQUIRE(ptrFields->GetField(0)->GetName() == "OID");
}

TEST_CASE("Field properties", "[geodatabase][fields]")
{
    CField field;
    field.SetName("Value");
    field.SetAliasName("The value");
    field.SetType(dtDouble);
    field.SetLength(8);
    field.SetPrecision(10);
    field.SetScale(3);
    field.SetIsNullable(false);
    field.SetIsPrimaryKey(true);

    IFieldPtr ptrClone = field.Clone();
    REQUIRE(ptrClone->GetName() == "Value");
    REQUIRE(ptrClone->GetAliasName() == "The value");
    REQUIRE(ptrClone->GetType() == dtDouble);
    REQUIRE(ptrClone->GetLength() == 8);
    REQUIRE(ptrClone->GetPrecision() == 10);
    REQUIRE(ptrClone->GetScale() == 3);
    REQUIRE_FALSE(ptrClone->GetIsNullable());
    REQUIRE(ptrClone->GetIsPrimaryKey());
}

TEST_CASE("Field set", "[geodatabase][fields]")
{
    CFieldSet fieldSet;
    REQUIRE(fieldSet.GetCount() == 0);

    fieldSet.Add("A");
    fieldSet.Add("B");
    fieldSet.Add("A"); // duplicates are ignored
    REQUIRE(fieldSet.GetCount() == 2);
    REQUIRE(fieldSet.Find("B") >= 0);
    REQUIRE(fieldSet.Find("C") == -1);

    std::vector<std::string> names;
    std::string name;
    fieldSet.Reset();
    while(fieldSet.Next(name))
        names.push_back(name);
    REQUIRE(names == std::vector<std::string>{"A", "B"});

    fieldSet.Remove("A");
    REQUIRE(fieldSet.GetCount() == 1);
    REQUIRE(fieldSet.Get(0) == "B");
    fieldSet.Clear();
    REQUIRE(fieldSet.GetCount() == 0);
}

TEST_CASE("Query filter", "[geodatabase][filter]")
{
    CQueryFilter filter("CODE > 1");
    REQUIRE(filter.GetWhereClause() == "CODE > 1");
    REQUIRE(filter.GetFieldSet() != nullptr);       // created on demand
    REQUIRE(filter.GetFieldSet()->GetCount() == 0);

    CommonLib::bbox bb;
    bb.type = CommonLib::bbox_type_normal;
    bb.xMin = 1; bb.yMin = 2; bb.xMax = 3; bb.yMax = 4;
    filter.SetBB(bb);
    filter.SetSpatialRel(srlIntersects);
    filter.SetPrecision(0.5);
    REQUIRE(filter.GetBB().xMax == 3);
    REQUIRE(filter.GetSpatialRel() == srlIntersects);
    REQUIRE(filter.GetPrecision() == 0.5);
}

TEST_CASE("Row values", "[geodatabase][row]")
{
    IFieldsPtr ptrFields = std::make_shared<CFields>();
    ptrFields->AddField(CreateField("I", dtInteger64));
    ptrFields->AddField(CreateField("D", dtDouble));
    ptrFields->AddField(CreateField("S", dtString));
    ptrFields->AddField(CreateField("G", dtGeometry));

    CRow row(ptrFields);
    REQUIRE(row.ColumnCount() == 4);
    REQUIRE(row.ColumnName(2) == "S");
    REQUIRE(row.GetColumnType(3) == dtGeometry);
    REQUIRE(row.ColumnIsNull(0));

    row.SetInt64(0, 42);
    row.SetDouble(1, 2.5);
    row.SetText(2, "text");
    row.SetShape(3, CreatePoint(1., 2.));

    REQUIRE(row.GetValue(0)->Get<int64_t>() == 42);
    REQUIRE(row.GetValue(1)->Get<double>() == 2.5);
    REQUIRE(row.GetValue(2)->Get<std::string>() == "text");

    // the row keeps its own copy of the shape, the next SetShape reuses it
    CommonLib::IGeoShapePtr ptrShape = row.GetValue(3)->Get<CommonLib::IGeoShapePtr>();
    REQUIRE(ptrShape->GetPoints()[0].y == 2.);
    row.SetShape(3, CreatePoint(5., 6.));
    REQUIRE(row.GetValue(3)->Get<CommonLib::IGeoShapePtr>().get() == ptrShape.get());
    REQUIRE(ptrShape->GetPoints()[0].x == 5.);

    row.SetNull(2);
    REQUIRE(row.ColumnIsNull(2));
    REQUIRE_THROWS(row.GetValue(10));
}

TEST_CASE("SQLite type names", "[geodatabase][sqlite]")
{
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("INTEGER") == dtInteger64);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("integer") == dtInteger64);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("BIGINT") == dtInteger64);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("VARCHAR(10)") == dtString);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("TEXT") == dtString);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("REAL") == dtDouble);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("DOUBLE PRECISION") == dtDouble);
    REQUIRE(CSQLiteUtils::SQLiteType2FieldType("BLOB") == dtBlob);

    REQUIRE(CSQLiteUtils::FieldType2SQLiteType(dtGeometry) == "BLOB");
    REQUIRE(CSQLiteUtils::FieldType2SQLiteType(dtInteger32) == "INTEGER");
    REQUIRE_THROWS(CSQLiteUtils::FieldType2SQLiteType(dtUnknown));
}
