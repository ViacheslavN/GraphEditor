cmake_minimum_required(VERSION 3.17)
project(GisGeometryLibJni)

# GisGeometryLibJni.dll (Windows) / libGisGeometryLibJni.so (Linux / Android): wrapper over GisGeometry

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        GisGeometryLibJni.cpp
)

add_library(GisGeometryLibJni SHARED ${SOURCE_FILES})

target_compile_definitions(GisGeometryLibJni PRIVATE GRAPHENGINEJNI_EXPORTS)
target_link_libraries(GisGeometryLibJni PRIVATE CommonLib GisGeometry)
