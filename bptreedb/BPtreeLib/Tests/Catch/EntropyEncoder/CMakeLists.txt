cmake_minimum_required(VERSION 3.17)

# Catch2 unit tests for BPtreeLib/Compressor/EntropyEncoder
# (bit stream, sign encoder, unsigned/signed NumLen encoders).
# Catch2 v3 comes from vcpkg:  vcpkg install catch2   (x64-windows)
set(BIN_NAME BPtreeLibEntropyEncoderCatchTests)

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
        TestBitStream.cpp
        TestBoolEncoder.cpp
        TestNumLenEncoder.cpp
)

add_executable(${BIN_NAME} ${SOURCE_FILES})

# Headers are included as "BPtreeLib/xxx.h" / "CommonLib/xxx.h" relative to the repo root
target_include_directories(${BIN_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

target_link_libraries(${BIN_NAME} PRIVATE
        BPtreeLib
        CommonLib
        Catch2::Catch2WithMain
)

# Register every TEST_CASE with CTest
include(CTest)
include(Catch)
catch_discover_tests(${BIN_NAME})
