# Catch2 unit tests for CommonLib, one executable per module.
#
# MSVC: the project links the static CRT (/MT, /MTd), but vcpkg's x64-windows Catch2
# is built with the DLL CRT (/MD, /MDd) -> LNK2038 'RuntimeLibrary' mismatch.
# So on MSVC Catch2 is downloaded and built together with the tests, using /MT.
# Elsewhere an installed Catch2 v3 (vcpkg, system package) is used if available.
if(MSVC)
    set(GRAPHENGINE_FETCH_CATCH2_DEFAULT ON)
else()
    set(GRAPHENGINE_FETCH_CATCH2_DEFAULT OFF)
endif()
option(GRAPHENGINE_FETCH_CATCH2 "Download Catch2 and build it with the project's compiler flags"
       ${GRAPHENGINE_FETCH_CATCH2_DEFAULT})

if(GRAPHENGINE_FETCH_CATCH2)
    include(FetchContent)
    # built with the project's CMAKE_MSVC_RUNTIME_LIBRARY (static CRT, set in the root CMakeLists.txt)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(Catch2
            URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.1.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(Catch2)
    # the project defines UNICODE/_UNICODE globally, which makes Catch2WithMain define wmain()
    # instead of main() -> LNK2019 'unresolved external symbol main'. Keep the plain main().
    target_compile_definitions(Catch2WithMain PRIVATE DO_NOT_USE_WMAIN)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)   # for include(Catch)
else()
    find_package(Catch2 3 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found - CommonLib Catch tests are skipped")
        return()
    endif()
endif()

add_subdirectory(Stream)
add_subdirectory(Variant)
add_subdirectory(EntropyCoders)
