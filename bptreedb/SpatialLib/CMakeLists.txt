cmake_minimum_required(VERSION 3.17)
project(SpatialLib)


set(SOURCE_FILES
    ZCurve/Point/SpatialPointQuery.cpp
    ZCurve/Rect/SpatialRectQuery.cpp
)


add_library(SpatialLib ${SOURCE_FILES})


target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
# RTree (header-only) keeps its nodes in the pages of StorageLib
target_link_libraries(${PROJECT_NAME} PUBLIC StorageLib)
# RTreeNumLenCompressor uses the NumLen entropy encoders of BPtreeLib
target_link_libraries(${PROJECT_NAME} PUBLIC BPtreeLib)

target_include_directories(${PROJECT_NAME} PUBLIC ./)

option(SPATIALLIB_BUILD_TESTS "Build SpatialLib Catch2 tests" ON)
if(SPATIALLIB_BUILD_TESTS)
    add_subdirectory(Tests/Catch)
    add_subdirectory(Tests/TestRectIndex)
    add_subdirectory(Tests/BPRTreeTest)
    add_subdirectory(Tests/BPZOrderTreeTest)
endif()

