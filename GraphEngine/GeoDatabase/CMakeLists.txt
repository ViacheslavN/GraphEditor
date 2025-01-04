cmake_minimum_required(VERSION 3.22)
project(GeoDatabase)
#set(LIB_NAME GeoDatabase)


set(COMMON_SOURCE_FILES
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
        InsertCursorBase.h
        InsertCursorBase.cpp
        GeoDatabaseShape/ShapefileUtils.h
        GeoDatabaseShape/ShapefileUtils.cpp
        SpatialTableBase.h
        TableBase.h
        GeoDatabaseShape/SHPObject.h
        DataSetBase.h
        GeoDatabaseShape/ShapefileSpatialTable.h
        GeoDatabaseShape/ShapeFile.h
        GeoDatabaseShape/ShapeFile.cpp
        GeoDatabaseShape/SHPObject.cpp

)

set(WINDOWS_SOURCE_FILES
)

IF (WIN32)
    set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${WINDOWS_SOURCE_FILES})

ELSE()
    set(SOURCE_FILES ${COMMON_SOURCE_FILES}
            GeoDatabaseShape/ShapeDBFile.h
            GeoDatabaseShape/ShapeDBFile.cpp
            GeoDatabaseShape/ShapefileRowCursor.h
            GeoDatabaseShape/ShapefileRowCursor.cpp
            GeoDatabaseShape/ShapeRow.h
            GeoDatabaseShape/ShapeRow.cpp
            BaseRow.h
            GeoDatabaseShape/ShapeTree.h
            GeoDatabaseShape/ShapeTree.cpp

    )

ENDIF()


add_library(GeoDatabase ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC ShapeLib)

