#include "TestCommon.h"
#include "../../selectors/ExpressionSymbolSelector/ExpressionSymbolSelector.h"
#include "../../../GeoDatabase/Join.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    // OID, Name (text), Type (int32), Area (double), Shape; with a join: t2.Kind (text) as a joined cursor names it
    enum { ColOID, ColName, ColType, ColArea, ColShape, ColKind };

    GeoDatabase::IFieldsPtr CreateFields(bool bJoin = false)
    {
        GeoDatabase::IFieldsPtr ptrFields = std::make_shared<GeoDatabase::CFields>();
        ptrFields->AddField(CreateField("OID", GeoDatabase::dtInteger64));
        ptrFields->AddField(CreateField("Name", GeoDatabase::dtString));
        ptrFields->AddField(CreateField("Type", GeoDatabase::dtInteger32));
        ptrFields->AddField(CreateField("Area", GeoDatabase::dtDouble));
        ptrFields->AddField(CreateField("Shape", GeoDatabase::dtGeometry));
        if(bJoin)
            ptrFields->AddField(CreateField("t2.Kind", GeoDatabase::dtString));
        return ptrFields;
    }

    GeoDatabase::IRowPtr CreateRow(const char* pszName, int32_t nType, double dArea, const char* pszKind = nullptr)
    {
        GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields(pszKind != nullptr));
        ptrRow->SetInt64(ColOID, 1);
        if(pszName)
            ptrRow->SetText(ColName, pszName);
        else
            ptrRow->SetNull(ColName);
        ptrRow->SetInt32(ColType, nType);
        ptrRow->SetDouble(ColArea, dArea);
        ptrRow->SetShape(ColShape, CreatePoint(1., 1.));
        if(pszKind)
            ptrRow->SetText(ColKind, pszKind);
        return ptrRow;
    }

    Display::ISymbolPtr Line(uint8_t r, uint8_t g, uint8_t b)
    {
        return std::make_shared<Display::CSimpleLineSymbol>(Display::Color(r, g, b), 1., Display::SimpleLineStyleSolid);
    }

    Display::Color ColorOf(Display::ISymbolPtr ptrSymbol)
    {
        Display::ILineSymbolPtr ptrLine = std::dynamic_pointer_cast<Display::ILineSymbol>(ptrSymbol);
        REQUIRE(ptrLine != nullptr);
        return ptrLine->GetColor();
    }

    // a selector with one expression per symbol
    std::shared_ptr<CExpressionSymbolSelector> CreateSelector(const std::vector<std::pair<std::string, Display::ISymbolPtr>>& vecExpr)
    {
        std::shared_ptr<CExpressionSymbolSelector> ptrSelector = std::make_shared<CExpressionSymbolSelector>();
        ptrSelector->SetExpressionCount((int)vecExpr.size());
        for(int i = 0; i < (int)vecExpr.size(); ++i)
        {
            ptrSelector->SetExpression(i, vecExpr[i].first);
            ptrSelector->SetSymbol(i, vecExpr[i].second);
        }
        return ptrSelector;
    }
}

TEST_CASE("Expression selector: the first true expression gives the symbol", "[cartography][selector][expression]")
{
    CCountingSymbolPtr ptrBigRoad = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrRoad = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrOther = std::make_shared<CCountingSymbol>();

    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({
            {"Name LIKE 'road%' AND Area >= 100", ptrBigRoad},
            {"Name LIKE 'road%'", ptrRoad}});
    ptrSelector->SetDefaultSymbol(ptrOther);
    REQUIRE(ptrSelector->GetSymbolSelectorID() == ExpressionSymbolSelectorID);
    REQUIRE(ptrSelector->GetExpressionCount() == 2);

    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("road 1", 0, 150.)) == ptrBigRoad);   // both are true: the first
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("Road 2", 0, 10.)) == ptrRoad);      // LIKE ignores case
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("river", 0, 150.)) == ptrOther);

    ptrSelector->SetUseDefaultSymbol(false);
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("river", 0, 150.)) == nullptr);       // not drawn
    REQUIRE(ptrSelector->GetSymbolByFeature(GeoDatabase::IRowPtr()) == nullptr);
}

