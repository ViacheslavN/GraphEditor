set(BIN_NAME TestSQL)

# SQL against an in-memory model: DDL, INSERT / UPDATE / DELETE, index and scan paths (the same results),
# joins (index, hash, nested loop; inner, outer), aggregation, subqueries, UNION, spatial queries, errors
add_executable(${BIN_NAME} main.cpp)

target_link_libraries(${BIN_NAME} PUBLIC CommonLib)
target_link_libraries(${BIN_NAME} PUBLIC DBLib)

if(MSVC)
    target_compile_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()
