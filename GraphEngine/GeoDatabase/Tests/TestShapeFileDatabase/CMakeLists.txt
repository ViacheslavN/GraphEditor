cmake_minimum_required(VERSION 3.17)
project(TestShapeFileDatabase)
set(BIN_NAME TestShapeFileDatabase)

set(SOURCE_FILES
        main.cpp
)



add_executable(${BIN_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE   CommonLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   GeoDatabase)
target_link_libraries(${PROJECT_NAME} PRIVATE   GisGeometry)
target_link_libraries(${PROJECT_NAME} PRIVATE   ShapeLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   Proj4)
IF (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE rpcrt4.lib)
ENDIF()