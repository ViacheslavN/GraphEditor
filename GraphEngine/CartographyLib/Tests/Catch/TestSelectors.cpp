#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    // OID, Name (text), Type (int32), Area (double), Shape
    enum { ColOID, ColName, ColType, ColArea, ColShape };

    GeoDatabase::IFieldsPtr CreateFields()
    {
        GeoDatabase::IFieldsPtr ptrFields = std::make_shared<GeoDatabase::CFields>();
        ptrFields->AddField(CreateField("OID", GeoDatabase::dtInteger64));
        ptrFields->AddField(CreateField("Name", GeoDatabase::dtString));
        ptrFields->AddField(CreateField("Type", GeoDatabase::dtInteger32));
        ptrFields->AddField(CreateField("Area", GeoDatabase::dtDouble));
        ptrFields->AddField(CreateField("Shape", GeoDatabase::dtGeometry));
        return ptrFields;
    }

    GeoDatabase::IRowPtr CreateRow(const char* pszName, int32_t nType, double dArea)
    {
        GeoDatabase::IRowPtr ptrRow = std::make_shared<GeoDatabase::CRow>(CreateFields());
        ptrRow->SetInt64(ColOID, 1);
        if(pszName)
            ptrRow->SetText(ColName, pszName);
        ptrRow->SetInt32(ColType, nType);
        ptrRow->SetDouble(ColArea, dArea);
        ptrRow->SetShape(ColShape, CreatePoint(1., 1.));
        return ptrRow;
    }

    CommonLib::CVariant Text(const char* psz)
    {
        return CommonLib::CVariant(CommonLib::astr_t(psz));
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
}

// ---------------- unique value

TEST_CASE("Unique value selector: symbol by a text field", "[cartography][selector][uniquevalue]")
{
    CCountingSymbolPtr ptrRiver = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrRoad = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrOther = std::make_shared<CCountingSymbol>();

    CUniqueValueSymbolSelector selector("Name");
    REQUIRE(selector.GetSymbolSelectorID() == UniqueValueSymbolSelectorID);
    REQUIRE(selector.AddValue({Text("river")}, ptrRiver, "River") == 0);
    REQUIRE(selector.AddValue({Text("road")}, ptrRoad, "Road") == 1);
    selector.SetDefaultSymbol(ptrOther);

    REQUIRE(selector.GetSymbolByFeature(CreateRow("road", 0, 0.)) == ptrRoad);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("river", 0, 0.)) == ptrRiver);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("lake", 0, 0.)) == ptrOther);

    selector.SetUseDefaultSymbol(false);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("lake", 0, 0.)) == nullptr);   // not drawn

    REQUIRE(selector.GetLabel(1) == "Road");
    REQUIRE_THROWS(selector.GetLabel(2));
    REQUIRE_THROWS(selector.AddValue({Text("a"), Text("b")}, ptrRoad));   // 2 values for 1 field
}

TEST_CASE("Unique value selector: numbers match whatever their type", "[cartography][selector][uniquevalue]")
{
    CCountingSymbolPtr ptrFive = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrHalf = std::make_shared<CCountingSymbol>();

    CUniqueValueSymbolSelector selector("Type");
    selector.AddValue({CommonLib::CVariant((int64_t)5)}, ptrFive);   // int64 value, int32 field
    selector.AddValue({CommonLib::CVariant(7.5)}, ptrHalf);
    selector.SetUseDefaultSymbol(false);

    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 5, 0.)) == ptrFive);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 6, 0.)) == nullptr);

    CUniqueValueSymbolSelector byArea("Area");
    byArea.AddValue({CommonLib::CVariant((int32_t)3)}, ptrFive);
    byArea.AddValue({CommonLib::CVariant(7.5)}, ptrHalf);
    REQUIRE(byArea.GetSymbolByFeature(CreateRow("x", 0, 3.)) == ptrFive);   // double 3.0 == int 3
    REQUIRE(byArea.GetSymbolByFeature(CreateRow("x", 0, 7.5)) == ptrHalf);
}

