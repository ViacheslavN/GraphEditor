set(BIN_NAME TestCrypto)

set(SOURCE_FILES
        TestCrypto.cpp
        TestXTS.cpp
)

add_executable(${BIN_NAME} ${SOURCE_FILES})
target_link_libraries(${BIN_NAME} PRIVATE CommonLib)