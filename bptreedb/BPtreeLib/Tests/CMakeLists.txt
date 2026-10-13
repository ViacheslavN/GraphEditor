add_subdirectory(TestBPTree)
add_subdirectory(BPTreeStrings)
add_subdirectory(BPTreeBlobs)
add_subdirectory(BPTreeTypes)

option(BPTREELIB_BUILD_CATCH_TESTS "Build BPtreeLib Catch2 tests" ON)
if (BPTREELIB_BUILD_CATCH_TESTS)
    add_subdirectory(Catch)
endif()