TEST_CASE("Expression selector: SQL conditions on the row", "[cartography][selector][expression]")
{
    CCountingSymbolPtr ptrYes = std::make_shared<CCountingSymbol>();
    auto matches = [&](const std::string& sExpr, GeoDatabase::IRowPtr ptrRow)
    {
        std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{sExpr, ptrYes}});
        ptrSelector->SetUseDefaultSymbol(false);
        return ptrSelector->GetSymbolByFeature(ptrRow) == ptrYes;
    };

    GeoDatabase::IRowPtr ptrRow = CreateRow("Main Street", 3, 12.5);
    REQUIRE(matches("Type IN (1, 3, 5)", ptrRow));
    REQUIRE(matches("Area BETWEEN 10 AND 20", ptrRow));
    REQUIRE(matches("type * 2 + 1 = 7", ptrRow));                // field names ignore case
    REQUIRE(matches("UPPER(SUBSTR(Name, 1, 4)) = 'MAIN'", ptrRow));
    REQUIRE(matches("CASE WHEN Area > 10 THEN 'big' END = 'big'", ptrRow));
    REQUIRE_FALSE(matches("Type NOT IN (1, 3, 5)", ptrRow));

    // NULL: a comparison is unknown, it doesn't match (as in WHERE)
    GeoDatabase::IRowPtr ptrNullName = CreateRow(nullptr, 3, 12.5);
    REQUIRE(matches("Name IS NULL", ptrNullName));
    REQUIRE_FALSE(matches("Name = 'x'", ptrNullName));
    REQUIRE_FALSE(matches("NOT (Name = 'x')", ptrNullName));
    REQUIRE(matches("COALESCE(Name, '') = ''", ptrNullName));
}

TEST_CASE("Expression selector: an empty expression matches every row", "[cartography][selector][expression]")
{
    CCountingSymbolPtr ptrFirst = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrAll = std::make_shared<CCountingSymbol>();

    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"Type = 1", ptrFirst}, {"  ", ptrAll}});
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 1, 0.)) == ptrFirst);
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 2, 0.)) == ptrAll);

    // a new entry has an empty expression too
    std::shared_ptr<CExpressionSymbolSelector> ptrNew = std::make_shared<CExpressionSymbolSelector>();
    ptrNew->SetExpressionCount(1);
    ptrNew->SetSymbol(0, ptrAll);
    REQUIRE(ptrNew->GetExpression(0).empty());
    REQUIRE(ptrNew->GetSymbolByFeature(CreateRow("a", 2, 0.)) == ptrAll);
}

TEST_CASE("Expression selector: a wrong expression is refused, the old one stays", "[cartography][selector][expression]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"Type = 1", ptrSymbol}});
    ptrSelector->SetUseDefaultSymbol(false);

    REQUIRE_THROWS(ptrSelector->SetExpression(0, "Type ="));
    REQUIRE_THROWS(ptrSelector->SetExpression(0, "Name = 'abc"));
    REQUIRE_THROWS(ptrSelector->SetExpression(0, "NoSuchFunc(Type) = 1"));
    REQUIRE(ptrSelector->GetExpression(0) == "Type = 1");
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 1, 0.)) == ptrSymbol);

    REQUIRE_THROWS(ptrSelector->SetExpression(1, "Type = 1"));   // index out of range
    REQUIRE_THROWS(ptrSelector->GetLabel(-1));
    REQUIRE_THROWS(ptrSelector->SetExpressionCount(-1));
}

TEST_CASE("Expression selector: a field missing in the rows turns the expression off until PrepareFilter",
          "[cartography][selector][expression]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingSymbolPtr ptrMissing = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrType = std::make_shared<CCountingSymbol>();

    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"t2.Kind = 'road'", ptrMissing}, {"Type = 1", ptrType}});
    ptrSelector->SetUseDefaultSymbol(false);

    // no t2.Kind in this row: the first expression is skipped, not an error of the draw
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 1, 0.)) == ptrType);
    // and stays off, even for a row that has the column
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 1, 0., "road")) == ptrType);

    ptrSelector->PrepareFilter(ptrTable, std::make_shared<GeoDatabase::CQueryFilter>());
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 1, 0., "road")) == ptrMissing);
}

