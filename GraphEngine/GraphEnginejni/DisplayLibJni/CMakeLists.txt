cmake_minimum_required(VERSION 3.17)
project(DisplayLibJni)

# DisplayLibJni.dll (Windows) / libDisplayLibJni.so (Linux / Android): wrapper over DisplayLib

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        DisplayLibJni.cpp
)

add_library(DisplayLibJni SHARED ${SOURCE_FILES})

target_compile_definitions(DisplayLibJni PRIVATE GRAPHENGINEJNI_EXPORTS)
target_link_libraries(DisplayLibJni PRIVATE CommonLib DisplayLib)
