//
// Created by yuwenyong on 17-2-7.
//

#ifndef TINYCORE_UTIL_H
#define TINYCORE_UTIL_H

#include "tinycore/common/common.h"

#include <boost/format.hpp>

class TC_COMMON_API String {
public:
    typedef boost::format FormatType;

    static std::string format(const char *fmt) {
        return std::string(fmt);
    }

    template <typename... Args>
    static std::string format(const char *fmt, Args&&... args) {
        FormatType fmter(fmt);
        _format(fmter, std::forward<Args>(args)...);
        return fmter.str();
    }

protected:
    template <typename T, typename... Args>
    static void _format(FormatType &fmter, T &&value, Args&&... args) {
        fmter % value;
        _format(fmter, std::forward<Args>(args)...);
    }

    template <typename T>
    static void _format(FormatType &fmter, T &&value) {
        fmter % value;
    }
};

#endif //TINYCORE_UTIL_H
