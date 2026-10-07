add_subdirectory(TestGraphicsAgg)
if(WIN32)
    add_subdirectory(TestGraphicsWTL)   # WTL UI test, Windows only
endif()
add_subdirectory(Catch)
