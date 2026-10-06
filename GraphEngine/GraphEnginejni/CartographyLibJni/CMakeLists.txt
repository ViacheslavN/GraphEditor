cmake_minimum_required(VERSION 3.17)
project(CartographyLibJni)

# CartographyLibJni.dll (Windows) / libCartographyLibJni.so (Linux / Android): wrapper over CartographyLib

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        CartographyLibJni.cpp
)

add_library(CartographyLibJni SHARED ${SOURCE_FILES})

target_compile_definitions(CartographyLibJni PRIVATE GRAPHENGINEJNI_EXPORTS)
target_link_libraries(CartographyLibJni PRIVATE CommonLib CartographyLib)
