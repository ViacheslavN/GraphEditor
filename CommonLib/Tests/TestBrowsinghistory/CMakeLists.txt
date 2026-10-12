set(BIN_NAME TestBrowsinghistory)

set(SOURCE_FILES
        main.cpp
        BrowsingHistory.cpp
        HistoryFilter.cpp

)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

#set_target_properties(TestGraphicsWTL PROPERTIES LINK_FLAGS " /MANIFEST:NO")

add_executable(${BIN_NAME} ${SOURCE_FILES})
target_link_libraries(${BIN_NAME} PRIVATE CommonLib)
