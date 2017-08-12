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
#include <chrono>

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

#include <boost/config.hpp>
#include <boost/date_time.hpp>


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

TC_COMMON_API const char * StrNStr(const char *s1, size_t len1, const char *s2, size_t len2);

inline const char * StrNStr(const char *s1, const char *s2, size_t len2) {
    return StrNStr(s1, strlen(s1), s2, len2);
}

inline const char * StrNStr(const char *s1, size_t len1, const char *s2) {
    return StrNStr(s1, len1, s2, strlen(s2));
}

template <typename T>
inline T* pointer(T *param) {
    return param;
}

template <typename T>
inline T* pointer(T &param) {
    static_assert(!std::is_pointer<T>::value, "");
    return &param;
}

typedef uint8_t Byte;
typedef std::vector<Byte> ByteArray;
typedef std::vector<std::string> StringVector;
typedef std::map<std::string, std::string> StringMap;
typedef std::set<std::string> StringSet;

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif

#define SET_MASK(lhs, rhs)  lhs |= (rhs)
#define CLR_MASK(lhs, rhs)  lhs &= (~(rhs))
#define TST_MASK(lhs, rhs)  (((lhs) & (rhs)) == (rhs))

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&& ...args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#define BOOST_LOG_DYN_LINK
#define BOOST_REGEX_DYN_LINK

#ifndef NDEBUG
#   define BOOST_ASIO_DISABLE_BUFFER_DEBUGGING
#endif


using DateTime = boost::posix_time::ptime;
using Date = boost::gregorian::date;
using Time = boost::posix_time::time_duration;
using TimestampClock = std::chrono::steady_clock;
using Timestamp = TimestampClock::time_point;
using Duration = TimestampClock::duration;

#define HAS_CPP_FORMAT
#define HAS_RAPID_JSON


#endif //TINYCORE_COMMON_H
