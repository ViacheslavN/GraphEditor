cmake_minimum_required(VERSION 3.22)
project(FreeType)
set(LIB_NAME FreeType)



set(SOURCE_FILES
        src/autofit/afangles.c
        src/autofit/afcjk.c
        src/autofit/afdummy.c
        src/autofit/afglobal.c
        src/autofit/afhints.c
        src/autofit/afindic.c
        src/autofit/aflatin.c
        src/autofit/afloader.c
        src/autofit/afmodule.c
        src/autofit/afwarp.c
        src/base/ftbitmap.c
        src/base/ftcalc.c
        src/base/ftgloadr.c
        src/base/ftglyph.c
        src/base/ftinit.c
        src/base/ftobjs.c
        src/base/ftoutln.c
        src/base/ftrfork.c
        src/base/ftstream.c
        src/base/ftsystem.c
        src/base/fttrigon.c
        src/base/ftutil.c
        src/bdf/bdfdrivr.c
        src/bdf/bdflib.c
        src/cff/cffcmap.c
        src/cff/cffdrivr.c
        src/cff/cffgload.c
        src/cff/cffload.c
        src/cff/cffobjs.c
        src/cff/cffparse.c
        src/cid/cidgload.c
        src/cid/cidload.c
        src/cid/cidobjs.c
        src/cid/cidparse.c
        src/cid/cidriver.c
        src/gzip/ftgzip.c
        src/lzw/ftlzw.c
        src/pcf/pcfdrivr.c
        src/pcf/pcfread.c
        src/pcf/pcfutil.c
        src/pfr/pfrcmap.c
        src/pfr/pfrdrivr.c
        src/pfr/pfrgload.c
        src/pfr/pfrload.c
        src/pfr/pfrobjs.c
        src/pfr/pfrsbit.c
        src/pshinter/pshalgo.c
        src/pshinter/pshglob.c
        src/pshinter/pshmod.c
        src/pshinter/pshrec.c
        src/psaux/afmparse.c
        src/psaux/psauxmod.c
        src/psaux/psconv.c
        src/psaux/psobjs.c
        src/psaux/t1cmap.c
        src/psaux/t1decode.c
        src/psnames/psmodule.c
        src/raster/ftraster.c
        src/raster/ftrend1.c
        src/sfnt/sfdriver.c
        src/sfnt/sfobjs.c
        src/sfnt/ttbdf.c
        src/sfnt/ttcmap.c
        src/sfnt/ttkern.c
        src/sfnt/ttload.c
        src/sfnt/ttmtx.c
        src/sfnt/ttpost.c
        src/sfnt/ttsbit.c
        src/smooth/ftgrays.c
        src/smooth/ftsmooth.c
        src/truetype/ttdriver.c
        src/truetype/ttgload.c
        src/truetype/ttgxvar.c
        src/truetype/ttinterp.c
        src/truetype/ttobjs.c
        src/truetype/ttpload.c
        src/type1/t1afm.c
        src/type1/t1driver.c
        src/type1/t1gload.c
        src/type1/t1load.c
        src/type1/t1objs.c
        src/type1/t1parse.c
        src/type42/t42drivr.c
        src/type42/t42objs.c
        src/type42/t42parse.c
        src/winfonts/winfnt.c
)

add_compile_definitions(FT2_BUILD_LIBRARY)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ./)
target_include_directories(${PROJECT_NAME} PUBLIC ./include)


