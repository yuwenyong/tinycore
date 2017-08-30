//
// Created by yuwenyong on 17-2-4.
//

#ifndef TINYCORE_DEFINE_H
#define TINYCORE_DEFINE_H

#include "tinycore/common/compilerdefs.h"

#include <cstddef>
#include <cinttypes>
#include <climits>

#define TINY_LITTLEENDIAN 0
#define TINY_BIGENDIAN    1

#if !defined(TINY_ENDIAN)
#  if defined (BOOST_BIG_ENDIAN)
#    define TINY_ENDIAN TINY_BIGENDIAN
#  else
#    define TINY_ENDIAN TINY_LITTLEENDIAN
#  endif
#endif

#if PLATFORM == PLATFORM_WINDOWS
#  define TINY_PATH_MAX MAX_PATH
#  define _USE_MATH_DEFINES
#  ifndef DECLSPEC_NORETURN
#    define DECLSPEC_NORETURN __declspec(noreturn)
#  endif //DECLSPEC_NORETURN
#  ifndef DECLSPEC_DEPRECATED
#    define DECLSPEC_DEPRECATED __declspec(deprecated)
#  endif //DECLSPEC_DEPRECATED
#else //PLATFORM != PLATFORM_WINDOWS
#  define TINY_PATH_MAX PATH_MAX
#  define DECLSPEC_NORETURN
#  define DECLSPEC_DEPRECATED
#endif //PLATFORM

#if !defined(COREDEBUG)
#  define TINY_INLINE inline
#else //COREDEBUG
#  if !defined(TINY_DEBUG)
#    define TINY_DEBUG
#  endif //TINY_DEBUG
#  define TINY_INLINE
#endif //!COREDEBUG

#if COMPILER == COMPILER_GNU
#  define ATTR_NORETURN __attribute__((__noreturn__))
#  define ATTR_PRINTF(F, V) __attribute__ ((__format__ (__printf__, F, V)))
#  define ATTR_DEPRECATED __attribute__((__deprecated__))
#else //COMPILER != COMPILER_GNU
#  define ATTR_NORETURN
#  define ATTR_PRINTF(F, V)
#  define ATTR_DEPRECATED
#endif //COMPILER == COMPILER_GNU

#ifdef TINY_API_USE_DYNAMIC_LINKING
#  if COMPILER == COMPILER_MICROSOFT
#    define TC_API_EXPORT __declspec(dllexport)
#    define TC_API_IMPORT __declspec(dllimport)
#  elif COMPILER == COMPILER_GNU
#    define TC_API_EXPORT __attribute__((visibility("default")))
#    define TC_API_IMPORT
#  else
#    error compiler not supported!
#  endif
#else
#  define TC_API_EXPORT
#  define TC_API_IMPORT
#endif

#ifdef TINY_API_EXPORT_COMMON
#  define TC_COMMON_API TC_API_EXPORT
#else
#  define TC_COMMON_API TC_API_IMPORT
#endif

typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;


#ifndef NDEBUG
#	ifndef _DEBUG
#		define _DEBUG
#	endif
#	ifndef DEBUG
#		define DEBUG
#	endif
#   ifndef _GLIBCXX_DEBUG
#       define _GLIBCXX_DEBUG
#   endif
#endif

#define TINYCORE_VERSION      "2.1.0"
#define TINYCORE_VER          "tinycore/" TINYCORE_VERSION

#endif //TINYCORE_DEFINE_H
