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



add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ./)
target_include_directories(${PROJECT_NAME} PUBLIC ./include)


