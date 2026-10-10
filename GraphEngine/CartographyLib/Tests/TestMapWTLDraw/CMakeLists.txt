cmake_minimum_required(VERSION 3.17)
project(TestMapWTLDraw)
set(BIN_NAME TestMapWTLDraw)

# WTL test application for CartographyLib: shape files as layers, background map drawing
# (CMapDrawer), pan / wheel zoom, selection, project save / load. Windows only.

set(SOURCE_FILES
        stdafx.h
        stdafx.cpp
        resource.h
        TestMapDraw.h
        TestMapDraw.cpp
        MainFrm.h
        MainFrm.cpp
        MapView.h
        MapView.cpp
        MapProject.h
        MapProject.cpp
        AboutDlg.h
        AboutDlg.cpp
        AddShapeFileDlg.h
        AddShapeFileDlg.cpp
        AddSQLiteDlg.h
        AddSQLiteDlg.cpp
        DialogUtils.h
        SymbologyModel.h
        SymbologyModel.cpp
        SymbolEditorPanel.h
        SymbolEditorPanel.cpp
        SymbologyPage.h
        SymbologyPage.cpp
        AnnotationPage.h
        AnnotationPage.cpp
        LabelPage.h
        LabelPage.cpp
        LayerPages.h
        LayerPages.cpp
        LayerGeneralPage.h
        LayerGeneralPage.cpp
        LayerDataPage.h
        LayerDataPage.cpp
        MapPropertiesDlg.h
        MapPropertiesDlg.cpp
        LayerPropertiesDlg.h
        LayerPropertiesDlg.cpp
        LayerTreePane.h
        LayerTreePane.cpp
        ConvertOSMDlg.h
        ConvertOSMDlg.cpp
        OSMProgressDlg.h
        OSMProgressDlg.cpp
        TestMapDraw.rc
)

add_executable(${BIN_NAME} WIN32 ${SOURCE_FILES})
target_compile_definitions(${BIN_NAME} PRIVATE _WINDOWS)

target_link_libraries(${BIN_NAME} PRIVATE CartographyLib)
target_link_libraries(${BIN_NAME} PRIVATE FreeType)
target_link_libraries(${BIN_NAME} PRIVATE Jpeg)

# OSM converter (Conversions menu): only when OSMConvertorLib is built (expat found); it is defined later
# (Convertors is added after CartographyLib), so generator expressions are used
target_link_libraries(${BIN_NAME} PRIVATE $<TARGET_NAME_IF_EXISTS:OSMConvertorLib>)
target_compile_definitions(${BIN_NAME} PRIVATE $<$<TARGET_EXISTS:OSMConvertorLib>:HAVE_OSM_CONVERTOR>)

target_include_directories(${BIN_NAME} PRIVATE ../../../ThirdParty/WTL)
