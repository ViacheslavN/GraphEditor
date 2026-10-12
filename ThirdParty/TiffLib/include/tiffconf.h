/*
  tiffconf.h for GraphEngine (hand-written from libtiff/tiffconf.h.cmake.in, libtiff 4.7.x).
  Targets: Windows (MSVC, x86/x64/arm64) and Android (clang, armeabi-v7a/arm64-v8a/x86/x86_64).
  Codec switches come from ThirdParty/TiffLib/CMakeLists.txt:
    GRAPHENGINE_TIFF_JPEG_SUPPORT -> JPEG_SUPPORT, OJPEG_SUPPORT
    GRAPHENGINE_TIFF_ZIP_SUPPORT  -> ZIP_SUPPORT, PIXARLOG_SUPPORT
*/

#ifndef _TIFFCONF_
#define _TIFFCONF_

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#define TIFF_INT8_T int8_t
#define TIFF_INT16_T int16_t
#define TIFF_INT32_T int32_t
#define TIFF_INT64_T int64_t
#define TIFF_UINT8_T uint8_t
#define TIFF_UINT16_T uint16_t
#define TIFF_UINT32_T uint32_t
#define TIFF_UINT64_T uint64_t

/* Signed size type */
#if defined(_WIN64) || defined(__LP64__) || defined(_LP64)
#define TIFF_SSIZE_T int64_t
#else
#define TIFF_SSIZE_T int32_t
#endif

#define HAVE_IEEEFP 1

#define HOST_FILLORDER FILLORDER_LSB2MSB

/* all supported targets are little-endian */
#define HOST_BIGENDIAN 0

#define CCITT_SUPPORT 1
#define LOGLUV_SUPPORT 1
#define LZW_SUPPORT 1
#define NEXT_SUPPORT 1
#define PACKBITS_SUPPORT 1
#define THUNDER_SUPPORT 1
#define MDI_SUPPORT 1

#ifdef GRAPHENGINE_TIFF_JPEG_SUPPORT
#define JPEG_SUPPORT 1
#define OJPEG_SUPPORT 1
#endif

#ifdef GRAPHENGINE_TIFF_ZIP_SUPPORT
#define ZIP_SUPPORT 1
#define PIXARLOG_SUPPORT 1
#endif

/* not built: JBIG_SUPPORT, LERC_SUPPORT, LIBDEFLATE_SUPPORT */

#define STRIPCHOP_DEFAULT TIFF_STRIPCHOP
#define SUBIFD_SUPPORT 1
#define DEFAULT_EXTRASAMPLE_AS_ALPHA 1
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1

#define COLORIMETRY_SUPPORT
#define YCBCR_SUPPORT
#define CMYK_SUPPORT
#define ICC_SUPPORT
#define PHOTOSHOP_SUPPORT
#define IPTC_SUPPORT

#endif /* _TIFFCONF_ */
