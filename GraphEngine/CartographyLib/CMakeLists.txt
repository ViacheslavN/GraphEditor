cmake_minimum_required(VERSION 3.17)
project(CartographyLib)


set(SOURCE_FILES
    layers/LayerBase.h
        layers/FeatureLayer.cpp
        layers/FeatureLayer.h
        renders/RenderersLoader.h
        renders/RenderersLoader.cpp
        layers/Layers.h
        layers/Layers.cpp
        Map.h
        Map.cpp
)



add_library(CartographyLib ${SOURCE_FILES})


target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)


target_include_directories(${PROJECT_NAME} PRIVATE ./)

#add_subdirectory(Tests)