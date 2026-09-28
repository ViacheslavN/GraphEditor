cmake_minimum_required(VERSION 3.8)
project(graphengine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")
add_compile_definitions(SQLITE_WITHOUT_MSIZE)
add_compile_definitions(FT2_BUILD_LIBRARY)

macro(set_static_cxx_flags)
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endmacro()

# Android: only the static libraries are built (no tests, no Windows UI)
if(ANDROID)
    set(GRAPHENGINE_BUILD_TESTS_DEFAULT OFF)
else()
    set(GRAPHENGINE_BUILD_TESTS_DEFAULT ON)
endif()
option(GRAPHENGINE_BUILD_TESTS "Build test executables" ${GRAPHENGINE_BUILD_TESTS_DEFAULT})

if(NOT MSVC)
    # legacy code (AGG, CPL) uses 'register', which is an error in C++17 for clang/gcc
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-register>)
endif()

if(ANDROID)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    message(STATUS "GraphEngine: Android build, ABI=${ANDROID_ABI}, platform=${ANDROID_PLATFORM}, STL=${ANDROID_STL}")
endif()

if(WIN32)
   add_definitions(-DUNICODE -D_UNICODE)
    set_static_cxx_flags() # Macro is used because it's needed to update variable from parent scope
endif()


add_subdirectory(CommonLib)
add_subdirectory(DisplayLib)
add_subdirectory(GisGeometry)
if(ANDROID)
    # GeoDatabase / CartographyLib are work in progress and do not compile yet
    # (ctor mismatch in SQLite tables). Targets exist but are not part of 'all';
    # build them explicitly (cmake --build ... --target GeoDatabase) once fixed.
    add_subdirectory(GeoDatabase EXCLUDE_FROM_ALL)
    add_subdirectory(CartographyLib EXCLUDE_FROM_ALL)
else()
    add_subdirectory(GeoDatabase)
    add_subdirectory(CartographyLib)
endif()
if(NOT ANDROID)
    add_subdirectory(UILib)   # Windows UI (WTL/sciter) only
endif()
add_subdirectory(ThirdParty)


set(ignoreMe "${CMAKE_TOOLCHAIN_FILE}")
