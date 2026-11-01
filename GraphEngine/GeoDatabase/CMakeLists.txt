cmake_minimum_required(VERSION 3.22)
project(GeoDatabase)
#set(LIB_NAME GeoDatabase)


# Row filter (ISQLRowFilter): SQL conditions checked on rows in memory. The parser (bison) and the scanner (flex)
# are generated into SQLRowFilter/Grammar and kept in the tree (Grammar/generate.bat / generate.sh regenerate them).
# They are generated in the build when they are missing or with GRAPHENGINE_GENERATE_SQL_GRAMMAR=ON:
# bison 3.8+ and flex 2.6+ (win_bison / win_flex of win_flex_bison on Windows).
set(SQL_ROW_FILTER_GRAMMAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SQLRowFilter/Grammar)
set(SOURCE_FILES_SQL_ROW_FILTER
        SQLRowFilter/SQLValue.h
        SQLRowFilter/SQLValue.cpp
        SQLRowFilter/SQLExpr.h
        SQLRowFilter/SQLExpr.cpp
        SQLRowFilter/SQLExprDriver.h
        SQLRowFilter/SQLExprDriver.cpp
        SQLRowFilter/SQLRowFilter.h
        SQLRowFilter/SQLRowFilter.cpp
        SQLRowFilter/Grammar/SQLExprParser.hpp
        SQLRowFilter/Grammar/SQLExprParser.cpp
        SQLRowFilter/Grammar/SQLExprLexer.cpp
)

option(GRAPHENGINE_GENERATE_SQL_GRAMMAR "Regenerate the SQL row filter parser and scanner with bison / flex" OFF)
if(GRAPHENGINE_GENERATE_SQL_GRAMMAR OR NOT EXISTS ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprParser.cpp
        OR NOT EXISTS ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprLexer.cpp)
    find_package(BISON 3.8)
    find_package(FLEX 2.6)
    if(NOT BISON_FOUND OR NOT FLEX_FOUND)
        message(FATAL_ERROR "SQLRowFilter: bison 3.8+ and flex 2.6+ are needed to generate SQLRowFilter/Grammar "
                            "(win_flex_bison on Windows: choco install winflexbison3 or vcpkg install winflexbison), "
                            "or run SQLRowFilter/Grammar/generate.bat once")
    endif()
    # run in the grammar folder with relative names: the generated files don't get the paths of the build
    add_custom_command(
            OUTPUT ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprParser.cpp ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprParser.hpp
            COMMAND ${BISON_EXECUTABLE} -Wall --no-lines -o SQLExprParser.cpp --header=SQLExprParser.hpp SQLExprParser.y
            DEPENDS ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprParser.y
            WORKING_DIRECTORY ${SQL_ROW_FILTER_GRAMMAR_DIR}
            COMMENT "bison: SQLExprParser.y")
    add_custom_command(
            OUTPUT ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprLexer.cpp
            COMMAND ${FLEX_EXECUTABLE} --noline -o SQLExprLexer.cpp SQLExprLexer.l
            DEPENDS ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprLexer.l ${SQL_ROW_FILTER_GRAMMAR_DIR}/SQLExprParser.hpp
            WORKING_DIRECTORY ${SQL_ROW_FILTER_GRAMMAR_DIR}
            COMMENT "flex: SQLExprLexer.l")
endif()


