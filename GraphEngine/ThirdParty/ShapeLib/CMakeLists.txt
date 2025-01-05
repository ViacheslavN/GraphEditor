cmake_minimum_required(VERSION 3.22)
project(ShapeLib)
set(LIB_NAME ShapeLib)



set(SOURCE_FILES
        shpopen.c
        dbfopen.c
        safileio.c
        shptree.c
        sbnsearch.c
        shapefil.h
        shapefil_private.h
        shapelib.def)


if (WIN32)

    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")

endif()

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ./)
target_include_directories(${PROJECT_NAME} PUBLIC ./include)


