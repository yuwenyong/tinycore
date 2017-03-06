//
// Created by yuwenyong on 17-2-4.
//

#ifndef TINYCORE_COMMON_H
#define TINYCORE_COMMON_H

#include "tinycore/common/define.h"

#include <algorithm>
#include <array>
#include <exception>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <numeric>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <csignal>

#if PLATFORM == PLATFORM_WINDOWS
#  include <ws2tcpip.h>

#  if defined(__INTEL_COMPILER)
#    if !defined(BOOST_ASIO_HAS_MOVE)
#      define BOOST_ASIO_HAS_MOVE
#    endif // !defined(BOOST_ASIO_HAS_MOVE)
#  endif // if defined(__INTEL_COMPILER)

#else
#  include <sys/types.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#  include <netdb.h>
#endif

#include <boost/noncopyable.hpp>

#if COMPILER == COMPILER_MICROSOFT

#include <float.h>

#define snprintf _snprintf
#define atoll _atoi64
#define vsnprintf _vsnprintf
#define llabs _abs64

#else

#define stricmp strcasecmp
#define strnicmp strncasecmp

#endif

inline unsigned long atoul(char const* str) {
    return strtoul(str, nullptr, 10);
}

inline unsigned long long atoull(char const* str) {
    return strtoull(str, nullptr, 10);
}

#define STRINGIZE(a) #a

TC_COMMON_API const char * strnstr(const char *s1, const char *s2, size_t len2);

typedef std::vector<std::string> StringVector;

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&& ...args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#define BOOST_LOG_DYN_LINK

typedef boost::noncopyable NonCopyable;

#endif //TINYCORE_COMMON_H