set(COMMON_SOURCE_FILES
        GetDatabase.cpp
        Fields.h
        Fields.cpp
        FieldSet.h
        FieldSet.cpp
        OIDSet.h
        OIDSet.cpp
        GeometryDefinition.h
        GeometryDefinition.cpp
        InsertCursorBase.h
        InsertCursorBase.cpp
        GeoDatabaseShape/ShapefileUtils.h
        GeoDatabaseShape/ShapefileUtils.cpp
        TableBase.h
        GeoDatabaseShape/SHPObject.h
        DataSetBase.h
        GeoDatabaseShape/ShapefileSpatialTable.h
        GeoDatabaseShape/ShapeFile.h
        GeoDatabaseShape/ShapeFile.cpp
        GeoDatabaseShape/SHPObject.cpp
        GeoDatabaseShape/ShapeDBFile.h
        GeoDatabaseShape/ShapeDBFile.cpp
        GeoDatabaseShape/ShapefileRowCursor.h
        GeoDatabaseShape/ShapefileRowCursor.cpp
        BaseRow.h
        Row.h
        Row.cpp
        DatasetLoader.h
        DatasetLoader.cpp
        WorkspaceHolder.h
        WorkspaceHolder.cpp
        TableCopier.h
        TableCopier.cpp
        GeoDatabaseShape/ShapeTree.h
        GeoDatabaseShape/ShapeTree.cpp
        CursorBase.h
        GeoDatabaseShape/ShapefileSpatialTable.cpp
        Field.h
        Field.cpp
        QueryFilter.h
        QueryFilter.cpp
        GeoDatabaseShape/ShapefileWorkspace.h
        GeoDatabaseShape/ShapefileWorkspace.cpp
        WorkspaceBase.h
        GeoDatabaseSQlite/SQLiteWorkspace.h
        GeoDatabaseSQlite/SQLiteWorkspace.cpp
        GeoDatabaseSQlite/SQLiteTable.h
        GeoDatabaseSQlite/SQLiteTable.cpp
        GeoDatabaseSQlite/SQLiteTableBase.h
        GeoDatabaseSQlite/SQLiteUtils.h
        GeoDatabaseUtils.h
        GeoDatabaseUtils.cpp
        GeoDatabaseSQlite/SQLiteSelectCursor.h
        GeoDatabaseSQlite/SQLiteSelectCursor.cpp
        GeoDatabaseSQlite/SQLiteUtils.cpp
        GeoDatabaseSQlite/SQLiteInsertCursor.h
        GeoDatabaseSQlite/SQLiteInsertCursor.cpp
        GeoDatabaseSQlite/SQLiteRTreeBuilder.h
        GeoDatabaseSQlite/SQLiteRTreeBuilder.cpp
        GeoDatabaseSQlite/SQLiteTransaction.h
        GeoDatabaseSQlite/SQLiteTransaction.cpp
        Join.h
        Jont.cpp
        GeoDatabaseSQlite/SQLiteSpatialTable.h
        GeoDatabaseSQlite/SQLiteSpatialTable.cpp
        Utils.h
        utils.cpp
        GeoDatabaseRaster/RasterReader.h
        GeoDatabaseRaster/TIFF/TIFF.h
        GeoDatabaseRaster/TIFF/TIFF.cpp
        GeoDatabaseRaster/TIFFReader.h
        GeoDatabaseRaster/TIFFReader.cpp
        GeoDatabaseRaster/RasterBlock.h
        GeoDatabaseRaster/RasterBlock.cpp
        GeoDatabaseRaster/RasterSpatialFilter.h
        GeoDatabaseRaster/RasterSpatialFilter.cpp
        GeoDatabaseRaster/RasterCursor.h
        GeoDatabaseRaster/RasterCursor.cpp
        GeoDatabaseRaster/RasterDataset.h
        GeoDatabaseRaster/RasterDataset.cpp
        GeoDatabaseRaster/RasterWorkspace.h
        GeoDatabaseRaster/RasterWorkspace.cpp
        GeoDatabaseRaster/TIFF/TIFF.cpp
        GeoDatabaseBPTreeDB/BPtreeDBContext.h
        GeoDatabaseBPTreeDB/BPtreeDBContext.cpp
        GeoDatabaseBPTreeDB/BPtreeDBUtils.h
        GeoDatabaseBPTreeDB/BPtreeDBUtils.cpp
        GeoDatabaseBPTreeDB/CBPtreeDBWorkspace.h
        GeoDatabaseBPTreeDB/CBPtreeDBWorkspace.cpp
        GeoDatabaseBPTreeDB/BPtreeDBTableBase.h
        GeoDatabaseBPTreeDB/BPtreeDBTable.h
        GeoDatabaseBPTreeDB/BPtreeDBTable.cpp
        GeoDatabaseBPTreeDB/BPtreeDBSpatialTable.h
        GeoDatabaseBPTreeDB/BPtreeDBSpatialTable.cpp
        GeoDatabaseBPTreeDB/BPtreeDBSelectCursor.h
        GeoDatabaseBPTreeDB/BPtreeDBSelectCursor.cpp
        GeoDatabaseBPTreeDB/BPtreeDBInsertCursor.h
        GeoDatabaseBPTreeDB/BPtreeDBInsertCursor.cpp
        GeoDatabaseBPTreeDB/BPtreeDBTransaction.h
        GeoDatabaseBPTreeDB/BPtreeDBTransaction.cpp
        ${SOURCE_FILES_SQL_ROW_FILTER}
)

set(WINDOWS_SOURCE_FILES
)

IF (WIN32)
    set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${WINDOWS_SOURCE_FILES})

ELSE()
    set(SOURCE_FILES ${COMMON_SOURCE_FILES}


    )

ENDIF()


add_library(GeoDatabase ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC GeometryCompression)
target_link_libraries(${PROJECT_NAME} PUBLIC ShapeLib)
target_link_libraries(${PROJECT_NAME} PUBLIC Tiff)
target_link_libraries(${PROJECT_NAME} PUBLIC DBLib)      # bptreedb (GeoDatabaseBPTreeDB)

if(GRAPHENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
