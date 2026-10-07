#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    Display::ISymbolPtr CreateLineSymbol()
    {
        return std::make_shared<Display::CSimpleLineSymbol>(Display::Color(255, 0, 0), 2., Display::SimpleLineStyleSolid);
    }
}

TEST_CASE("Simple symbol selector save / load", "[cartography][serialize]")
{
    CSimpleSymbolSelector selector(CreateLineSymbol());
    selector.SetLabel("Roads");
    selector.SetDescription("All roads");

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    selector.Save(ptrRoot);

    ISymbolSelectorPtr ptrLoaded = CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot);
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetSymbolSelectorID() == SimpleSymbolSelectorID);

    ISimpleSymbolSelectorPtr ptrSimple = std::dynamic_pointer_cast<ISimpleSymbolSelector>(ptrLoaded);
    REQUIRE(ptrSimple != nullptr);
    REQUIRE(ptrSimple->GetLabel() == "Roads");
    REQUIRE(ptrSimple->GetDescription() == "All roads");
    REQUIRE(ptrSimple->GetSymbol() != nullptr);
    REQUIRE(ptrSimple->GetSymbol()->GetSymbolID() == Display::SimpleLineSymbolID);
}

TEST_CASE("Simple symbol selector without symbol save / load", "[cartography][serialize]")
{
    CSimpleSymbolSelector selector;
    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    selector.Save(ptrRoot);

    ISimpleSymbolSelectorPtr ptrLoaded = std::dynamic_pointer_cast<ISimpleSymbolSelector>(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot));
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetSymbol() == nullptr);
}

TEST_CASE("Feature renderer save / load", "[cartography][serialize]")
{
    CFeatureRenderer renderer;
    renderer.SetMinimumScale(100000.);
    renderer.SetMaximumScale(1000.);
    renderer.SetShapeField("Shape2");
    renderer.SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>(CreateLineSymbol()));

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    renderer.Save(ptrRoot);

    IFeatureRendererPtr ptrLoaded = CLoaderRenderers::LoadRenderer(ptrRoot);
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetFeatureRendererID() == SimpleFeatureRendererID);
    REQUIRE(ptrLoaded->GetMinimumScale() == 100000.);
    REQUIRE(ptrLoaded->GetMaximumScale() == 1000.);
    REQUIRE(ptrLoaded->GetShapeField() == "Shape2");
    REQUIRE(ptrLoaded->GetSymbolSelector() != nullptr);
    REQUIRE(ptrLoaded->GetSymbolSelector()->GetSymbolSelectorID() == SimpleSymbolSelectorID);
}

TEST_CASE("Loaders reject unknown ids", "[cartography][serialize]")
{
    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    REQUIRE_THROWS(CLoaderRenderers::LoadRenderer(ptrRoot));
    REQUIRE_THROWS(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot));
    REQUIRE_THROWS(CLayersLoader::LoadLayer(ptrRoot));

    ptrRoot->AddPropertyInt32U("FeatureRendererID", 1000);
    ptrRoot->AddPropertyInt32U("SymbolSelectorID", 1000);
    ptrRoot->AddPropertyInt32U("LayerTypeID", 1000);
    REQUIRE_THROWS(CLoaderRenderers::LoadRenderer(ptrRoot));
    REQUIRE_THROWS(CSymbolSelectorsLoader::LoadSymbolSelector(ptrRoot));
    REQUIRE_THROWS(CLayersLoader::LoadLayer(ptrRoot));
}

TEST_CASE("Feature layer save / load", "[cartography][serialize]")
{
    std::shared_ptr<CFeatureLayer> ptrLayer = CreateFeatureLayer("Roads");
    ptrLayer->SetMinimumScale(500000.);
    ptrLayer->SetDisplayField("Name");
    ptrLayer->SetOIDField("FID");
    ptrLayer->SetShapeField("Geom");
    ptrLayer->SetDefinitionQuery("Type = 1");
    ptrLayer->SetSelectable(false);
    std::shared_ptr<CFeatureRenderer> ptrRenderer = std::make_shared<CFeatureRenderer>();
    ptrRenderer->SetSymbolSelector(std::make_shared<CSimpleSymbolSelector>(CreateLineSymbol()));
    ptrLayer->AddRenderer(ptrRenderer);
    ptrLayer->AddRenderer(std::make_shared<CFeatureRenderer>());

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrLayer->Save(ptrRoot);

    ILayerPtr ptrLoaded = CLayersLoader::LoadLayer(ptrRoot);
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetLayerTypeID() == FeatureLayerID);
    REQUIRE(ptrLoaded->GetLayerId() == ptrLayer->GetLayerId());
    REQUIRE(ptrLoaded->GetName() == "Roads");
    REQUIRE(ptrLoaded->GetVisible());
    REQUIRE(ptrLoaded->GetMinimumScale() == 500000.);

    IFeatureLayerPtr ptrFeatureLayer = std::dynamic_pointer_cast<IFeatureLayer>(ptrLoaded);
    REQUIRE(ptrFeatureLayer != nullptr);
    REQUIRE(ptrFeatureLayer->GetDisplayField() == "Name");
    REQUIRE(ptrFeatureLayer->GetOIDField() == "FID");
    REQUIRE(ptrFeatureLayer->GetShapeField() == "Geom");
    REQUIRE(ptrFeatureLayer->GetDefinitionQuery() == "Type = 1");
    REQUIRE_FALSE(ptrFeatureLayer->GetSelectable());
    REQUIRE(ptrFeatureLayer->GetRendererCount() == 2);
    REQUIRE(ptrFeatureLayer->GetRenderer(0)->GetSymbolSelector() != nullptr);
    REQUIRE(ptrFeatureLayer->GetRenderer(1)->GetSymbolSelector() == nullptr);
    REQUIRE(ptrFeatureLayer->GetLayerTable() == nullptr);
}

TEST_CASE("Map save / load", "[cartography][serialize][map]")
{
    CMap map;
    map.SetName("World");
    map.SetMinimumScale(1000000.);
    map.SetHasReferenceScale(true);
    map.SetReferenceScale(25000.);
    map.SetVerticalFlip(true);

    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");
    map.GetLayers()->AddLayer(ptrA);
    map.GetLayers()->AddLayer(ptrB);

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    map.Save(ptrRoot);

    CMap loaded;
    loaded.Load(ptrRoot);

    REQUIRE(loaded.GetName() == "World");
    REQUIRE(loaded.GetMinimumScale() == 1000000.);
    REQUIRE(loaded.GetHasReferenceScale());
    REQUIRE(loaded.GetReferenceScale() == 25000.);
    REQUIRE(loaded.GetVerticalFlip());
    REQUIRE_FALSE(loaded.GetHorizontalFlip());

    ILayersPtr ptrLayers = loaded.GetLayers();
    REQUIRE(ptrLayers->GetLayerCount() == 2);
    REQUIRE(ptrLayers->GetLayer(0)->GetName() == "a");
    REQUIRE(ptrLayers->GetLayer(1)->GetName() == "b");
    REQUIRE(ptrLayers->GetLayerById(ptrB->GetLayerId()) != nullptr);
}
