cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for CartographyLib.
# Catch2 itself is provided by CommonLib/Tests/Catch (FetchContent on MSVC, installed package elsewhere).
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found - CartographyLib Catch tests are skipped")
        return()
    endif()
endif()

# include(Catch) lives in Catch2's 'extras' folder when Catch2 was downloaded by FetchContent
include(FetchContent)
FetchContent_GetProperties(Catch2)
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

set(BIN_NAME CartographyLibCatchTests)

set(SOURCE_FILES
        TestCommon.h
        TestLayers.cpp
        TestSelection.cpp
        TestFeatureRenderer.cpp
        TestSerialize.cpp
        TestMapDrawer.cpp
        TestRasterLayer.cpp
        TestSelectors.cpp
        TestSymbologyModel.cpp
        TestLabeling.cpp
        # platform independent part of the TestMapWTLDraw application
        ../TestMapWTLDraw/SymbologyModel.cpp
        ../TestMapWTLDraw/MapProject.cpp
)

add_executable(${BIN_NAME} ${SOURCE_FILES})

# CartographyLib headers are included as "../../xxx.h" from here, its own includes need CartographyLib dir
target_include_directories(${BIN_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(${BIN_NAME} PRIVATE
        CartographyLib
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
