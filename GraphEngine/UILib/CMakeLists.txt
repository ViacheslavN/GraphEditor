cmake_minimum_required(VERSION 3.22)
project(UILib)
#set(LIB_NAME UILib)



set(WINDOWS_SOURCE_FILES
        Windows/WTLHeaders.h
        Windows/WTLBase/BaseHtmlWindowWTL.h
        Windows/WTLBase/BaseHtmlWindowsWTL.cpp
        Windows/BaseWindow.h
        Windows/BaseWindow.cpp
        Windows/BaseHtmlWindow.h
        Windows/BaseHtmlWindow.cpp
        Windows/WndProcThunk.h
        "Windows/WndProcThunk.cpp"

)
set(LINUX_SOURCE_FILES

)

set(COMMON_SOURCE_FILES
        UILib.h

)

IF (WIN32)
    set(SOURCE_FILES ${COMMON_SOURCE_FILES}   ${WINDOWS_SOURCE_FILES})
ELSE()
    set(SOURCE_FILES ${COMMON_SOURCE_FILES}  ${LINUX_SOURCE_FILES})
ENDIF()



add_library(UILib ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_include_directories(${PROJECT_NAME} PRIVATE ../ThirdParty/WTL)
target_include_directories(${PROJECT_NAME} PRIVATE ../ThirdParty/sciter-js-sdk/include)

add_subdirectory(Tests)