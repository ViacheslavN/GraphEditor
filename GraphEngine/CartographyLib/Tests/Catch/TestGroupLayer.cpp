#include "TestCommon.h"
#include "../../layers/GroupLayer.h"
#include "../../Labeling/LabelDrawer.h"

using namespace GraphEngine;
using namespace GraphEngine::Cartography;
using namespace cartography_test;

TEST_CASE("Group layer: type, visible and expanded by default", "[cartography][grouplayer]")
{
    CGroupLayerPtr ptrGroup = std::make_shared<CGroupLayer>("Roads");
    REQUIRE(ptrGroup->GetLayerTypeID() == GroupLayerID);
    REQUIRE(ptrGroup->GetName() == "Roads");
    REQUIRE(ptrGroup->GetVisible());
    REQUIRE(ptrGroup->GetExpanded());
    REQUIRE(ptrGroup->IsValid());
    REQUIRE(ptrGroup->GetChildren() != nullptr);
    REQUIRE(ptrGroup->GetChildren()->GetLayerCount() == 0);
    REQUIRE(ptrGroup->GetSupportedDrawPhases() == DrawPhaseNone);
    REQUIRE(ptrGroup->GetExtent() == nullptr);
}

TEST_CASE("Group layer: layers of the groups are found by id", "[cartography][grouplayer]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    CGroupLayerPtr ptrGroup = std::make_shared<CGroupLayer>("group");
    CGroupLayerPtr ptrSubGroup = std::make_shared<CGroupLayer>("sub group");
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ILayerPtr ptrB = CreateFeatureLayer("b");

    ptrLayers->AddLayer(ptrGroup);
    ptrGroup->GetChildren()->AddLayer(ptrA);
    ptrGroup->GetChildren()->AddLayer(ptrSubGroup);
    ptrSubGroup->GetChildren()->AddLayer(ptrB);

    REQUIRE(ptrLayers->GetLayerCount() == 1);
    REQUIRE(ptrLayers->GetLayerById(ptrGroup->GetLayerId()) == ptrGroup);
    REQUIRE(ptrLayers->GetLayerById(ptrA->GetLayerId()) == ptrA);
    REQUIRE(ptrLayers->GetLayerById(ptrB->GetLayerId()) == ptrB);
    REQUIRE(ptrLayers->GetLayerById(CommonLib::CGuid::CreateNew()) == nullptr);
}

TEST_CASE("Group layer: changes of the children change the counter of the list", "[cartography][grouplayer]")
{
    ILayersPtr ptrLayers = std::make_shared<CLayers>();
    CGroupLayerPtr ptrGroup = std::make_shared<CGroupLayer>("group");
    ptrLayers->AddLayer(ptrGroup);

    uint64_t nCounter = ptrLayers->GetChangeCounter();
    ptrGroup->GetChildren()->AddLayer(CreateFeatureLayer("a"));
    REQUIRE(ptrLayers->GetChangeCounter() > nCounter);

    nCounter = ptrLayers->GetChangeCounter();
    ptrGroup->GetChildren()->AddLayer(CreateFeatureLayer("b"));
    REQUIRE(ptrLayers->GetChangeCounter() > nCounter);

    // the counter of the removed group doesn't take the sum back
    nCounter = ptrLayers->GetChangeCounter();
    ptrLayers->RemoveLayer(ptrGroup);
    REQUIRE(ptrLayers->GetChangeCounter() > nCounter);

    ptrLayers->AddLayer(ptrGroup);
    nCounter = ptrLayers->GetChangeCounter();
    ptrLayers->RemoveAllLayers();
    REQUIRE(ptrLayers->GetChangeCounter() > nCounter);
}

TEST_CASE("Group layer: the label drawer is given to the children", "[cartography][grouplayer]")
{
    CGroupLayerPtr ptrGroup = std::make_shared<CGroupLayer>("group");
    ILayerPtr ptrA = CreateFeatureLayer("a");
    ptrGroup->GetChildren()->AddLayer(ptrA);

    ILabelDrawerPtr ptrLabelDrawer = std::make_shared<CLabelDrawer>();
    ptrGroup->SetLabelDrawer(ptrLabelDrawer);
    REQUIRE(ptrA->GetLabelDrawer() == ptrLabelDrawer);

    ptrGroup->SetLabelDrawer(ILabelDrawerPtr());
    REQUIRE(ptrA->GetLabelDrawer() == nullptr);
}

TEST_CASE("Group layer save / load", "[cartography][grouplayer][serialize]")
{
    CGroupLayerPtr ptrGroup = std::make_shared<CGroupLayer>("group");
    ptrGroup->SetExpanded(false);
    ptrGroup->SetMinimumScale(100000.);
    CGroupLayerPtr ptrSubGroup = std::make_shared<CGroupLayer>("sub group");
    ptrGroup->GetChildren()->AddLayer(CreateFeatureLayer("a"));
    ptrGroup->GetChildren()->AddLayer(ptrSubGroup);
    ptrSubGroup->GetChildren()->AddLayer(CreateFeatureLayer("b"));

    CommonLib::ISerializeObjPtr ptrRoot = CreateSerializeRoot();
    ptrGroup->Save(ptrRoot);

    ILayerPtr ptrLoaded = CLayersLoader::LoadLayer(ptrRoot);
    REQUIRE(ptrLoaded != nullptr);
    REQUIRE(ptrLoaded->GetLayerTypeID() == GroupLayerID);
    REQUIRE(ptrLoaded->GetLayerId() == ptrGroup->GetLayerId());
    REQUIRE(ptrLoaded->GetName() == "group");
    REQUIRE(ptrLoaded->GetMinimumScale() == 100000.);

    IGroupLayerPtr ptrLoadedGroup = std::dynamic_pointer_cast<IGroupLayer>(ptrLoaded);
    REQUIRE(ptrLoadedGroup != nullptr);
    REQUIRE_FALSE(ptrLoadedGroup->GetExpanded());
    REQUIRE(ptrLoadedGroup->GetChildren()->GetLayerCount() == 2);
    REQUIRE(ptrLoadedGroup->GetChildren()->GetLayer(0)->GetName() == "a");

    IGroupLayerPtr ptrLoadedSub = std::dynamic_pointer_cast<IGroupLayer>(ptrLoadedGroup->GetChildren()->GetLayer(1));
    REQUIRE(ptrLoadedSub != nullptr);
    REQUIRE(ptrLoadedSub->GetChildren()->GetLayerCount() == 1);
    REQUIRE(ptrLoadedSub->GetChildren()->GetLayer(0)->GetName() == "b");
}
