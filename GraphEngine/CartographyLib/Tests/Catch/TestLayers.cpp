#include "TestCommon.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

namespace
{
    std::vector<std::string> LayerNames(ILayersPtr ptrLayers)
    {
        std::vector<std::string> names;
        for(int i = 0; i < ptrLayers->GetLayerCount(); ++i)
            names.push_back(ptrLayers->GetLayer(i)->GetName());
        return names;
    }
}

TEST_CASE("Every new layer gets its own id", "[cartography][layers]")
{
    ILayerPtr ptrLayer1 = CreateFeatureLayer("a");
    ILayerPtr ptrLayer2 = CreateFeatureLayer("b");

    REQUIRE(ptrLayer1->GetLayerId() != CommonLib::CGuid());
    REQUIRE(ptrLayer1->GetLayerId() != ptrLayer2->GetLayerId());
}

TEST_CASE("Add, get by index and by id", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");

    ptrLayers->AddLayer(ptrA);
    ptrLayers->AddLayer(ptrB);

    REQUIRE(ptrLayers->GetLayerCount() == 2);
    REQUIRE(ptrLayers->GetLayer(0) == ptrA);
    REQUIRE(ptrLayers->GetLayer(1) == ptrB);
    REQUIRE(ptrLayers->GetLayerById(ptrB->GetLayerId()) == ptrB);
    REQUIRE(ptrLayers->GetLayerById(CommonLib::CGuid::CreateNew()) == nullptr);
}

TEST_CASE("Adding the same layer twice throws", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");

    ptrLayers->AddLayer(ptrA);
    REQUIRE_THROWS(ptrLayers->AddLayer(ptrA));
    REQUIRE_THROWS(ptrLayers->InsertLayer(ptrA, 0));
    REQUIRE(ptrLayers->GetLayerCount() == 1);
}

TEST_CASE("Get layer out of range throws", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ptrLayers->AddLayer(CreateFeatureLayer("a"));

    REQUIRE_THROWS(ptrLayers->GetLayer(1));
    REQUIRE_THROWS(ptrLayers->GetLayer(-1));
}

TEST_CASE("Inserted layer can be found by id", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");
    ILayerPtr ptrC = CreateFeatureLayer("c");

    ptrLayers->AddLayer(ptrA);
    ptrLayers->AddLayer(ptrB);
    ptrLayers->InsertLayer(ptrC, 1);

    REQUIRE(LayerNames(ptrLayers) == std::vector<std::string>{"a", "c", "b"});
    REQUIRE(ptrLayers->GetLayerById(ptrC->GetLayerId()) == ptrC);

    ILayerPtr ptrD = CreateFeatureLayer("d");
    ptrLayers->InsertLayer(ptrD, 100); // past the end -> appended
    REQUIRE(LayerNames(ptrLayers) == std::vector<std::string>{"a", "c", "b", "d"});
}

TEST_CASE("Remove layer", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");
    ILayerPtr ptrC = CreateFeatureLayer("c");
    ptrLayers->AddLayer(ptrA);
    ptrLayers->AddLayer(ptrB);
    ptrLayers->AddLayer(ptrC);

    ptrLayers->RemoveLayer(ptrB);

    REQUIRE(LayerNames(ptrLayers) == std::vector<std::string>{"a", "c"});
    REQUIRE(ptrLayers->GetLayerById(ptrB->GetLayerId()) == nullptr);
    REQUIRE_THROWS(ptrLayers->RemoveLayer(ptrB));
}

TEST_CASE("Move layer keeps it registered", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");
    ILayerPtr ptrC = CreateFeatureLayer("c");
    ptrLayers->AddLayer(ptrA);
    ptrLayers->AddLayer(ptrB);
    ptrLayers->AddLayer(ptrC);

    ptrLayers->MoveLayer(ptrC, 0);
    REQUIRE(LayerNames(ptrLayers) == std::vector<std::string>{"c", "a", "b"});

    ptrLayers->MoveLayer(ptrC, 10);
    REQUIRE(LayerNames(ptrLayers) == std::vector<std::string>{"a", "b", "c"});

    REQUIRE(ptrLayers->GetLayerById(ptrC->GetLayerId()) == ptrC);
    REQUIRE_THROWS(ptrLayers->MoveLayer(CreateFeatureLayer("x"), 0));
}

TEST_CASE("Remove all layers", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ptrLayers->AddLayer(ptrA);
    ptrLayers->AddLayer(CreateFeatureLayer("b"));

    ptrLayers->RemoveAllLayers();

    REQUIRE(ptrLayers->GetLayerCount() == 0);
    REQUIRE(ptrLayers->GetLayerById(ptrA->GetLayerId()) == nullptr);
    ptrLayers->AddLayer(ptrA); // can be added again
    REQUIRE(ptrLayers->GetLayerCount() == 1);
}

TEST_CASE("Layers change counter follows the changes", "[cartography][layers]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    REQUIRE(ptrLayers->GetChangeCounter() == 0);

    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");
    ptrLayers->AddLayer(ptrA);                                      // +1
    ptrLayers->InsertLayer(ptrB, 0);                                // +1
    ptrLayers->MoveLayer(ptrB, 1);                                  // +1
    ptrLayers->RemoveLayer(ptrA);                                   // +1
    REQUIRE(ptrLayers->GetChangeCounter() == 4);

    // failed operations don't change it
    REQUIRE_THROWS(ptrLayers->AddLayer(ptrB));                      // already added
    REQUIRE_THROWS(ptrLayers->RemoveLayer(ptrA));                   // not in the list
    REQUIRE_THROWS(ptrLayers->MoveLayer(ptrA, 0));
    REQUIRE(ptrLayers->GetChangeCounter() == 4);

    ptrLayers->RemoveAllLayers();                                   // +1
    ptrLayers->RemoveAllLayers();                                   // already empty
    REQUIRE(ptrLayers->GetChangeCounter() == 5);
}
