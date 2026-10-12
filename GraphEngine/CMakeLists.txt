cmake_minimum_required(VERSION 3.15)

# GraphEngine libraries. The global settings and the shared libraries (CommonLib, GeometryCompression,
# ThirdParty) live in the GraphEditor root CMakeLists.txt - configure from there.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "GraphEngine is part of GraphEditor: configure the parent folder (${CMAKE_CURRENT_SOURCE_DIR}/..) instead.")
endif()

project(graphengine)

add_subdirectory(DisplayLib)
add_subdirectory(GisGeometry)
add_subdirectory(GeoDatabase)
add_subdirectory(CartographyLib)
if(WIN32)
    add_subdirectory(UILib)   # Windows UI (WTL/sciter) only
endif()
add_subdirectory(GraphEnginejni)   # GraphEnginejni.dll / libGraphEnginejni.so
add_subdirectory(Convertors)