TEST_CASE("Expression selector: can assign by the table fields", "[cartography][selector][expression]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();

    REQUIRE(CreateSelector({{"name = 'a' AND AREA > 1", ptrSymbol}})->CanAssign(ptrTable));   // names ignore case
    REQUIRE(CreateSelector({{"t2.Kind = 'road'", ptrSymbol}})->CanAssign(ptrTable));        // a join field: checked on the rows
    REQUIRE(CreateSelector({{"", ptrSymbol}})->CanAssign(ptrTable));
    REQUIRE_FALSE(CreateSelector({{"Type = 1", ptrSymbol}, {"NoSuchField = 1", ptrSymbol}})->CanAssign(ptrTable));
    REQUIRE_FALSE(CreateSelector({{"Type = 1", ptrSymbol}})->CanAssign(GeoDatabase::ITablePtr()));
}

TEST_CASE("Expression selector: prepare filter adds the fields of the table and of the joins",
          "[cartography][selector][expression]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();

    SECTION("the table fields, with the case of the table, once")
    {
        std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"name = 'a' AND type > 1", ptrSymbol}, {"Type < 0", ptrSymbol}});

        // the layer prepares the renderers on an empty field set and adds the OID after them
        GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
        ptrSelector->PrepareFilter(ptrTable, ptrFilter);
        ptrSelector->PrepareFilter(ptrTable, ptrFilter);

        GeoDatabase::IFieldSetPtr ptrFields = ptrFilter->GetFieldSet();
        REQUIRE(ptrFields->GetCount() == 2);
        REQUIRE(ptrFields->Find("Name") >= 0);
        REQUIRE(ptrFields->Find("Type") >= 0);
    }

    SECTION("the fields of a join go to the join with that prefix, the main prefix to the table")
    {
        GeoDatabase::IJoinPtr ptrJoin = std::make_shared<GeoDatabase::CJoin>();
        ptrJoin->SetTableName("kinds");
        ptrJoin->SetTablePrefix("t2");
        GeoDatabase::IJoinPtr ptrOtherJoin = std::make_shared<GeoDatabase::CJoin>();
        ptrOtherJoin->SetTableName("others");
        ptrOtherJoin->SetTablePrefix("t3");

        GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
        ptrFilter->SetTablePrefix("t1");
        ptrFilter->SetJoins({ptrJoin, ptrOtherJoin});
        ptrFilter->GetFieldSet()->Add("OID");

        std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"T2.Kind = 'road' AND t1.Area > 10", ptrSymbol}});
        ptrSelector->PrepareFilter(ptrTable, ptrFilter);

        REQUIRE(ptrFilter->GetFieldSet()->Find("Area") >= 0);
        REQUIRE(ptrFilter->GetFieldSet()->Find("Kind") < 0);
        REQUIRE(ptrJoin->GetFieldSet()->GetCount() == 1);
        REQUIRE(ptrJoin->GetFieldSet()->Find("Kind") >= 0);
        REQUIRE(ptrOtherJoin->GetFieldSet()->GetCount() == 0);
    }

    SECTION("a prefix of no join adds nothing")
    {
        GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
        CreateSelector({{"t9.Kind = 'road'", ptrSymbol}})->PrepareFilter(ptrTable, ptrFilter);
        REQUIRE(ptrFilter->GetFieldSet()->GetCount() == 0);
    }
}

