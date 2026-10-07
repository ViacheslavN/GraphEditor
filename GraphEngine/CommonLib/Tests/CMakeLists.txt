IF (WIN32)
    add_subdirectory(TestCrypto)
    add_subdirectory(TestSQlite)
    add_subdirectory(TestBrowsinghistory)   # uses shlobj.h
ELSE ()
    add_subdirectory(TestLog)
ENDIF()

add_subdirectory(TestStr)
add_subdirectory(TestVariant)
add_subdirectory(Catch)