TEST_CASE("Unique value selector: null values and the first duplicate wins", "[cartography][selector][uniquevalue]")
{
    CCountingSymbolPtr ptrNull = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrFirst = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrSecond = std::make_shared<CCountingSymbol>();

    CUniqueValueSymbolSelector selector("Name");
    selector.AddValue({CommonLib::CVariant()}, ptrNull);
    selector.AddValue({Text("a")}, ptrFirst);
    selector.AddValue({Text("a")}, ptrSecond);

    REQUIRE(selector.GetSymbolByFeature(CreateRow(nullptr, 0, 0.)) == ptrNull);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("a", 0, 0.)) == ptrFirst);

    // changing a value rebuilds the lookup
    selector.SetValue(1, 0, Text("b"));
    REQUIRE(selector.GetSymbolByFeature(CreateRow("a", 0, 0.)) == ptrSecond);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("b", 0, 0.)) == ptrFirst);

    selector.RemoveValue(0);
    selector.SetUseDefaultSymbol(false);
    REQUIRE(selector.GetSymbolByFeature(CreateRow(nullptr, 0, 0.)) == nullptr);
}

TEST_CASE("Unique value selector: several fields", "[cartography][selector][uniquevalue]")
{
    CCountingSymbolPtr ptrRoad1 = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrRoad2 = std::make_shared<CCountingSymbol>();

    CUniqueValueSymbolSelector selector;
    selector.SetFieldCount(2);
    selector.SetField(0, "Name");
    selector.SetField(1, "Type");
    selector.AddValue({Text("road"), CommonLib::CVariant((int32_t)1)}, ptrRoad1);
    selector.AddValue({Text("road"), CommonLib::CVariant((int32_t)2)}, ptrRoad2);
    selector.SetUseDefaultSymbol(false);

    REQUIRE(selector.GetSymbolByFeature(CreateRow("road", 2, 0.)) == ptrRoad2);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("road", 1, 0.)) == ptrRoad1);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("road", 3, 0.)) == nullptr);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("river", 1, 0.)) == nullptr);
}

TEST_CASE("Unique value selector: filter, can assign, symbols setup", "[cartography][selector][uniquevalue]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    CCountingSymbolPtr ptrA = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrDefault = std::make_shared<CCountingSymbol>();

    CUniqueValueSymbolSelector selector;
    selector.SetFieldCount(2);
    selector.SetField(0, "Name");
    selector.SetField(1, "Type");
    selector.AddValue({Text("a"), CommonLib::CVariant((int32_t)1)}, ptrA);
    selector.SetDefaultSymbol(ptrDefault);
    REQUIRE(selector.CanAssign(ptrTable));

    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
    selector.PrepareFilter(ptrTable, ptrFilter);
    selector.PrepareFilter(ptrTable, ptrFilter);
    REQUIRE(ptrFilter->GetFieldSet()->GetCount() == 2);
    REQUIRE(ptrFilter->GetFieldSet()->Find("Type") >= 0);

    selector.SetField(1, "NoSuchField");
    REQUIRE_FALSE(selector.CanAssign(ptrTable));
    REQUIRE_THROWS(selector.GetSymbolByFeature(CreateRow("a", 1, 0.)));   // the row has no such column

    selector.SetupSymbols(Display::IDisplayPtr());   // no display - nothing
    REQUIRE(ptrA->nPrepare == 0);
    selector.ResetSymbols();
    selector.FlushBuffers(Display::IDisplayPtr(), Display::ITrackCancelPtr());
    REQUIRE(ptrA->nReset == 1);
    REQUIRE(ptrDefault->nReset == 1);
    REQUIRE(ptrA->nFlush == 1);
}

