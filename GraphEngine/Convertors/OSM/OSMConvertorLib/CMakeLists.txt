cmake_minimum_required(VERSION 3.22)
project(OSMConvertorLib)

# OpenStreetMap (.osm / .pbf) converter, reads the files with readosm (expat for XML, zlib from CommonLib for PBF).
# expat: the static library built from ThirdParty/expat, otherwise an installed package (vcpkg ...).
if(NOT TARGET expat::expat)
    find_package(expat CONFIG QUIET)
endif()
if(NOT TARGET expat::expat)
    message(WARNING "OSMConvertorLib: expat not found - the OSM converter is skipped")
    return()
endif()

set(SOURCE_FILES
        readosm/readosm.h
        readosm/osmxml.c
        readosm/osm_objects.c
        readosm/protobuf.c
        readosm/readosm.c

        OSMConvertorLib.h
        OSMTags.h
        OSMMap.h
        OSMMap.cpp
        OSMSchema.h
        OSMSchema.cpp
        OSMReader.h
        OSMReader.cpp
        OSMConvertor.h
        OSMConvertor.cpp
        OSMNodeStore.h
        OSMNodeStore.cpp
        OSMWayStore.h
        OSMWayStore.cpp
        OSMGeometry.h
        OSMGeometry.cpp
        OSMTableWriter.h
        OSMTableWriter.cpp
        OSMMapStyle.h
        OSMMapStyle.cpp
        OSMConvertSession.h
        OSMConvertSession.cpp
)

add_library(OSMConvertorLib ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC CartographyLib)
target_link_libraries(${PROJECT_NAME} PUBLIC DisplayLib)
target_link_libraries(${PROJECT_NAME} PUBLIC GeoDatabase)
target_link_libraries(${PROJECT_NAME} PUBLIC GisGeometry)
target_link_libraries(${PROJECT_NAME} PRIVATE expat::expat)

if(GRAPHENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
