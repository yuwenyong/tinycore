//
// Created by yuwenyong on 17-2-4.
//

#ifndef TINYCORE_COMPILERDEFS_H
#define TINYCORE_COMPILERDEFS_H

#define PLATFORM_WINDOWS 0
#define PLATFORM_UNIX    1
#define PLATFORM_APPLE   2
#define PLATFORM_INTEL   3


#if defined( _WIN64 )
#  define PLATFORM PLATFORM_WINDOWS
#elif defined( __WIN32__ ) || defined( WIN32 ) || defined( _WIN32 )
#  define PLATFORM PLATFORM_WINDOWS
#elif defined( __APPLE_CC__ )
#  define PLATFORM PLATFORM_APPLE
#elif defined( __INTEL_COMPILER )
#  define PLATFORM PLATFORM_INTEL
#else
#  define PLATFORM PLATFORM_UNIX
#endif

#define COMPILER_MICROSOFT 0
#define COMPILER_GNU       1
#define COMPILER_BORLAND   2
#define COMPILER_INTEL     3

#ifdef _MSC_VER
#  define COMPILER COMPILER_MICROSOFT
#elif defined( __BORLANDC__ )
#  define COMPILER COMPILER_BORLAND
#elif defined( __INTEL_COMPILER )
#  define COMPILER COMPILER_INTEL
#elif defined( __GNUC__ )
#  define COMPILER COMPILER_GNU
#  define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#  error "FATAL ERROR: Unknown compiler."
#endif

#endif //TINYCORE_COMPILERDEFS_H