TEST_CASE("Unique value selector: save / load", "[cartography][selector][uniquevalue][serialize]")
{
    std::shared_ptr<CUniqueValueSymbolSelector> ptrSelector = std::make_shared<CUniqueValueSymbolSelector>();
    ptrSelector->SetFieldCount(2);
    ptrSelector->SetField(0, "Name");
    ptrSelector->SetField(1, "Type");
    ptrSelector->SetHeadingLabel("Roads");
    ptrSelector->AddValue({Text("road"), CommonLib::CVariant((int32_t)1)}, Line(255, 0, 0), "Main road");
    ptrSelector->AddValue({Text("road"), CommonLib::CVariant(2.5)}, Line(0, 255, 0), "Side road");
    ptrSelector->AddValue({CommonLib::CVariant(), CommonLib::CVariant((uint64_t)7)}, Line(0, 0, 255));
    ptrSelector->SetDescription(0, "description");
    ptrSelector->SetGroup(1, 3);
    ptrSelector->SetDefaultSymbol(Line(10, 10, 10));
    ptrSelector->SetDefaultLabel("Other");
    ptrSelector->SetUseDefaultSymbol(false);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrSelector->Save(ptrRoot);
    IUniqueValueSymbolSelectorPtr ptrLoaded = std::dynamic_pointer_cast<IUniqueValueSymbolSelector>(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot));
    REQUIRE(ptrLoaded != nullptr);

    REQUIRE(ptrLoaded->GetFieldCount() == 2);
    REQUIRE(ptrLoaded->GetField(1) == "Type");
    REQUIRE(ptrLoaded->GetHeadingLabel() == "Roads");
    REQUIRE(ptrLoaded->GetValueCount() == 3);
    REQUIRE(ptrLoaded->GetLabel(1) == "Side road");
    REQUIRE(ptrLoaded->GetDescription(0) == "description");
    REQUIRE(ptrLoaded->GetGroup(1) == 3);
    REQUIRE(ptrLoaded->GetValue(0, 0).Get<CommonLib::astr_t>() == "road");
    REQUIRE(ptrLoaded->GetValue(1, 1).Get<double>() == 2.5);
    REQUIRE(ptrLoaded->GetValue(2, 0).IsNull());
    REQUIRE(ptrLoaded->GetValue(2, 1).Get<uint64_t>() == 7);
    REQUIRE(ColorOf(ptrLoaded->GetSymbol(0)) == Display::Color(255, 0, 0));
    REQUIRE(ColorOf(ptrLoaded->GetDefaultSymbol()) == Display::Color(10, 10, 10));
    REQUIRE(ptrLoaded->GetDefaultLabel() == "Other");
    REQUIRE_FALSE(ptrLoaded->GetUseDefaultSymbol());

    REQUIRE(ColorOf(ptrLoaded->GetSymbolByFeature(CreateRow("road", 1, 0.))) == Display::Color(255, 0, 0));
}

// ---------------- range

TEST_CASE("Range selector: symbol by the range of the value", "[cartography][selector][range]")
{
    CCountingSymbolPtr ptrSmall = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrBig = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrOther = std::make_shared<CCountingSymbol>();

    CRangeSymbolSelector selector("Area");
    REQUIRE(selector.GetSymbolSelectorID() == RangeSymbolSelectorID);
    selector.AddRange(0., 10., ptrSmall, "small");
    selector.AddRange(100., 10., ptrBig, "big");   // from / to are ordered
    selector.SetDefaultSymbol(ptrOther);

    double dFrom = 0., dTo = 0.;
    selector.GetRange(1, &dFrom, &dTo);
    REQUIRE(dFrom == 10.);
    REQUIRE(dTo == 100.);

    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, 5.)) == ptrSmall);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, 10.)) == ptrSmall);   // the first range with it
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, 10.5)) == ptrBig);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, 100.)) == ptrBig);    // inclusive
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, -1.)) == ptrOther);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, 1000.)) == ptrOther);

    selector.SetUseDefaultSymbol(false);
    REQUIRE(selector.GetSymbolByFeature(CreateRow("x", 0, 1000.)) == nullptr);
}

