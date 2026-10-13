cmake_minimum_required(VERSION 3.17)
project(DBLib)

# The database engine on the B+ trees: bptreedb.h is the SDK (the only public header).

set(SOURCE_FILES_FIELDS
        Fields/Field.cpp
        Fields/Index.cpp
        Fields/FieldSet.cpp
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

set(SOURCE_FILES
        Values.cpp
        DBTrees.cpp
        Table.cpp
        Schema.cpp
        Database.cpp
        ${SOURCE_FILES_FIELDS}
        ${SOURCE_FILES_CURSORS}
        ${SOURCE_FILES_TRANSACTIONS}
)

add_library(DBLib ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC UtilsLib)
target_link_libraries(${PROJECT_NAME} PUBLIC StorageLib)
target_link_libraries(${PROJECT_NAME} PUBLIC BPtreeLib)
# the spatial indexes (R-tree, Z-order B+ tree)
target_link_libraries(${PROJECT_NAME} PUBLIC SpatialLib)
target_include_directories(${PROJECT_NAME} PRIVATE ./)

if(BPTREEDB_BUILD_TESTS)
    add_subdirectory(Tests/DBTest)
endif()
