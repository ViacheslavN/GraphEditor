cmake_minimum_required(VERSION 3.17)
project(TestSQLiteDatabase)
set(BIN_NAME TestSQLiteDatabase)

set(SOURCE_FILES
        main.cpp
        CreateDatabase.cpp
        TestingData.h
        TestingData.cpp
        InsertDataIntoDatabase.cpp
        SelectFromDatabaseTest.cpp
        TestSpatialData.cpp
)



add_executable(${BIN_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE   CommonLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   GeoDatabase)
target_link_libraries(${PROJECT_NAME} PRIVATE   GisGeometry)
target_link_libraries(${PROJECT_NAME} PRIVATE   ShapeLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   Proj4)