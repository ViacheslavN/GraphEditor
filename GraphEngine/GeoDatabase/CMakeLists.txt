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
        GeoDatabaseShape/ShapeDBFile.h
        GeoDatabaseShape/ShapeDBFile.cpp
        GeoDatabaseShape/ShapefileRowCursor.h
        GeoDatabaseShape/ShapefileRowCursor.cpp
        GeoDatabaseShape/ShapeRow.h
        GeoDatabaseShape/ShapeRow.cpp
        BaseRow.h
        GeoDatabaseShape/ShapeTree.h
        GeoDatabaseShape/ShapeTree.cpp
        CursorBase.h
        GeoDatabaseShape/ShapefileSpatialTable.cpp
        Field.h
        Field.cpp
        QueryFilter.h
        QueryFilter.cpp
        GeoDatabaseShape/ShapefileWorkspace.h
        GeoDatabaseShape/ShapefileWorkspace.cpp
        WorkspaceBase.h
        GeoDatabaseSQlite/SQLiteWorkspace.h
        GeoDatabaseSQlite/SQLiteWorkspace.cpp
        GeoDatabaseSQlite/SQLiteTable.h
        GeoDatabaseSQlite/SQLiteTable.cpp
        GeoDatabaseSQlite/SQLiteTableBase.h
        GeoDatabaseSQlite/SQLiteUtils.h
        GeoDatabaseUtils.h
        GeoDatabaseUtils.cpp

)

set(WINDOWS_SOURCE_FILES
)

IF (WIN32)
    set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${WINDOWS_SOURCE_FILES})

ELSE()
    set(SOURCE_FILES ${COMMON_SOURCE_FILES}
            GeoDatabaseSQlite/SQLiteUtils.cpp)

ENDIF()


add_library(GeoDatabase ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC ShapeLib)

add_subdirectory(Tests)
