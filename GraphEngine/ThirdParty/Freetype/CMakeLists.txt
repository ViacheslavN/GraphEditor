cmake_minimum_required(VERSION 3.22)
project(FreeType)
#set(LIB_NAME FreeType)



set(SOURCE_FILES
        src/autofit/autofit.c
        src/base/ftadvanc.c
        src/base/ftbbox.c
        src/base/ftbdf.c
        src/base/ftbitmap.c
        src/base/ftcalc.c
        src/base/ftcid.c
        src/base/ftdbgmem.c
        src/base/ftdebug.c
        src/base/ftfstype.c
        src/base/ftgasp.c
        src/base/ftgloadr.c
        src/base/ftglyph.c
        src/base/ftgxval.c
        src/base/ftinit.c
        src/base/ftlcdfil.c
        src/base/ftmm.c
        src/base/ftobjs.c
        src/base/ftotval.c
        src/base/ftoutln.c
        src/base/ftpatent.c
        src/base/ftpfr.c
        src/base/ftrfork.c
        src/base/ftsnames.c
        src/base/ftstream.c
        src/base/ftstroke.c
        src/base/ftsynth.c
        src/base/ftsystem.c
        src/base/fttrigon.c
        src/base/fttype1.c
        src/base/ftutil.c
        src/base/ftwinfnt.c
        src/base/ftxf86.c
        src/bdf/bdf.c
        src/bzip2/ftbzip2.c
        src/cache/ftcache.c
        src/cff/cff.c
        src/cid/type1cid.c
        src/gzip/ftgzip.c
        src/lzw/ftlzw.c
        src/pcf/pcf.c
        src/pfr/pfr.c
        src/psaux/psaux.c
        src/pshinter/pshinter.c
        src/psnames/psmodule.c
        src/raster/raster.c
        src/sfnt/sfnt.c
        src/smooth/smooth.c
        src/truetype/truetype.c
        src/type1/type1.c
        src/type42/type42.c
        src/winfonts/winfnt.c
)

include_directories("src/truetype")
include_directories("src/sfnt")
include_directories("src/autofit")
include_directories("src/smooth")
include_directories("src/raster")
include_directories("src/psaux")
include_directories("src/psnames")

if (WIN32)

            set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
            set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
            set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
            set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")

endif()


add_compile_definitions(FT2_BUILD_LIBRARY)

add_library(FreeType ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ./)
target_include_directories(${PROJECT_NAME} PUBLIC ./include)


