set(BIN_NAME TestBPTreeTypes)

# B+ trees with float / double / string keys and values, and with duplicate keys (TBPMultiMap)
add_executable(${BIN_NAME} main.cpp)

target_include_directories(${BIN_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(${BIN_NAME} PUBLIC CommonLib)
target_link_libraries(${BIN_NAME} PUBLIC BPtreeLib)

if(MSVC)
    target_compile_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()
