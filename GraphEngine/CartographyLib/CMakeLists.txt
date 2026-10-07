cmake_minimum_required(VERSION 3.17)
project(CartographyLib)


set(SOURCE_FILES
        Cartography.h
        Map.h
        Map.cpp
        layers/LayerBase.h
        layers/FeatureLayer.h
        layers/FeatureLayer.cpp
        layers/Layers.h
        layers/Layers.cpp
        layers/LoaderLayers.h
        layers/LoaderLayers.cpp
        renders/FeatureRendererBase.h
        renders/FeatureRenderer.h
        renders/FeatureRenderer.cpp
        renders/RenderersLoader.h
        renders/RenderersLoader.cpp
        selectors/SimpleSymbolSelector.h
        selectors/SimpleSymbolSelector.cpp
        selectors/SymbolSelectorsLoader.h
        selectors/SymbolSelectorsLoader.cpp
        selection/Selection.h
        selection/Selection.cpp
        drawer/TrackCancel.h
        drawer/DrawTask.h
        drawer/DrawThread.h
        drawer/DrawThread.cpp
        drawer/MapTask.h
        drawer/MapTask.cpp
        drawer/MapDrawer.h
        drawer/MapDrawer.cpp
)



add_library(CartographyLib ${SOURCE_FILES})


target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC DisplayLib GeoDatabase GisGeometry)


target_include_directories(${PROJECT_NAME} PRIVATE ./)

if(GRAPHENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
