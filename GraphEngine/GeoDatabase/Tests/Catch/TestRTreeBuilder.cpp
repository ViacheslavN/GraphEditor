// the bulk load of SQLite R-trees (CSQLiteRTreeBuilder): the packed tree passes rtreecheck() and answers
// the window queries as a brute force search, it can be changed by ordinary inserts / deletes afterwards
#include "TestCommon.h"
#include "../../GeoDatabaseSQlite/SQLiteRTreeBuilder.h"
#include "../../../CommonLib/sqlitelib/Database.h"
#include <filesystem>
#include <random>

using namespace GraphEngine::GeoDatabase;

namespace
{
    struct SBox
    {
        int64_t id;
        double  xMin, xMax, yMin, yMax;
    };

    CommonLib::database::IDatabasePtr CreateRTreeDatabase(const std::string& sName)
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineRTreeTests";
        std::filesystem::create_directories(dir);
        std::string sPath = (dir / (sName + ".sqlite")).string();
        std::filesystem::remove(sPath);
        CommonLib::database::IDatabasePtr ptrDb = CommonLib::database::IDatabaseSQLiteCreator::Create(sPath.c_str(), CommonLib::database::CreateDatabase);
        ptrDb->Execute("CREATE VIRTUAL TABLE idx USING rtree(feature_id, minX, maxX, minY, maxY)");
        return ptrDb;
    }

    std::string Check(CommonLib::database::IDatabasePtr ptrDb)
    {
        CommonLib::database::IStatmentPtr ptrStmt = ptrDb->PrepareQuery("SELECT rtreecheck('idx')");
        ptrStmt->Next();
        return ptrStmt->ReadText(0);
    }

    std::vector<int64_t> Query(CommonLib::database::IDatabasePtr ptrDb, double xMin, double xMax, double yMin, double yMax)
    {
        CommonLib::database::IStatmentPtr ptrStmt = ptrDb->PrepareQuery("SELECT feature_id FROM idx WHERE maxX >= ? AND minX <= ? AND maxY >= ? AND minY <= ? ORDER BY feature_id");
        ptrStmt->BindDouble(1, xMin);
        ptrStmt->BindDouble(2, xMax);
        ptrStmt->BindDouble(3, yMin);
        ptrStmt->BindDouble(4, yMax);
        std::vector<int64_t> ids;
        while(ptrStmt->Next())
            ids.push_back(ptrStmt->ReadInt64(0));
        return ids;
    }

    std::vector<int64_t> BruteForce(const std::vector<SBox>& boxes, double xMin, double xMax, double yMin, double yMax)
    {
        std::vector<int64_t> ids;
        for(const SBox& b : boxes)
        {
            // the index keeps float boxes rounded outwards: the same test with a tiny tolerance is done by the caller
            if(b.xMax >= xMin && b.xMin <= xMax && b.yMax >= yMin && b.yMin <= yMax)
                ids.push_back(b.id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    CommonLib::bbox ToBBox(const SBox& b)
    {
        CommonLib::bbox bb;
        bb.type = CommonLib::bbox_type_normal;
        bb.xMin = b.xMin; bb.xMax = b.xMax; bb.yMin = b.yMin; bb.yMax = b.yMax;
        return bb;
    }
}

TEST_CASE("SQLite R-tree bulk load", "[geodatabase][sqlite][rtree]")
{
    std::mt19937_64 rnd(11);
    std::uniform_real_distribution<double> coord(-2.0e7, 2.0e7), size(0., 500.);

    for(size_t nCount : {(size_t)0, (size_t)1, (size_t)51, (size_t)52, (size_t)20000})
    {
        INFO("boxes " << nCount);
        CommonLib::database::IDatabasePtr ptrDb = CreateRTreeDatabase("bulk_" + std::to_string(nCount));
        std::vector<SBox> boxes;
        // a small memory limit: the sorted runs go to a temporary file and are merged
        CSQLiteRTreeBuilder builder(std::string(), 64 * 1024);
        for(size_t i = 0; i < nCount; ++i)
        {
            double x = coord(rnd), y = coord(rnd);
            SBox b = {(int64_t)(i * 3 + 1), x, x + size(rnd), y, y + size(rnd)};
            boxes.push_back(b);
            builder.Add(b.id, ToBBox(b));
        }
        ptrDb->Execute("BEGIN");   // as the insert cursor does it: in the transaction
        builder.Build(ptrDb, "idx");
        ptrDb->Execute("COMMIT");
        REQUIRE(Check(ptrDb) == "ok");

        CommonLib::database::IStatmentPtr ptrCount = ptrDb->PrepareQuery("SELECT count(*) FROM idx");
        ptrCount->Next();
        REQUIRE(ptrCount->ReadInt64(0) == (int64_t)nCount);

        // window queries: the margin hides the float rounding of the borders
        for(int q = 0; q < 50; ++q)
        {
            double x = coord(rnd), y = coord(rnd), w = q % 2 ? 2.0e6 : 2.0e4;
            std::vector<int64_t> found = Query(ptrDb, x, x + w, y, y + w);
            std::vector<int64_t> inner = BruteForce(boxes, x + 4., x + w - 4., y + 4., y + w - 4.);
            std::vector<int64_t> outer = BruteForce(boxes, x - 4., x + w + 4., y - 4., y + w + 4.);
            REQUIRE(std::includes(found.begin(), found.end(), inner.begin(), inner.end()));
            REQUIRE(std::includes(outer.begin(), outer.end(), found.begin(), found.end()));
        }

        // the packed tree is an ordinary R-tree: inserts and deletes keep it valid
        ptrDb->Execute("INSERT INTO idx VALUES(1000000000, 1, 2, 3, 4)");
        if(nCount > 0)
            ptrDb->Execute(("DELETE FROM idx WHERE feature_id = " + std::to_string(boxes[nCount / 2].id)).c_str());
        REQUIRE(Check(ptrDb) == "ok");
        REQUIRE(Query(ptrDb, 1.5, 1.6, 3.5, 3.6) == std::vector<int64_t>{1000000000});

        // a second bulk load keeps the entries of the index
        CSQLiteRTreeBuilder more;
        SBox extra = {2000000000, 10., 11., 10., 11.};
        more.Add(extra.id, ToBBox(extra));
        ptrDb->Execute("BEGIN");
        more.Build(ptrDb, "idx");
        ptrDb->Execute("COMMIT");
        REQUIRE(Check(ptrDb) == "ok");
        ptrCount->Reset();
        ptrCount->Next();
        REQUIRE(ptrCount->ReadInt64(0) == (int64_t)(nCount + 2 - (nCount > 0 ? 1 : 0)));
        REQUIRE(Query(ptrDb, 10.5, 10.6, 10.5, 10.6) == std::vector<int64_t>{2000000000});
        REQUIRE(Query(ptrDb, 1.5, 1.6, 3.5, 3.6) == std::vector<int64_t>{1000000000});
    }
}
