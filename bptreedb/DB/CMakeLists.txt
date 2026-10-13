cmake_minimum_required(VERSION 3.17)
project(DBLib)

# The database engine on the B+ trees: bptreedb.h is the SDK (the only public header).

set(SOURCE_FILES_FIELDS
        Fields/Field.cpp
        Fields/Index.cpp
        Fields/FieldSet.cpp
)

# the trees of the tables: split by the tree types (one object file of all of them exceeds the section limit of MSVC)
set(SOURCE_FILES_TREES
        Trees/RowTree.cpp
        Trees/ValueFields.cpp
        Trees/ValueFieldsSigned.cpp
        Trees/ValueFieldsUnsigned.cpp
        Trees/IndexTrees.cpp
        Trees/IndexTreesSigned.cpp
        Trees/IndexTreesUnsigned.cpp
        Trees/SpatialIndexTrees.cpp
)

set(SOURCE_FILES_CURSORS
        Cursors/Row.cpp
        Cursors/CursorColumns.cpp
        Cursors/SelectCursor.cpp
        Cursors/UpdateCursor.cpp
        Cursors/InsertCursor.cpp
        Cursors/DeleteCursor.cpp
)

set(SOURCE_FILES_TRANSACTIONS
        Transactions/Transaction.cpp
        Transactions/TranStorageIO.cpp
        Transactions/TransactionPager.cpp
        Transactions/RedoLog.cpp
)

# SQL: the syntax tree, the driver, the parser (bison) and the scanner (flex) generated into SQLEngine/Grammar;
# the execution: values, bound expressions, operators, the compiler (names, plans), statements and cursors
set(SOURCE_FILES_SQL
        SQLEngine/AST.cpp
        SQLEngine/SQLDriver.cpp
        SQLEngine/Grammar/SQLParser.cpp
        SQLEngine/Grammar/SQLLexer.cpp
        SQLEngine/SQLValue.cpp
        SQLEngine/Expr.cpp
        SQLEngine/Operators.cpp
        SQLEngine/Compiler.cpp
        SQLEngine/Statement.cpp
)

# The generated files are in the repository (SQLEngine/Grammar/generate.bat / generate.sh regenerate them).
# BPTREEDB_GENERATE_SQL_GRAMMAR=ON regenerates them in the build on a change of the grammar:
# bison 3.2+ and flex 2.6+ (win_bison / win_flex of win_flex_bison on Windows).
option(BPTREEDB_GENERATE_SQL_GRAMMAR "Regenerate the SQL parser and scanner with bison / flex" OFF)
if(BPTREEDB_GENERATE_SQL_GRAMMAR)
    find_package(BISON 3.2 REQUIRED)
    find_package(FLEX 2.6 REQUIRED)
    # run in the grammar folder with relative names, as generate.sh: the generated files don't get the paths of the build
    set(SQL_GRAMMAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SQLEngine/Grammar)
    add_custom_command(
            OUTPUT ${SQL_GRAMMAR_DIR}/SQLParser.cpp ${SQL_GRAMMAR_DIR}/SQLParser.hpp
            COMMAND ${BISON_EXECUTABLE} -Wall --no-lines -o SQLParser.cpp --header=SQLParser.hpp SQLParser.y
            DEPENDS ${SQL_GRAMMAR_DIR}/SQLParser.y
            WORKING_DIRECTORY ${SQL_GRAMMAR_DIR}
            COMMENT "bison: SQLParser.y")
    add_custom_command(
            OUTPUT ${SQL_GRAMMAR_DIR}/SQLLexer.cpp
            COMMAND ${FLEX_EXECUTABLE} --noline -o SQLLexer.cpp SQLLexer.l
            DEPENDS ${SQL_GRAMMAR_DIR}/SQLLexer.l ${SQL_GRAMMAR_DIR}/SQLParser.hpp
            WORKING_DIRECTORY ${SQL_GRAMMAR_DIR}
            COMMENT "flex: SQLLexer.l")
endif()

set(SOURCE_FILES
        Values.cpp
        Table.cpp
        Schema.cpp
        Database.cpp
        ${SOURCE_FILES_TREES}
        ${SOURCE_FILES_FIELDS}
        ${SOURCE_FILES_CURSORS}
        ${SOURCE_FILES_TRANSACTIONS}
        ${SOURCE_FILES_SQL}
)

add_library(DBLib ${SOURCE_FILES})
if(MSVC)
    # the template instantiations of the trees: a margin over the section limit of an object file
    target_compile_options(DBLib PRIVATE /bigobj)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC UtilsLib)
target_link_libraries(${PROJECT_NAME} PUBLIC StorageLib)
target_link_libraries(${PROJECT_NAME} PUBLIC BPtreeLib)
# the spatial indexes (R-tree, Z-order B+ tree)
target_link_libraries(${PROJECT_NAME} PUBLIC SpatialLib)
target_include_directories(${PROJECT_NAME} PRIVATE ./)
# the generated scanner includes <FlexLexer.h> (the copy of flex 2.6.4 in SQLEngine/Grammar)
target_include_directories(${PROJECT_NAME} PRIVATE SQLEngine/Grammar)

if(BPTREEDB_BUILD_TESTS)
    add_subdirectory(Tests/DBTest)
    add_subdirectory(Tests/SQLParserTest)
    add_subdirectory(Tests/SQLTest)
endif()
