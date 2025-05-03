cmake_minimum_required(VERSION 3.17)
project(TestHTMLWindowsWinAPI)
set(BIN_NAME TestHTMLWindowsWinAPI)

set(SOURCE_FILES
        main.cpp
        HtmlWindow.h
        HtmlWindow.cpp

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
target_link_libraries(${PROJECT_NAME} PRIVATE   UILib)


target_include_directories(${PROJECT_NAME} PRIVATE ../../../ThirdParty/WTL)
target_include_directories(${PROJECT_NAME} PRIVATE ../../../ThirdParty/sciter-js-sdk/include)