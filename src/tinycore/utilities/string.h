//
// Created by yuwenyong on 17-3-30.
//

#ifndef TINYCORE_STRING_H
#define TINYCORE_STRING_H

#include "tinycore/common/common.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>


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

    static StringVector split(const std::string &s, bool keepEmpty=true) {
        StringVector result;
        boost::split(result, s, boost::is_space(), keepEmpty?boost::token_compress_off:boost::token_compress_on);
        return result;
    }

    static StringVector split(const std::string &s, char delim, bool keepEmpty=true) {
        StringVector result;
        boost::split(result, s, [delim](char c) {
            return c == delim;
        }, keepEmpty?boost::token_compress_off:boost::token_compress_on);
        return result;
    }

    static StringVector split(const std::string &s, const std::string &delim, bool keepEmtpy=true);
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

#endif //TINYCORE_STRING_H
