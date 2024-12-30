cmake_minimum_required(VERSION 3.17)
project(TestGraphicsAgg)
set(BIN_NAME TestGraphicsAgg)

set(SOURCE_FILES
        main.cpp
)



add_executable(${BIN_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE   CommonLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   DisplayLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   FreeType)
target_link_libraries(${PROJECT_NAME} PRIVATE   Jpeg)