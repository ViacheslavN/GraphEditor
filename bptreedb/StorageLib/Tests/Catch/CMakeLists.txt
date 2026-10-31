cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for StorageLib: the pages of different sizes, the page cache, the page streams.
# Catch2 v3 comes from vcpkg:  vcpkg install catch2   (x64-windows)
set(BIN_NAME StorageLibCatchTests)

# Inside GraphEditor, Catch2 is provided by CommonLib/Tests/Catch (FetchContent with the static CRT on MSVC);
# standalone, an installed Catch2 v3 (vcpkg) is used.
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 CONFIG REQUIRED)
endif()

include(FetchContent)
FetchContent_GetProperties(Catch2)
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

set(SOURCE_FILES
        TestStoragePages.cpp
)

add_executable(${BIN_NAME} ${SOURCE_FILES})

target_link_libraries(${BIN_NAME} PRIVATE
        StorageLib
        UtilsLib
        CommonLib
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