TEST_CASE("Range selector: integer and numeric text fields", "[cartography][selector][range]")
{
    CCountingSymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();

    CRangeSymbolSelector byType("Type");
    byType.AddRange(1., 3., ptrSymbol);
    byType.SetUseDefaultSymbol(false);
    REQUIRE(byType.GetSymbolByFeature(CreateRow("x", 2, 0.)) == ptrSymbol);
    REQUIRE(byType.GetSymbolByFeature(CreateRow("x", 4, 0.)) == nullptr);

    CRangeSymbolSelector byName("Name");
    byName.AddRange(1., 3., ptrSymbol);
    byName.SetUseDefaultSymbol(false);
    REQUIRE(byName.GetSymbolByFeature(CreateRow("2.5", 0, 0.)) == ptrSymbol);
    REQUIRE(byName.GetSymbolByFeature(CreateRow("abc", 0, 0.)) == nullptr);
    REQUIRE(byName.GetSymbolByFeature(CreateRow(nullptr, 0, 0.)) == nullptr);
}

TEST_CASE("Range selector: sort, filter and save / load", "[cartography][selector][range][serialize]")
{
    GeoDatabase::ITablePtr ptrTable = std::make_shared<CTestTable>(CreateFields());
    std::shared_ptr<CRangeSymbolSelector> ptrSelector = std::make_shared<CRangeSymbolSelector>("Area");
    ptrSelector->AddRange(10., 20., Line(0, 255, 0), "middle");
    ptrSelector->AddRange(0.125, 10., Line(255, 0, 0), "low");
    ptrSelector->SetDescription(0, "10 - 20");
    ptrSelector->SetDefaultSymbol(Line(1, 2, 3));
    ptrSelector->SetDefaultLabel("Other");

    ptrSelector->SortRanges();
    REQUIRE(ptrSelector->GetLabel(0) == "low");
    REQUIRE(ptrSelector->GetDescription(1) == "10 - 20");

    REQUIRE(ptrSelector->CanAssign(ptrTable));
    GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
    ptrSelector->PrepareFilter(ptrTable, ptrFilter);
    REQUIRE(ptrFilter->GetFieldSet()->Find("Area") >= 0);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrSelector->Save(ptrRoot);
    IRangeSymbolSelectorPtr ptrLoaded = std::dynamic_pointer_cast<IRangeSymbolSelector>(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot));
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetField() == "Area");
    REQUIRE(ptrLoaded->GetRangeCount() == 2);
    double dFrom = 0., dTo = 0.;
    ptrLoaded->GetRange(0, &dFrom, &dTo);
    REQUIRE(dFrom == 0.125);   // full precision
    REQUIRE(dTo == 10.);
    REQUIRE(ColorOf(ptrLoaded->GetSymbol(1)) == Display::Color(0, 255, 0));
    REQUIRE(ColorOf(ptrLoaded->GetDefaultSymbol()) == Display::Color(1, 2, 3));
    REQUIRE(ptrLoaded->GetDefaultLabel() == "Other");
    REQUIRE(ColorOf(ptrLoaded->GetSymbolByFeature(CreateRow("x", 0, 15.))) == Display::Color(0, 255, 0));

    ptrLoaded->RemoveRange(0);
    REQUIRE(ptrLoaded->GetRangeCount() == 1);
    REQUIRE_THROWS(ptrLoaded->GetLabel(1));
}

TEST_CASE("Feature renderer draws with the unique value selector symbol", "[cartography][selector][renderer]")
{
    CCountingSymbolPtr ptrRoad = std::make_shared<CCountingSymbol>();
    CCountingSymbolPtr ptrRiver = std::make_shared<CCountingSymbol>();
    std::shared_ptr<CUniqueValueSymbolSelector> ptrSelector = std::make_shared<CUniqueValueSymbolSelector>("Name");
    ptrSelector->AddValue({Text("road")}, ptrRoad);
    ptrSelector->AddValue({Text("river")}, ptrRiver);

    CFeatureRenderer renderer;
    renderer.SetSymbolSelector(ptrSelector);
    renderer.PrepareFilter(std::make_shared<CTestTable>(CreateFields()), std::make_shared<GeoDatabase::CQueryFilter>());

    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("river", 0, 0.));
    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("river", 0, 0.));
    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("road", 0, 0.));
    renderer.DrawFeature(Display::IDisplayPtr(), CreateRow("lake", 0, 0.));   // no default symbol - skipped

    REQUIRE(ptrRiver->nDraw == 2);
    REQUIRE(ptrRoad->nDraw == 1);
}
