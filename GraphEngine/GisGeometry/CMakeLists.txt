cmake_minimum_required(VERSION 3.22)
project(GisGeometry)
set(LIB_NAME GisGeometry)



set(SOURCE_FILES)



add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ./)
target_include_directories(${PROJECT_NAME} PUBLIC ./include)


