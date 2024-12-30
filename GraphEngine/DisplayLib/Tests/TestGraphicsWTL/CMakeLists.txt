cmake_minimum_required(VERSION 3.17)
project(TestGraphicsWTL)
set(BIN_NAME TestGraphicsWTL)

set(SOURCE_FILES
        MainFrm.cpp
        stdafx.cpp
        TestGraphics.cpp
        TestGraphicsView.cpp
        AboutDlg.cpp
        TestGraphics.rc
)

add_compile_definitions(_WINDOWS)
add_compile_definitions(WIN64)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
#set_target_properties(TestGraphicsWTL PROPERTIES LINK_FLAGS " /MANIFEST:NO")

add_executable(${BIN_NAME} WIN32 ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE   CommonLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   DisplayLib)
target_link_libraries(${PROJECT_NAME} PRIVATE   FreeType)
target_link_libraries(${PROJECT_NAME} PRIVATE   Jpeg)


target_include_directories(${PROJECT_NAME} PRIVATE ../../../ThirdParty/WTL)