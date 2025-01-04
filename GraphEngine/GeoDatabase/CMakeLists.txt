cmake_minimum_required(VERSION 3.22)
project(GeoDatabase)
#set(LIB_NAME GeoDatabase)


set(SOURCE_FILES
        GetDatabase.cpp
        Fields.h
        Fields.cpp
        FieldSet.h
        FieldSet.cpp
        OIDSet.h
        OIDSet.cpp
        GeometryDefinition.h
        GeometryDefinition.cpp
        CursorBase.h
)



add_library(GeoDatabase ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC ShapeLib)

