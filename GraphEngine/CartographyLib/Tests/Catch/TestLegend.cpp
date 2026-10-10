#include "TestCommon.h"
#include "../../legend/Legend.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    Display::ISymbolPtr CreateLineSymbol(unsigned char red)
    {
        return std::make_shared<Display::CSimpleLineSymbol>(Display::Color(red, 0, 0), 1., Display::SimpleLineStyleSolid);
    }
}

TEST_CASE("Legend: simple selector gives one class", "[cartography][legend]")
{
    Display::ISymbolPtr ptrSymbol = CreateLineSymbol(255);
    std::shared_ptr<CSimpleSymbolSelector> ptrSelector = std::make_shared<CSimpleSymbolSelector>(ptrSymbol);
    ptrSelector->SetLabel("Roads");

    std::vector<ILegendGroupPtr> vecGroups = CLegendUtils::GetLegendGroups(ptrSelector.get());
    REQUIRE(vecGroups.size() == 1);
    REQUIRE(vecGroups[0]->GetClassCount() == 1);
    REQUIRE(vecGroups[0]->GetClass(0)->GetLabel() == "Roads");
    REQUIRE(vecGroups[0]->GetClass(0)->GetSymbol() == ptrSymbol);

    // no symbol - no legend
    CSimpleSymbolSelector empty;
    REQUIRE(empty.GetLegendGroupCount() == 0);
}

TEST_CASE("Legend: the class symbol is written back to the selector", "[cartography][legend]")
{
    std::shared_ptr<CSimpleSymbolSelector> ptrSelector = std::make_shared<CSimpleSymbolSelector>(CreateLineSymbol(255));
    ILegendClassPtr ptrClass = ptrSelector->GetLegendGroup(0)->GetClass(0);

    Display::ISymbolPtr ptrNew = CreateLineSymbol(10);
    ptrClass->SetSymbol(ptrNew);
    REQUIRE(ptrSelector->GetSymbol() == ptrNew);

    // the selector is changed after the legend was made: the old class doesn't overwrite it
    Display::ISymbolPtr ptrOther = CreateLineSymbol(20);
    ptrSelector->SetSymbol(ptrOther);
    ptrClass->SetSymbol(CreateLineSymbol(30));
    REQUIRE(ptrSelector->GetSymbol() == ptrOther);
}

TEST_CASE("Legend: selector not kept by a shared_ptr gives a read only legend", "[cartography][legend]")
{
    Display::ISymbolPtr ptrSymbol = CreateLineSymbol(255);
    CSimpleSymbolSelector selector(ptrSymbol);
    ILegendClassPtr ptrClass = selector.GetLegendGroup(0)->GetClass(0);
    ptrClass->SetSymbol(CreateLineSymbol(10));
    REQUIRE(selector.GetSymbol() == ptrSymbol);
}

TEST_CASE("Legend: unique values - a class per value and the default class", "[cartography][legend]")
{
    std::shared_ptr<CUniqueValueSymbolSelector> ptrSelector = std::make_shared<CUniqueValueSymbolSelector>("TYPE");
    ptrSelector->AddValue({CommonLib::CVariant(std::string("road"))}, CreateLineSymbol(1), "Road");
    ptrSelector->AddValue({CommonLib::CVariant((int32_t)5)}, CreateLineSymbol(2));
    ptrSelector->SetDefaultSymbol(CreateLineSymbol(3));
    ptrSelector->SetDefaultLabel("Other");

    ILegendGroupPtr ptrGroup = ptrSelector->GetLegendGroup(0);
    REQUIRE(ptrGroup->GetHeading() == "TYPE");
    REQUIRE(ptrGroup->GetClassCount() == 3);
    REQUIRE(ptrGroup->GetClass(0)->GetLabel() == "Road");
    REQUIRE(ptrGroup->GetClass(1)->GetLabel() == "5");   // no label - the value
    REQUIRE(ptrGroup->GetClass(2)->GetLabel() == "Other");

    Display::ISymbolPtr ptrNew = CreateLineSymbol(50);
    ptrGroup->GetClass(1)->SetSymbol(ptrNew);
    REQUIRE(ptrSelector->GetSymbol(1) == ptrNew);
    ptrGroup->GetClass(2)->SetSymbol(ptrNew);
    REQUIRE(ptrSelector->GetDefaultSymbol() == ptrNew);

    ptrSelector->SetUseDefaultSymbol(false);
    REQUIRE(ptrSelector->GetLegendGroup(0)->GetClassCount() == 2);
}

TEST_CASE("Legend: ranges - a class per range", "[cartography][legend]")
{
    std::shared_ptr<CRangeSymbolSelector> ptrSelector = std::make_shared<CRangeSymbolSelector>("POP");
    ptrSelector->AddRange(0., 10., CreateLineSymbol(1));
    ptrSelector->AddRange(10., 20.5, CreateLineSymbol(2), "Big");
    ptrSelector->SetUseDefaultSymbol(false);

    ILegendGroupPtr ptrGroup = ptrSelector->GetLegendGroup(0);
    REQUIRE(ptrGroup->GetHeading() == "POP");
    REQUIRE(ptrGroup->GetClassCount() == 2);
    REQUIRE(ptrGroup->GetClass(0)->GetLabel() == "0 - 10");
    REQUIRE(ptrGroup->GetClass(1)->GetLabel() == "Big");
}

TEST_CASE("Legend: feature layer collects the groups of its renderers", "[cartography][legend]")
{
    std::shared_ptr<CFeatureLayer> ptrLayer = CreateFeatureLayer("layer");
    REQUIRE(ptrLayer->GetLegendGroupCount() == 0);

    std::shared_ptr<CFeatureRenderer> ptrRenderer1 = std::make_shared<CFeatureRenderer>();
    ptrRenderer1->SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>(CreateLineSymbol(1)));
    std::shared_ptr<CFeatureRenderer> ptrRenderer2 = std::make_shared<CFeatureRenderer>();
    std::shared_ptr<CRangeSymbolSelector> ptrRanges = std::make_shared<CRangeSymbolSelector>("POP");
    ptrRanges->AddRange(0., 1., CreateLineSymbol(2));
    ptrRenderer2->SetSymbolSelector(ptrRanges);
    ptrLayer->AddRenderer(ptrRenderer1);
    ptrLayer->AddRenderer(ptrRenderer2);

    std::vector<ILegendGroupPtr> vecGroups = CLegendUtils::GetLayerLegend(ptrLayer);
    REQUIRE(ptrLayer->GetLegendGroupCount() == 2);
    REQUIRE(vecGroups.size() == 2);
    REQUIRE(vecGroups[1]->GetHeading() == "POP");
    REQUIRE_THROWS(ptrLayer->GetLegendGroup(2));
}
