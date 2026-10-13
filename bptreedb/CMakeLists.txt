cmake_minimum_required(VERSION 3.17)
project(bptreedb)

# bptreedb uses the shared CommonLib of GraphEditor (../CommonLib).
# Normally it is built as part of GraphEditor (the root CMakeLists.txt adds CommonLib first);
# configured on its own, it adds ../CommonLib itself.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_CXX_STANDARD 17)
    set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")
    add_compile_definitions(SQLITE_WITHOUT_MSIZE)
    if(WIN32)
        add_definitions(-DUNICODE -D_UNICODE)
    endif()
    option(BPTREEDB_BUILD_TESTS "Build bptreedb test executables" ON)
endif()

if(NOT TARGET CommonLib)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../CommonLib ${CMAKE_CURRENT_BINARY_DIR}/CommonLib)
endif()

# the Catch2 test options of the modules follow BPTREEDB_BUILD_TESTS
set(UTILSLIB_BUILD_TESTS ${BPTREEDB_BUILD_TESTS} CACHE BOOL "Build UtilsLib Catch2 tests")
set(SPATIALLIB_BUILD_TESTS ${BPTREEDB_BUILD_TESTS} CACHE BOOL "Build SpatialLib Catch2 tests")
set(BPTREELIB_BUILD_CATCH_TESTS ${BPTREEDB_BUILD_TESTS} CACHE BOOL "Build BPtreeLib Catch2 tests")

add_subdirectory(StorageLib)
add_subdirectory(UtilsLib)
add_subdirectory(BPtreeLib)
add_subdirectory(SpatialLib)
add_subdirectory(DB)

if(BPTREEDB_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

set(ignoreMe "${CMAKE_TOOLCHAIN_FILE}")
