cmake_minimum_required(VERSION 3.17)
project(GeoDatabaseLibJni)

# GeoDatabaseLibJni.dll (Windows) / libGeoDatabaseLibJni.so (Linux / Android): wrapper over GeoDatabase

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        GeoDatabaseLibJni.cpp
)

add_library(GeoDatabaseLibJni SHARED ${SOURCE_FILES})

target_compile_definitions(GeoDatabaseLibJni PRIVATE GRAPHENGINEJNI_EXPORTS)
target_link_libraries(GeoDatabaseLibJni PRIVATE CommonLib GeoDatabase)
