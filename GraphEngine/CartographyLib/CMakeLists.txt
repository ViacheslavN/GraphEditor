cmake_minimum_required(VERSION 3.17)
project(CartographyLib)


set(SOURCE_FILES
        Cartography.h
        Map.h
        Map.cpp
        layers/LayerBase.h
        layers/FeatureLayer.h
        layers/FeatureLayer.cpp
        layers/RasterLayer.h
        layers/RasterLayer.cpp
        layers/Layers.h
        layers/Layers.cpp
        layers/LoaderLayers.h
        layers/LoaderLayers.cpp
        renders/FeatureRendererBase.h
        renders/FeatureRenderer.h
        renders/FeatureRenderer.cpp
        renders/AnnotationRenderer.h
        renders/AnnotationRenderer.cpp
        renders/LabelRenderer.h
        renders/LabelRenderer.cpp
        renders/RenderersLoader.h
        renders/RenderersLoader.cpp
        renders/Raster/RasterPixelUtils.h
        renders/Raster/RasterStatistics.h
        renders/Raster/RasterStatistics.cpp
        renders/Raster/RasterRendererBase.h
        renders/Raster/RasterRendererBase.cpp
        renders/Raster/RasterRGBRenderer.h
        renders/Raster/RasterRGBRenderer.cpp
        renders/Raster/RasterStretchRenderer.h
        renders/Raster/RasterStretchRenderer.cpp
        selectors/SimpleSymbolSelector.h
        selectors/SimpleSymbolSelector.cpp
        selectors/SymbolSelectorsLoader.h
        selectors/SymbolSelectorsLoader.cpp
        selectors/SymbolSelectorUtils.h
        selectors/SymbolSelectorUtils.cpp
        selectors/UniqueValueSymbolSelector.h
        selectors/UniqueValueSymbolSelector.cpp
        selectors/RangeSymbolSelector.h
        selectors/RangeSymbolSelector.cpp
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
        Labeling/Geom/LabelGeometry.h
        Labeling/Geom/LabelGeometry.cpp
        Labeling/LabelTypes.h
        Labeling/LabelStyle.h
        Labeling/LabelStyle.cpp
        Labeling/LabelStrategy.h
        Labeling/LabelStrategy.cpp
        Labeling/LabelCollisionGrid.h
        Labeling/LabelCollisionGrid.cpp
        Labeling/LabelDrawer.h
        Labeling/LabelDrawer.cpp
)



add_library(CartographyLib ${SOURCE_FILES})


target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC DisplayLib GeoDatabase GisGeometry)


target_include_directories(${PROJECT_NAME} PRIVATE ./)

if(GRAPHENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
