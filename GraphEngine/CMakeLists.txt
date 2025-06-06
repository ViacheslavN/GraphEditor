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

if(WIN32)
   add_definitions(-DUNICODE -D_UNICODE)
    set_static_cxx_flags() # Macro is used because it's needed to update variable from parent scope
endif()


add_subdirectory(CommonLib)
add_subdirectory(DisplayLib)
add_subdirectory(GisGeometry)
add_subdirectory(GeoDatabase)
add_subdirectory(CartographyLib)
add_subdirectory(UILib)
add_subdirectory(ThirdParty)


set(ignoreMe "${CMAKE_TOOLCHAIN_FILE}")
