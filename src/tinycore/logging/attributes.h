//
// Created by yuwenyong on 17-8-4.
//

#ifndef TINYCORE_ATTRIBUTES_H
#define TINYCORE_ATTRIBUTES_H

#include "tinycore/common/common.h"
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/string_literal.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/log/trivial.hpp>
#include <boost/phoenix.hpp>


namespace logging = boost::log;
namespace expr = boost::log::expressions;


enum LogLevel {
    LOG_LEVEL_UNSET,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
};


template< typename CharT, typename TraitsT >
std::basic_ostream< CharT, TraitsT >& operator<< (std::basic_ostream< CharT, TraitsT>& strm, LogLevel lvl) {
    static const char* const str[] = {
            "UNSET",
            "DEBUG",
            "INFO ",
            "WARN ",
            "ERROR",
            "FATAL",
    };
    if (static_cast<size_t>(lvl) < (sizeof(str) / sizeof(*str))) {
        strm << str[lvl];
    } else {
        strm << static_cast<int>(lvl);
    }
    return strm;
}


BOOST_LOG_ATTRIBUTE_KEYWORD(attr_severity, "Severity", LogLevel)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_timestamp, "TimeStamp", DateTime)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_file, "File", logging::string_literal)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_line, "Line", size_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_func, "Function", logging::string_literal)


class TimeStampFormatterFactory: public logging::basic_formatter_factory<char, DateTime> {
public:
    formatter_type create_formatter(const logging::attribute_name &name, const args_map &args) {
        auto it = args.find("format");
        if (it != args.end()) {
            return expr::stream << expr::format_date_time<DateTime>(expr::attr<DateTime>(name), it->second);
        } else {
            return expr::stream << expr::attr<DateTime>(name);
        }
    }
};


class UptimeFormatterFactory: public logging::basic_formatter_factory<char, Time> {
public:
    formatter_type create_formatter(const logging::attribute_name &name, const args_map &args) {
        auto it = args.find("format");
        if (it != args.end()) {
            return expr::stream << expr::format_date_time<Time>(expr::attr<Time>(name), it->second);
        } else {
            return expr::stream << expr::attr<Time>(name);
        }
    }
};


class ChannelFilterFactory: public logging::filter_factory<char> {
public:
    logging::filter on_custom_relation(logging::attribute_name const& name, string_type const& rel,
                                       string_type const& arg) {
        if (rel == "child_of") {
            return boost::phoenix::bind(&childOf, expr::attr<string_type>(name), arg);
        }
        throw std::runtime_error("Unsupported filter relation: " + rel);
    }

    static bool childOf(const logging::value_ref<string_type> &channel, const string_type &r) {
        return isChildOf(channel.get(), r);
    }

    static bool isChildOf(const string_type &channel, const string_type &r);
};


#endif //TINYCORE_ATTRIBUTES_H
