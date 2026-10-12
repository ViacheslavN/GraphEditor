cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for SpatialLib.
# Catch2 v3 comes from vcpkg:  vcpkg install catch2   (x64-windows)
set(BIN_NAME SpatialLibCatchTests)

# Inside GraphEditor, Catch2 is provided by CommonLib/Tests/Catch (FetchContent with the static CRT on MSVC);
# standalone, an installed Catch2 v3 (vcpkg) is used.
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 CONFIG REQUIRED)
endif()

# include(Catch) lives in Catch2's 'extras' folder when Catch2 was downloaded by FetchContent
include(FetchContent)
FetchContent_GetProperties(Catch2)
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

set(SOURCE_FILES
        TestCommon.h
        TestSpatialKey.cpp
        TestZOrderPoint16.cpp
        TestZOrderPoint32.cpp
        TestZOrderPoint64.cpp
        TestFindMinZVal.cpp
        TestPointZOrderQuery.cpp
        TestZOrderRect.cpp
        TestZOrderRectBits.cpp
        TestFindRectMinZVal.cpp
        TestRectZOrderQuery.cpp
)

add_executable(${BIN_NAME} ${SOURCE_FILES})

# "SpatialLib/..." / "CommonLib/..." resolve from the repo root; SpatialLib's own headers
# include "pch.h" and "SpatialKey.h" relative to the SpatialLib root.
target_include_directories(${BIN_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(${BIN_NAME} PRIVATE
        SpatialLib
        CommonLib
        Catch2::Catch2WithMain
)

# Register every TEST_CASE with CTest
include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
