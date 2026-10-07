cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for GeoDatabase (fields, rows, shapefile, SQLite, table copy).
# Catch2 itself is provided by CommonLib/Tests/Catch (FetchContent on MSVC, installed package elsewhere).
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found - GeoDatabase Catch tests are skipped")
        return()
    endif()
endif()

# include(Catch) lives in Catch2's 'extras' folder when Catch2 was downloaded by FetchContent
include(FetchContent)
FetchContent_GetProperties(Catch2)
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

set(BIN_NAME GeoDatabaseCatchTests)

set(SOURCE_FILES
        TestCommon.h
        TestFields.cpp
        TestShapefile.cpp
        TestSQLite.cpp
        TestTableCopier.cpp
)

add_executable(${BIN_NAME} ${SOURCE_FILES})

target_link_libraries(${BIN_NAME} PRIVATE
        GeoDatabase
        GisGeometry
        ShapeLib
        Catch2::Catch2WithMain
)
IF (WIN32)
    target_link_libraries(${BIN_NAME} PRIVATE rpcrt4.lib)
ENDIF()

include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
