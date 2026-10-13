set(BIN_NAME TestSQLParser)

# The SQL front end: statements parsed and printed back (the grouping of the expressions), joins,
# parameters, positions, syntax errors
add_executable(${BIN_NAME} main.cpp)

target_link_libraries(${BIN_NAME} PUBLIC CommonLib)
target_link_libraries(${BIN_NAME} PUBLIC DBLib)
