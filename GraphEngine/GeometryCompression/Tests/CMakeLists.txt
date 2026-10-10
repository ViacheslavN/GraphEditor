cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for GeometryCompression.
# Catch2 itself is provided by CommonLib/Tests/Catch (FetchContent on MSVC, installed package elsewhere).
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found - GeometryCompression Catch tests are skipped")
        return()
    endif()
endif()

include(FetchContent)
FetchContent_GetProperties(Catch2)
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

set(BIN_NAME GeometryCompressionCatchTests)

add_executable(${BIN_NAME} TestGeometryCompression.cpp)

target_link_libraries(${BIN_NAME} PRIVATE
        GeometryCompression
        Catch2::Catch2WithMain
)
IF (WIN32)
    target_link_libraries(${BIN_NAME} PRIVATE rpcrt4.lib)
ENDIF()

include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
