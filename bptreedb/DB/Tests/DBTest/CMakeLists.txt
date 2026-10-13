set(BIN_NAME TestDB)

# The database engine against an in-memory model: schema, CRUD, indexes, spatial queries,
# rollback, recovery from the journal, parallel read transactions
add_executable(${BIN_NAME} main.cpp)

target_link_libraries(${BIN_NAME} PUBLIC CommonLib)
target_link_libraries(${BIN_NAME} PUBLIC DBLib)

if(MSVC)
    target_compile_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(${BIN_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()
