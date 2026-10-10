cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for OSMConvertorLib (Catch2 is provided by CommonLib/Tests/Catch or an installed package)
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found - OSMConvertorLib tests are skipped")
        return()
    endif()
endif()

include(FetchContent)
FetchContent_GetProperties(Catch2)
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

set(BIN_NAME OSMConvertorLibCatchTests)

add_executable(${BIN_NAME} TestOSMMap.cpp TestOSMConvert.cpp)
target_link_libraries(${BIN_NAME} PRIVATE OSMConvertorLib Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
