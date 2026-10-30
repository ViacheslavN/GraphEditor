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
        OSMPbfReader.h
        OSMPbfReader.cpp
        OSMConvertor.h
        OSMConvertor.cpp
        OSMNodeStore.h
        OSMNodeStore.cpp
        OSMFlatNodeStore.h
        OSMFlatNodeStore.cpp
        OSMTempFile.h
        OSMTempFile.cpp
        OSMProgressThrottle.h
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
        LayerColumnExcludeList.h
        LayerColumnExcludeList.cpp
        OSMFeatureWriter.h
        OSMFeatureWriter.cpp
        OSMAnalyzer.h
        OSMAnalyzer.cpp
        OSMSplitFormat.h
        OSMSplitFormat.cpp
        OSMSplitter.h
        OSMSplitter.cpp
        OSMSplitExport.h
        OSMSplitExport.cpp

        # the new conversion into bptreedb B+ trees (the nodes first)
        BPTree/OSMPointE7.h
        BPTree/OSMPointCompressor.h
        BPTree/OSMNodeLeafEncoder.h
        BPTree/OSMNodeLeafCompressor.h
        BPTree/OSMBPTreeTypes.h
        BPTree/OSMBPTreeSettings.h
        BPTree/OSMBPTreeUtils.h
        BPTree/OSMBPTreeUtils.cpp
        BPTree/OSMBPTreeNodeFile.h
        BPTree/OSMBPTreeNodeFile.cpp
        BPTree/OSMNodeQueue.h
        BPTree/OSMNodeQueue.cpp
        BPTree/OSMReaderThread.h
        BPTree/OSMBulkSource.h
        BPTree/OSMNodeTreeBuilder.h
        BPTree/OSMNodeTreeBuilder.cpp
        BPTree/OSMParallelLeafBuilder.h
        BPTree/OSMParallelLeafBuilder.cpp
        BPTree/OSMTagTreeTypes.h
        BPTree/OSMTagWriter.h
        BPTree/OSMTagWriter.cpp
        BPTree/OSMBPTreeTagFile.h
        BPTree/OSMBPTreeTagFile.cpp
        BPTree/OSMWayTreeTypes.h
        BPTree/OSMSharedLeafCache.h
        BPTree/OSMJobPipeline.h
        BPTree/OSMBlobSource.h
        BPTree/OSMBPTreeProgress.h
        BPTree/OSMSharedNodeReader.h
        BPTree/OSMSharedNodeReader.cpp
        BPTree/OSMWayResolver.h
        BPTree/OSMWayResolver.cpp
        BPTree/OSMWayReader.h
        BPTree/OSMWayReader.cpp
        BPTree/OSMBPTreeWayFile.h
        BPTree/OSMBPTreeWayFile.cpp
        BPTree/OSMSharedWayReader.h
        BPTree/OSMSharedWayReader.cpp
        BPTree/OSMRelationRecord.h
        BPTree/OSMRingBuilder.h
        BPTree/OSMRingBuilder.cpp
        BPTree/OSMRelationResolver.h
        BPTree/OSMRelationResolver.cpp
        BPTree/OSMRelationReader.h
        BPTree/OSMRelationReader.cpp
        BPTree/OSMBPTreeRelationFile.h
        BPTree/OSMBPTreeRelationFile.cpp
        BPTree/OSMBPTreeRelationConvertor.cpp
        BPTree/OSMBPTreeConvertor.h
        BPTree/OSMBPTreeConvertor.cpp
)

add_library(OSMConvertorLib ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC CartographyLib)
target_link_libraries(${PROJECT_NAME} PUBLIC DisplayLib)
target_link_libraries(${PROJECT_NAME} PUBLIC GeoDatabase)
target_link_libraries(${PROJECT_NAME} PUBLIC GisGeometry)
target_link_libraries(${PROJECT_NAME} PUBLIC BPtreeLib)   # BPTree/: the B+ trees of bptreedb
target_link_libraries(${PROJECT_NAME} PRIVATE expat::expat)

if(GRAPHENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
