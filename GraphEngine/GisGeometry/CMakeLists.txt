cmake_minimum_required(VERSION 3.22)
project(GisGeometry)
#set(LIB_NAME GisGeometry)




#set(CPL_SOURCE_FILES
#        cpl/cpl_atomic_ops.cpp cpl/cpl_conv.cpp cpl/cpl_csv.cpp cpl/cpl_error.cpp cpl/cpl_findfile.cpp cpl/cpl_http.cpp cpl/cpl_minixml.cpp cpl/cpl_multiproc.cpp
#        cpl/cpl_path.cpp cpl/cpl_recode.cpp cpl/cpl_recode_stub.cpp cpl/cpl_string.cpp cpl/cpl_strtod.cpp cpl/cpl_vsi_mem.cpp cpl/cpl_vsil.cpp cpl/cpl_vsil_abstract_archive.cpp
#        cpl/cpl_vsil_cache.cpp cpl/cpl_vsil_sparsefile.cpp cpl/cpl_vsil_stdin.cpp cpl/cpl_vsil_stdout.cpp cpl/cpl_vsil_subfile.cpp cpl/cpl_vsil_tar.cpp cpl/cpl_vsisimple.cpp cpl/cplgetsymbol.cpp
#        cpl/cplstring.cpp cpl/cplstringlist.cpp cpl/cpl_vsil_unix_stdio_64.cpp)

#set(OGR_SOURCE_FILES
#        ogr/ogr_fromepsg.cpp ogr/ogr_srs_dict.cpp ogr/ogr_srs_esri.cpp ogr/ogr_srs_proj4.cpp ogr/ogr_srs_xml.cpp ogr/ogr_srsnode.cpp ogr/ogrct.cpp ogr/ogrspatialreference.cpp)

include_directories("cpl")
include_directories("ogr")


set(SOURCE_FILES
        SpatialReferenceProj4/SpatialReferenceProj4.h
        SpatialReferenceProj4/SpatialReferenceProj4.cpp
        SpatialReferenceProj4/CodeProj.h
        SpatialReferenceProj4/CodeProj.cpp
        Envelope.h
        Envelope.cpp)



add_library(GisGeometry ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC CommonLib)
target_link_libraries(${PROJECT_NAME} PUBLIC Proj4)

target_include_directories(${PROJECT_NAME} PRIVATE ./)
target_include_directories(${PROJECT_NAME} PUBLIC ./include)


