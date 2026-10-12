add_subdirectory(TestBPTree)

option(BPTREELIB_BUILD_CATCH_TESTS "Build BPtreeLib Catch2 tests" ON)
if (BPTREELIB_BUILD_CATCH_TESTS)
    add_subdirectory(Catch)
endif()
