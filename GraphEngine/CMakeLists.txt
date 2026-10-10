cmake_minimum_required(VERSION 3.15)

# Windows: static C/C++ runtime everywhere (/MT, /MTd). Requires policy CMP0091 (CMake >= 3.15),
# which also keeps /MD out of the default CMAKE_<LANG>_FLAGS_<CONFIG>, so there is no
# "D9025 overriding /MDd with /MTd" warning. Ignored by non-MSVC compilers.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(graphengine)

if(MSVC)
    # A build folder configured before the switch to CMAKE_MSVC_RUNTIME_LIBRARY still has
    # /MD, /MDd in its cached CMAKE_<LANG>_FLAGS_<CONFIG>; drop them so only /MT, /MTd is passed.
    foreach(lang C CXX)
        foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            string(REGEX REPLACE "[/-]M[DT]d?( |$)" "" CMAKE_${lang}_FLAGS_${cfg} "${CMAKE_${lang}_FLAGS_${cfg}}")
        endforeach()
    endforeach()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")
add_compile_definitions(SQLITE_WITHOUT_MSIZE)
add_compile_definitions(FT2_BUILD_LIBRARY)

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
endif()


add_subdirectory(CommonLib)
add_subdirectory(DisplayLib)
add_subdirectory(GisGeometry)
add_subdirectory(GeoDatabase)
add_subdirectory(CartographyLib)
if(WIN32)
    add_subdirectory(UILib)   # Windows UI (WTL/sciter) only
endif()
add_subdirectory(ThirdParty)
add_subdirectory(GraphEnginejni)   # GraphEnginejni.dll / libGraphEnginejni.so
add_subdirectory(Convertors)

set(ignoreMe "${CMAKE_TOOLCHAIN_FILE}")