TEST_CASE("Expression selector: a field of a join in the row", "[cartography][selector][expression]")
{
    CCountingSymbolPtr ptrRoad = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrOther = std::make_shared<CCountingSymbol>();

    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"t2.Kind IN ('road', 'street') AND t1.Type = 2", ptrRoad}});
    ptrSelector->SetDefaultSymbol(ptrOther);

    // t2.Kind - the column "t2.Kind"; t1.Type - no column "t1.Type", the main column "Type"
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 2, 0., "street")) == ptrRoad);
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 2, 0., "river")) == ptrOther);
    REQUIRE(ptrSelector->GetSymbolByFeature(CreateRow("a", 3, 0., "road")) == ptrOther);
}

TEST_CASE("Expression selector: symbols setup, reset and flush", "[cartography][selector][expression]")
{
    CCountingSymbolPtr ptrA = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrDefault = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"Type = 1", ptrA}, {"Type = 2", Display::ISymbolPtr()}});
    ptrSelector->SetDefaultSymbol(ptrDefault);

    ptrSelector->SetupSymbols(Display::IDisplayPtr());   // no display - nothing
    REQUIRE(ptrA->nPrepare == 0);
    ptrSelector->ResetSymbols();
    ptrSelector->FlushBuffers(Display::IDisplayPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrA->nReset == 1);
    REQUIRE(ptrDefault->nReset == 1);
    REQUIRE(ptrA->nFlush == 1);
    REQUIRE(ptrDefault->nFlush == 1);
}

TEST_CASE("Expression selector: legend", "[cartography][selector][expression][legend]")
{
    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({
            {"Type = 1", Line(255, 0, 0)},
            {"Type = 2", Line(0, 255, 0)},
            {"", Line(0, 0, 255)}});
    ptrSelector->SetLabel(0, "Highways");
    ptrSelector->SetDescription(0, "type 1");
    ptrSelector->SetDefaultSymbol(Line(1, 1, 1));
    ptrSelector->SetDefaultLabel("Other");

    REQUIRE(ptrSelector->GetLegendGroupCount() == 1);
    ILegendGroupPtr ptrGroup = ptrSelector->GetLegendGroup(0);
    REQUIRE(ptrGroup->GetClassCount() == 4);   // 3 expressions and the default
    REQUIRE(ptrGroup->GetClass(0)->GetLabel() == "Highways");
    REQUIRE(ptrGroup->GetClass(0)->GetDescription() == "type 1");
    REQUIRE(ptrGroup->GetClass(1)->GetLabel() == "Type = 2");         // no label: the expression
    REQUIRE(ptrGroup->GetClass(2)->GetLabel() == "<all features>");   // no label, no expression
    REQUIRE(ptrGroup->GetClass(3)->GetLabel() == "Other");
    REQUIRE(ColorOf(ptrGroup->GetClass(3)->GetSymbol()) == Display::Color(1, 1, 1));

    // the class symbol is written back to the selector
    Display::ISymbolPtr ptrNew = Line(9, 9, 9);
    ptrGroup->GetClass(1)->SetSymbol(ptrNew);
    REQUIRE(ptrSelector->GetSymbol(1) == ptrNew);

    ptrSelector->SetUseDefaultSymbol(false);
    REQUIRE(ptrSelector->GetLegendGroup(0)->GetClassCount() == 3);
    REQUIRE_THROWS(ptrSelector->GetLegendGroup(1));
}

