set(BIN_NAME TestBPTree)

set(SOURCE_FILES
        main.cpp
        BPTreeMapWithoutComp.cpp
        PerfLog/PerfLog.cpp
)
add_executable(${BIN_NAME} ${SOURCE_FILES})

# SpatialLib's headers include "SpatialKey.h" and "pch.h" relative to the SpatialLib
# root; the repo root lets headers be included as   / "CommonLib/...".
target_include_directories(${BIN_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(${BIN_NAME} PUBLIC CommonLib)
target_link_libraries(${BIN_NAME} PUBLIC BPtreeLib)

# Release keeps optimizations but gets symbols, so crashes have a usable call stack
if(MSVC)
    target_compile_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()
