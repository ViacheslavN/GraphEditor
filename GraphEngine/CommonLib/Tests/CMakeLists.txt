IF (WIN32)
    add_subdirectory(TestCrypto)
    add_subdirectory(TestSQlite)
ELSE ()
    add_subdirectory(TestLog)
ENDIF()

add_subdirectory(TestStr)