TEST_CASE("Expression selector: save / load", "[cartography][selector][expression][serialize]")
{
    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({
            {"Name LIKE 'road%' AND t2.Kind = 'main'", Line(255, 0, 0)},
            {"Area > 0.125", Line(0, 255, 0)},
            {"", Display::ISymbolPtr()}});
    ptrSelector->SetLabel(0, "Main roads");
    ptrSelector->SetDescription(1, "positive area");
    ptrSelector->SetDefaultSymbol(Line(10, 10, 10));
    ptrSelector->SetDefaultLabel("Other");
    ptrSelector->SetUseDefaultSymbol(false);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrSelector->Save(ptrRoot);
    IExpressionSymbolSelectorPtr ptrLoaded = std::dynamic_pointer_cast<IExpressionSymbolSelector>(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot));
    REQUIRE(ptrLoaded != nullptr);

    REQUIRE(ptrLoaded->GetExpressionCount() == 3);
    REQUIRE(ptrLoaded->GetExpression(0) == "Name LIKE 'road%' AND t2.Kind = 'main'");
    REQUIRE(ptrLoaded->GetExpression(1) == "Area > 0.125");
    REQUIRE(ptrLoaded->GetExpression(2).empty());
    REQUIRE(ptrLoaded->GetLabel(0) == "Main roads");
    REQUIRE(ptrLoaded->GetDescription(1) == "positive area");
    REQUIRE(ColorOf(ptrLoaded->GetSymbol(0)) == Display::Color(255, 0, 0));
    REQUIRE(ptrLoaded->GetSymbol(2) == nullptr);
    REQUIRE(ColorOf(ptrLoaded->GetDefaultSymbol()) == Display::Color(10, 10, 10));
    REQUIRE(ptrLoaded->GetDefaultLabel() == "Other");
    REQUIRE_FALSE(ptrLoaded->GetUseDefaultSymbol());

    // the loaded expressions work
    REQUIRE(ColorOf(ptrLoaded->GetSymbolByFeature(CreateRow("road 7", 0, 0., "main"))) == Display::Color(255, 0, 0));
    REQUIRE(ColorOf(ptrLoaded->GetSymbolByFeature(CreateRow("river", 0, 1.))) == Display::Color(0, 255, 0));
}

TEST_CASE("Expression selector: a wrong expression in a document is kept, never matches", "[cartography][selector][expression][serialize]")
{
    // written by hand: SetExpression doesn't take a wrong expression
    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrRoot->AddPropertyInt32U("SymbolSelectorID", ExpressionSymbolSelectorID);
    ptrRoot->AddPropertyBool("UseDefaultSymbol", false);
    CommonLib::ISerializeObjPtr ptrEntries = ptrRoot->CreateChildNode("Expressions");
    ptrEntries->CreateChildNode("Expression")->AddPropertyString("Expression", "Type = = 1");
    ptrEntries->CreateChildNode("Expression")->AddPropertyString("Expression", "Type = 1");

    IExpressionSymbolSelectorPtr ptrLoaded;
    REQUIRE_NOTHROW(ptrLoaded = std::dynamic_pointer_cast<IExpressionSymbolSelector>(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot)));
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetExpressionCount() == 2);
    REQUIRE(ptrLoaded->GetExpression(0) == "Type = = 1");   // kept to be fixed

    CCountingSymbolPtr ptrWrong = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrRight = std::make_shared<CCountingSymbol>();
    ptrLoaded->SetSymbol(0, ptrWrong);
    ptrLoaded->SetSymbol(1, ptrRight);
    REQUIRE(ptrLoaded->GetSymbolByFeature(CreateRow("a", 1, 0.)) == ptrRight);

    // fixed: it matches
    ptrLoaded->SetExpression(0, "Type = 1");
    REQUIRE(ptrLoaded->GetSymbolByFeature(CreateRow("a", 1, 0.)) == ptrWrong);
}

TEST_CASE("Feature renderer draws with the expression selector symbol", "[cartography][selector][expression][renderer]")
{
    CCountingSymbolPtr ptrBig = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrSmall = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CExpressionSymbolSelector> ptrSelector = CreateSelector({{"Area >= 100", ptrBig}, {"Area > 0", ptrSmall}});
    ptrSelector->SetUseDefaultSymbol(false);

    CFeatureRenderer renderer;
    renderer.SetSymbolSelector(ptrSelector);
    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
    renderer.PrepareFilter(std::make_shared<CTestTable>(CreateFields()), ptrFilter);
    REQUIRE(ptrFilter->GetFieldSet()->Find("Area") >= 0);   // the draw query gets the field of the expressions

    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("a", 0, 500.));
    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("b", 0, 5.));
    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("c", 0, 50.));
    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("d", 0, -1.));   // no symbol - skipped

    REQUIRE(ptrBig->nDraw == 1);
    REQUIRE(ptrSmall->nDraw == 2);
}
