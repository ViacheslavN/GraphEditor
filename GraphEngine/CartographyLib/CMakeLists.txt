cmake_minimum_required(VERSION 3.17)
project(CartographyLib)


set(SOURCE_FILES

)



add_library(DisplayLib ${SOURCE_FILES})


target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)


target_include_directories(${PROJECT_NAME} PRIVATE ./)

add_subdirectory(Tests)