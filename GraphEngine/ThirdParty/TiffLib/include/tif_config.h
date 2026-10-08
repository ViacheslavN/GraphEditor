/*
  tif_config.h for GraphEngine (hand-written from libtiff/tif_config.h.cmake.in, libtiff 4.7.x).
  Library-private: included only by libtiff sources (via tiffiop.h).
*/

#include "tiffconf.h"

#define CCITT_SUPPORT 1
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1
/* #undef CHUNKY_STRIP_READ_SUPPORT */
/* #undef CXX_SUPPORT */
/* #undef DEFER_STRILE_LOAD */
/* #undef JPEG_DUAL_MODE_8_12 */
/* #undef HAVE_JPEGTURBO_DUAL_MODE_8_12 */
/* #undef LERC_SUPPORT */
/* #undef LZMA_SUPPORT */
/* #undef WEBP_SUPPORT */
/* #undef ZSTD_SUPPORT */

#define HAVE_ASSERT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_TYPES_H 1

#if defined(_WIN32)
#  define HAVE_IO_H 1
#  define HAVE_SETMODE 1
#  define HAVE_DECL_OPTARG 0
#  define USE_WIN32_FILEIO 1
#else
#  define HAVE_UNISTD_H 1
#  define HAVE_STRINGS_H 1
#  define HAVE_MMAP 1
#  define HAVE_FSEEKO 1
#  define HAVE_GETOPT 1
#  define HAVE_DECL_OPTARG 1
#endif

#define LIBJPEG_12_PATH ""

#define PACKAGE "LibTIFF Software"
#define PACKAGE_BUGREPORT "tiff@lists.osgeo.org"
#define PACKAGE_NAME "LibTIFF Software"
#define PACKAGE_TARNAME "tiff"
#define PACKAGE_URL ""

#if defined(_WIN64) || defined(__LP64__) || defined(_LP64)
#  define SIZEOF_SIZE_T 8
#else
#  define SIZEOF_SIZE_T 4
#endif

#define STRIP_SIZE_DEFAULT 8192
#define TIFF_MAX_DIR_COUNT 1048576

/* all supported targets are little-endian */
#define WORDS_BIGENDIAN 0

#if !defined(__MINGW32__)
#  define TIFF_SIZE_FORMAT "zu"
#endif
#if SIZEOF_SIZE_T == 8
#  define TIFF_SSIZE_FORMAT PRId64
#  if defined(__MINGW32__)
#    define TIFF_SIZE_FORMAT PRIu64
#  endif
#elif SIZEOF_SIZE_T == 4
#  define TIFF_SSIZE_FORMAT PRId32
#  if defined(__MINGW32__)
#    define TIFF_SIZE_FORMAT PRIu32
#  endif
#else
#  error "Unsupported size_t size"
#endif
