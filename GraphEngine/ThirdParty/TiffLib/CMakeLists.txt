cmake_minimum_required(VERSION 3.22)
project(Tiff C)

# libtiff (ThirdParty/tiff, 4.7.x) built as a static library for GraphEngine (Windows + Android).
# The upstream libtiff CMake project is not used: it builds shared libs/tools/tests by default and
# probes the system for codecs. Here the configuration headers are fixed (TiffLib/include) and
# the codecs are:
#   always   : None, PackBits, LZW, CCITT G3/G4, ThunderScan, NeXT, LogLuv, Predictor
#   JPEG     : new-style JPEG + Old-JPEG via ThirdParty/Jpeg (IJG libjpeg 9)
#   Deflate  : zlib (+ PixarLog) when found - always on Android (NDK system libz),
#              on Windows only if find_package(ZLIB) finds one (e.g. vcpkg); otherwise disabled
#   not built: LZMA, ZSTD, WebP, JBIG, LERC, 12-bit JPEG

set(TIFF_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../tiff/libtiff)

option(GRAPHENGINE_TIFF_JPEG "libtiff: JPEG / Old-JPEG codecs (ThirdParty/Jpeg)" ON)
option(GRAPHENGINE_TIFF_ZLIB "libtiff: Deflate / PixarLog codecs (needs zlib)" ON)

set(SOURCE_FILES
        ${TIFF_SRC_DIR}/tif_aux.c
        ${TIFF_SRC_DIR}/tif_close.c
        ${TIFF_SRC_DIR}/tif_codec.c
        ${TIFF_SRC_DIR}/tif_color.c
        ${TIFF_SRC_DIR}/tif_compress.c
        ${TIFF_SRC_DIR}/tif_dir.c
        ${TIFF_SRC_DIR}/tif_dirinfo.c
        ${TIFF_SRC_DIR}/tif_dirread.c
        ${TIFF_SRC_DIR}/tif_dirwrite.c
        ${TIFF_SRC_DIR}/tif_dumpmode.c
        ${TIFF_SRC_DIR}/tif_error.c
        ${TIFF_SRC_DIR}/tif_extension.c
        ${TIFF_SRC_DIR}/tif_fax3.c
        ${TIFF_SRC_DIR}/tif_fax3sm.c
        ${TIFF_SRC_DIR}/tif_flush.c
        ${TIFF_SRC_DIR}/tif_getimage.c
        ${TIFF_SRC_DIR}/tif_hash_set.c
        ${TIFF_SRC_DIR}/tif_jbig.c
        ${TIFF_SRC_DIR}/tif_jpeg.c
        ${TIFF_SRC_DIR}/tif_lerc.c
        ${TIFF_SRC_DIR}/tif_luv.c
        ${TIFF_SRC_DIR}/tif_lzma.c
        ${TIFF_SRC_DIR}/tif_lzw.c
        ${TIFF_SRC_DIR}/tif_next.c
        ${TIFF_SRC_DIR}/tif_ojpeg.c
        ${TIFF_SRC_DIR}/tif_open.c
        ${TIFF_SRC_DIR}/tif_packbits.c
        ${TIFF_SRC_DIR}/tif_pixarlog.c
        ${TIFF_SRC_DIR}/tif_predict.c
        ${TIFF_SRC_DIR}/tif_print.c
        ${TIFF_SRC_DIR}/tif_read.c
        ${TIFF_SRC_DIR}/tif_strip.c
        ${TIFF_SRC_DIR}/tif_swab.c
        ${TIFF_SRC_DIR}/tif_thunder.c
        ${TIFF_SRC_DIR}/tif_tile.c
        ${TIFF_SRC_DIR}/tif_version.c
        ${TIFF_SRC_DIR}/tif_warning.c
        ${TIFF_SRC_DIR}/tif_webp.c
        ${TIFF_SRC_DIR}/tif_write.c
        ${TIFF_SRC_DIR}/tif_zip.c
        ${TIFF_SRC_DIR}/tif_zstd.c
)

if(WIN32)
    set(TIFF_IO_FILE ${TIFF_SRC_DIR}/tif_win32.c)   # Win32 file API, provides TIFFOpenW
else()
    set(TIFF_IO_FILE ${TIFF_SRC_DIR}/tif_unix.c)
endif()
list(APPEND SOURCE_FILES ${TIFF_IO_FILE})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

# same as upstream: only the file-io and open units may use the non-"Ext" alloc functions
target_compile_definitions(${PROJECT_NAME} PRIVATE TIFF_DO_NOT_USE_NON_EXT_ALLOC_FUNCTIONS)
set_property(SOURCE ${TIFF_SRC_DIR}/tif_open.c ${TIFF_IO_FILE}
             APPEND PROPERTY COMPILE_DEFINITIONS ALLOW_TIFF_NON_EXT_ALLOC_FUNCTIONS)

# include/ (tiffconf.h, tif_config.h) must come before the libtiff source folder
target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${TIFF_SRC_DIR})

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4244 /wd4267 /wd4018 /wd4996)
endif()

if(GRAPHENGINE_TIFF_JPEG)
    target_compile_definitions(${PROJECT_NAME} PUBLIC GRAPHENGINE_TIFF_JPEG_SUPPORT)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Jpeg)
    target_link_libraries(${PROJECT_NAME} PUBLIC Jpeg)
endif()

if(GRAPHENGINE_TIFF_ZLIB)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        target_compile_definitions(${PROJECT_NAME} PUBLIC GRAPHENGINE_TIFF_ZIP_SUPPORT)
        target_link_libraries(${PROJECT_NAME} PUBLIC ZLIB::ZLIB)
        message(STATUS "Tiff: Deflate/PixarLog enabled (zlib: ${ZLIB_LIBRARIES})")
    else()
        message(STATUS "Tiff: zlib not found - Deflate/PixarLog compressed TIFFs are not supported")
    endif()
endif()

if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC m)
endif()
