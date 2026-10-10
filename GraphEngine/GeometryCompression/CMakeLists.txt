cmake_minimum_required(VERSION 3.15)
project(GeometryCompression)

# Compression of the shape geometry (NumLen coding of the coordinate deltas), see ShapeFormat.h.
# Header only: CommonLib (CGeoShape) reads the compressed blobs with CCompressedShapeReader.
add_library(GeometryCompression INTERFACE)
target_sources(GeometryCompression INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/CompressionUtils.h
        ${CMAKE_CURRENT_SOURCE_DIR}/RangeCoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/NumLenCoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ValueCoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ShapeFormat.h
        ${CMAKE_CURRENT_SOURCE_DIR}/CompressedShapeReader.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ShapeCompressor.h
)
target_link_libraries(GeometryCompression INTERFACE CommonLib)

if(GRAPHENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
