cmake_minimum_required(VERSION 3.17)
project(DisplayLib)


set(AGG_SOURCE_FILES
        Agg/agg_arc.cpp
        Agg/agg_arrowhead.cpp
        Agg/agg_bezier_arc.cpp
        Agg/agg_bspline.cpp
        Agg/agg_curves.cpp
        Agg/agg_embedded_raster_fonts.cpp
        Agg/agg_font_freetype.cpp
        Agg/agg_gsv_text.cpp
        Agg/agg_image_filters.cpp
        Agg/agg_line_aa_basics.cpp
        Agg/agg_line_profile_aa.cpp
        Agg/agg_rounded_rect.cpp
        Agg/agg_sqrt_tables.cpp
        Agg/agg_trans_affine.cpp
        Agg/agg_trans_double_path.cpp
        Agg/agg_trans_single_path.cpp
        Agg/agg_trans_warp_magnifier.cpp
        Agg/agg_vcgen_bspline.cpp
        Agg/agg_vcgen_contour.cpp
        Agg/agg_vcgen_dash.cpp
        Agg/agg_vcgen_markers_term.cpp
        Agg/agg_vcgen_smooth_poly1.cpp
        Agg/agg_vcgen_stroke.cpp
        Agg/agg_vpgen_clip_polygon.cpp
        Agg/agg_vpgen_clip_polyline.cpp
        Agg/agg_vpgen_segmentator.cpp
        Agg/polygon_clipper.cpp
        Agg/sin_cos_table.cpp
)

set(COMMON_SOURCE_FILES
        Clip/ClipRectAlloc.cpp
        Clip/RectClipper.cpp
        Clip/ClipPolygon.cpp
        Clip/ClipLine.cpp
        Clip/PointsBuffer.cpp
        DisplayLib.h

        GraphTypes/GraphTypes.h
        GraphTypes/Bitmap.cpp
        GraphTypes/Color.cpp
        GraphTypes/Pen.cpp
        GraphTypes/Point.cpp
        GraphTypes/Rect.cpp

        DisplayMath.h
        DisplayMath.cpp
        GraphTypes/img/ReadPng.h
        GraphTypes/img/ReadPng.cpp
        GraphTypes/img/ReadJPG.h
        GraphTypes/img/ReadJPG.cpp
        GraphTypes/Font.h
        GraphTypes/Font.cpp
        GraphTypes/Brush.h
        GraphTypes/Brush.cpp
        Graphics.h
        DisplayTypes.h
        Graphics/GraphicsAgg/GraphicsAgg.h
        Graphics/GraphicsAgg/GraphicsAgg.cpp
        Graphics/GraphicsAgg/VertexSrc.h
        Graphics/GraphicsAgg/VertexSrc.cpp
        Graphics/GraphicsAgg/ConvOffset.h
        Graphics/GraphicsAgg/ConvOffset.cpp
        Symbols.h
        Transformation/DisplayTransformation2D.h
        Transformation/DisplayTransformation2D.cpp
        Transformation/Matrix4.h
        Transformation/Vector3d.h
        Display/Display.cpp
        Display/Display.h
        Symbols/SymbolBase.h
        Symbols/SymbolFillBase.h
        Symbols/SymbolsLoader.h
        Symbols/SymbolsLoader.cpp
        Symbols/SimpleLineSymbol.h
        Symbols/SimpleLineSymbol.cpp
        Symbols/MarkerSymbolBase.h
        Symbols/SimpleFillSymbol.cpp
        Symbols/SimpleFillSymbol.h
        Symbols/SimpleMarketSymbol.h
        Symbols/SimpleMarketSymbol.cpp
        DisplayUtils.h
        DisplayUtils.cpp
        Symbols/TextSymbol.h
        Symbols/TextSymbol.cpp
)

set(LINUX_SOURCE_FILES

)

set(WINDOWS_SOURCE_FILES
        Graphics/GraphicsWinGDI/GraphicsWinGDI.h
        Graphics/GraphicsWinGDI/GraphicsWinGDI.cpp

)


IF (WIN32)
    set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${WINDOWS_SOURCE_FILES} ${AGG_SOURCE_FILES})
ELSE()
    set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${LINUX_SOURCE_FILES} ${AGG_SOURCE_FILES}

    )
ENDIF()

add_library(DisplayLib ${SOURCE_FILES})


target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC FreeType)
target_link_libraries(${PROJECT_NAME} PUBLIC Jpeg)

target_include_directories(${PROJECT_NAME} PRIVATE ./)

add_subdirectory(Tests)