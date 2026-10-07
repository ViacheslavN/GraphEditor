#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    struct CSelectionListener
    {
        int nChanged = 0;
        void OnChanged() {++nChanged;}
    };
}

TEST_CASE("Empty selection", "[cartography][selection]")
{
    CSelection selection(std::make_shared<CLayers>());

    REQUIRE(selection.IsEmpty());
    REQUIRE(selection.GetLayers().empty());
    REQUIRE(selection.GetFeatures(CommonLib::CGuid::CreateNew()).empty());
}

TEST_CASE("Rows are kept unique and sorted per layer", "[cartography][selection]")
{
    CSelection selection(std::make_shared<CLayers>());
    CommonLib::CGuid layer1 = CommonLib::CGuid::CreateNew();
    CommonLib::CGuid layer2 = CommonLib::CGuid::CreateNew();

    selection.AddRow(layer1, 5);
    selection.AddRow(layer1, 1);
    selection.AddRow(layer1, 5);
    selection.AddRow(layer2, 7);

    REQUIRE_FALSE(selection.IsEmpty());
    REQUIRE(selection.GetFeatures(layer1) == std::vector<int64_t>{1, 5});
    REQUIRE(selection.GetFeatures(layer2) == std::vector<int64_t>{7});
}

TEST_CASE("Remove feature, clear for layer, clear", "[cartography][selection]")
{
    CSelection selection(std::make_shared<CLayers>());
    CommonLib::CGuid layer1 = CommonLib::CGuid::CreateNew();
    CommonLib::CGuid layer2 = CommonLib::CGuid::CreateNew();
    selection.AddRow(layer1, 1);
    selection.AddRow(layer1, 2);
    selection.AddRow(layer2, 3);

    selection.RemoveFeature(layer1, 1);
    REQUIRE(selection.GetFeatures(layer1) == std::vector<int64_t>{2});

    selection.RemoveFeature(layer1, 2);
    REQUIRE(selection.GetFeatures(layer1).empty());

    selection.ClearForLayer(layer2);
    REQUIRE(selection.IsEmpty());

    selection.AddRow(layer2, 3);
    selection.Clear();
    REQUIRE(selection.IsEmpty());
}

TEST_CASE("GetLayers returns only layers that exist in the map", "[cartography][selection]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");
    ptrLayers->AddLayer(ptrA);
    ptrLayers->AddLayer(ptrB);

    CSelection selection(ptrLayers);
    selection.AddRow(ptrA->GetLayerId(), 1);
    selection.AddRow(CommonLib::CGuid::CreateNew(), 2); // unknown layer

    std::vector<ILayerPtr> vecLayers = selection.GetLayers();
    REQUIRE(vecLayers.size() == 1);
    REQUIRE(vecLayers[0] == ptrA);

    ptrLayers->RemoveLayer(ptrA);
    REQUIRE(selection.GetLayers().empty());
}

TEST_CASE("Select change event fires only on real changes", "[cartography][selection]")
{
    CSelection selection(std::make_shared<CLayers>());
    CSelectionListener listener;
    selection.SetOnSelectChange(CommonLib::Delegate(&listener, &CSelectionListener::OnChanged), true);

    CommonLib::CGuid layer = CommonLib::CGuid::CreateNew();
    selection.AddRow(layer, 1);                                     // +1
    selection.AddRow(layer, 1);                                     // duplicate
    selection.RemoveFeature(layer, 100);                            // not selected
    selection.RemoveFeature(CommonLib::CGuid::CreateNew(), 1);      // unknown layer
    selection.ClearForLayer(CommonLib::CGuid::CreateNew());         // unknown layer
    selection.AddRow(layer, 2);                                     // +1
    selection.RemoveFeature(layer, 2);                              // +1
    selection.ClearForLayer(layer);                                 // +1
    selection.Clear();                                              // already empty

    REQUIRE(listener.nChanged == 4);
}

TEST_CASE("Selection symbol", "[cartography][selection]")
{
    CSelection selection(std::make_shared<CLayers>());
    REQUIRE(selection.GetSymbol() == nullptr);

    Display::ISymbolPtr ptrSymbol = std::make_shared<CCountingSymbol>();
    selection.SetSymbol(ptrSymbol);
    REQUIRE(selection.GetSymbol() == ptrSymbol);
}

TEST_CASE("Draw without symbol or without layers does nothing", "[cartography][selection]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ptrLayers->AddLayer(ptrA);

    CSelection selection(ptrLayers);
    selection.AddRow(ptrA->GetLayerId(), 1);

    // no symbol
    REQUIRE_NOTHROW(selection.Draw(Display::IDisplayPtr(), Display::ITrackCancelPtr()));

    // symbol set, but the layer is not valid (no table / renderers) -> skipped
    selection.SetSymbol(std::make_shared<CCountingSymbol>());
    REQUIRE_NOTHROW(selection.Draw(Display::IDisplayPtr(), Display::ITrackCancelPtr()));

    // map layers are gone
    CSelection orphan{ILayersPtr()};
    orphan.AddRow(ptrA->GetLayerId(), 1);
    orphan.SetSymbol(std::make_shared<CCountingSymbol>());
    REQUIRE_NOTHROW(orphan.Draw(Display::IDisplayPtr(), Display::ITrackCancelPtr()));
    REQUIRE(orphan.GetLayers().empty());
}

TEST_CASE("Map creates selection over its layers", "[cartography][selection][map]")
{
    CMap map;
    REQUIRE(map.GetSelection() != nullptr);

    ILayerPtr ptrA = CreateFeatureLayer("a");
    map.GetLayers()->AddLayer(ptrA);
    map.GetSelection()->AddRow(ptrA->GetLayerId(), 10);

    std::vector<ILayerPtr> vecLayers = map.GetSelection()->GetLayers();
    REQUIRE(vecLayers.size() == 1);
    REQUIRE(vecLayers[0] == ptrA);
}
